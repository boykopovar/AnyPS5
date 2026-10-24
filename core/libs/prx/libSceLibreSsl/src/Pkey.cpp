#include <stdexcept>
#include <string>
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
constexpr int NidGostR34102001 = 811;
constexpr int NidGost2814789Mac = 815;
constexpr int NidHmac = 855;
constexpr int NidCmac = 894;
constexpr int NidGostR34102012With256 = 946;
constexpr int NidGostR34102012With512 = 947;

}

extern "C" {

int APS5_VABI EVP_PKEY_type_nid_postfix(int type) {
    switch (type) {
    case NidRsaEncryption:
    case NidRsa:
        return NidRsaEncryption;
    case NidEcPublicKey:
    case NidHmac:
    case NidCmac:
        return type;
    case NidDhKeyAgreement:
    case NidDsaWithSha:
    case NidDsa2:
    case NidDsaWithSha1Old:
    case NidDsaWithSha1:
    case NidDsa:
    case NidGostR34102001:
    case NidGost2814789Mac:
    case NidGostR34102012With256:
    case NidGostR34102012With512:
        throw std::runtime_error(std::string(__func__) + ": unverified key type " + std::to_string(type));
    default:
        return NidUndef;
    }
}

}
