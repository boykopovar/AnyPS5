#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"

namespace AgcDriver {

std::atomic<bool> PerformanceTrace::recording{false};
thread_local TraceThread* PerformanceTrace::local = nullptr;

}

extern "C" bool AgcDriverTraceRecording_nid_postfix() {
    return AgcDriver::PerformanceTrace::recording.load(std::memory_order_relaxed);
}

extern "C" AgcDriver::TraceThread* AgcDriverTraceThread_nid_postfix() {
    return AgcDriver::PerformanceTrace::local;
}
