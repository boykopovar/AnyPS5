/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::atomic<std::int32_t> g_minSeverity{WebRtcLsNone};
std::atomic<std::int32_t> g_debugSeverity{WebRtcLsNone};
std::atomic<bool> g_logToStderr{true};
std::atomic<bool> g_logThread{false};
std::atomic<bool> g_logTimestamp{false};
std::atomic<bool> g_streamsEmpty{true};
WebRtcLogSink* g_streams = nullptr;

std::mutex& LoggingLock() {
    static std::mutex lock;
    return lock;
}

void UpdateMinLogSeverity() {
    std::int32_t minSeverity = g_debugSeverity.load();
    for (WebRtcLogSink* entry = g_streams; entry != nullptr; entry = entry->next)
        minSeverity = std::min(minSeverity, entry->minSeverity);
    g_minSeverity.store(minSeverity);
}

constexpr std::size_t StringCapacityMask = 15;
constexpr std::int64_t MinusInfinity = INT64_MIN;
constexpr int LineRefSinkSlot = 8;
constexpr int StringSeveritySinkSlot = 3;

char* Data(WebRtcGuestString& string) {
    return string.capacity > WebRtcGuestStringInlineCapacity ? string.pointer : string.buffer;
}

void InitString(WebRtcGuestString& string) {
    string.reserved = 0;
    string.buffer[0] = 0;
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
}

void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    InitString(string);
}

void MoveString(WebRtcGuestString& target, WebRtcGuestString& source) {
    ReleaseString(target);
    target = source;
    InitString(source);
}

void Append(WebRtcGuestString& string, std::string_view text) {
    const std::size_t size = string.size + text.size();
    if (size > string.capacity) {
        const std::size_t rounded = size | StringCapacityMask;
        std::size_t capacity = rounded;
        if (string.capacity > WebRtcGuestStringInlineCapacity && string.capacity / 2 > rounded / 3)
            capacity = string.capacity + string.capacity / 2;
        auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
        std::memcpy(data, Data(string), string.size);
        if (string.capacity > WebRtcGuestStringInlineCapacity)
            GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
        string.pointer = data;
        string.capacity = capacity;
    }
    if (!text.empty())
        std::memcpy(Data(string) + string.size, text.data(), text.size());
    string.size = size;
    Data(string)[size] = 0;
}

std::string_view View(const WebRtcGuestString& string) {
    return {WebRtcGuestStringData(string), string.size};
}

template<typename TValue> std::string Decimal(TValue value) {
    char buffer[24];
    return std::string(buffer, std::to_chars(buffer, buffer + sizeof(buffer), value).ptr);
}

template<typename... TValues> std::string Format(const char* format, TValues... values) {
    char buffer[64];
    const int length = snprintf_nid_postfix(buffer, sizeof(buffer), format, values...);
    return length <= 0 ? std::string() : std::string(buffer, std::min<std::size_t>(static_cast<std::size_t>(length), sizeof(buffer) - 1));
}

const char* FilenameFromPath(const char* file) {
    const char* slash = std::strrchr(file, '/');
    const char* backslash = std::strrchr(file, '\\');
    if (!slash && !backslash)
        return file;
    return (slash > backslash) ? slash + 1 : backslash + 1;
}

std::int64_t LogStartTime() {
    static const std::int64_t start = _ZN3rtc10TimeMillisEv_nid_postfix();
    return start;
}

std::string DefaultLogLine(const WebRtcLogLineRef& line) {
    std::string output;
    if (line.timestampUs != MinusInfinity) {
        const std::int64_t milliseconds = line.timestampUs / 1000;
        output += Format("[%03ld:%03ld]", milliseconds / 1000, milliseconds % 1000);
    }
    if (line.hasThreadId)
        output += "[" + Decimal(line.threadId) + "] ";
    if (line.filename.size != 0)
        output += "(" + std::string(line.filename.data, line.filename.size) + ":" + Decimal(line.line) + "): ";
    output += View(line.message);
    return output;
}

