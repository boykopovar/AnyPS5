#include <nid/NidCompute.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Kernel mapper export check failed at line %u\n", location.line());
        std::abort();
    }
}

static void* LoadLibraryFile(const std::filesystem::path& path) {
#ifdef _WIN32
    auto* module = reinterpret_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    return module;
}

static void* FindSymbol(void* module, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
    return dlsym(module, name);
#endif
}

int main(int argc, char** argv) {
    Require(argc == 3);
    const auto directory = std::filesystem::absolute(argv[1]);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    static const char* const exports[] = {
        "sceKernelCallIndirectBuffer",
        "sceKernelCallIndirectBuffer2",
        "sceKernelCallIndirectBuffer3",
        "sceKernelGiveDirectMemoryToMapper",
        "sceKernelMapperGetParam",
        "sceKernelWaitCommandBufferCompletion",
        "sceKernelWriteEventQueueOnCompletionCommand",
        "sceKernelWriteMapCommand",
        "sceKernelWriteMapCommand2",
        "sceKernelWriteMapDirectCommand",
        "sceKernelWriteMapDirectWithGpuMaskIdCommand",
        "sceKernelWriteMapWithGpuMaskIdCommand",
        "sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand",
        "sceKernelWriteModifyProtectCommand",
        "sceKernelWriteModifyProtectWithGpuMaskIdCommand",
        "sceKernelWriteMultiMapCommand",
        "sceKernelWriteMultiMapWithGpuMaskIdCommand",
        "sceKernelWriteRemapCommand",
        "sceKernelWriteRemapWithGpuMaskIdCommand",
        "sceKernelWriteUnmapCommand",
        "sceKernelWriteUnmapToPrtCommand",
        "sceKernelWriteWaitOnAddressCommand",
        "sceKernelWriteWaitOnCounterCommand",
        "sceKernelWriteWriteAddressCommand",
        "sceKernelWriteWriteCounterCommand",
    };
    for (const char* moduleName : {"libkernel.prx", "libkernel_sys.prx"}) {
        auto* module = LoadLibraryFile(directory / moduleName);
        for (const char* name : exports) {
            const auto symbol = patched ? Nid::ComputeNid(name, "") : std::string(name);
            if (FindSymbol(module, symbol.c_str()) == nullptr) {
                std::fprintf(stderr, "%s does not export %s (%s)\n", moduleName, name, symbol.c_str());
                return 1;
            }
        }
        const auto absent = patched ? Nid::ComputeNid("sceKernelMapperExportParityProbe", "") : std::string("sceKernelMapperExportParityProbe");
        if (FindSymbol(module, absent.c_str()) != nullptr) {
            std::fprintf(stderr, "%s resolved %s although nothing exports it\n", moduleName, absent.c_str());
            return 1;
        }
    }
    std::puts("libkernel.prx and libkernel_sys.prx export every AMM mapper entry point");
    return 0;
}
