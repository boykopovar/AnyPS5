#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

std::uint64_t WorkerAffinityMask() {
    static const std::uint64_t mask = [] {
        if (std::getenv("APS5_NO_WORKER_AFFINITY") != nullptr) return std::uint64_t{0};
        const auto requested = CpuTopology::MaskFromEnvironment("APS5_WORKER_AFFINITY_MASK");
        if (requested != 0) return requested;
#ifndef __linux__
        if (std::getenv("APS5_WORKER_AFFINITY") == nullptr) return std::uint64_t{0};
#endif
        const auto& layout = CpuTopology::Get();
        return layout.hybrid ? layout.performant : std::uint64_t{0};
    }();
    return mask;
}

void PinWorkerThread(const char* role) {
    const auto mask = WorkerAffinityMask();
    if (mask == 0) return;
    static std::once_flag summary;
    std::call_once(summary, [mask] {
        const auto& layout = CpuTopology::Get();
        std::fprintf(stderr, "[affinity] queue workers and presenter -> 0x%llx (hybrid=%d efficient=0x%llx performant=0x%llx process=0x%llx)\n", static_cast<unsigned long long>(mask), layout.hybrid ? 1 : 0, static_cast<unsigned long long>(layout.efficient), static_cast<unsigned long long>(layout.performant), static_cast<unsigned long long>(layout.process));
    });
    CpuTopology::PinTraced(role, nullptr, mask);
}

}
