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
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
int APS5_VABI sceKernelVirtualQuery(const void*, int, VirtualQueryInfo*, std::uint64_t);
std::uint64_t GuestImageSegmentBytes_nid_no_patch();
void* APS5_VABI dlopen_nid_postfix(const char*, int);
char* APS5_VABI dlerror_nid_postfix();
int APS5_VABI dlclose_nid_postfix(void*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Flexible memory budget check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

static void CheckPosixRelease(std::size_t before) {
    constexpr std::size_t page = PS5_PAGE_SIZE;
    const auto checkBudget = [&](const char* stage, std::size_t expected) {
        std::size_t available = 0;
        Require(sceKernelAvailableFlexibleMemorySize(&available) == 0);
        std::printf("POSIX flexible release %s: available=%zu expected=%zu\n", stage, available, expected);
        std::fflush(stdout);
        Require(available == expected);
    };
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, page * 4, 3, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapped);
    bytes[0] = 17;
    bytes[page * 4 - 1] = 29;
    checkBudget("allocated", before - page * 4);
    auto* middle = static_cast<unsigned char*>(mapped) + page;
    Require(munmap_nid_postfix(middle, page + 1) == 0);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(middle, 0, &info, sizeof(info)) != 0);
    Require(sceKernelVirtualQuery(middle + page, 0, &info, sizeof(info)) != 0);
    Require(bytes[0] == 17 && bytes[page * 4 - 1] == 29);
    checkBudget("rounded partial unmap", before - page * 2);
    Require(sceKernelReleaseFlexibleMemory(mapped, page) == 0);
    checkBudget("kernel releases first page", before - page);
    auto* last = static_cast<unsigned char*>(mapped) + page * 3;
    Require(munmap_nid_postfix(last, 1) == 0);
    Require(sceKernelVirtualQuery(last, 0, &info, sizeof(info)) != 0);
    checkBudget("POSIX releases last page", before);
    Require(munmap_nid_postfix(mapped, page * 4) == 0);
    checkBudget("already unmapped", before);
    mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, page * 3, 3, 0) == 0);
    Require(munmap_nid_postfix(mapped, page * 3 - 1) == 0);
    for (std::size_t offset = 0; offset < page * 3; offset += page) {
        Require(sceKernelVirtualQuery(static_cast<unsigned char*>(mapped) + offset, 0, &info, sizeof(info)) != 0);
    }
    checkBudget("rounded full unmap", before);
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
    CheckPosixRelease(before);
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
    }
    if (module) {
        Require(dlclose_nid_postfix(module) == 0);
        Require(sceKernelAvailableFlexibleMemorySize(&available) == 0);
        Require(available > before && available + GuestImageSegmentBytes_nid_no_patch() == configured);
    }
    std::puts("Flexible memory budget checks passed");
}
