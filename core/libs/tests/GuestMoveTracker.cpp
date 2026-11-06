#include "prx/libSceMoveTracker/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrInvalidArg = static_cast<int>(0x80ef0003);
constexpr std::uint32_t kFill = 0xaaaaaaaau;

void CheckInitValidation() {
    std::uint8_t memory[16]{};
    Require(sceMoveTrackerInit(nullptr, 1, 0, 0) == kErrInvalidArg);
    Require(sceMoveTrackerInit(memory, 0, 0, 0) == kErrInvalidArg);
    Require(sceMoveTrackerInit(memory, 1, 8, 0) == kErrInvalidArg);
    Require(sceMoveTrackerInit(memory, 1, 0, 8) == kErrInvalidArg);
    Require(sceMoveTrackerInit(nullptr, 0, 8, 8) == kErrInvalidArg);
}

}

int main() {
    CheckInitValidation();
    std::array<std::uint32_t, 2> total{kFill, kFill};
    std::array<std::uint32_t, 2> processing{kFill, kFill};

    Require(sceMoveTrackerGetWorkingMemorySize(nullptr, nullptr) == kErrInvalidArg);
    Require(sceMoveTrackerGetWorkingMemorySize(reinterpret_cast<std::uint32_t*>(0x10), processing.data()) == kErrInvalidArg);
    Require(sceMoveTrackerGetWorkingMemorySize(total.data(), reinterpret_cast<std::uint32_t*>(0x10)) == kErrInvalidArg);
    Require(sceMoveTrackerGetWorkingMemorySize(total.data(), nullptr) == kErrInvalidArg);
    Require(sceMoveTrackerGetWorkingMemorySize(nullptr, processing.data()) == kErrInvalidArg);
    Require(total[0] == kFill && total[1] == kFill && processing[0] == kFill && processing[1] == kFill);

    Require(sceMoveTrackerGetWorkingMemorySize(total.data(), processing.data()) == 0);
    Require(total[0] == 0xa00000 && processing[0] == 0x800000);
    Require(total[1] == kFill && processing[1] == kFill);
    return 0;
}
