#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::int32_t NidSha512_256 = 1030;
constexpr std::int32_t NidSha512_256WithRsa = 1026;
constexpr std::int32_t PkeyRsa = 6;
constexpr std::int32_t PkeyRsa2 = 19;
constexpr std::uint64_t DigestAlgorithmIdAbsent = 0x8;
constexpr std::int32_t DigestLength = 32;
constexpr std::int32_t BlockLength = 128;

Sha512Context& State(EvpMdContext* context) {
    if (!context || !context->mdData) throw std::invalid_argument("EVP_sha512_256: digest context without data");
    return *static_cast<Sha512Context*>(context->mdData);
}

int APS5_VABI Init(EvpMdContext* context) {
    static constexpr std::uint64_t Initial[8] {
        0x22312194fc2bf72c, 0x9f555fa3c84c64c2, 0x2393b86b6f53b151, 0x963877195940eabd,
        0x96283ee2a88effe3, 0xbe5e1e2553863992, 0x2b0199fc2c85b8aa, 0x0eb72ddc81c52ca2,
    };
    auto& state = State(context);
    if (SHA512_Init_nid_postfix(&state) != 1) return 0;
    std::memcpy(state.h, Initial, sizeof(Initial));
    return 1;
}

int APS5_VABI Update(EvpMdContext* context, const void* data, std::size_t length) {
    return SHA512_Update_nid_postfix(&State(context), data, length);
}

int APS5_VABI Final(EvpMdContext* context, unsigned char* digest) {
    std::array<unsigned char, 64> full{};
    const int result = SHA512_Final_nid_postfix(full.data(), &State(context));
    if (result == 1) std::memcpy(digest, full.data(), DigestLength);
    return result;
}

constexpr EvpMd Sha512_256 {
    NidSha512_256, NidSha512_256WithRsa, DigestLength, DigestAlgorithmIdAbsent, Init, Update, Final, nullptr, nullptr, nullptr,
    nullptr, {PkeyRsa, PkeyRsa2, 0, 0, 0}, BlockLength, static_cast<std::int32_t>(sizeof(void*) + sizeof(Sha512Context)), nullptr,
};

}

extern "C" const EvpMd* APS5_VABI EVP_sha512_256_nid_postfix() {
    return &Sha512_256;
}
