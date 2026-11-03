#include "prx/libSceNet/libSceNet.hpp"
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "Patched IPV6_V6ONLY check failed at line %u\n", location.line());
    std::abort();
}

template<typename TFunction>
static TFunction Function(void* module, const char* name) {
    const auto symbol = Nid::ComputeNid(name, "libSceNet");
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), symbol.c_str()));
#else
    auto* address = dlsym(module, symbol.c_str());
#endif
    if (!address) std::fprintf(stderr, "Missing patched function %s (%s)\n", name, symbol.c_str());
    Require(address != nullptr);
    return reinterpret_cast<TFunction>(address);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const auto path = std::filesystem::absolute(argv[1]) / "libSceNet.prx";
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS);
    auto* module = static_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
    if (!module) std::fprintf(stderr, "libSceNet.prx: LoadLibraryEx failed with error %lu\n", GetLastError());
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "libSceNet.prx: %s\n", dlerror());
#endif
    Require(module != nullptr);
    const auto socket = Function<decltype(&sceNetSocket)>(module, "sceNetSocket");
    const auto set = Function<decltype(&sceNetSetsockopt)>(module, "sceNetSetsockopt");
    const auto get = Function<decltype(&sceNetGetsockopt)>(module, "sceNetGetsockopt");
    const auto close = Function<decltype(&sceNetSocketClose)>(module, "sceNetSocketClose");
    const auto error = Function<decltype(&sceNetErrnoLoc)>(module, "sceNetErrnoLoc");
    for (const int type : {1, 2}) {
        const int descriptor = socket("patched-ipv6", 28, type, 0);
        if (descriptor < 0 && (*error() == 43 || *error() == 47)) return 77;
        Require(descriptor >= 0);
        for (const int value : {0, 1, -1, 16}) {
            *error() = 1234;
            Require(set(descriptor, 41, 27, &value, sizeof(value)) == 0 && *error() == 1234);
            const int expected = value != 0 ? 1 : 0;
            for (std::uint32_t capacity = 0; capacity <= 8; ++capacity) {
                std::array<unsigned char, 10> storage;
                storage.fill(0xa5);
                auto length = capacity;
                Require(get(descriptor, 41, 27, storage.data() + 1, &length) == 0 && *error() == 1234);
                Require(length == std::min<std::uint32_t>(capacity, sizeof(expected)));
                Require(std::memcmp(storage.data() + 1, &expected, length) == 0);
                Require(storage.front() == 0xa5 && std::all_of(storage.begin() + 1 + length, storage.end(), [](unsigned char byte) { return byte == 0xa5; }));
            }
            Require(set(descriptor, 41, 27, &value, sizeof(value) - 1) == static_cast<int>(0x80410116u) && *error() == 22);
            int output = -1;
            std::uint32_t length = sizeof(output);
            Require(get(descriptor, 41, 27, &output, &length) == 0 && output == expected && length == sizeof(output) && *error() == 22);
        }
        Require(close(descriptor) == 0);
    }
}
