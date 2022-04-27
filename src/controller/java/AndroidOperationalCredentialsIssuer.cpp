/*
 *
 *    Copyright (c) 2021-2022 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AndroidOperationalCredentialsIssuer.h"
#include <algorithm>
#include <credentials/CHIPCert.h>
#include <lib/core/CASEAuthTag.h>
#include <lib/core/CHIPTLV.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/PersistentStorageMacros.h>
#include <lib/support/SafeInt.h>
#include <lib/support/ScopedBuffer.h>
#include <lib/support/TestGroupData.h>

#include <lib/support/CHIPJNIError.h>
#include <lib/support/JniReferences.h>

namespace chip {
namespace Controller {
constexpr const char kOperationalCredentialsIssuerKeypairStorage[]             = "AndroidCARootKey";
constexpr const char kOperationalCredentialsIntermediateIssuerKeypairStorage[] = "AndroidCAIntermediateKey";
constexpr const char kOperationalCredentialsRootCertificateStorage[]           = "AndroidCARootCert";
constexpr const char kOperationalCredentialsIntermediateCertificateStorage[]   = "AndroidCAIntermediateCert";

using namespace Credentials;
using namespace Crypto;
using namespace TLV;

CHIP_ERROR AndroidOperationalCredentialsIssuer::Initialize(PersistentStorageDelegate & storage, jobject javaObjectRef)
{
    using namespace ASN1;
    ASN1UniversalTime effectiveTime;

    // Initializing the default start validity to start of 2021. The default validity duration is 10 years.
    CHIP_ZERO_AT(effectiveTime);
    effectiveTime.Year  = 2021;
    effectiveTime.Month = 6;
    effectiveTime.Day   = 10;
    ReturnErrorOnFailure(ASN1ToChipEpochTime(effectiveTime, mNow));

    Crypto::P256SerializedKeypair serializedKey;
    uint16_t keySize = serializedKey.Capacity();

    if (storage.SyncGetKeyValue(kOperationalCredentialsIssuerKeypairStorage, serializedKey.Bytes(), keySize) != CHIP_NO_ERROR)
    {
        // Orange customization: key pair must be pre loaded
        ChipLogProgress(Controller, "Root certificate key pair is not found in storage");
        ReturnErrorOnFailure(CHIP_ERROR_NOT_IMPLEMENTED);

        // Storage doesn't have an existing keypair. Let's create one and add it to the storage.
        ReturnErrorOnFailure(mIssuer.Initialize());
        ReturnErrorOnFailure(mIssuer.Serialize(serializedKey));

        keySize = static_cast<uint16_t>(sizeof(serializedKey));
        ReturnErrorOnFailure(storage.SyncSetKeyValue(kOperationalCredentialsIssuerKeypairStorage, serializedKey.ConstBytes(), serializedKey.Length()));
    }
    else
    {
        // Use the keypair from the storage
        serializedKey.SetLength(keySize);
        ReturnErrorOnFailure(mIssuer.Deserialize(serializedKey));
    }

    keySize = serializedKey.Capacity();
    if (storage.SyncGetKeyValue(kOperationalCredentialsIntermediateIssuerKeypairStorage, serializedKey.Bytes(), keySize) != CHIP_NO_ERROR)
    {
        // Orange customization: key pair must be pre loaded
        ChipLogProgress(Controller, "Intermediate certificate key pair is not found in storage");
        ReturnErrorOnFailure(CHIP_ERROR_NOT_IMPLEMENTED);

        // Storage doesn't have an existing keypair. Let's create one and add it to the storage.
        ReturnErrorOnFailure(mIntermediateIssuer.Initialize());
        ReturnErrorOnFailure(mIntermediateIssuer.Serialize(serializedKey));

        keySize = static_cast<uint16_t>(sizeof(serializedKey));
        ReturnErrorOnFailure(storage.SyncSetKeyValue(kOperationalCredentialsIntermediateIssuerKeypairStorage, serializedKey.ConstBytes(), serializedKey.Length()));
    }
    else
    {
        // Use the keypair from the storage
        serializedKey.SetLength(keySize);
        ReturnErrorOnFailure(mIntermediateIssuer.Deserialize(serializedKey));
    }

    mStorage       = &storage;
    mJavaObjectRef = javaObjectRef;

    mInitialized = true;
    return CHIP_NO_ERROR;
}

CHIP_ERROR AndroidOperationalCredentialsIssuer::GenerateNOCChainAfterValidation(NodeId nodeId, FabricId fabricId,
                                                                                const CATValues & cats,
                                                                                const Crypto::P256PublicKey & pubkey,
                                                                                MutableByteSpan & rcac, MutableByteSpan & icac,
                                                                                MutableByteSpan & noc)
{
    ChipDN rcac_dn;
    uint16_t rcacBufLen = static_cast<uint16_t>(std::min(rcac.size(), static_cast<size_t>(UINT16_MAX)));
    CHIP_ERROR err      = CHIP_NO_ERROR;
    // node is set to 0L instead of fabricId as root and intermediate certificates do not depend on fabric
    PERSISTENT_KEY_OP(0L, kOperationalCredentialsRootCertificateStorage, key,
                      err = mStorage->SyncGetKeyValue(key, rcac.data(), rcacBufLen));
    if (err == CHIP_NO_ERROR)
    {
        uint64_t rcacId;
        // Found root certificate in the storage.
        rcac.reduce_size(rcacBufLen);
        ReturnErrorOnFailure(ExtractSubjectDNFromX509Cert(rcac, rcac_dn));
        ReturnErrorOnFailure(rcac_dn.GetCertChipId(rcacId));
        //[ORANGE] : disable suspicious check. mIssuerId is never assigned, use certificate data as source of truth instead
        //VerifyOrReturnError(rcacId == mIssuerId, CHIP_ERROR_INTERNAL);
        SetIssuerId(rcacId);
        ChipLogProgress(Controller, "RCAC loaded");
    }
    // If root certificate not found in the storage, generate new root certificate.
    else
    {
        ReturnErrorOnFailure(rcac_dn.AddAttribute_MatterRCACId(mIssuerId));

        ChipLogProgress(Controller, "Generating RCAC");
        chip::Credentials::X509CertRequestParams rcac_request = { 0, mNow, mNow + mValidity, rcac_dn, rcac_dn };
        ReturnErrorOnFailure(NewRootX509Cert(rcac_request, mIssuer, rcac));

        VerifyOrReturnError(CanCastTo<uint16_t>(rcac.size()), CHIP_ERROR_INTERNAL);
        PERSISTENT_KEY_OP(0L, kOperationalCredentialsRootCertificateStorage, key,
                          ReturnErrorOnFailure(mStorage->SyncSetKeyValue(key, rcac.data(), static_cast<uint16_t>(rcac.size()))));
    }

    ChipDN icac_dn;
    uint16_t icacBufLen = static_cast<uint16_t>(std::min(icac.size(), static_cast<size_t>(UINT16_MAX)));

    // node is set to 0L instead of fabricId as root and intermediate certificates do not depend on fabric
    PERSISTENT_KEY_OP(0L, kOperationalCredentialsIntermediateCertificateStorage, key,
                      err = mStorage->SyncGetKeyValue(key, icac.data(), icacBufLen));
    if (err == CHIP_NO_ERROR)
    {
        uint64_t icacId;
        // Found intermediate certificate in the storage.
        icac.reduce_size(icacBufLen);
        ReturnErrorOnFailure(ExtractSubjectDNFromX509Cert(icac, icac_dn));
        ReturnErrorOnFailure(icac_dn.GetCertChipId(icacId));
        SetIntermediateIssuerId(icacId);
        ChipLogProgress(Controller, "ICAC loaded");
    }
    else
    {
        // Orange customization: intermediate certificate must be pre loaded
        ChipLogError(Controller, "Intermediate certificate is not found in storage");
        ReturnErrorOnFailure(CHIP_ERROR_NOT_IMPLEMENTED);
    }

    ChipDN noc_dn;
    ReturnErrorOnFailure(noc_dn.AddAttribute_MatterFabricId(fabricId));
    ReturnErrorOnFailure(noc_dn.AddAttribute_MatterNodeId(nodeId));
    ReturnErrorOnFailure(noc_dn.AddCATs(cats));

    ChipLogProgress(Controller, "Generating NOC");
    chip::Credentials::X509CertRequestParams noc_request = { 1, mNow, mNow + mValidity, noc_dn, icac_dn };
    return NewNodeOperationalX509Cert(noc_request, pubkey, mIntermediateIssuer, noc);
}

CHIP_ERROR AndroidOperationalCredentialsIssuer::GenerateNOCChain(const ByteSpan & csrElements, const ByteSpan & csrNonce,
                                                                 const ByteSpan & attestationSignature,
                                                                 const ByteSpan & attestationChallenge, const ByteSpan & DAC,
                                                                 const ByteSpan & PAI,
                                                                 Callback::Callback<OnNOCChainGeneration> * onCompletion)
{
    jmethodID method;
    CHIP_ERROR err = CHIP_NO_ERROR;
    err            = JniReferences::GetInstance().FindMethod(JniReferences::GetInstance().GetEnvForCurrentThread(), mJavaObjectRef,
                                                  "onOpCSRGenerationComplete", "([B)V", &method);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "Error invoking onOpCSRGenerationComplete: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    NodeId assignedId;
    if (mNodeIdRequested)
    {
        assignedId       = mNextRequestedNodeId;
        mNodeIdRequested = false;
    }
    else
    {
        assignedId = mNextAvailableNodeId++;
    }

    TLVReader reader;
    reader.Init(csrElements);

    if (reader.GetType() == kTLVType_NotSpecified)
    {
        ReturnErrorOnFailure(reader.Next());
    }

    VerifyOrReturnError(reader.GetType() == kTLVType_Structure, CHIP_ERROR_WRONG_TLV_TYPE);
    VerifyOrReturnError(reader.GetTag() == AnonymousTag(), CHIP_ERROR_UNEXPECTED_TLV_ELEMENT);

    TLVType containerType;
    ReturnErrorOnFailure(reader.EnterContainer(containerType));
    ReturnErrorOnFailure(reader.Next(kTLVType_ByteString, TLV::ContextTag(1)));

    ByteSpan csr(reader.GetReadPoint(), reader.GetLength());
    reader.ExitContainer(containerType);

    P256PublicKey pubkey;
    ReturnErrorOnFailure(VerifyCertificateSigningRequest(csr.data(), csr.size(), pubkey));

    ChipLogProgress(chipTool, "VerifyCertificateSigningRequest");

    Platform::ScopedMemoryBuffer<uint8_t> noc;
    ReturnErrorCodeIf(!noc.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    MutableByteSpan nocSpan(noc.Get(), kMaxCHIPDERCertLength);

    Platform::ScopedMemoryBuffer<uint8_t> icac;
    ReturnErrorCodeIf(!icac.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    MutableByteSpan icacSpan(icac.Get(), kMaxCHIPDERCertLength);

    Platform::ScopedMemoryBuffer<uint8_t> rcac;
    ReturnErrorCodeIf(!rcac.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    MutableByteSpan rcacSpan(rcac.Get(), kMaxCHIPDERCertLength);

    ReturnErrorOnFailure(
        GenerateNOCChainAfterValidation(assignedId, mNextFabricId, chip::kUndefinedCATs, pubkey, rcacSpan, icacSpan, nocSpan));

    // TODO: Force callers to set IPK if used before GenerateNOCChain will succeed.
    ByteSpan defaultIpkSpan = chip::GroupTesting::DefaultIpkValue::GetDefaultIpk();

    // The below static assert validates a key assumption in types used (needed for public API conformance)
    static_assert(CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES == kAES_CCM128_Key_Length, "IPK span sizing must match");

    // Prepare IPK to be sent back. A more fully-fledged operational credentials delegate
    // would obtain a suitable key per fabric.
    uint8_t ipkValue[CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES];
    Crypto::AesCcm128KeySpan ipkSpan(ipkValue);

    ReturnErrorCodeIf(defaultIpkSpan.size() != sizeof(ipkValue), CHIP_ERROR_INTERNAL);

    memcpy(&ipkValue[0], defaultIpkSpan.data(), defaultIpkSpan.size());

    // Call-back into commissioner with the generated data.
    onCompletion->mCall(onCompletion->mContext, CHIP_NO_ERROR, nocSpan, icacSpan, rcacSpan, MakeOptional(ipkSpan),
                        Optional<NodeId>());

    jbyteArray javaCsr;
    JniReferences::GetInstance().GetEnvForCurrentThread()->ExceptionClear();
    JniReferences::GetInstance().N2J_ByteArray(JniReferences::GetInstance().GetEnvForCurrentThread(), csrElements.data(),
                                               csrElements.size(), javaCsr);
    JniReferences::GetInstance().GetEnvForCurrentThread()->CallVoidMethod(mJavaObjectRef, method, javaCsr);
    return CHIP_NO_ERROR;
}

} // namespace Controller
} // namespace chip
