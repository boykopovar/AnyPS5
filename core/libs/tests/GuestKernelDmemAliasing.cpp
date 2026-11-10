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

constexpr std::size_t PS5_PAGE_SIZE = 0x4000;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "dmem aliasing check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void AuditInternalMemoryAvailableSize() {
    Require(sceKernelInternalMemoryGetAvailableSize(nullptr) == -1);

    std::size_t validSize = 0xdeadbeefcafeull;
    bool caughtException = false;
    try {
        sceKernelInternalMemoryGetAvailableSize(&validSize);
    } catch (const std::runtime_error&) {
        caughtException = true;
    }
    Require(caughtException);
    Require(validSize == 0xdeadbeefcafeull);
}

void AuditEnableDmemAliasingAndDirectMapping() {
    Require(sceKernelEnableDmemAliasing() == 0);
    Require(sceKernelEnableDmemAliasing() == 0);

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

    Require(sceKernelMprotect(vaddr2, page, 1) == 0);
    p1[page + 10] = 0x55;
    Require(p2[10] == 0x55);

    Require(sceKernelMprotect(vaddr2, page, 3) == 0);

    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(vaddr2), page) == 0);
    Require(!DirectMemoryAliased_nid_postfix(reinterpret_cast<std::uintptr_t>(vaddr1) + page, page));

    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(vaddr1), page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
}

void AuditDirectMemoryParameterBounds() {
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
}

} // namespace

int main() {
    AuditInternalMemoryAvailableSize();
    AuditEnableDmemAliasingAndDirectMapping();
    AuditDirectMemoryParameterBounds();
    return 0;
}
