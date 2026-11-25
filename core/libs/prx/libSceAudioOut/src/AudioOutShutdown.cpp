#include <atomic>
#include <cstdint>
#include "prx/libc/include/Shutdown.hpp"
#include "AudioOutShutdown.hpp"

namespace {

std::atomic<std::uint32_t> inside{0};
std::atomic<bool> stopping{false};
thread_local std::uint32_t threadDepth{0};
thread_local std::uint32_t suspendedDepth{0};

void Leave() {
    if (inside.fetch_sub(1) == 1 || stopping.load()) inside.notify_all();
}

void SuspendForShutdown() {
    const auto depth = threadDepth;
    if (depth == 0) return;
    suspendedDepth = depth;
    inside.fetch_sub(depth);
    inside.notify_all();
}

void ResumeAfterShutdown() {
    const auto depth = suspendedDepth;
    if (depth == 0) return;
    suspendedDepth = 0;
    inside.fetch_add(depth);
}

void Stop() {
    stopping.store(true);
    for (auto count = inside.load(); count > threadDepth; count = inside.load()) inside.wait(count);
}

}

AudioOutCall::AudioOutCall() {
    static const bool registered = [] {
        LibcRegisterShutdownWaitHook_nid_no_patch(SuspendForShutdown, ResumeAfterShutdown);
        LibcRegisterShutdown_nid_postfix(Stop);
        return true;
    }();
    static_cast<void>(registered);
    ++threadDepth;
    inside.fetch_add(1);
    if (stopping.load() || LibcShutdownToken_nid_postfix().stop_requested()) {
        --threadDepth;
        Leave();
        throw ProcessShutdown{};
    }
}

AudioOutCall::~AudioOutCall() {
    --threadDepth;
    Leave();
}

std::uint32_t AudioOutCall::ActiveCount() {
    return inside.load();
}

std::uint32_t AudioOutCall::ThreadDepth() {
    return threadDepth;
}
