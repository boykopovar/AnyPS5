#include "prx/libSceNet/libSceNet.hpp"
#include <nid/NidCompute.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
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
    Require(address != nullptr, name);
    return reinterpret_cast<TFunction>(address);
}

int main(int argc, char** argv) {
    Require(argc == 2, "supply patched module directory");
    const auto path = std::filesystem::absolute(argv[1]) / "libSceNet.prx";
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS);
    auto* module = static_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr, "load patched Net and its kernel dependency");
    const auto socket = Function<decltype(&sceNetSocket)>(module, "sceNetSocket");
    const auto set = Function<decltype(&sceNetSetsockopt)>(module, "sceNetSetsockopt");
    const auto get = Function<decltype(&sceNetGetsockopt)>(module, "sceNetGetsockopt");
    const auto close = Function<decltype(&sceNetSocketClose)>(module, "sceNetSocketClose");
    for (const int type : {1, 2}) {
        const int descriptor = socket("patched-reuse", 2, type, 0);
        Require(descriptor >= 0, "create socket through NID");
        for (const int value : {1, -1, 0}) {
            Require(set(descriptor, 0xffff, 0x200, &value, sizeof(value)) == 0, "set reuseport through NID");
            const int one = 1;
            Require(set(descriptor, 0xffff, 0x4, &one, sizeof(one)) == 0, "set independent reuseaddr");
            for (const int option : {0x4, 0x200}) {
                int actual = -1;
                std::uint32_t length = sizeof(actual);
                Require(get(descriptor, 0xffff, option, &actual, &length) == 0, "read reuse flag through NID");
                Require(actual == (option == 0x4 || value != 0 ? option : 0), "independent patched flag value");
            }
        }
        Require(close(descriptor) == 0, "close patched socket");
    }
}
