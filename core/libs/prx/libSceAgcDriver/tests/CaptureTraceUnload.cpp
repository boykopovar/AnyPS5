#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"

extern "C" void CaptureTraceRecord() {
    for (unsigned i = 0; i < 32; ++i) AgcDriver::CaptureTrace::Log("unload-record item=%u", i);
}

extern "C" void CaptureTraceStop() {
    AgcDriver::CaptureTrace::Stop();
}
