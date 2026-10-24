#include "prx/libc/include/exceptions/Unwind.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" _Unwind_Reason_Code APS5_VABI _Unwind_Backtrace_nid_postfix(_Unwind_Trace_Fn trace, void* argument);
extern "C" _Unwind_Reason_Code APS5_VABI _Unwind_ForcedUnwind_nid_postfix(_Unwind_Exception* exception, _Unwind_Stop_Fn stop, void* argument);
extern "C" _Unwind_Ptr APS5_VABI _Unwind_GetRegionStart_nid_postfix(_Unwind_Context* context);
extern "C" void* APS5_VABI __cxa_begin_catch_nid_postfix(void* exception);
extern "C" void APS5_VABI __cxa_end_catch_nid_postfix();
extern "C" [[gnu::noinline]] void BacktraceFromHere();
extern "C" [[gnu::noinline]] void ForceUnwindFromHere();

namespace {

constexpr std::uint64_t ForeignClass = 0x54455354464f5200;
constexpr std::uint64_t GnuClass = 0x474e5543432b2b00;

using Cleanup = decltype(_Unwind_Exception::exception_cleanup);

struct Call {
    int count;
    int version;
    _Unwind_Action actions;
    std::uint64_t exceptionClass;
    _Unwind_Exception* exception;
    void* argument;
    _Unwind_Ptr region;
    _Unwind_Reason_Code reason;
};

Call trace;
Call stop;
Call cleanup;
int traceArgument;
int stopArgument;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "unwind callback check failed: %s\n", message);
    std::exit(1);
}

_Unwind_Reason_Code APS5_VABI Trace(_Unwind_Context* context, void* argument) {
    ++trace.count;
    trace.argument = argument;
    trace.region = argument == &traceArgument ? _Unwind_GetRegionStart_nid_postfix(context) : 0;
    return _URC_NORMAL_STOP;
}

_Unwind_Reason_Code APS5_VABI Stop(int version, _Unwind_Action actions, std::uint64_t exceptionClass, _Unwind_Exception* exception,
                                   _Unwind_Context* context, void* argument) {
    ++stop.count;
    stop.version = version;
    stop.actions = actions;
    stop.exceptionClass = exceptionClass;
    stop.exception = exception;
    stop.argument = argument;
    stop.region = argument == &stopArgument ? _Unwind_GetRegionStart_nid_postfix(context) : 0;
    return _URC_NORMAL_STOP;
}

void APS5_VABI GuestCleanup(_Unwind_Reason_Code reason, _Unwind_Exception* exception) {
    ++cleanup.count;
    cleanup.reason = reason;
    cleanup.exception = exception;
}

void HostCleanup(_Unwind_Reason_Code reason, _Unwind_Exception* exception) {
    ++cleanup.count;
    cleanup.reason = reason;
    cleanup.exception = exception;
}

void CheckCleanup(const _Unwind_Exception& exception) {
    Require(cleanup.count == 1, "exception_cleanup is called once");
    Require(cleanup.reason == _URC_FOREIGN_EXCEPTION_CAUGHT, "exception_cleanup gets _URC_FOREIGN_EXCEPTION_CAUGHT");
    Require(cleanup.exception == &exception, "exception_cleanup gets the deleted exception");
}

void CheckDeleteException() {
    _Unwind_Exception exception {};
    exception.exception_class = ForeignClass;
    exception.exception_cleanup = GuestCleanup;
    _Unwind_DeleteException_nid_postfix(&exception);
    CheckCleanup(exception);
}

void CheckEndCatch(std::uint64_t exceptionClass, Cleanup function) {
    _Unwind_Exception exception {};
    exception.exception_class = exceptionClass;
    exception.exception_cleanup = function;
    __cxa_begin_catch_nid_postfix(&exception);
    __cxa_end_catch_nid_postfix();
    CheckCleanup(exception);
}

}

extern "C" void BacktraceFromHere() {
    Require(_Unwind_Backtrace_nid_postfix(Trace, &traceArgument) == _URC_NORMAL_STOP, "_Unwind_Backtrace stops when the callback asks");
    Require(trace.count == 1, "_Unwind_Backtrace calls the callback once");
    Require(trace.argument == &traceArgument, "the trace callback gets the argument given to _Unwind_Backtrace");
    Require(trace.region == reinterpret_cast<_Unwind_Ptr>(&BacktraceFromHere), "the trace callback gets the context of the caller of _Unwind_Backtrace");
}

extern "C" void ForceUnwindFromHere() {
    _Unwind_Exception exception {};
    exception.exception_class = ForeignClass;
    _Unwind_ForcedUnwind_nid_postfix(&exception, Stop, &stopArgument);
    Require(stop.count == 1, "_Unwind_ForcedUnwind calls the stop function once");
    Require(stop.version == 1, "the stop function gets version 1");
    Require(stop.actions == (_UA_CLEANUP_PHASE | _UA_FORCE_UNWIND), "the stop function gets _UA_CLEANUP_PHASE | _UA_FORCE_UNWIND");
    Require(stop.exceptionClass == ForeignClass, "the stop function gets the exception class");
    Require(stop.exception == &exception, "the stop function gets the exception");
    Require(stop.argument == &stopArgument, "the stop function gets the argument given to _Unwind_ForcedUnwind");
    Require(stop.region == reinterpret_cast<_Unwind_Ptr>(&ForceUnwindFromHere), "the stop function gets the context of the caller of _Unwind_ForcedUnwind");
}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "backtrace") BacktraceFromHere();
    else if (mode == "forced_unwind") ForceUnwindFromHere();
    else if (mode == "delete_exception") CheckDeleteException();
    else if (mode == "end_catch") CheckEndCatch(ForeignClass, GuestCleanup);
    else if (mode == "end_catch_gnu") CheckEndCatch(GnuClass, reinterpret_cast<Cleanup>(&HostCleanup));
    else Require(false, "unknown mode");
    std::printf("unwind callback ABI %s test passed\n", mode.c_str());
    return 0;
}
