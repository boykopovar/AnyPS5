#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
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
        const auto checkBudget = [&](std::size_t expected) {
            std::size_t current = 0;
            Require(sceKernelAvailableFlexibleMemorySize(&current) == 0);
            std::printf("Fixed flexible mapping: available=%zu expected=%zu\n", current, expected);
            std::fflush(stdout);
            Require(current == expected);
        };
        constexpr auto page = PS5_PAGE_SIZE;
        void* original = nullptr;
        Require(sceKernelMapFlexibleMemory(&original, page * 4, 3, 0) == 0);
        auto* bytes = static_cast<volatile unsigned char*>(original);
        for (std::size_t i = 0; i < 4; ++i) bytes[i * page] = static_cast<unsigned char>(17 + i);
        auto* middle = static_cast<unsigned char*>(original) + page;
        void* replacement = middle;
        Require(sceKernelMapFlexibleMemory(&replacement, page, 3, 0x10) == 0 && replacement == middle);
        Require(bytes[0] == 17 && bytes[page] == 0 && bytes[page * 2] == 19 && bytes[page * 3] == 20);
        checkBudget(before - page * 4);
        bytes[page] = 23;
        Require(sceKernelMapFlexibleMemory(&replacement, page, 3, 0x90) == SCE_KERNEL_ERROR_ENOMEM);
        Require(bytes[page] == 23);
        checkBudget(before - page * 4);
        replacement = original;
        Require(sceKernelMapFlexibleMemory(&replacement, page, 3, 0x10) == 0 && replacement == original);
        Require(bytes[0] == 0 && bytes[page] == 23 && bytes[page * 3] == 20);
        checkBudget(before - page * 4);
        Require(sceKernelMapFlexibleMemory(&replacement, page * 3, 3, 0x10) == 0 && replacement == original);
        Require(bytes[0] == 0 && bytes[page] == 0 && bytes[page * 2] == 0 && bytes[page * 3] == 20);
        checkBudget(before - page * 4);
        Require(sceKernelReleaseFlexibleMemory(middle, page * 2) == 0);
        checkBudget(before - page * 2);
        replacement = middle;
        Require(sceKernelMapFlexibleMemory(&replacement, page * 3, 3, 0x10) == 0 && replacement == middle);
        Require(bytes[page] == 0 && bytes[page * 3] == 0);
        checkBudget(before - page * 4);
        Require(sceKernelReleaseFlexibleMemory(original, page * 4) == 0);
        checkBudget(before);
        const auto length = before & ~(PS5_PAGE_SIZE - 1);
        void* full = nullptr;
        Require(sceKernelMapFlexibleMemory(&full, length, 3, 0) == 0);
        void* extra = nullptr;
        Require(sceKernelMapFlexibleMemory(&extra, PS5_PAGE_SIZE, 3, 0) == SCE_KERNEL_ERROR_ENOMEM && extra == nullptr);
        static_cast<volatile unsigned char*>(full)[0] = 31;
        replacement = full;
        Require(sceKernelMapFlexibleMemory(&replacement, length, 3, 0x90) == SCE_KERNEL_ERROR_ENOMEM);
        Require(static_cast<volatile unsigned char*>(full)[0] == 31);
        checkBudget(before - length);
        Require(sceKernelMapFlexibleMemory(&replacement, length + page, 3, 0x10) == SCE_KERNEL_ERROR_ENOMEM);
        Require(static_cast<volatile unsigned char*>(full)[0] == 31);
        checkBudget(before - length);
        Require(sceKernelMapFlexibleMemory(&replacement, length, 3, 0x10) == 0 && replacement == full);
        Require(static_cast<volatile unsigned char*>(full)[0] == 0);
        checkBudget(before - length);
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
