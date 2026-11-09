#include "prx/libkernel/Time/include/Time.hpp"
#include <array>
#include <chrono>
#include <thread>

int main() {
    const std::array<int, 8> inactive{};
    const int active = 0;
    const int zeroDuration = 0;
    for (const auto& site : inactive) KernelTraceWait_nid_postfix("wait_trace_inactive", &site, 1000000u, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    KernelTraceWait_nid_postfix("wait_trace_active", &active, 2000000u, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    KernelTraceWait_nid_postfix("wait_trace_active_zero", &zeroDuration, 0u, false);
    return 0;
}
