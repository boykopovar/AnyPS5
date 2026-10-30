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
    Require(scePadTrackerGetWorkingMemorySize(first.data(), nullptr) == kErrInvalidArg);
    Require(scePadTrackerGetWorkingMemorySize(nullptr, second.data()) == kErrInvalidArg);
    Require(first[0] == kFill && first[1] == kFill && second[0] == kFill && second[1] == kFill);

    Require(scePadTrackerGetWorkingMemorySize(first.data(), second.data()) == 0);
    Require(first[0] == 0x800000 && second[0] == 0x800000);
    Require(first[1] == kFill && second[1] == kFill);
    return 0;
}
