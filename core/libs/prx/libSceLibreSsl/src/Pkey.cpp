#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int NidUndef = 0;
constexpr int NidRsaEncryption = 6;
constexpr int NidRsa = 19;
constexpr int NidDhKeyAgreement = 28;
constexpr int NidDsaWithSha = 66;
constexpr int NidDsa2 = 67;
constexpr int NidDsaWithSha1Old = 70;
constexpr int NidDsaWithSha1 = 113;
constexpr int NidDsa = 116;
constexpr int NidEcPublicKey = 408;
constexpr int NidHmac = 855;
constexpr int NidCmac = 894;

}

extern "C" {

int APS5_VABI EVP_PKEY_type_nid_postfix(int type) {
    switch (type) {
    case NidRsaEncryption:
    case NidRsa:
        return NidRsaEncryption;
    case NidDhKeyAgreement:
    case NidEcPublicKey:
    case NidHmac:
    case NidCmac:
        return type;
    case NidDsaWithSha:
    case NidDsa2:
    case NidDsaWithSha1Old:
    case NidDsaWithSha1:
    case NidDsa:
        return NidDsa;
    default:
        return NidUndef;
    }
}

}