void Construct(WebRtcLogMessage* message, const char* file, int line, std::int32_t severity, std::int32_t errorContext, int error) {
    if (!message)
        throw std::invalid_argument("LogMessage constructor requires a message object");
    WebRtcLogLineRef& logLine = message->logLine;
    InitString(logLine.message);
    logLine.filename = {nullptr, 0};
    logLine.line = 0;
    logLine.threadId = 0;
    logLine.hasThreadId = false;
    logLine.timestampUs = MinusInfinity;
    logLine.tag = {"libjingle", 9};
    logLine.severity = severity;
    InitString(message->extra);
    InitString(message->printStream);
    if (g_logTimestamp.load()) {
        const std::int64_t start = LogStartTime();
        logLine.timestampUs = (_ZN3rtc10TimeMillisEv_nid_postfix() - start) * 1000;
    }
    if (g_logThread.load()) {
        logLine.threadId = _ZN3rtc15CurrentThreadIdEv_nid_postfix();
        logLine.hasThreadId = true;
    }
    if (file) {
        const char* name = FilenameFromPath(file);
        logLine.filename = {name, std::strlen(name)};
        logLine.line = line;
    }
    if (errorContext != WebRtcErrorContextNone) {
        std::string extra = Format("[0x%08X]", static_cast<unsigned>(error));
        if (errorContext == WebRtcErrorContextErrno) {
            const char* text = strerror_nid_postfix(error);
            extra += " ";
            extra += text ? text : "";
        }
        Append(message->extra, extra);
    }
}

void OutputToDebug(const WebRtcLogLineRef& line) {
    if (!g_logToStderr.load())
        return;
    const std::string text = DefaultLogLine(line);
    fwrite_nid_postfix(text.data(), 1, text.size(), &_Stderr_nid_postfix);
    fflush_nid_postfix(&_Stderr_nid_postfix);
}

template<typename TFunction> TFunction SinkSlot(const WebRtcLogSink* sink, int slot) {
    return reinterpret_cast<TFunction>(static_cast<void* const*>(sink->vtable)[slot]);
}

#ifdef _WIN32
struct SysvVaListTag {
    unsigned gpOffset;
    unsigned fpOffset;
    char* overflowArea;
    char* registerSaveArea;
};

constexpr unsigned GeneralRegisterBytes = 48;

template<typename TValue> TValue ReadOverflow(SysvVaListTag* tag, std::size_t alignment) {
    const auto address = (reinterpret_cast<std::uintptr_t>(tag->overflowArea) + alignment - 1) & ~(alignment - 1);
    TValue value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    tag->overflowArea = reinterpret_cast<char*>(address + ((sizeof(value) + 7) & ~std::size_t {7}));
    return value;
}

template<typename TList> SysvVaListTag* Tag(TList& args) {
    return reinterpret_cast<SysvVaListTag*>(&args[0]);
}

template<typename TList> WebRtcLogMetadata ReadMetadata(TList& args) {
    SysvVaListTag* tag = Tag(args);
    if (tag->gpOffset + sizeof(WebRtcLogMetadata) > GeneralRegisterBytes)
        return ReadOverflow<WebRtcLogMetadata>(tag, 8);
    WebRtcLogMetadata metadata;
    std::memcpy(&metadata, tag->registerSaveArea + tag->gpOffset, sizeof(metadata));
    tag->gpOffset += sizeof(metadata);
    return metadata;
}

template<typename TList> WebRtcLogMetadataErr ReadMetadataErr(TList& args) {
    return ReadOverflow<WebRtcLogMetadataErr>(Tag(args), 8);
}

template<typename TList> long double ReadLongDouble(TList& args) {
    return ReadOverflow<long double>(Tag(args), 16);
}
#else
template<typename TList> WebRtcLogMetadata ReadMetadata(TList& args) {
    return va_arg(args, WebRtcLogMetadata);
}

template<typename TList> WebRtcLogMetadataErr ReadMetadataErr(TList& args) {
    return va_arg(args, WebRtcLogMetadataErr);
}

template<typename TList> long double ReadLongDouble(TList& args) {
    return va_arg(args, long double);
}
#endif

