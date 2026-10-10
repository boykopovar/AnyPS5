#include "prx/libkernel/Time/include/Time.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <string>

namespace {

using Testing::Case;
using Testing::Require;

constexpr std::uint64_t minuteMicros = 60ULL * 1000 * 1000;

const Case counterStartsNearZero{"ProcessTimeCounter_FirstRead_IsRelativeToProcessStart", [] {
    const std::uint64_t counter = sceKernelGetProcessTimeCounter();
    Require(counter < minuteMicros * 1000, "process time counter wrapped: " + std::to_string(counter));
}};

const Case timeStartsNearZero{"ProcessTime_Read_IsRelativeToProcessStart", [] {
    const std::uint64_t time = sceKernelGetProcessTime();
    Require(time < minuteMicros, "process time wrapped: " + std::to_string(time));
}};

const Case timeIsMonotonic{"ProcessTime_ConsecutiveReads_NeverGoBackwards", [] {
    const std::uint64_t first = sceKernelGetProcessTime();
    const std::uint64_t second = sceKernelGetProcessTime();
    Require(second >= first, "process time went backwards");
}};

} // namespace
