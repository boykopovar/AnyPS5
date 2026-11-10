#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int InvalidParameter = static_cast<int>(0x80930005);
constexpr std::uint32_t Untouched = 0x5a5a5a5au;

using InitializeFn = int (APS5_VABI*)(std::int64_t, std::uint32_t*);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceAjm.native.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceAjm.native\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Ajm initialize context pointer: %s\n", message);
    std::abort();
}

}

int main() {
    const auto initialize = Resolve<InitializeFn>("sceAjmInitialize");

    std::uint32_t first = Untouched;
    Require(initialize(0, &first) == 0 && first == 1, "a valid context pointer was not written with the first id");
    std::uint32_t second = Untouched;
    Require(initialize(0, &second) == 0 && second != Untouched && second != first, "two calls did not yield unique ids");

    Require(initialize(0, nullptr) == InvalidParameter, "a null context pointer was not rejected");
    Require(initialize(0, reinterpret_cast<std::uint32_t*>(0x10)) == InvalidParameter, "a dangling context pointer was not rejected");

    std::puts("Ajm initialize context pointer tests passed");
    return 0;
}
