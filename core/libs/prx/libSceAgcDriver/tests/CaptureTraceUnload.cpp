#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include <windows.h>

namespace {

HANDLE exitReceipt = INVALID_HANDLE_VALUE;

}

extern "C" bool CaptureTracePrepareProcessExit() {
    const wchar_t* path = _wgetenv(L"APS5_CAPTURE_TRACE_EXIT_RECEIPT");
    if (path == nullptr || path[0] == L'\0') return false;
    exitReceipt = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    return exitReceipt != INVALID_HANDLE_VALUE;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_DETACH && exitReceipt != INVALID_HANDLE_VALUE) {
        const char marker = reserved != nullptr ? 'P' : 'U';
        DWORD written = 0;
        WriteFile(exitReceipt, &marker, 1, &written, nullptr);
        CloseHandle(exitReceipt);
        exitReceipt = INVALID_HANDLE_VALUE;
    }
    return TRUE;
}

extern "C" void CaptureTraceRecord() {
    for (unsigned i = 0; i < 32; ++i) AgcDriver::CaptureTrace::Log("unload-record item=%u", i);
}

extern "C" void CaptureTraceStop() {
    AgcDriver::CaptureTrace::Stop();
}
