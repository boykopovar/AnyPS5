#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int InvalidParameter = static_cast<int>(0x80930005);
constexpr int InvalidFlags = static_cast<int>(0x8093000C);

using InstanceGetSizeFn = int (APS5_VABI*)(std::uint32_t, std::uint32_t, std::uint64_t);
using GetFailedInstanceFn = int (APS5_VABI*)(std::uint32_t, void*);
using TelemetryGetLogFn = int (APS5_VABI*)(std::uint32_t, std::uint8_t*, void*);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Ajm driver queries: %s\n", message);
    std::abort();
}

template <typename Function>
Function Resolve(const char* library, const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA(library), name));
#else
    void* module = library ? dlopen(library, RTLD_NOW) : dlopen(nullptr, RTLD_NOW);
    void* symbol = module ? dlsym(module, name) : nullptr;
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by %s\n", name, library ? library : "main");
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void TestQueries(InstanceGetSizeFn instanceGetSize, GetFailedInstanceFn getFailedInstance, TelemetryGetLogFn telemetryGetLog, std::uint32_t context) {
    for (const std::uint64_t flags : {0x180ull, 0x300ull, 0x380ull, 0x2000ull, 0x2380ull, 0x4000ull, 0x8000ull, 1ull << 60, 0xffffffffffffffffull}) {
        for (const std::uint32_t codec : {0u, 1u, 2u}) {
            Require(instanceGetSize(context, codec, flags) == InvalidFlags, "sceAjmInstanceGetSize took a rejected flag combination");
        }
    }
    Require(getFailedInstance(context, nullptr) == InvalidParameter, "sceAjmGetFailedInstance took a null output");
    std::uint8_t flag = 0x5a;
    Require(telemetryGetLog(context, &flag, nullptr) == InvalidParameter && flag == 0x5a, "sceAjmTelemetryGetLog took a null log");
    Require(telemetryGetLog(context, nullptr, nullptr) == InvalidParameter, "sceAjmTelemetryGetLog took a null log with a null flag");
}

}

int main(int argc, char** argv) {
    std::uint32_t context = 0;
    Require(sceAjmInitialize(0, &context) == 0, "sceAjmInitialize failed");

#ifdef _WIN32
    const char* nativeLibrary = argc > 1 ? argv[1] : "libSceAjm.native.prx";
    const char* ajmLibrary = argc > 2 ? argv[2] : "libSceAjm.prx";
#else
    const char* nativeLibrary = argc > 1 ? argv[1] : nullptr;
    const char* ajmLibrary = argc > 2 ? argv[2] : nullptr;
#endif

    const auto instanceGetSize = Resolve<InstanceGetSizeFn>(nativeLibrary, "sceAjmInstanceGetSize");
    const auto getFailedInstance = Resolve<GetFailedInstanceFn>(nativeLibrary, "sceAjmGetFailedInstance");
    const auto telemetryGetLog = Resolve<TelemetryGetLogFn>(nativeLibrary, "sceAjmTelemetryGetLog");
    TestQueries(instanceGetSize, getFailedInstance, telemetryGetLog, context);

    const auto ajmInstanceGetSize = Resolve<InstanceGetSizeFn>(ajmLibrary, "sceAjmInstanceGetSize_nid_disambig1");
    const auto ajmGetFailedInstance = Resolve<GetFailedInstanceFn>(ajmLibrary, "sceAjmGetFailedInstance_nid_disambig1");
    const auto ajmTelemetryGetLog = Resolve<TelemetryGetLogFn>(ajmLibrary, "sceAjmTelemetryGetLog_nid_disambig1");
    TestQueries(ajmInstanceGetSize, ajmGetFailedInstance, ajmTelemetryGetLog, context);

    Require(sceAjmFinalize(context) == 0, "sceAjmFinalize failed");
    std::puts("Ajm driver query tests passed");
    return 0;
}
