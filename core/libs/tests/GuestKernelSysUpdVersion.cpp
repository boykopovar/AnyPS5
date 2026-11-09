#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using GetUpdVersionFn = int (APS5_VABI*)(std::uint32_t*);

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

GetUpdVersionFn Resolve() {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libkernel_sys.prx"), "sceKernelGetUpdVersion"));
#else
    void* symbol = dlsym(RTLD_DEFAULT, "sceKernelGetUpdVersion");
#endif
    Require(symbol != nullptr, "sceKernelGetUpdVersion is not exported by libkernel_sys");
    return reinterpret_cast<GetUpdVersionFn>(symbol);
}

}

int main() {
    const GetUpdVersionFn getUpdVersion = Resolve();
    struct {
        std::uint32_t version;
        std::uint32_t after;
    } update{0xaaaaaaaau, 0xaaaaaaaau}, system{0xaaaaaaaau, 0xaaaaaaaau};
    Require(getUpdVersion(&update.version) == sysKernelGetUpdVersion_nid_postfix(&system.version), "the result differs from sysKernelGetUpdVersion");
    Require(update.version == system.version && update.after == 0xaaaaaaaau, "the version differs from sysKernelGetUpdVersion or more than 4 bytes were written");
    Require(getUpdVersion(nullptr) == static_cast<int>(0x80020016u), "a null output was not refused with EINVAL");
    std::puts("update version tests passed");
    return 0;
}
