#include <ostream>
#include <sstream>
#include "CertUtils.h"


namespace chip {
namespace Credentials {

std::ostream &operator<<(std::ostream &os, const ChipDN &dn) {
    uint8_t rdnCount = dn.RDNCount();
    char valueStr[128];

    os << "[[ ";

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

        os << chip::ASN1::GetOIDName(dn.rdn[i].mAttrOID) << " = " << valueStr;
        if (i < rdnCount - 1) {
            os << ", ";
        }
    }

    os << " ]]";

    return os;
}

std::string ToString(const ChipDN &dn) {
    std::stringstream ss;
    ss << dn;
    return ss.str();
}

}
}
