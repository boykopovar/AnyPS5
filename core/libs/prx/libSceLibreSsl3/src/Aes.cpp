#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstring>
#include <stdexcept>

namespace {

constexpr unsigned char DefaultIv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};

}

extern "C" {

int APS5_VABI AES_unwrap_key_nid_postfix(AesKey* key, const unsigned char* iv, unsigned char* out, const unsigned char* in, unsigned int length) {
    if ((length & 0x7) || length < 24)
        return -1;
    if (!key || !out || !in)
        throw std::invalid_argument("AES_unwrap_key requires a key, input and output");
    length -= 8;
    unsigned char block[16];
    unsigned int step = 6 * (length >> 3);
    std::memcpy(block, in, 8);
    std::memmove(out, in + 8, length);
    for (unsigned int round = 0; round < 6; ++round) {
        unsigned char* chunk = out + length - 8;
        for (unsigned int offset = 0; offset < length; offset += 8, --step, chunk -= 8) {
            block[7] ^= static_cast<unsigned char>(step & 0xff);
            if (step > 0xff) {
                block[6] ^= static_cast<unsigned char>((step >> 8) & 0xff);
                block[5] ^= static_cast<unsigned char>((step >> 16) & 0xff);
                block[4] ^= static_cast<unsigned char>((step >> 24) & 0xff);
            }
            std::memcpy(block + 8, chunk, 8);
            AES_decrypt_nid_postfix(block, block, key);
            std::memcpy(chunk, block + 8, 8);
        }
    }
    if (!iv)
        iv = DefaultIv;
    if (std::memcmp(block, iv, 8)) {
        std::memset(out, 0, length);
        return 0;
    }
    return static_cast<int>(length);
}

}
