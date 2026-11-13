#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc LogMessage check failed at line %u\n", location.line());
    std::abort();
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

int main() {
    Require(MinSeverity() == WebRtcLsNone);
    Require(DebugSeverity() == WebRtcLsNone);
    TestDebugSeverity();
    TestStreams();
    TestIsNoop();
    TestFlags();
    TestInvalid();
    std::puts("libwebrtc LogMessage tests passed");
}
