#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace AgcDriver {
namespace {

struct HostHdrState {
    std::mutex mutex;
    HostHdrStatus status;
};

HostHdrState& State() {
    static HostHdrState state;
    return state;
}

std::atomic<bool> presentingHdr10{false};

bool SameDisplay(const HostDisplayHdr& a, const HostDisplayHdr& b) {
    return a.hdr == b.hdr && a.maxLuminance == b.maxLuminance && a.minLuminance == b.minLuminance && a.maxFullFrameLuminance == b.maxFullFrameLuminance;
}

void Publish(HostHdrStatus& status, const HostDisplayHdr& display, bool surfaceHdr10) {
    const bool displayChanged = !SameDisplay(status.display, display);
    const bool available = display.hdr && surfaceHdr10 && std::getenv("APS5_FORCE_SDR") == nullptr;
    const bool availabilityChanged = available != status.available;
    status.display = display;
    status.surfaceHdr10 = surfaceHdr10;
    status.available = available;
    if (displayChanged) ++status.displayGeneration;
    if (!availabilityChanged && !(available && displayChanged)) return;
    ++status.generation;
    if (available) std::fprintf(stderr, "[videoout] HDR output available: peak %.1f nits, full frame %.1f nits, black %.4f nits\n", display.maxLuminance, display.maxFullFrameLuminance, display.minLuminance);
    else std::fprintf(stderr, "[videoout] HDR output unavailable (display in HDR: %s, surface offers HDR10: %s)\n", display.hdr ? "yes" : "no", surfaceHdr10 ? "yes" : "no");
}

}

void SetPresentingHdr10(bool hdr10) {
    presentingHdr10.store(hdr10, std::memory_order_release);
}

}

extern "C" void AgcDriverSetHostDisplayHdr_nid_postfix(const AgcDriver::HostDisplayHdr& display) {
    auto& state = AgcDriver::State();
    std::lock_guard lock(state.mutex);
    AgcDriver::Publish(state.status, display, state.status.surfaceHdr10);
}

extern "C" void AgcDriverSetSurfaceHdr10_nid_postfix(bool offered) {
    auto& state = AgcDriver::State();
    std::lock_guard lock(state.mutex);
    AgcDriver::Publish(state.status, state.status.display, offered);
}

extern "C" void AgcDriverNoteReportedToneMapPeak_nid_postfix(float nits) {
    auto& state = AgcDriver::State();
    std::lock_guard lock(state.mutex);
    state.status.reportedToneMapPeak = nits;
}

extern "C" void AgcDriverHostHdr_nid_postfix(AgcDriver::HostHdrStatus& status) {
    auto& state = AgcDriver::State();
    std::lock_guard lock(state.mutex);
    status = state.status;
}

extern "C" bool AgcDriverPresentingHdr10_nid_postfix() {
    return AgcDriver::presentingHdr10.load(std::memory_order_acquire);
}
