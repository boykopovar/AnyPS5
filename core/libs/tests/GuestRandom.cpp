#include "prx/libSceRandom/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }
int main() {
    constexpr int invalid = static_cast<int>(0x817C0016);
    unsigned char buffer[80];
    Require(sceRandomGetRandomNumber(nullptr, 16) == invalid);
    Require(sceRandomGetRandomNumber(nullptr, 0) == invalid);
    Require(sceRandomGetRandomNumber(nullptr, 64) == invalid);
    Require(sceRandomGetRandomNumber(nullptr, 65) == invalid);
    Require(sceRandomGetRandomNumber(nullptr, SIZE_MAX) == invalid);
    std::memset(buffer, 0xAA, sizeof(buffer));
    Require(sceRandomGetRandomNumber(buffer, 65) == invalid);
    Require(sceRandomGetRandomNumber(buffer, 66) == invalid);
    Require(sceRandomGetRandomNumber(buffer, 128) == invalid);
    Require(sceRandomGetRandomNumber(buffer, SIZE_MAX) == invalid);
    Require(buffer[0] == 0xAA);
    Require(sceRandomGetRandomNumber(buffer, 0) == 0);
    Require(buffer[0] == 0xAA);
    Require(sceRandomGetRandomNumber(buffer, 1) == 0);
    Require(sceRandomGetRandomNumber(buffer, 7) == 0);
    Require(buffer[7] == 0xAA);
    unsigned char maxBuf[64] = {};
    Require(sceRandomGetRandomNumber(maxBuf, 64) == 0);
    unsigned char first[64] = {};
    unsigned char second[64] = {};
    Require(sceRandomGetRandomNumber(first, sizeof(first)) == 0);
    Require(sceRandomGetRandomNumber(second, sizeof(second)) == 0);
    Require(std::memcmp(first, second, sizeof(first)) != 0);
}
