#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "SceTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "System flexible memory check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

using MapSystemFlexible = int32_t (APS5_VABI*)(void**, std::size_t, int, int, const char*);

static MapSystemFlexible mapSystemFlexible = nullptr;

static MapSystemFlexible Resolve() {
#ifdef _WIN32
    const HMODULE module = GetModuleHandleA("libkernel.prx");
    Require(module != nullptr);
    auto* symbol = reinterpret_cast<void*>(GetProcAddress(module, "sceKernelMapNamedSystemFlexibleMemory"));
#else
    auto* symbol = dlsym(RTLD_DEFAULT, "sceKernelMapNamedSystemFlexibleMemory");
#endif
    if (!symbol) {
        std::fprintf(stderr, "sceKernelMapNamedSystemFlexibleMemory is not exported by libkernel\n");
        std::abort();
    }
    return reinterpret_cast<MapSystemFlexible>(symbol);
}

static std::size_t Available() {
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0);
    return available;
}

int main() {
    mapSystemFlexible = Resolve();
    constexpr std::size_t length = PS5_PAGE_SIZE * 4;
    const std::size_t before = Available();
    void* mapped = nullptr;
    Require(mapSystemFlexible(&mapped, length, 3, 0, "system mapping") == 0 && mapped != nullptr);
    auto* bytes = static_cast<volatile unsigned char*>(mapped);
    bytes[0] = 17;
    bytes[length - 1] = 29;
    Require(bytes[0] == 17 && bytes[length - 1] == 29);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(mapped, 0, &info, sizeof(info)) == 0);
    Require(std::strcmp(info.name, "system mapping") == 0 && info.protection == 3 && info.is_direct == 0);
    Require(Available() == before);

    void* game = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&game, length, 3, 0, "game mapping") == 0 && game != nullptr);
    Require(Available() == before - length);
    void* replacement = static_cast<unsigned char*>(game) + PS5_PAGE_SIZE;
    void* fixed = replacement;
    Require(mapSystemFlexible(&fixed, PS5_PAGE_SIZE, 3, 0x10, "system fixed") == 0 && fixed == replacement);
    Require(Available() == before - length + PS5_PAGE_SIZE);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(game), length) == 0);
    Require(Available() == before);

    void* invalid = nullptr;
    Require(mapSystemFlexible(&invalid, 0, 3, 0, "empty") == SCE_KERNEL_ERROR_EINVAL && invalid == nullptr);
    Require(mapSystemFlexible(&invalid, PS5_PAGE_SIZE + 1, 3, 0, "unaligned") == SCE_KERNEL_ERROR_EINVAL && invalid == nullptr);
    Require(mapSystemFlexible(&invalid, PS5_PAGE_SIZE + 1, 3, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL && invalid == nullptr);
    void* longest = nullptr;
    Require(mapSystemFlexible(&longest, PS5_PAGE_SIZE, 3, 0, "abcdefghijklmnopqrstuvwxyz01234") == 0 && longest != nullptr);
    Require(sceKernelVirtualQuery(longest, 0, &info, sizeof(info)) == 0 && std::strcmp(info.name, "abcdefghijklmnopqrstuvwxyz01234") == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(longest), PS5_PAGE_SIZE) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapped), length) == 0);
    Require(Available() == before);
    return 0;
}
