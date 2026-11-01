#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"

namespace AgcDriver {

std::atomic<bool> PerformanceTrace::recording{false};
thread_local TraceThread* PerformanceTrace::local = nullptr;
thread_local const char* PerformanceTrace::localName = nullptr;
std::atomic<TraceThread* (*)()> PerformanceTrace::binder{nullptr};

}

extern "C" bool AgcDriverTraceRecording_nid_postfix() {
    return AgcDriver::PerformanceTrace::recording.load(std::memory_order_relaxed);
}

extern "C" AgcDriver::TraceThread* AgcDriverTraceThread_nid_postfix() {
    if (auto* thread = AgcDriver::PerformanceTrace::local) return thread;
    const auto bind = AgcDriver::PerformanceTrace::binder.load(std::memory_order_acquire);
    return bind != nullptr ? bind() : nullptr;
}

extern "C" void AgcDriverTraceCount_nid_postfix(const char* scope) {
    if (AgcDriver::PerformanceTrace::binder.load(std::memory_order_relaxed) == nullptr) return;
    if (auto* thread = AgcDriverTraceThread_nid_postfix()) thread->Add(scope, nullptr, {}, {});
}
