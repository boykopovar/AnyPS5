#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/IndirectDispatch.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include <cstdio>
#include <cstdlib>

namespace AgcDriver::DriverDetail {

namespace {

const char* IndirectPathName(int path) {
    switch (path) {
    case IndirectGpu: return "gpu-side";
    case IndirectPendingImage: return "pending image results";
    case IndirectCopiedWrite: return "pending label or copied write";
    case IndirectNotImported: return "not imported";
    case IndirectThreadDimensions: return "thread dimensions";
    case IndirectFillKernel: return "fill kernel";
    case IndirectMisaligned: return "misaligned";
    case IndirectDisabled: return "disabled";
    case IndirectWorkgroupMemory: return "device memory LDS";
    default: return "unknown";
    }
}

}

void Driver::countIndirect(int path, double readMs) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile || path < 0 || path >= IndirectPaths) return;
    static AgcDriver::Mutex countsMutex;
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
    std::fprintf(stderr, "[indirect] (cumulative): gpu-side %llu, cpu-side %llu (thread dimensions %llu, fill kernel %llu, pending image results %llu, pending label or copied write %llu, not imported %llu, misaligned %llu, disabled %llu, device memory LDS %llu); CPU argument reads took %.1f s\n", static_cast<unsigned long long>(counts[IndirectGpu]), static_cast<unsigned long long>(cpu), static_cast<unsigned long long>(counts[IndirectThreadDimensions]), static_cast<unsigned long long>(counts[IndirectFillKernel]), static_cast<unsigned long long>(counts[IndirectPendingImage]), static_cast<unsigned long long>(counts[IndirectCopiedWrite]), static_cast<unsigned long long>(counts[IndirectNotImported]), static_cast<unsigned long long>(counts[IndirectMisaligned]), static_cast<unsigned long long>(counts[IndirectDisabled]), static_cast<unsigned long long>(counts[IndirectWorkgroupMemory]), readWaitedMs / 1000);
}

int Driver::indirectArgumentsPath(std::span<const std::uint32_t> packet, std::uint64_t arguments) {
    static const bool gpuIndirect = std::getenv("APS5_NO_GPU_INDIRECT") == nullptr;
    if (!gpuIndirect) return IndirectDisabled;
    static const bool gpuThreadDimensions = std::getenv("APS5_NO_GPU_THREAD_DIMENSIONS") == nullptr;
    if ((packet.back() & 0x20u) != 0 && !gpuThreadDimensions) return IndirectThreadDimensions;
    if (arguments % 4 != 0) return IndirectMisaligned;
    return IndirectGpu;
}

Driver::IndirectSource& Driver::indirectSource() {
    static thread_local IndirectSource source;
    return source;
}

std::string Driver::describeIndirectSource() {
    const auto& source = indirectSource();
    if (source.arguments == 0) return {};
    const char* ahead = source.ahead == AheadArguments::Matched ? "adopted, its counts matched the re-read" : source.ahead == AheadArguments::Differed ? "dropped, its counts differed from the re-read" : "none";
    char text[256];
    std::snprintf(text, sizeof(text), "; DISPATCH_INDIRECT on queue 0x%x, arguments at 0x%llx, driver path %s, read-ahead %s", source.queue, static_cast<unsigned long long>(source.arguments), IndirectPathName(source.path), ahead);
    return text;
}

std::array<std::uint32_t, 5> Driver::readDispatchArguments(std::uint32_t queue, std::uint64_t arguments, std::uint32_t initiator) {
    recordQueuedLabelsBeforeRead(queue);
    if (!resolvingAhead()) {
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Indirect);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        Graphics::FinishLeaseWork();
    }
    return Pm4::ReadDispatchArguments(arguments, initiator);
}

void Driver::dispatchIndirect(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission) {
    const auto arguments = Pm4::DispatchArgumentAddress(packet, queue);
    const auto initiator = packet.back();
    const int path = indirectArgumentsPath(packet, arguments);
    struct Scope {
        IndirectSource& source;
        IndirectSource previous;
        ~Scope() { source = previous; }
    } scope{indirectSource(), indirectSource()};
    scope.source = IndirectSource{arguments, path, submission.queue, AheadArguments::None};
    if (path != IndirectGpu) {
        const auto readStart = std::chrono::steady_clock::now();
        const auto direct = readDispatchArguments(submission.queue, arguments, initiator);
        countIndirect(path, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
        if (!resolvingAhead()) {
            auto& resolved = resolvedAhead();
            if (const auto found = resolved.find(currentPacketOffset()); found != resolved.end() && found->second.indirectArguments == 0 && found->second.packet[0] == 0xc0031500u) {
                if (found->second.packet == direct) {
                    scope.source.ahead = AheadArguments::Matched;
                } else {
                    resolved.erase(found);
                    ++groupCaptureStats().argumentMismatches;
                    scope.source.ahead = AheadArguments::Differed;
                }
            }
        }
        dispatch(queue, direct, submission);
        return;
    }

    const std::array<std::uint32_t, 5> unresolved{0xc0031500u, 0, 0, 0, initiator};
    dispatch(queue, unresolved, submission, arguments);
}

}
