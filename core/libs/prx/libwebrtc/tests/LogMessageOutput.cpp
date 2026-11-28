#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <string>
#include <vector>

namespace {

int allocations = 0;
int frees = 0;

void* APS5_VABI TestAllocate(std::size_t size) { ++allocations; return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { if (pointer) ++frees; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { ++allocations; return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { ++allocations; return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    ++allocations;
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc LogMessage output check failed at line %u\n", location.line());
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

using Type = WebRtcLogArgType;

std::string Text(const WebRtcGuestString& string) {
    return std::string(WebRtcGuestStringData(string), string.size);
}

struct Line {
    std::string message;
    std::string filename;
    int line;
    int threadId;
    bool hasThreadId;
    std::int64_t timestampUs;
    std::string tag;
    int severity;
};

struct CaptureSink {
    WebRtcLogSink sink;
    std::vector<Line> lines;
    std::vector<std::string> strings;
    std::vector<int> severities;
};

void APS5_VABI SinkDestroy(WebRtcLogSink*) {}

void APS5_VABI CaptureLine(WebRtcLogSink* sink, const WebRtcLogLineRef* line) {
    reinterpret_cast<CaptureSink*>(sink)->lines.push_back({Text(line->message), std::string(line->filename.data ? line->filename.data : "", line->filename.size), line->line,
                                                           line->threadId, line->hasThreadId, line->timestampUs, std::string(line->tag.data, line->tag.size), line->severity});
}

void APS5_VABI CaptureString(WebRtcLogSink* sink, const WebRtcGuestString* message, std::int32_t severity) {
    auto* capture = reinterpret_cast<CaptureSink*>(sink);
    capture->strings.push_back(Text(*message));
    capture->severities.push_back(severity);
}

void* const LineVtable[] = {
    reinterpret_cast<void*>(&SinkDestroy), reinterpret_cast<void*>(&SinkDestroy), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    reinterpret_cast<void*>(&CaptureLine)};

void* const DefaultVtable[] = {
    reinterpret_cast<void*>(&SinkDestroy), reinterpret_cast<void*>(&SinkDestroy), nullptr, reinterpret_cast<void*>(&CaptureString), nullptr, nullptr, nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3rtc7LogSink12OnLogMessageERKNS_10LogLineRefE_nid_postfix)};

CaptureSink lineSink {{LineVtable, nullptr, 0}, {}, {}, {}};
CaptureSink defaultSink {{DefaultVtable, nullptr, 0}, {}, {}, {}};

void Clear() {
    lineSink.lines.clear();
    defaultSink.strings.clear();
    defaultSink.severities.clear();
}

int Outstanding() { return allocations - frees; }

void Emit(const char* file, int line, std::int32_t severity, const char* text) {
    WebRtcLogMessage message;
    _ZN3rtc10LogMessageC1EPKciNS_15LoggingSeverityE_nid_postfix(&message, file, line, severity);
    WebRtcGuestString* stream = _ZN3rtc10LogMessage6streamEv_nid_postfix(&message);
    Require(stream == &message.printStream && stream->size == 0 && stream->capacity == WebRtcGuestStringInlineCapacity);
    const std::size_t length = std::strlen(text);
    Require(length <= WebRtcGuestStringInlineCapacity);
    std::memcpy(stream->buffer, text, length + 1);
    stream->size = length;
    _ZN3rtc10LogMessageD1Ev_nid_postfix(&message);
}

WebRtcLogMetadata Metadata(const char* file, int line, std::int32_t severity) {
    return WebRtcLogMetadata {file, static_cast<std::uint32_t>(line) << 3 | static_cast<std::uint32_t>(severity)};
}

void TestMessageReachesSinks() {
    Clear();
    const int before = Outstanding();
    Emit("a/b\\c/file.cc", 12, WebRtcLsWarning, "hello");
    Require(lineSink.lines.size() == 1);
    const Line& line = lineSink.lines[0];
    Require(line.message == "hello\n" && line.filename == "file.cc" && line.line == 12 && line.severity == WebRtcLsWarning);
    Require(line.tag == "libjingle" && !line.hasThreadId && line.timestampUs == INT64_MIN);
    Require(defaultSink.strings == std::vector<std::string> {"(file.cc:12): hello\n"} && defaultSink.severities == std::vector<int> {WebRtcLsWarning});
    Require(Outstanding() == before);
}

void TestNoFileAndSeverityFilter() {
    Clear();
    Emit(nullptr, 99, WebRtcLsVerbose, "quiet");
    Require(lineSink.lines.empty());
    Require(defaultSink.strings == std::vector<std::string> {"quiet\n"});
}

void TestThreadsAndTimestamps() {
    Clear();
    _ZN3rtc10LogMessage10LogThreadsEb_nid_postfix(true);
    _ZN3rtc10LogMessage13LogTimestampsEb_nid_postfix(true);
    Emit("dir/t.cc", 3, WebRtcLsError, "x");
    _ZN3rtc10LogMessage10LogThreadsEb_nid_postfix(false);
    _ZN3rtc10LogMessage13LogTimestampsEb_nid_postfix(false);
    Require(lineSink.lines.size() == 1);
    const Line& line = lineSink.lines[0];
    const int thread = _ZN3rtc15CurrentThreadIdEv_nid_postfix();
    Require(line.hasThreadId && line.threadId == thread);
    Require(line.timestampUs >= 0 && line.timestampUs % 1000 == 0 && line.timestampUs < 60000000);
    const std::int64_t milliseconds = line.timestampUs / 1000;
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "[%03lld:%03lld][%d] (t.cc:3): x\n", static_cast<long long>(milliseconds / 1000), static_cast<long long>(milliseconds % 1000), thread);
    Require(defaultSink.strings == std::vector<std::string> {prefix});
}

void TestLogFormatsEveryArgument() {
    Clear();
    const int before = Outstanding();
    WebRtcGuestString string {};
    const std::string longText(40, 'g');
    string.pointer = const_cast<char*>(longText.c_str());
    string.size = longText.size();
    string.capacity = longText.size();
    const WebRtcStringView view {"view!", 4};
    const Type format[] = {Type::LogMetadata, Type::Int, Type::Long, Type::UInt, Type::ULong, Type::LongLong, Type::ULongLong, Type::Double, Type::LongDouble,
                           Type::CharP, Type::CharP, Type::StdString, Type::StringView, Type::VoidP, Type::End};
    _ZN3rtc19webrtc_logging_impl3LogEPKNS0_10LogArgTypeEz_nid_postfix(format, Metadata("x/y.cc", 7, WebRtcLsInfo), -1, std::int64_t {-5000000000}, 7u,
                                                                      std::uint64_t {1} << 63, std::int64_t {-2}, std::uint64_t {3}, 1.5, 2.25L,
                                                                      static_cast<const char*>(nullptr), " ", &string, &view,
                                                                      reinterpret_cast<const void*>(std::uintptr_t {0x1234abcd5678}));
    Require(lineSink.lines.size() == 1);
    const Line& line = lineSink.lines[0];
    Require(line.filename == "y.cc" && line.line == 7 && line.severity == WebRtcLsInfo);
    Require(line.message == "-1-500000000079223372036854775808-231.52.25(null) " + longText + "viewabcd5678\n");
    Require(Outstanding() == before);
}

void TestLogWithErrno() {
    Clear();
    const WebRtcLogMetadataErr meta {Metadata("e.cc", 5, WebRtcLsError), WebRtcErrorContextErrno, 2};
    const Type format[] = {Type::LogMetadataErr, Type::CharP, Type::End};
    _ZN3rtc19webrtc_logging_impl3LogEPKNS0_10LogArgTypeEz_nid_postfix(format, meta, "open failed");
    Require(lineSink.lines.size() == 1);
    Require(lineSink.lines[0].message == std::string("open failed : [0x00000002] ") + strerror_nid_postfix(2) + "\n");
    Require(lineSink.lines[0].filename == "e.cc" && lineSink.lines[0].line == 5);
}

void TestLongMessage() {
    Clear();
    const int before = Outstanding();
    const std::string text(5000, 'm');
    const Type format[] = {Type::LogMetadata, Type::CharP, Type::CharP, Type::End};
    _ZN3rtc19webrtc_logging_impl3LogEPKNS0_10LogArgTypeEz_nid_postfix(format, Metadata("l.cc", 1, WebRtcLsError), text.c_str(), "!");
    Require(lineSink.lines.size() == 1 && lineSink.lines[0].message == text + "!\n");
    Require(Outstanding() == before);
}

void TestUnknownArgumentStopsTheMessage() {
    Clear();
    const Type format[] = {Type::LogMetadata, Type::Int, static_cast<Type>(99), Type::Int, Type::End};
    _ZN3rtc19webrtc_logging_impl3LogEPKNS0_10LogArgTypeEz_nid_postfix(format, Metadata("u.cc", 2, WebRtcLsError), 5, 6);
    Require(lineSink.lines.size() == 1 && lineSink.lines[0].message == "5\n");
}

void TestMissingMetadataLogsNothing() {
    Clear();
    const Type format[] = {Type::Int, Type::End};
    _ZN3rtc19webrtc_logging_impl3LogEPKNS0_10LogArgTypeEz_nid_postfix(format, 5);
    Require(lineSink.lines.empty() && defaultSink.strings.empty());
}

void WriteToStderr() {
    _ZN3rtc10LogMessage10LogToDebugENS_15LoggingSeverityE_nid_postfix(WebRtcLsInfo);
    Emit("p/s.cc", 4, WebRtcLsWarning, "to stderr");
    Emit("p/s.cc", 5, WebRtcLsVerbose, "hidden");
    _ZN3rtc10LogMessage14SetLogToStderrEb_nid_postfix(false);
    Emit("p/s.cc", 6, WebRtcLsError, "muted");
    _ZN3rtc10LogMessage14SetLogToStderrEb_nid_postfix(true);
    Emit("p/s.cc", 7, WebRtcLsError, "end");
}

void TestNullArgumentsThrow() {
    RequireInvalidArgument([] { _ZN3rtc10LogMessageC1EPKciNS_15LoggingSeverityE_nid_postfix(nullptr, "f", 1, WebRtcLsInfo); });
    RequireInvalidArgument([] { _ZN3rtc10LogMessageD1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN3rtc10LogMessage6streamEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN3rtc7LogSink12OnLogMessageERKNS_10LogLineRefE_nid_postfix(nullptr, nullptr); });
    RequireInvalidArgument([] { _ZN3rtc19webrtc_logging_impl3LogEPKNS0_10LogArgTypeEz_nid_postfix(nullptr); });
}

}

void TestInaccessibleMessagesAndLines() {
    const int before = Outstanding();
    GuardedPages hidden(sizeof(WebRtcLogMessage));
    hidden.Protect(Access::None);
    auto* hiddenMessage = hidden.As<WebRtcLogMessage>();
    Require(ThrowsInvalidArgument([&] { _ZN3rtc10LogMessageD1Ev_nid_postfix(hiddenMessage); }));
    Require(_ZN3rtc10LogMessage6streamEv_nid_postfix(hiddenMessage) == hidden.As<WebRtcGuestString>(offsetof(WebRtcLogMessage, printStream)));

    GuardedPages readOnly(sizeof(WebRtcLogMessage));
    auto* message = readOnly.As<WebRtcLogMessage>();
    _ZN3rtc10LogMessageC1EPKciNS_15LoggingSeverityE_nid_postfix(message, "ro.cc", 3, WebRtcLsWarning);
    readOnly.Protect(Access::ReadOnly);
    Clear();
    Require(ThrowsInvalidArgument([&] { _ZN3rtc10LogMessageD1Ev_nid_postfix(message); }));
    Require(lineSink.lines.empty() && defaultSink.strings.empty());
    readOnly.Protect(Access::ReadWrite);
    _ZN3rtc10LogMessageD1Ev_nid_postfix(message);
    Require(lineSink.lines.size() == 1 && lineSink.lines[0].message == "\n" && defaultSink.strings == std::vector<std::string> {"(ro.cc:3): \n"});

    WebRtcLogLineRef line{};
    line.message.capacity = WebRtcGuestStringInlineCapacity;
    line.timestampUs = INT64_MIN;
    line.tag = {"t", 1};
    line.severity = WebRtcLsWarning;
    line.line = 1;
    auto onLogMessage = [&](const WebRtcLogLineRef* reference) { _ZN3rtc7LogSink12OnLogMessageERKNS_10LogLineRefE_nid_postfix(&defaultSink.sink, reference); };
    Clear();
    Require(ThrowsInvalidArgument([&] { onLogMessage(hidden.As<WebRtcLogLineRef>()); }));
    line.filename = {hidden.As<char>(), 5};
    Require(ThrowsInvalidArgument([&] { onLogMessage(&line); }));
    GuardedPages tail(2 * hidden.pageSize);
    std::memcpy(tail.As<char>(tail.pageSize - 3), "abc", 3);
    tail.Protect(Access::None, tail.pageSize);
    line.filename = {tail.As<char>(tail.pageSize - 3), std::size_t {1} << 40};
    Require(ThrowsInvalidArgument([&] { onLogMessage(&line); }));
    line.filename = {tail.As<char>(tail.pageSize - 3), 3};
    line.message.pointer = hidden.As<char>();
    line.message.size = 20;
    line.message.capacity = 31;
    Require(ThrowsInvalidArgument([&] { onLogMessage(&line); }));
    Require(defaultSink.strings.empty());
    line.message.pointer = nullptr;
    line.message.size = 0;
    line.message.capacity = WebRtcGuestStringInlineCapacity;
    onLogMessage(&line);
    Require(defaultSink.strings == std::vector<std::string> {"(abc:1): "});
    Require(Outstanding() == before);
}

int main(int argc, char** argv) {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    if (argc > 1 && std::strcmp(argv[1], "stderr") == 0) {
        WriteToStderr();
        return 0;
    }
    _ZN3rtc10LogMessage14SetLogToStderrEb_nid_postfix(false);
    _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(&lineSink.sink, WebRtcLsInfo);
    _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(&defaultSink.sink, WebRtcLsVerbose);
    TestMessageReachesSinks();
    TestNoFileAndSeverityFilter();
    TestThreadsAndTimestamps();
    TestLogFormatsEveryArgument();
    TestLogWithErrno();
    TestLongMessage();
    TestUnknownArgumentStopsTheMessage();
    TestMissingMetadataLogsNothing();
    TestInaccessibleMessagesAndLines();
    TestNullArgumentsThrow();
    _ZN3rtc10LogMessage17RemoveLogToStreamEPNS_7LogSinkE_nid_postfix(&lineSink.sink);
    _ZN3rtc10LogMessage17RemoveLogToStreamEPNS_7LogSinkE_nid_postfix(&defaultSink.sink);
    return 0;
}
