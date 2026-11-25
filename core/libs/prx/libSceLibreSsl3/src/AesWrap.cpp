#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr unsigned char DefaultIv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    return copied;
}

bool CopyInChunks(std::vector<unsigned char>& host, const unsigned char* source, std::size_t bytes) {
    constexpr std::size_t Chunk = 64 * 1024;
    host.clear();
    for (std::size_t done = 0; done < bytes;) {
        const std::size_t size = std::min(Chunk, bytes - done);
        host.resize(done + size);
        if (!CopyIn(host.data() + done, source + done, size)) return false;
        done += size;
    }
    return true;
}

bool CopyOutChunks(unsigned char* destination, const unsigned char* host, std::size_t bytes) {
    constexpr std::size_t Chunk = 64 * 1024;
    for (std::size_t done = 0; done < bytes;) {
        const std::size_t size = std::min(Chunk, bytes - done);
        if (!CopyOut(destination + done, host + done, size)) return false;
        done += size;
    }
    return true;
}

}

extern "C" {

int APS5_VABI AES_wrap_key_nid_postfix(AesKey* key, const unsigned char* iv, unsigned char* out, const unsigned char* in, std::uint32_t inlen) {
    if (!key || !out || !in) throw std::invalid_argument(std::string(__func__) + ": null argument");
    if (inlen < 16 || (inlen & 7) != 0) return -1;
    const std::uint32_t blocks = inlen / 8;
    AesKey schedule;
    unsigned char a[8];
    std::vector<unsigned char> r;
    if (!CopyIn(&schedule, key, sizeof(schedule)) || !CopyIn(a, iv ? iv : DefaultIv, sizeof(a)) || !CopyInChunks(r, in, inlen)) {
        throw std::invalid_argument(std::string(__func__) + ": unreadable key, iv or input");
    }
    for (std::uint32_t round = 0; round < 6; ++round) {
        for (std::uint32_t index = 1; index <= blocks; ++index) {
            unsigned char block[16];
            std::memcpy(block, a, 8);
            std::memcpy(block + 8, r.data() + (index - 1) * 8, 8);
            unsigned char encrypted[16];
            AES_encrypt_nid_postfix(block, encrypted, &schedule);
            std::memcpy(a, encrypted, 8);
            std::memcpy(r.data() + (index - 1) * 8, encrypted + 8, 8);
            const std::uint64_t counter = static_cast<std::uint64_t>(blocks) * round + index;
            for (std::uint32_t byte = 0; byte < 8; ++byte) {
                a[7 - byte] ^= static_cast<unsigned char>(counter >> (8 * byte));
            }
        }
    }
    if (!CopyOut(out, a, sizeof(a)) || !CopyOutChunks(out + 8, r.data(), inlen)) throw std::invalid_argument(std::string(__func__) + ": unwritable output");
    return static_cast<int>(inlen + 8);
}

}