template<typename TList> void LogArguments(const WebRtcLogArgType* format, TList& args) {
    WebRtcLogMetadataErr meta;
    switch (*format) {
    case WebRtcLogArgType::LogMetadata:
        meta = {ReadMetadata(args), WebRtcErrorContextNone, 0};
        break;
    case WebRtcLogArgType::LogMetadataErr:
        meta = ReadMetadataErr(args);
        break;
    default:
        return;
    }
    WebRtcLogMessage message;
    Construct(&message, meta.meta.file, static_cast<int>(meta.meta.lineAndSeverity >> 3), static_cast<std::int32_t>(meta.meta.lineAndSeverity & 7), meta.errorContext, meta.error);
    WebRtcGuestString& stream = message.printStream;
    for (++format; *format != WebRtcLogArgType::End; ++format) {
        switch (*format) {
        case WebRtcLogArgType::Int:
            Append(stream, Decimal(va_arg(args, int)));
            break;
        case WebRtcLogArgType::Long:
        case WebRtcLogArgType::LongLong:
            Append(stream, Decimal(va_arg(args, std::int64_t)));
            break;
        case WebRtcLogArgType::UInt:
            Append(stream, Decimal(va_arg(args, unsigned)));
            break;
        case WebRtcLogArgType::ULong:
        case WebRtcLogArgType::ULongLong:
            Append(stream, Decimal(va_arg(args, std::uint64_t)));
            break;
        case WebRtcLogArgType::Double:
            Append(stream, Format("%g", va_arg(args, double)));
            break;
        case WebRtcLogArgType::LongDouble:
            Append(stream, Format("%Lg", ReadLongDouble(args)));
            break;
        case WebRtcLogArgType::CharP: {
            const char* text = va_arg(args, const char*);
            Append(stream, text ? text : "(null)");
            break;
        }
        case WebRtcLogArgType::StdString:
            Append(stream, View(*va_arg(args, const WebRtcGuestString*)));
            break;
        case WebRtcLogArgType::StringView: {
            const auto* view = va_arg(args, const WebRtcStringView*);
            Append(stream, std::string_view(view->data, view->size));
            break;
        }
        case WebRtcLogArgType::VoidP:
            Append(stream, Format("%x", static_cast<int>(reinterpret_cast<std::uintptr_t>(va_arg(args, const void*)))));
            break;
        default:
            _ZN3rtc10LogMessageD1Ev_nid_postfix(&message);
            return;
        }
    }
    _ZN3rtc10LogMessageD1Ev_nid_postfix(&message);
}

}

