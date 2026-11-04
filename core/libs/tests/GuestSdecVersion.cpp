#include "prx/libSceVdecSavc/include/Api.hpp"
#include "prx/libSceVdecSavc2/include/Api.hpp"
#include "prx/libSceVdecShevc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

using GetVersion = void (APS5_VABI*)(std::uint32_t*);

void CheckVersion(GetVersion getVersion, std::uint32_t expected, const char* name) {
    for (unsigned char fill : {0xA5, 0x5A}) {
        alignas(std::uint32_t) unsigned char buffer[12];
        std::memset(buffer, fill, sizeof(buffer));
        getVersion(reinterpret_cast<std::uint32_t*>(buffer));
        std::uint32_t version = 0;
        std::memcpy(&version, buffer, sizeof(version));
        if (version != expected) {
            std::fprintf(stderr, "%s wrote 0x%08x, expected 0x%08x\n", name, version, expected);
            std::abort();
        }
        for (std::size_t i = sizeof(version); i < sizeof(buffer); ++i)
            Require(buffer[i] == fill, "GetVersion writes only four bytes");
    }
    getVersion(nullptr);
}

}

int main() {
    CheckVersion(sceSdecGetVersionSw, 0u, "sceSdecGetVersionSw");
    CheckVersion(sceSdecGetVersionSw2, 0x13590001u, "sceSdecGetVersionSw2");
    CheckVersion(sceSdecGetVersionSwHevc, 0x13590001u, "sceSdecGetVersionSwHevc");
    std::puts("sdec version tests passed");
    return 0;
}
