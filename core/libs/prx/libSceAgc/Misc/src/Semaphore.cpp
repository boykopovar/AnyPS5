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

constexpr std::uint64_t semaphoreMemoryAlignment = 0x4000u;
constexpr std::uint64_t semaphoreLabelBytes = 32u;

std::mutex semaphoreMutex;
std::uint8_t* semaphoreMemory = nullptr;
std::uint64_t semaphoreBytes = 0;



}

extern "C" {

int APS5_VABI sceAgcSetAmmSemaphoreMemory(void* memory, std::uint64_t sizeInBytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    std::lock_guard lock(semaphoreMutex);
    if (semaphoreBytes != 0) return static_cast<int>(0x8a6c0048u);
    if (((address | sizeInBytes) & (semaphoreMemoryAlignment - 1u)) != 0) return static_cast<int>(0x8a6c0002u);
    Agc::Command::Require(memory != nullptr && sizeInBytes != 0, __func__, "null or empty AMM semaphore memory; the console's error is unknown");
    Agc::Command::Require(sizeInBytes - 1u <= std::numeric_limits<std::uintptr_t>::max() - address, __func__, "AMM semaphore memory wraps the address space");
    auto* labels = static_cast<std::uint8_t*>(memory);
    std::memset(labels, 0, sizeInBytes);
    semaphoreMemory = labels;
    semaphoreBytes = sizeInBytes;
    return 0;
}

int APS5_VABI sceAgcGetSemaphoreLabel(std::uint32_t index, void** label) {
    std::lock_guard lock(semaphoreMutex);
    if (semaphoreBytes == 0) return static_cast<int>(0x8a6c0049u);
    if (index >= semaphoreBytes / semaphoreLabelBytes) return static_cast<int>(0x8a6c000bu);
    Agc::Command::Require(label != nullptr, __func__, "null semaphore label output");
    *label = semaphoreMemory + static_cast<std::uint64_t>(index) * semaphoreLabelBytes;
    return 0;
}

}
