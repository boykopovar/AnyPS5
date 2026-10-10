#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/IndirectDispatch.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

void Driver::countIndirect(int path, double readMs) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile || path < 0 || path >= IndirectPaths) return;
    static std::mutex countsMutex;
    static std::uint64_t counts[IndirectPaths] = {};
    static double readWaitedMs = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    std::lock_guard lock(countsMutex);
    ++counts[path];
    readWaitedMs += readMs;
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport < std::chrono::seconds(10)) return;
    lastReport = now;
    std::uint64_t cpu = 0;
    for (int i = 1; i < IndirectPaths; ++i) cpu += counts[i];
    std::fprintf(stderr, "[indirect] gpu-side %llu, cpu-side %llu (thread dimensions %llu, fill kernel %llu, pending image results %llu, pending label or copied write %llu, not imported %llu, misaligned %llu, disabled %llu); CPU argument reads took %.1f s\n", static_cast<unsigned long long>(counts[IndirectGpu]), static_cast<unsigned long long>(cpu), static_cast<unsigned long long>(counts[IndirectThreadDimensions]), static_cast<unsigned long long>(counts[IndirectFillKernel]), static_cast<unsigned long long>(counts[IndirectPendingImage]), static_cast<unsigned long long>(counts[IndirectCopiedWrite]), static_cast<unsigned long long>(counts[IndirectNotImported]), static_cast<unsigned long long>(counts[IndirectMisaligned]), static_cast<unsigned long long>(counts[IndirectDisabled]), readWaitedMs / 1000);
}

void Driver::dispatchIndirect(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission) {
    static const bool gpuIndirect = std::getenv("APS5_NO_GPU_INDIRECT") == nullptr;
    const auto arguments = Pm4::DispatchArgumentAddress(packet, queue);
    const auto initiator = packet.back();
    int path = IndirectGpu;
    if (!gpuIndirect) path = IndirectDisabled;
    else if ((initiator & 0x20u) != 0) path = IndirectThreadDimensions;
    else if (arguments % 4 != 0) path = IndirectMisaligned;
    if (path != IndirectGpu) {
        if (!resolvingAhead()) {
            const auto& resolved = resolvedAhead();
            if (const auto found = resolved.find(currentPacketOffset()); found != resolved.end() && found->second.indirectArguments == 0 && found->second.packet[0] == 0xc0031500u) {
                const auto direct = found->second.packet;
                dispatch(queue, direct, submission);
                return;
            }
        }
        recordQueuedLabelsBeforeRead(submission.queue);
        const auto readStart = std::chrono::steady_clock::now();
        const auto direct = Pm4::ReadDispatchArguments(arguments, initiator);
        countIndirect(path, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
        dispatch(queue, direct, submission);
        return;
    }

    const std::array<std::uint32_t, 5> unresolved{0xc0031500u, 0, 0, 0, initiator};
    dispatch(queue, unresolved, submission, arguments);
}

}
