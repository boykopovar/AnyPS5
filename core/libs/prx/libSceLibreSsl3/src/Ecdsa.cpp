#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::int32_t NidEcdsaWithSha1 = 416;
constexpr std::int32_t PkeyEc = 408;
constexpr std::uint64_t PkeyDigest = 0x2;
constexpr std::int32_t DigestLength = 20;
constexpr std::int32_t BlockLength = 64;

Sha1Context& State(EvpMdContext* context) {
    if (!context || !context->mdData) throw std::invalid_argument("EVP_ecdsa: digest context without data");
    return *static_cast<Sha1Context*>(context->mdData);
}

int APS5_VABI Init(EvpMdContext* context) {
    return SHA1_Init_nid_postfix(&State(context));
}

int APS5_VABI Update(EvpMdContext* context, const void* data, std::size_t length) {
    return SHA1_Update_nid_postfix(&State(context), data, length);
}

int APS5_VABI Final(EvpMdContext* context, unsigned char* digest) {
    return SHA1_Final_nid_postfix(digest, &State(context));
}

const EvpMd Ecdsa {
    NidEcdsaWithSha1, NidEcdsaWithSha1, DigestLength, PkeyDigest, Init, Update, Final, nullptr, nullptr,
    reinterpret_cast<void*>(&ECDSA_sign_nid_postfix), reinterpret_cast<void*>(&ECDSA_verify_nid_postfix), {PkeyEc, 0, 0, 0, 0},
    BlockLength, static_cast<std::int32_t>(sizeof(void*) + sizeof(Sha1Context)), nullptr,
};

}

extern "C" const EvpMd* APS5_VABI EVP_ecdsa_nid_postfix() {
    return &Ecdsa;
}
