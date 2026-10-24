#include "prx/libSceAgc/Misc/include/Semaphore.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int GRAPHICS5_ERROR_INVALID_ALIGNMENT = static_cast<int>(0x8a6c0002u);
constexpr int GRAPHICS5_ERROR_OUT_OF_RANGE = static_cast<int>(0x8a6c000bu);
constexpr int GRAPHICS5_ERROR_SEMAPHORE_MEMORY_REGISTERED = static_cast<int>(0x8a6c0048u);
constexpr int GRAPHICS5_ERROR_SEMAPHORE_MEMORY_UNREGISTERED = static_cast<int>(0x8a6c0049u);
constexpr std::uint64_t semaphoreMemoryAlignment = 0x4000u;
constexpr std::uint64_t semaphoreMemoryLimit = 0x40000000u;
constexpr std::uint64_t semaphoreLabelBytes = 32u;

std::mutex semaphoreMutex;
std::uint8_t* semaphoreMemory = nullptr;
std::uint64_t semaphoreBytes = 0;

bool aligned(std::uint64_t value) {
    return (value & (semaphoreMemoryAlignment - 1u)) == 0;
}

int registerSemaphoreMemory(void* memory, std::uint64_t sizeInBytes, bool limited, const char* name) {
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    std::lock_guard lock(semaphoreMutex);
    if (semaphoreBytes != 0) return GRAPHICS5_ERROR_SEMAPHORE_MEMORY_REGISTERED;
    if (!aligned(sizeInBytes)) return GRAPHICS5_ERROR_INVALID_ALIGNMENT;
    if (limited && sizeInBytes > semaphoreMemoryLimit) return GRAPHICS5_ERROR_OUT_OF_RANGE;
    if (!aligned(address)) return GRAPHICS5_ERROR_INVALID_ALIGNMENT;
    if (sizeInBytes != 0) {
        Agc::Command::Require(memory != nullptr, name, "null semaphore memory with a non-zero size");
        Agc::Command::Require(sizeInBytes - 1u <= std::numeric_limits<std::uintptr_t>::max() - address, name, "semaphore memory wraps the address space");
        std::memset(memory, 0, sizeInBytes);
    }
    semaphoreMemory = static_cast<std::uint8_t*>(memory);
    semaphoreBytes = sizeInBytes;
    return 0;
}

}

extern "C" {

int APS5_VABI sceAgcSetAmmSemaphoreMemory(void* memory, std::uint64_t sizeInBytes) {
    return registerSemaphoreMemory(memory, sizeInBytes, false, __func__);
}

int APS5_VABI sceAgcSetSemaphoreMemory(void* memory, std::uint64_t sizeInBytes) {
    return registerSemaphoreMemory(memory, sizeInBytes, true, __func__);
}

int APS5_VABI sceAgcGetSemaphoreLabel(std::uint32_t index, void** label) {
    std::lock_guard lock(semaphoreMutex);
    if (semaphoreBytes == 0) return GRAPHICS5_ERROR_SEMAPHORE_MEMORY_UNREGISTERED;
    if (index >= semaphoreBytes / semaphoreLabelBytes) return GRAPHICS5_ERROR_OUT_OF_RANGE;
    Agc::Command::Require(label != nullptr, __func__, "null semaphore label output");
    *label = semaphoreMemory + static_cast<std::uint64_t>(index) * semaphoreLabelBytes;
    return 0;
}

}
