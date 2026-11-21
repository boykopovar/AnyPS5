#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include "prx/libc/include/CallTrace.hpp"
#include "prx/libc/include/FailureReport.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/HostFault.hpp"
#include "prx/libc/include/LogFile.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/CrashText.hpp"
#include "prx/libc/include/HostThread.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>
#include <mutex>
#include "Sse4aEmulation.hpp"

namespace {
void ReportCallTrace() {
    static CallTraceEntry entries[64];
    const std::size_t count = CopyCallTrace_nid_no_patch(entries, std::size(entries));
    if (count == 0) return;
    char line[MAX_PATH + 64];
    Report("  last traced system library calls (APS5_TRACE_CALLS):\n");
    for (std::size_t index = 0; index < count; ++index) {
        DescribeAddress(entries[index].returnAddress, line, sizeof(line));
        Report("    #%llu thread %llu %s %s from %s\n", static_cast<unsigned long long>(entries[index].sequence),
            static_cast<unsigned long long>(entries[index].thread), entries[index].library, entries[index].name, line);
    }
    static CallTraceEntry own[32];
    const std::uint64_t thread = GetCurrentThreadId();
    const std::size_t owned = CopyThreadCallTrace_nid_no_patch(thread, own, std::size(own));
    if (owned != 0) {
        Report("  last traced calls of thread %llu:\n", static_cast<unsigned long long>(thread));
        for (std::size_t index = 0; index < owned; ++index) {
            DescribeAddress(own[index].returnAddress, line, sizeof(line));
            Report("    #%llu %s %s from %s\n", static_cast<unsigned long long>(own[index].sequence), own[index].library, own[index].name, line);
        }
    }
    static CallTraceCount counts[32];
    std::uint64_t total = 0;
    const std::size_t counted = CopyCallCounts_nid_no_patch(counts, std::size(counts), &total);
    if (counted == 0) return;
    Report("  most called traced imports (%llu calls in total):\n", static_cast<unsigned long long>(total));
    for (std::size_t index = 0; index < counted; ++index) {
        Report("    %12llu %s %s\n", static_cast<unsigned long long>(counts[index].calls), counts[index].library, counts[index].name);
    }
}

std::size_t CaptureFrames(const CONTEXT* context, std::uint64_t* frames, std::size_t capacity) {
    std::size_t count = 0;
    std::uint64_t frame = context->Rbp;
    while (count < capacity && frame != 0 && (frame & 7) == 0 && IsReadable(frame) && IsReadable(frame + 8)) {
        const auto returnAddress = reinterpret_cast<const std::uint64_t*>(frame)[1];
        if (!IsExecutable(returnAddress)) break;
        frames[count++] = returnAddress;
        const auto next = reinterpret_cast<const std::uint64_t*>(frame)[0];
        if (next <= frame) break;
        frame = next;
    }
    return count;
}

std::size_t CaptureStackCandidates(std::uint64_t rsp, std::uint64_t* candidates, std::size_t capacity) {
    std::size_t count = 0;
    for (std::uint64_t slot = rsp; count < capacity && slot < rsp + 0x8000; slot += 8) {
        if (!IsReadable(slot)) break;
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (IsExecutable(value)) candidates[count++] = value;
    }
    return count;
}

void ReportThreadContext(const CONTEXT* context, DWORD threadId, const char* name, const std::uint64_t* frames, std::size_t frameCount, const std::uint64_t* candidates, std::size_t candidateCount) {
    char line[MAX_PATH + 64];
    Report("  thread %lu '%s': rip ", static_cast<unsigned long>(threadId), name ? name : "");
    DescribeAddress(context->Rip, line, sizeof(line));
    Report("%s rsp 0x%016llx rbp 0x%016llx\n", line, static_cast<unsigned long long>(context->Rsp), static_cast<unsigned long long>(context->Rbp));
    for (std::size_t depth = 0; depth < frameCount; ++depth) {
        DescribeAddress(frames[depth], line, sizeof(line));
        Report("    #%llu %s\n", static_cast<unsigned long long>(depth), line);
    }
    if (candidateCount != 0) Report("    stack return address candidates:\n");
    for (std::size_t index = 0; index < candidateCount; ++index) {
        DescribeAddress(candidates[index], line, sizeof(line));
        Report("      %s\n", line);
    }
}

void ReportAllThreads() {
    const DWORD current = GetCurrentThreadId();
    const DWORD pid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    struct CapturedThread {
        DWORD id;
        char name[128];
        CONTEXT context;
        std::uint64_t frames[12];
        std::size_t frameCount;
        std::uint64_t candidates[24];
        std::size_t candidateCount;
    };
    static CapturedThread captured[128];
    std::size_t capturedCount = 0;
    std::size_t skippedCount = 0;
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID != pid || entry.th32ThreadID == current) continue;
            if (capturedCount >= sizeof(captured) / sizeof(captured[0])) {
                ++skippedCount;
                continue;
            }
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
            if (!thread) continue;
            if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
                CloseHandle(thread);
                continue;
            }
            CONTEXT context{};
            context.ContextFlags = CONTEXT_FULL;
            const BOOL haveContext = GetThreadContext(thread, &context);
            std::uint64_t frames[12] = {};
            const std::size_t frameCount = haveContext ? CaptureFrames(&context, frames, sizeof(frames) / sizeof(frames[0])) : 0;
            std::uint64_t candidates[24] = {};
            const std::size_t candidateCount = haveContext && frameCount < 2 ? CaptureStackCandidates(context.Rsp, candidates, sizeof(candidates) / sizeof(candidates[0])) : 0;
            ResumeThread(thread);
            char name[128] = "";
            if (PWSTR description = ThreadDescription(thread)) {
                WideCharToMultiByte(CP_UTF8, 0, description, -1, name, sizeof(name) - 1, nullptr, nullptr);
                LocalFree(description);
            }
            CloseHandle(thread);
            if (!haveContext) continue;
            captured[capturedCount].id = entry.th32ThreadID;
            std::memcpy(captured[capturedCount].name, name, sizeof(captured[capturedCount].name));
            captured[capturedCount].context = context;
            std::memcpy(captured[capturedCount].frames, frames, sizeof(captured[capturedCount].frames));
            captured[capturedCount].frameCount = frameCount;
            std::memcpy(captured[capturedCount].candidates, candidates, sizeof(captured[capturedCount].candidates));
            captured[capturedCount].candidateCount = candidateCount;
            ++capturedCount;
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    Report("  all threads:\n");
    for (std::size_t i = 0; i < capturedCount; ++i) {
        ReportThreadContext(&captured[i].context, captured[i].id, captured[i].name, captured[i].frames, captured[i].frameCount, captured[i].candidates, captured[i].candidateCount);
    }
    if (skippedCount != 0) Report("  %llu further thread(s) left out\n", static_cast<unsigned long long>(skippedCount));
}

