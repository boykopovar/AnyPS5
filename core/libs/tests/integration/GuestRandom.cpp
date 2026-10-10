#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceRandomGetRandomNumber(void*, std::size_t);
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    constexpr int invalid = static_cast<int>(0x817C0016);
    unsigned char buffer[80];
    Require(sceRandomGetRandomNumber(nullptr, 16) == invalid);
    std::memset(buffer, 0xAA, sizeof(buffer));
    Require(sceRandomGetRandomNumber(buffer, 65) == invalid);
    Require(buffer[0] == 0xAA);
    Require(sceRandomGetRandomNumber(buffer, 0) == 0);
    Require(buffer[0] == 0xAA);
    Require(sceRandomGetRandomNumber(buffer, 7) == 0);
    Require(buffer[7] == 0xAA);
    unsigned char first[64] = {};
    unsigned char second[64] = {};
    Require(sceRandomGetRandomNumber(first, sizeof(first)) == 0);
    Require(sceRandomGetRandomNumber(second, sizeof(second)) == 0);
    Require(std::memcmp(first, second, sizeof(first)) != 0);
}