extern "C" {

int APS5_VABI _ZN3rtc10LogMessage17GetMinLogSeverityEv_nid_postfix(void) {
    return g_minSeverity.load();
}

std::int32_t APS5_VABI _ZN3rtc10LogMessage13GetLogToDebugEv_nid_postfix(void) {
    return g_debugSeverity.load();
}

void APS5_VABI _ZN3rtc10LogMessage10LogThreadsEb_nid_postfix(bool on) {
    g_logThread.store(on);
}

void APS5_VABI _ZN3rtc10LogMessage13LogTimestampsEb_nid_postfix(bool on) {
    g_logTimestamp.store(on);
}

void APS5_VABI _ZN3rtc10LogMessage10LogToDebugENS_15LoggingSeverityE_nid_postfix(std::int32_t minSeverity) {
    g_debugSeverity.store(minSeverity);
    std::lock_guard lock(LoggingLock());
    UpdateMinLogSeverity();
}

void APS5_VABI _ZN3rtc10LogMessage14SetLogToStderrEb_nid_postfix(bool logToStderr) {
    g_logToStderr.store(logToStderr);
}

void APS5_VABI _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(WebRtcLogSink* stream, std::int32_t minSeverity) {
    if (!stream)
        throw std::invalid_argument("LogMessage::AddLogToStream requires a sink");
    std::lock_guard lock(LoggingLock());
    for (WebRtcLogSink* entry = g_streams; entry != nullptr; entry = entry->next) {
        if (entry == stream)
            throw std::invalid_argument("LogMessage::AddLogToStream sink is already registered");
    }
    stream->minSeverity = minSeverity;
    stream->next = g_streams;
    g_streams = stream;
    g_streamsEmpty.store(false);
    UpdateMinLogSeverity();
}

void APS5_VABI _ZN3rtc10LogMessage17RemoveLogToStreamEPNS_7LogSinkE_nid_postfix(WebRtcLogSink* stream) {
    std::lock_guard lock(LoggingLock());
    for (WebRtcLogSink** entry = &g_streams; *entry != nullptr; entry = &(*entry)->next) {
        if (*entry == stream) {
            *entry = (*entry)->next;
            break;
        }
    }
    g_streamsEmpty.store(g_streams == nullptr);
    UpdateMinLogSeverity();
}

bool APS5_VABI _ZN3rtc10LogMessage6IsNoopENS_15LoggingSeverityE_nid_postfix(std::int32_t severity) {
    if (severity >= g_debugSeverity.load() || severity >= g_minSeverity.load())
        return false;
    return g_streamsEmpty.load();
}

void APS5_VABI _ZN3rtc10LogMessageC1EPKciNS_15LoggingSeverityE_nid_postfix(WebRtcLogMessage* message, const char* file, int line, std::int32_t severity) {
    Construct(message, file, line, severity, WebRtcErrorContextNone, 0);
}

WebRtcGuestString* APS5_VABI _ZN3rtc10LogMessage6streamEv_nid_postfix(WebRtcLogMessage* message) {
    if (!message)
        throw std::invalid_argument("LogMessage::stream requires a message object");
    return &message->printStream;
}

void APS5_VABI _ZN3rtc10LogMessageD1Ev_nid_postfix(WebRtcLogMessage* message) {
    if (!message)
        throw std::invalid_argument("LogMessage destructor requires a message object");
    if (message->extra.size != 0) {
        Append(message->printStream, " : ");
        Append(message->printStream, View(message->extra));
    }
    Append(message->printStream, "\n");
    WebRtcLogLineRef& logLine = message->logLine;
    MoveString(logLine.message, message->printStream);
    if (logLine.severity >= g_debugSeverity.load())
        OutputToDebug(logLine);
    {
        std::lock_guard lock(LoggingLock());
        for (WebRtcLogSink* entry = g_streams; entry != nullptr; entry = entry->next) {
            if (logLine.severity >= entry->minSeverity)
                SinkSlot<void (APS5_VABI*)(WebRtcLogSink*, const WebRtcLogLineRef*)>(entry, LineRefSinkSlot)(entry, &logLine);
        }
    }
    ReleaseString(logLine.message);
    ReleaseString(message->extra);
    ReleaseString(message->printStream);
}

void APS5_VABI _ZN3rtc7LogSink12OnLogMessageERKNS_10LogLineRefE_nid_postfix(WebRtcLogSink* sink, const WebRtcLogLineRef* line) {
    if (!sink || !line)
        throw std::invalid_argument("LogSink::OnLogMessage requires a sink and a log line");
    WebRtcGuestString text;
    InitString(text);
    Append(text, DefaultLogLine(*line));
    try {
        SinkSlot<void (APS5_VABI*)(WebRtcLogSink*, const WebRtcGuestString*, std::int32_t)>(sink, StringSeveritySinkSlot)(sink, &text, line->severity);
    } catch (...) {
        ReleaseString(text);
        throw;
    }
    ReleaseString(text);
}

void APS5_VABI _ZN3rtc19webrtc_logging_impl3LogEPKNS0_10LogArgTypeEz_nid_postfix(const WebRtcLogArgType* format, ...) {
    if (!format)
        throw std::invalid_argument("webrtc_logging_impl::Log requires an argument list");
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    try {
        LogArguments(format, args);
    } catch (...) {
        __builtin_sysv_va_end(args);
        throw;
    }
    __builtin_sysv_va_end(args);
#else
    va_list args;
    va_start(args, format);
    try {
        LogArguments(format, args);
    } catch (...) {
        va_end(args);
        throw;
    }
    va_end(args);
#endif
}

}
