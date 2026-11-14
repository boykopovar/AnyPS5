#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "direct memory mapping check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void AuditDirectMemoryAllocationBounds() {
    constexpr std::size_t page = PS5_PAGE_SIZE;
    int64_t phys = -1;

    Require(sceKernelAllocateDirectMemory(-1, 0x10000, page, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateDirectMemory(0x20000, 0x10000, page, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateDirectMemory(0, 0x10000, 0, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateDirectMemory(0, 0x10000, page + 1, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateDirectMemory(0, 0x10000, page, 0, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateDirectMemory(0, 0x10000, page, page - 1, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelAvailableDirectMemorySize(0, 0x100000, 0, nullptr, nullptr) == 0);
    Require(sceKernelAvailableDirectMemorySize(0, 0x100000, 0, reinterpret_cast<int64_t*>(std::uintptr_t{0x10}), nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAvailableDirectMemorySize(0, 0x100000, 0, nullptr, reinterpret_cast<std::size_t*>(std::uintptr_t{0x10})) == SCE_KERNEL_ERROR_EFAULT);

    Require(sceKernelReleaseDirectMemory(-1, page) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(0, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(1, page) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(0, page + 1) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelDirectMemoryQuery(0, 0, nullptr, 0x20) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelDirectMemoryQuery(-1, 0, &phys, 0x20) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelDirectMemoryQuery(0, 0, &phys, 1) == SCE_KERNEL_ERROR_EINVAL);

    int memType = 0;
    int64_t startOut = 0;
    int64_t endOut = 0;
    Require(sceKernelGetDirectMemoryType(0, nullptr, &startOut, &endOut) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetDirectMemoryType(0, &memType, nullptr, &endOut) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetDirectMemoryType(0, &memType, &startOut, nullptr) == SCE_KERNEL_ERROR_EINVAL);
}

void AuditDirectMemoryMappingValidation() {
    constexpr std::size_t page = PS5_PAGE_SIZE;
    constexpr std::uint64_t directMemSize = 0x7fffffffffll;
    int64_t phys = -1;
    Require(sceKernelAllocateDirectMemory(0, directMemSize, page * 2, 0, 0, &phys) == 0);
    Require(phys >= 0);

    bool caughtNull = false;
    try {
        sceKernelMapDirectMemory(nullptr, page, 3, 0, phys, 0);
    } catch (const std::invalid_argument&) {
        caughtNull = true;
    }
    Require(caughtNull);

    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, 0, 3, 0, phys, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory(&mapped, page - 1, 3, 0, phys, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory(&mapped, page + 1, 3, 0, phys, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory(&mapped, page, 3, 0, -1, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory(&mapped, page, 3, 0, 1, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory(&mapped, page, 3, 0, phys + 1, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory(&mapped, page, 0x1000, 0, phys, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory(&mapped, page, 4, 0, phys, 0) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelMapDirectMemory2(&mapped, 0, 0, 3, 0, phys, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory2(&mapped, page - 1, 0, 3, 0, phys, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapDirectMemory2(&mapped, page, 0, 3, 0, -1, 0) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelMapNamedDirectMemory(&mapped, 0, 3, 0, phys, 0, "bad") == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapNamedDirectMemory(&mapped, page - 1, 3, 0, phys, 0, "bad") == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMapNamedDirectMemory(&mapped, page, 3, 0, -1, 0, "bad") == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
}

void AuditDirectMemoryAliasingAndDataFlow() {
    Require(sceKernelEnableDmemAliasing() == 0);
    Require(sceKernelEnableDmemAliasing() == 0);
    Require(sceKernelGetDirectMemorySize() > 0);

    constexpr std::size_t page = PS5_PAGE_SIZE;
    int64_t phys = -1;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    Require(phys >= 0);

    void* vaddr1 = nullptr;
    Require(sceKernelMapDirectMemory(&vaddr1, page * 2, 3, 0, phys, 0) == 0);
    Require(vaddr1 != nullptr);

    Require(!DirectMemoryAliased_nid_postfix(reinterpret_cast<std::uintptr_t>(vaddr1), page));
    Require(!DirectMemoryAliased_nid_postfix(reinterpret_cast<std::uintptr_t>(vaddr1) + page, page));

    auto* p1 = static_cast<unsigned char*>(vaddr1);
    p1[0] = 0x42;
    p1[page + 10] = 0x99;

    Require(sceKernelEnableDmemAliasing() == 0);

    void* vaddr2 = nullptr;
    Require(sceKernelMapDirectMemory(&vaddr2, page, 3, 0, phys + page, 0) == 0);
    Require(vaddr2 != nullptr && vaddr2 != vaddr1);

    Require(DirectMemoryAliased_nid_postfix(reinterpret_cast<std::uintptr_t>(vaddr2), page));
    Require(DirectMemoryAliased_nid_postfix(reinterpret_cast<std::uintptr_t>(vaddr1) + page, page));
    Require(!DirectMemoryAliased_nid_postfix(reinterpret_cast<std::uintptr_t>(vaddr1), page));

    auto* p2 = static_cast<unsigned char*>(vaddr2);
    Require(p2[10] == 0x99);

    p2[10] = 0x77;
    Require(p1[page + 10] == 0x77);

    p1[page + 20] = 0x88;
    Require(p2[20] == 0x88);

    void* vaddr3 = nullptr;
    Require(sceKernelMapDirectMemory2(&vaddr3, page, 0, 3, 0, phys, 0) == 0);
    Require(vaddr3 != nullptr && vaddr3 != vaddr1);
    auto* p3 = static_cast<unsigned char*>(vaddr3);
    Require(p3[0] == 0x42);
    p3[0] = 0x33;
    Require(p1[0] == 0x33);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(vaddr3), page) == 0);

    void* vaddrNamed = nullptr;
    Require(sceKernelMapNamedDirectMemory(&vaddrNamed, page, 3, 0, phys, 0, "test_dmem_named") == 0);
    Require(vaddrNamed != nullptr);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(vaddrNamed), page) == 0);

    Require(sceKernelMprotect(vaddr2, page, 1) == 0);
    p1[page + 10] = 0x55;
    Require(p2[10] == 0x55);

    Require(sceKernelMprotect(vaddr2, page, 3) == 0);

    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(vaddr2), page) == 0);
    Require(!DirectMemoryAliased_nid_postfix(reinterpret_cast<std::uintptr_t>(vaddr1) + page, page));

    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(vaddr1), page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
}

} // namespace

int main() {
    AuditDirectMemoryAllocationBounds();
    AuditDirectMemoryMappingValidation();
    AuditDirectMemoryAliasingAndDataFlow();
    return 0;
}
