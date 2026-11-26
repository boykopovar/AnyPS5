#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"

namespace AgcDriver {

std::atomic<bool> PerformanceTrace::recording{false};
std::atomic<std::uint64_t> PerformanceTrace::ownedDrawUnbound{0};
std::atomic<std::uint64_t> PerformanceTrace::ownedStorageUnbound{0};

void PerformanceTrace::CountOwnedDraw(const char* scope, std::uint64_t amount) {
    if (amount == 0 || !Recording()) return;
    if (auto* thread = Thread()) thread->Count(scope, amount);
    else if (std::string_view(scope).starts_with("OwnedStorage.")) ownedStorageUnbound.fetch_add(1, std::memory_order_relaxed);
    else ownedDrawUnbound.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t PerformanceTrace::OwnedDrawUnbound() {
    return ownedDrawUnbound.load(std::memory_order_relaxed);
}
std::uint64_t PerformanceTrace::OwnedStorageUnbound() {
    return ownedStorageUnbound.load(std::memory_order_relaxed);
}

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
