#include "prx/libSceNpCommon/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value) {
    if (value) return;
    std::fputs("NpCommon SDK version check failed\n", stderr);
    std::abort();
}

int main() {
    std::array<unsigned char, 9> version = {'x'} ;
    version[5] = 'x';
    version[6] = 'x';
    version[7] = 'x';
    version[8] = 'x';
    sceNpGetSdkVersion(reinterpret_cast<char*>(version.data()));
    Require(std::memcmp(version.data(), "1.00", 4) == 0 && version[4] == '\0');
    Require(version[5] == 'x' && version[6] == 'x' && version[7] == 'x' && version[8] == 'x');

    sceNpGetSdkVersion(nullptr);

    return 0;
}
