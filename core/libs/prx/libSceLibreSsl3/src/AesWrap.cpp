#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr unsigned char DefaultIv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};

}

extern "C" {

int APS5_VABI AES_wrap_key_nid_postfix(AesKey* key, const unsigned char* iv, unsigned char* out, const unsigned char* in, std::uint32_t inlen) {
    if (!key || !out || !in) throw std::invalid_argument(std::string(__func__) + ": null argument");
    if (inlen < 16 || (inlen & 7) != 0) return -1;
    const std::uint32_t blocks = inlen / 8;
    unsigned char a[8];
    std::memcpy(a, iv ? iv : DefaultIv, sizeof(a));
    std::vector<unsigned char> r(in, in + inlen);
    for (std::uint32_t round = 0; round < 6; ++round) {
        for (std::uint32_t index = 1; index <= blocks; ++index) {
            unsigned char block[16];
            std::memcpy(block, a, 8);
            std::memcpy(block + 8, r.data() + (index - 1) * 8, 8);
            unsigned char encrypted[16];
            AES_encrypt_nid_postfix(block, encrypted, key);
            std::memcpy(a, encrypted, 8);
            std::memcpy(r.data() + (index - 1) * 8, encrypted + 8, 8);
            const std::uint64_t counter = static_cast<std::uint64_t>(blocks) * round + index;
            for (std::uint32_t byte = 0; byte < 8; ++byte) {
                a[7 - byte] ^= static_cast<unsigned char>(counter >> (8 * byte));
            }
        }
    }
    std::memcpy(out, a, 8);
    std::memcpy(out + 8, r.data(), inlen);
    return static_cast<int>(inlen + 8);
}

}
