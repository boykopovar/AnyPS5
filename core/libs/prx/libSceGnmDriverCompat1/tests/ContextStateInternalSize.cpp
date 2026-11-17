#include "SceTypes.hpp"
#include "tests/ProcessParametersFixture.hpp"
#include "Testing.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using SizeFunction = int (APS5_VABI*)(void);

SizeFunction LoadSizeFunction(const char* path) {
#ifdef _WIN32
    const auto library = LoadLibraryA(path);
    Require(library != nullptr, "the library did not load");
    return reinterpret_cast<SizeFunction>(GetProcAddress(library, "jajhf-Gi3AI_nid_no_patch_cut"));
#else
    const auto library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    Require(library != nullptr, "the library did not load");
    return reinterpret_cast<SizeFunction>(dlsym(library, "jajhf-Gi3AI_nid_no_patch_cut"));
#endif
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "the library path was not provided");
    const auto sizeFunction = LoadSizeFunction(argv[1]);
    Require(sizeFunction != nullptr, "the context-state-size export is missing");

    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "the process parameters could not be installed");

    for (const std::uint32_t sdkVersion : {0x00000000u, 0x03500000u, 0x03ffffffu}) {
        std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
        Require(sizeFunction() == 0x20, "the size before SDK 0x04000000 is not 0x20");
    }

    for (const std::uint32_t sdkVersion : {0x04000000u, 0x05500000u, 0x09000000u}) {
        std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
        Require(sizeFunction() == 0x100, "the size from SDK 0x04000000 is not 0x100");
    }

    std::puts("GnmDriverCompat1 ContextStateInternalSize tests passed");
    return 0;
}