DWORD WINAPI HangWatchdog(LPVOID param) {
    Libc::SetHostMxcsr();
    const auto seconds = static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(param));
    Sleep(static_cast<DWORD>(seconds * 1000ull));
    Report("\nFATAL: timed dump after %llu s (APS5_HANG_DUMP_SECS); aborting\n", seconds);
    std::fflush(stderr);
    std::abort();
    return 0;
}

void ReportStackCandidates(std::uint64_t rsp, int limit, std::uint64_t span) {
    char line[256];
    Report("  stack return address candidates:\n");
    int printed = 0;
    for (std::uint64_t slot = rsp; printed < limit && slot < rsp + span; slot += 8) {
        if (!IsReadable(slot)) break;
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (!IsExecutable(value)) continue;
        DescribeAddress(value, line, sizeof(line));
        Report("    [rsp+0x%llx] %s\n", static_cast<unsigned long long>(slot - rsp), line);
        ++printed;
    }
}

// abort() and the UCRT's invalid-parameter path end the process with a silent fast fail
// (0xc0000409); both are reported with the calling thread's stack first.
void ReportBacktrace(const char* what, bool reportThreads) {
    void* frames[48];
    const auto count = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
    Report("FATAL: %s on thread %lu\n", what, static_cast<unsigned long>(GetCurrentThreadId()));
    char line[256];
    for (USHORT i = 0; i < count; ++i) {
        DescribeAddress(reinterpret_cast<std::uint64_t>(frames[i]), line, sizeof(line));
        Report("    #%u %s\n", static_cast<unsigned>(i), line);
    }
    ReportStackCandidates(reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)), 32, 0x4000);
    ReportCallTrace();
    if (reportThreads) ReportAllThreads();
    std::fflush(stderr);
}

void AbortSignalHandler(int) {
    ReportBacktrace("abort() (SIGABRT)", true);
    LibcRunFailureReports_nid_no_patch();
}

void InvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, std::uintptr_t) {
    ReportBacktrace("UCRT invalid parameter", true);
    LibcRunFailureReports_nid_no_patch();
}

// Static destruction runs on the thread that ends the process (ExitProcess -> DLL_PROCESS_DETACH),
// so its stack names the exit's caller when no libc exit/abort path was taken.
struct ExitReporter {
    ~ExitReporter() {
        if (std::getenv("APS5_TRACE_EXIT") != nullptr) ReportBacktrace("process exit (static destruction)", false);
    }
} g_exitReporter;

const bool g_crashReportInstalled = [] {
    g_sse4aEmulation = !IsEnvironmentSet("APS5_NO_SSE4A_EMULATION");
    g_sse4aTrace = IsEnvironmentSet("APS5_TRACE_SSE4A");
    AddVectoredExceptionHandler(1, ReportCrash);
    std::signal(SIGABRT, AbortSignalHandler);
    _set_invalid_parameter_handler(InvalidParameterHandler);
    InstallWatch();
    if (const char* hang = std::getenv("APS5_HANG_DUMP_SECS")) {
        unsigned long long seconds = 0;
        if (hang[0] >= '0' && hang[0] <= '9') {
            char* end = nullptr;
            const auto value = std::strtoull(hang, &end, 10);
            if (end && *end == 0) seconds = value;
        }
        if (seconds > 0) CreateThread(nullptr, 0, HangWatchdog, reinterpret_cast<LPVOID>(static_cast<std::uintptr_t>(seconds)), 0, nullptr);
    }
    return true;
}();

}

extern "C" void LibcSetHostFaultHandler_nid_no_patch(LibcHostFaultHandler handler) noexcept {
    g_hostFaultHandler.store(handler, std::memory_order_release);
}
#endif
