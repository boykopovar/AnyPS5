#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc LogMessage check failed at line %u\n", location.line());
    std::abort();
}

enum class Access { ReadWrite, ReadOnly, None };

struct GuardedPages {
    std::size_t pageSize = 0;
    std::size_t size = 0;
    unsigned char* base = nullptr;

    explicit GuardedPages(std::size_t bytes = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        base = mapped == MAP_FAILED ? nullptr : static_cast<unsigned char*>(mapped);
#endif
        Require(base != nullptr);
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    template<typename T> T* As(std::size_t offset = 0) const { return reinterpret_cast<T*>(base + offset); }

    void Protect(Access access, std::size_t offset = 0) const {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD mode = access == Access::ReadWrite ? PAGE_READWRITE : access == Access::ReadOnly ? PAGE_READONLY : PAGE_NOACCESS;
        Require(VirtualProtect(base + offset, size - offset, mode, &previous) != 0);
#else
        const int mode = access == Access::ReadWrite ? PROT_READ | PROT_WRITE : access == Access::ReadOnly ? PROT_READ : PROT_NONE;
        Require(mprotect(base + offset, size - offset, mode) == 0);
#endif
    }
};

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

int MinSeverity() {
    return _ZN3rtc10LogMessage17GetMinLogSeverityEv_nid_postfix();
}

std::int32_t DebugSeverity() {
    return _ZN3rtc10LogMessage13GetLogToDebugEv_nid_postfix();
}

void LogToDebug(std::int32_t severity) {
    _ZN3rtc10LogMessage10LogToDebugENS_15LoggingSeverityE_nid_postfix(severity);
}

void Add(WebRtcLogSink& sink, std::int32_t severity) {
    _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(&sink, severity);
}

void Remove(WebRtcLogSink* sink) {
    _ZN3rtc10LogMessage17RemoveLogToStreamEPNS_7LogSinkE_nid_postfix(sink);
}

bool IsNoop(std::int32_t severity) {
    return _ZN3rtc10LogMessage6IsNoopENS_15LoggingSeverityE_nid_postfix(severity);
}

void TestDebugSeverity() {
    LogToDebug(WebRtcLsWarning);
    Require(DebugSeverity() == WebRtcLsWarning);
    Require(MinSeverity() == WebRtcLsWarning);
    LogToDebug(WebRtcLsVerbose);
    Require(DebugSeverity() == WebRtcLsVerbose);
    Require(MinSeverity() == WebRtcLsVerbose);
    LogToDebug(WebRtcLsNone);
    Require(DebugSeverity() == WebRtcLsNone);
    Require(MinSeverity() == WebRtcLsNone);
}

void TestStreams() {
    WebRtcLogSink first{nullptr, nullptr, 0};
    WebRtcLogSink second{nullptr, nullptr, 0};
    WebRtcLogSink third{nullptr, nullptr, 0};

    Add(first, WebRtcLsError);
    Require(first.minSeverity == WebRtcLsError);
    Require(first.next == nullptr);
    Require(MinSeverity() == WebRtcLsError);

    Add(second, WebRtcLsInfo);
    Require(second.next == &first);
    Require(MinSeverity() == WebRtcLsInfo);

    Add(third, WebRtcLsWarning);
    Require(third.next == &second);
    Require(MinSeverity() == WebRtcLsInfo);

    Remove(&second);
    Require(third.next == &first);
    Require(MinSeverity() == WebRtcLsWarning);

    Remove(&second);
    Require(MinSeverity() == WebRtcLsWarning);

    LogToDebug(WebRtcLsVerbose);
    Require(MinSeverity() == WebRtcLsVerbose);
    LogToDebug(WebRtcLsNone);
    Require(MinSeverity() == WebRtcLsWarning);

    Remove(nullptr);
    Remove(&third);
    Require(MinSeverity() == WebRtcLsError);
    Remove(&first);
    Require(MinSeverity() == WebRtcLsNone);
}

void TestIsNoop() {
    Require(IsNoop(WebRtcLsVerbose));
    Require(IsNoop(WebRtcLsError));
    Require(!IsNoop(WebRtcLsNone));

    LogToDebug(WebRtcLsWarning);
    Require(IsNoop(WebRtcLsInfo));
    Require(!IsNoop(WebRtcLsWarning));
    Require(!IsNoop(WebRtcLsError));
    LogToDebug(WebRtcLsNone);

    WebRtcLogSink sink{nullptr, nullptr, 0};
    Add(sink, WebRtcLsInfo);
    Require(MinSeverity() == WebRtcLsInfo);
    Require(!IsNoop(WebRtcLsVerbose));
    Require(!IsNoop(WebRtcLsInfo));
    Require(!IsNoop(WebRtcLsError));
    Remove(&sink);
    Require(IsNoop(WebRtcLsInfo));
}

void TestFlags() {
    _ZN3rtc10LogMessage10LogThreadsEb_nid_postfix(true);
    _ZN3rtc10LogMessage10LogThreadsEb_nid_postfix(false);
    _ZN3rtc10LogMessage13LogTimestampsEb_nid_postfix(true);
    _ZN3rtc10LogMessage13LogTimestampsEb_nid_postfix(false);
    _ZN3rtc10LogMessage14SetLogToStderrEb_nid_postfix(false);
    _ZN3rtc10LogMessage14SetLogToStderrEb_nid_postfix(true);
}

void TestInvalid() {
    RequireInvalidArgument([] { _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(nullptr, WebRtcLsInfo); });
    WebRtcLogSink sink{nullptr, nullptr, 0};
    Add(sink, WebRtcLsInfo);
    RequireInvalidArgument([&] { Add(sink, WebRtcLsError); });
    Require(sink.minSeverity == WebRtcLsInfo);
    Require(sink.next == nullptr);
    Remove(&sink);
    Require(MinSeverity() == WebRtcLsNone);
}

}

void TestInaccessibleSinks() {
    GuardedPages hidden;
    hidden.Protect(Access::None);
    Require(ThrowsInvalidArgument([&] { _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(hidden.As<WebRtcLogSink>(), WebRtcLsInfo); }));
    GuardedPages readOnly;
    *readOnly.As<WebRtcLogSink>() = WebRtcLogSink{nullptr, nullptr, 0};
    readOnly.Protect(Access::ReadOnly);
    Require(ThrowsInvalidArgument([&] { _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(readOnly.As<WebRtcLogSink>(), WebRtcLsInfo); }));
    Require(MinSeverity() == WebRtcLsNone && IsNoop(WebRtcLsInfo));

    WebRtcLogSink first{nullptr, nullptr, 0};
    GuardedPages secondPage;
    auto* second = secondPage.As<WebRtcLogSink>();
    *second = WebRtcLogSink{nullptr, nullptr, 0};
    Add(first, WebRtcLsError);
    Add(*second, WebRtcLsWarning);
    Require(second->next == &first);
    secondPage.Protect(Access::ReadOnly);
    Require(ThrowsInvalidArgument([&] { Remove(&first); }));
    Require(second->next == &first && MinSeverity() == WebRtcLsWarning);
    secondPage.Protect(Access::ReadWrite);
    Remove(&first);
    Require(second->next == nullptr && MinSeverity() == WebRtcLsWarning);
    Remove(second);
    Require(MinSeverity() == WebRtcLsNone);
}

int main() {
    Require(MinSeverity() == WebRtcLsNone);
    Require(DebugSeverity() == WebRtcLsNone);
    TestDebugSeverity();
    TestStreams();
    TestIsNoop();
    TestFlags();
    TestInvalid();
    TestInaccessibleSinks();
    std::puts("libwebrtc LogMessage tests passed");
}
