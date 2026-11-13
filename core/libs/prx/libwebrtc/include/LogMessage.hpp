#pragma once

#include <cstddef>
#include <cstdint>

#include "prx/libwebrtc/include/GuestString.hpp"

constexpr std::int32_t WebRtcLsVerbose = 0;
constexpr std::int32_t WebRtcLsInfo = 1;
constexpr std::int32_t WebRtcLsWarning = 2;
constexpr std::int32_t WebRtcLsError = 3;
constexpr std::int32_t WebRtcLsNone = 4;

struct WebRtcLogSink {
    const void* vtable;
    WebRtcLogSink* next;
    std::int32_t minSeverity;
};

static_assert(sizeof(WebRtcLogSink) == 24);
static_assert(offsetof(WebRtcLogSink, next) == 8);
static_assert(offsetof(WebRtcLogSink, minSeverity) == 16);

enum class WebRtcLogArgType : std::int8_t {
    End = 0,
    Int,
    Long,
    LongLong,
    UInt,
    ULong,
    ULongLong,
    Double,
    LongDouble,
    CharP,
    StdString,
    StringView,
    VoidP,
    LogMetadata,
    LogMetadataErr,
};

constexpr std::int32_t WebRtcErrorContextNone = 0;
constexpr std::int32_t WebRtcErrorContextErrno = 1;

struct WebRtcStringView {
    const char* data;
    std::size_t size;
};

struct WebRtcLogMetadata {
    const char* file;
    std::uint32_t lineAndSeverity;
};

struct WebRtcLogMetadataErr {
    WebRtcLogMetadata meta;
    std::int32_t errorContext;
    std::int32_t error;
};

struct WebRtcLogLineRef {
    WebRtcGuestString message;
    WebRtcStringView filename;
    std::int32_t line;
    std::int32_t threadId;
    bool hasThreadId;
    std::int64_t timestampUs;
    WebRtcStringView tag;
    std::int32_t severity;
};

struct WebRtcLogMessage {
    WebRtcLogLineRef logLine;
    WebRtcGuestString extra;
    WebRtcGuestString printStream;
};

static_assert(sizeof(WebRtcLogMetadata) == 16);
static_assert(sizeof(WebRtcLogMetadataErr) == 24);
static_assert(offsetof(WebRtcLogLineRef, filename) == 40);
static_assert(offsetof(WebRtcLogLineRef, line) == 56);
static_assert(offsetof(WebRtcLogLineRef, threadId) == 60);
static_assert(offsetof(WebRtcLogLineRef, hasThreadId) == 64);
static_assert(offsetof(WebRtcLogLineRef, timestampUs) == 72);
static_assert(offsetof(WebRtcLogLineRef, tag) == 80);
static_assert(offsetof(WebRtcLogLineRef, severity) == 96);
static_assert(sizeof(WebRtcLogLineRef) == 104);
static_assert(offsetof(WebRtcLogMessage, extra) == 104);
static_assert(offsetof(WebRtcLogMessage, printStream) == 144);
static_assert(sizeof(WebRtcLogMessage) == 184);
