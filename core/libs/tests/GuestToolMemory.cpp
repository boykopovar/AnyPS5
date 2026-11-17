#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <atomic>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <vector>


static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Tool memory check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    constexpr std::size_t page = PS5_PAGE_SIZE;
    const auto size = static_cast<int64_t>(sceKernelGetDirectMemorySize());
    int64_t start = -1;
    int64_t end = -1;

    Require(sceKernelGetToolMemoryRange(0x13, nullptr, &end) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetToolMemoryRange(0x13, &start, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetToolMemoryRange(0x13, &start, &end) == 0);
    Require(start == 0);
    Require(end == size);
    Require(sceKernelGetToolMemoryRange(0x14, &start, &end) == 0);
    Require(start == 0 && end == size);

    int64_t phys = 0;
    Require(sceKernelAllocateToolMemory(0x13, 0, size, 0, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateToolMemory(0x13, 0, size, page + 1, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateToolMemory(0x13, 0, size, page, 0, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateToolMemory(0x13, size, 0, page, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);

    int64_t availPhys = -1;
    size_t availSize = 0;
    Require(sceKernelAvailableToolMemorySize(0, size, 0, &availPhys, &availSize) == 0);
    Require(availSize >= page * 4);
    Require(availPhys >= 0 && availPhys + static_cast<int64_t>(availSize) <= size);

    phys = -1;
    Require(sceKernelAllocateToolMemory(0x13, 0, size, page * 4, 0, 0, &phys) == 0);
    Require(phys >= 0);
    Require((phys % static_cast<int64_t>(page)) == 0);
    Require(phys + static_cast<int64_t>(page * 4) <= size);

    size_t availAfter = 0;
    Require(sceKernelAvailableToolMemorySize(0, size, 0, nullptr, &availAfter) == 0);
    Require(availAfter <= availSize);

    void* mapped = nullptr;
    Require(sceKernelMapToolMemory(&mapped, page * 4, 0x33, 0, phys, 0) == 0);
    Require(mapped != nullptr);
    std::memset(mapped, 0xa5, page);
    Require(static_cast<unsigned char*>(mapped)[page - 1] == 0xa5);
    Require(sceKernelMunmap(reinterpret_cast<uint64_t>(mapped), page * 4) == 0);

    Require(sceKernelReleaseToolMemory(phys, page * 4) == 0);
    int64_t phys2 = -1;
    Require(sceKernelAllocateToolMemory(0x14, 0, size, page * 4, 0, 0, &phys2) == 0);
    Require(phys2 >= 0);
    Require(sceKernelReleaseToolMemory(phys2, page * 4) == 0);

    bool rangeThrew = false;
    try {
        sceKernelGetToolMemoryRange(0x15, &start, &end);
    } catch (const std::runtime_error&) {
        rangeThrew = true;
    }
    Require(rangeThrew);
    bool allocThrew = false;
    try {
        int64_t badPhys = 0;
        sceKernelAllocateToolMemory(0x15, 0, size, page, 0, 0, &badPhys);
    } catch (const std::runtime_error&) {
        allocThrew = true;
    }
    Require(allocThrew);
    bool negativeThrew = false;
    try {
        sceKernelGetToolMemoryRange(-1, &start, &end);
    } catch (const std::runtime_error&) {
        negativeThrew = true;
    }
    Require(negativeThrew);

    Require(sceKernelReleaseToolMemory(phys2, page * 4) == 0);
    Require(sceKernelReleaseToolMemory(phys2, page * 4) == 0);

    int64_t physA = -1;
    int64_t physB = -1;
    Require(sceKernelAllocateToolMemory(0x13, 0, size, page * 4, 0, 0, &physA) == 0);
    Require(sceKernelAllocateToolMemory(0x13, 0, size, page * 4, 0, 0, &physB) == 0);
    Require(physA >= 0 && physB >= 0);
    Require(physA + static_cast<int64_t>(page * 4) <= physB || physB + static_cast<int64_t>(page * 4) <= physA);
    void* mappedA = nullptr;
    void* mappedB = nullptr;
    Require(sceKernelMapToolMemory(&mappedA, page * 4, 0x33, 0, physA, 0) == 0);
    Require(sceKernelMapToolMemory(&mappedB, page * 4, 0x33, 0, physB, 0) == 0);
    std::memset(mappedA, 0xa1, page * 4);
    std::memset(mappedB, 0xb2, page * 4);
    Require(static_cast<unsigned char*>(mappedA)[0] == 0xa1 && static_cast<unsigned char*>(mappedA)[page * 4 - 1] == 0xa1);
    Require(static_cast<unsigned char*>(mappedB)[0] == 0xb2 && static_cast<unsigned char*>(mappedB)[page * 4 - 1] == 0xb2);
    Require(sceKernelMunmap(reinterpret_cast<uint64_t>(mappedA), page * 4) == 0);
    Require(sceKernelMunmap(reinterpret_cast<uint64_t>(mappedB), page * 4) == 0);
    Require(sceKernelReleaseToolMemory(physA, page * 4) == 0);
    Require(sceKernelReleaseToolMemory(physB, page * 4) == 0);

    {
        std::vector<std::thread> workers;
        std::atomic<int> failures{0};
        for (int worker = 0; worker < 4; ++worker) {
            const std::int32_t partition = worker % 2 == 0 ? 0x13 : 0x14;
            workers.emplace_back([partition, size, page, &failures] {
                for (int iteration = 0; iteration < 32; ++iteration) {
                    std::int64_t block = -1;
                    if (sceKernelAllocateToolMemory(partition, 0, size, page, 0, 0, &block) != 0) {
                        failures.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                    if (sceKernelReleaseToolMemory(block, page) != 0) {
                        failures.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                }
            });
        }
        for (auto& worker : workers) worker.join();
        Require(failures.load(std::memory_order_relaxed) == 0);
    }


    std::printf("tool memory range=[%lld, %lld) phys=%lld\n", static_cast<long long>(start), static_cast<long long>(end), static_cast<long long>(phys));
    return 0;
}
