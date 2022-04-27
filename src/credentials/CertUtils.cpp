#include "CertUtils.h"


namespace chip {
namespace Credentials {

std::string ToString(const ChipDN &dn) {
    uint8_t rdnCount = dn.RDNCount();
    char valueStr[128];
    std::string output = "[[ ";

    for (uint8_t i = 0; i < rdnCount; i++) {
        if (IsChip64bitDNAttr(dn.rdn[i].mAttrOID)) {
            snprintf(valueStr, sizeof(valueStr), "%016" PRIX64, dn.rdn[i].mChipVal);
        } else if (IsChip32bitDNAttr(dn.rdn[i].mAttrOID)) {
            snprintf(valueStr, sizeof(valueStr), "%08" PRIX32, static_cast<uint32_t>(dn.rdn[i].mChipVal));
        } else {
            size_t len = dn.rdn[i].mString.size();
            if (len > sizeof(valueStr) - 1) {
                len = sizeof(valueStr) - 1;
            }
            memcpy(valueStr, dn.rdn[i].mString.data(), len);
            valueStr[len] = 0;
        }

        output += chip::ASN1::GetOIDName(dn.rdn[i].mAttrOID);
        output += " = ";
        output += valueStr;

        if (i < rdnCount - 1) {
            output += ", ";
        }
    }

    output += " ]]";

    return output;
}

}
}
