#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string_view>

extern "C" {
int APS5_VABI sceKernelConfiguredFlexibleMemorySize(std::size_t*);
int APS5_VABI sceKernelAvailableFlexibleMemorySize(std::size_t*);
int APS5_VABI sceKernelMapFlexibleMemory(void**, std::size_t, int, int);
int APS5_VABI sceKernelReleaseFlexibleMemory(void*, std::size_t);
std::uint64_t GuestImageSegmentBytes_nid_no_patch();
void* APS5_VABI dlopen_nid_postfix(const char*, int);
char* APS5_VABI dlerror_nid_postfix();
int APS5_VABI dlclose_nid_postfix(void*);
std::size_t APS5_VABI sceKernelGetDirectMemorySize();
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelMapDirectMemory2(void**, std::size_t, int, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelMapNamedDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t, const char*);
int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry*, int, int*, int);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelVirtualQuery(const void*, int, VirtualQueryInfo*, std::uint64_t);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Flexible memory budget check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view mode = argv[1];
    Require(sceKernelConfiguredFlexibleMemorySize(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAvailableFlexibleMemorySize(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    std::size_t configured = 0;
    if (mode == "invalid") {
        bool rejected = false;
        try { sceKernelConfiguredFlexibleMemorySize(&configured); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected && configured == 0);
        return 0;
    }
    const std::size_t expected = (mode == "declared" ? 800u : mode == "limited" ? 16u : 448u) * 1024u * 1024u;
    Require(sceKernelConfiguredFlexibleMemorySize(&configured) == 0 && configured == expected);
    std::size_t before = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    Require(before + GuestImageSegmentBytes_nid_no_patch() == configured);
    void* module = nullptr;
    if (mode == "declared") {
        const std::filesystem::path fixture = GUEST_FLEXIBLE_MEMORY_MODULE;
        AddPathAlias_nid_no_patch("budget_module", fixture.parent_path().string().c_str());
        const auto path = "/budget_module/" + fixture.filename().generic_string();
        module = dlopen_nid_postfix(path.c_str(), 2);
        if (!module) std::fprintf(stderr, "Cannot load budget fixture: %s\n", dlerror_nid_postfix());
        Require(module != nullptr);
        Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
        Require(GuestImageSegmentBytes_nid_no_patch() > 448u * 1024u * 1024u);
        Require(before + GuestImageSegmentBytes_nid_no_patch() == configured && before > 0x80000);
    }
    constexpr std::size_t length = 0x80000;
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, length, 3, 0) == 0 && mapped != nullptr);
    static_cast<volatile unsigned char*>(mapped)[length - 1] = 7;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - length);
    Require(sceKernelReleaseFlexibleMemory(mapped, length / 2) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - length / 2);
    Require(sceKernelReleaseFlexibleMemory(static_cast<unsigned char*>(mapped) + length / 2, length / 2) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
    void* oversized = nullptr;
    Require(sceKernelMapFlexibleMemory(&oversized, before + PS5_PAGE_SIZE, 3, 0) == SCE_KERNEL_ERROR_ENOMEM && oversized == nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
    if (mode == "limited") {
        const auto length = before & ~(PS5_PAGE_SIZE - 1);
        void* full = nullptr;
        Require(sceKernelMapFlexibleMemory(&full, length, 3, 0) == 0);
        void* extra = nullptr;
        Require(sceKernelMapFlexibleMemory(&extra, PS5_PAGE_SIZE, 3, 0) == SCE_KERNEL_ERROR_ENOMEM && extra == nullptr);
        Require(sceKernelReleaseFlexibleMemory(full, length) == 0);
        Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
        for (int api = 0; api < 4; ++api) {
            constexpr auto directPage = PS5_PAGE_SIZE;
            const auto checkDirectBudget = [&](std::size_t expected) {
                std::size_t current = 0;
                Require(sceKernelAvailableFlexibleMemorySize(&current) == 0);
                std::printf("Direct replacement API=%d available=%zu expected=%zu\n", api, current, expected);
                std::fflush(stdout);
                Require(current == expected);
            };
            void* original = nullptr;
            Require(sceKernelMapFlexibleMemory(&original, directPage * 4, 3, 0) == 0);
            auto* bytes = static_cast<volatile unsigned char*>(original);
            bytes[0] = 17;
            bytes[directPage * 4 - 1] = 29;
            std::int64_t physical = -1;
            Require(sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), directPage * 2, directPage, 0, &physical) == 0);
            const auto mapDirect = [&](void*& address, int flags) {
                if (api == 0) return sceKernelMapDirectMemory(&address, directPage * 2, 3, flags, physical, directPage);
                if (api == 1) return sceKernelMapDirectMemory2(&address, directPage * 2, 0, 3, flags, physical, directPage);
                if (api == 2) return sceKernelMapNamedDirectMemory(&address, directPage * 2, 3, flags, physical, directPage, "budget direct");
                KernelBatchMapEntry entry{address, static_cast<std::uint64_t>(physical), directPage * 2, 3, 0, 0, 0};
                int processed = -1;
                const int result = sceKernelBatchMap2(&entry, 1, &processed, flags);
                Require(processed == (result == 0 ? 1 : 0));
                address = entry.start;
                return result;
            };
            void* middle = static_cast<unsigned char*>(original) + directPage;
            Require(mapDirect(middle, 0x90) == SCE_KERNEL_ERROR_ENOMEM);
            checkDirectBudget(before - directPage * 4);
            Require(bytes[0] == 17 && bytes[directPage * 4 - 1] == 29);
            Require(mapDirect(middle, 0x10) == 0 && middle == static_cast<unsigned char*>(original) + directPage);
            VirtualQueryInfo info{};
            Require(sceKernelVirtualQuery(middle, 0, &info, sizeof(info)) == 0 && info.is_direct && !info.is_flexible);
            Require(bytes[0] == 17 && bytes[directPage * 4 - 1] == 29);
            checkDirectBudget(before - directPage * 2);
            const auto recovered = before - directPage * 2;
            void* reused = nullptr;
            Require(sceKernelMapFlexibleMemory(&reused, recovered, 3, 0) == 0);
            static_cast<volatile unsigned char*>(reused)[recovered - 1] = 31;
            Require(sceKernelReleaseFlexibleMemory(reused, recovered) == 0);
            checkDirectBudget(before - directPage * 2);
            Require(sceKernelReleaseDirectMemory(physical, directPage * 2) == 0);
            checkDirectBudget(before - directPage * 2);
            Require(sceKernelReleaseFlexibleMemory(original, directPage * 4) == 0);
            checkDirectBudget(before);
        }
    }
    if (module) {
        Require(dlclose_nid_postfix(module) == 0);
        Require(sceKernelAvailableFlexibleMemorySize(&available) == 0);
        Require(available > before && available + GuestImageSegmentBytes_nid_no_patch() == configured);
    }
    std::puts("Flexible memory budget checks passed");
}
