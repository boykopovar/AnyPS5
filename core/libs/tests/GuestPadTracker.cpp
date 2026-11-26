#include "prx/libScePadTracker/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrInvalidArg = static_cast<int>(0x80c40003);
constexpr std::uint32_t kFill = 0xaaaaaaaau;

}

int main() {
    std::array<std::uint32_t, 2> first{kFill, kFill};
    std::array<std::uint32_t, 2> second{kFill, kFill};

    Require(scePadTrackerGetWorkingMemorySize(nullptr, nullptr) == kErrInvalidArg);
    Require(scePadTrackerGetWorkingMemorySize(reinterpret_cast<std::uint32_t*>(0x10), second.data()) == kErrInvalidArg);
    Require(scePadTrackerGetWorkingMemorySize(first.data(), reinterpret_cast<std::uint32_t*>(0x10)) == kErrInvalidArg);
    Require(scePadTrackerGetWorkingMemorySize(first.data(), nullptr) == kErrInvalidArg);
    Require(scePadTrackerGetWorkingMemorySize(nullptr, second.data()) == kErrInvalidArg);
    Require(first[0] == kFill && first[1] == kFill && second[0] == kFill && second[1] == kFill);

    Require(scePadTrackerGetWorkingMemorySize(first.data(), second.data()) == 0);
    Require(first[0] == 0x800000 && second[0] == 0x800000);
    Require(first[1] == kFill && second[1] == kFill);

    constexpr int kErrNotInitialized = static_cast<int>(0x80c40001);
    constexpr int kErrAlreadyInitialized = static_cast<int>(0x80c40002);
    constexpr int kErrHandleNotFound = static_cast<int>(0x80c40004);
    static std::array<std::uint8_t, 0xc000> memoryStorage{};
    auto* const memoryA = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(memoryStorage.data()) + 0x3fff) & ~static_cast<std::uintptr_t>(0x3fff));
    auto* const memoryB = memoryA + 0x4000;
    std::array<std::uint8_t, 0x18> state{};
    state.fill(0x5a);
    Require(scePadTrackerTerm() == kErrNotInitialized);
    Require(scePadTrackerCalibrate() == kErrNotInitialized);
    Require(scePadTrackerReadState(1, state.data()) == kErrNotInitialized);
    Require(scePadTrackerUpdate() == kErrNotInitialized);
    Require(scePadTrackerInit(nullptr, memoryB, 0, 0) == kErrInvalidArg);
    Require(scePadTrackerInit(memoryA + 0x10, memoryB, 0, 0) == kErrInvalidArg);
    Require(scePadTrackerInit(memoryA, nullptr, 0, 0) == kErrInvalidArg);
    Require(scePadTrackerInit(memoryA, memoryB + 0x10, 0, 0) == kErrInvalidArg);
    Require(scePadTrackerInit(memoryA, memoryB, -1, 0) == kErrInvalidArg);
    Require(scePadTrackerInit(memoryA, memoryB, 7, 0) == kErrInvalidArg);
    Require(scePadTrackerInit(memoryA, memoryB, 0, -1) == kErrInvalidArg);
    Require(scePadTrackerInit(memoryA, memoryB, 0, 8) == kErrInvalidArg);
    Require(scePadTrackerTerm() == kErrNotInitialized);
    Require(scePadTrackerInit(memoryA, memoryB, 6, 7) == 0);
    Require(scePadTrackerInit(memoryA, memoryB, 0, 0) == kErrAlreadyInitialized);
    Require(scePadTrackerInit(nullptr, memoryB, 0, 0) == kErrInvalidArg);
    Require(scePadTrackerCalibrate() == 0);
    Require(scePadTrackerReadState(1, nullptr) == kErrInvalidArg);
    Require(scePadTrackerReadState(1, state.data()) == kErrHandleNotFound);
    Require(state[0] == 0x5a && state[0x17] == 0x5a);
    Require(scePadTrackerTerm() == 0);
    Require(scePadTrackerTerm() == kErrNotInitialized);
    Require(scePadTrackerInit(memoryA, memoryB, 0, 0) == 0);
    Require(scePadTrackerTerm() == 0);
    return 0;
}
