#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "tests/TemporaryFile.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#endif
#ifdef __linux__
#include <sys/prctl.h>
#endif

namespace {

using namespace std::chrono_literals;
using Clock = AgcDriver::AsyncLog::Clock;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string ReadAll(std::FILE* file) {
    std::fflush(file);
    std::rewind(file);
    std::string text;
    char buffer[4096];
    for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
    return text;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void ClearLogEnvironment() {
#ifdef _WIN32
    _putenv("APS5_LOG_SYNC=");
    _putenv("APS5_LOG_TIMESTAMPS=");
#else
    unsetenv("APS5_LOG_SYNC");
    unsetenv("APS5_LOG_TIMESTAMPS");
#endif
}

void LockStream(std::FILE* file) {
#ifdef _WIN32
    _lock_file(file);
#else
    flockfile(file);
#endif
}

void UnlockStream(std::FILE* file) {
#ifdef _WIN32
    _unlock_file(file);
#else
    funlockfile(file);
#endif
}

class HeldStream {
public:
    explicit HeldStream(std::FILE* file) : file(file) {
        holder = std::thread([this] {
            LockStream(this->file);
            held.store(true);
            while (!releaseRequested.load()) std::this_thread::sleep_for(1ms);
            released.store(true);
            UnlockStream(this->file);
        });
        while (!held.load()) std::this_thread::sleep_for(1ms);
    }

    HeldStream(const HeldStream&) = delete;
    HeldStream& operator=(const HeldStream&) = delete;

    ~HeldStream() {
        Release();
        holder.join();
    }

    void Release() {
        releaseRequested.store(true);
    }

    bool Released() const {
        return released.load();
    }

private:
    std::FILE* file;
    std::atomic<bool> held{false};
    std::atomic<bool> releaseRequested{false};
    std::atomic<bool> released{false};
    std::thread holder;
};

std::string LargeLine(std::size_t index, std::size_t bytes) {
    std::string line = "large " + std::to_string(index) + " ";
    line.resize(bytes - 1, 'x');
    line += '\n';
    return line;
}

void OrderPerThread() {
    std::FILE* file = TemporaryFile::Open();
    Check(file != nullptr, "temporary file");
    auto& log = AgcDriver::AsyncLog::Get();
    std::string expected;
    for (int i = 0; i < 2000; ++i) {
        log.Defer(file, "deferred %d %s %llu\n", i, AgcDriver::LogLiteral{"literal"}, static_cast<unsigned long long>(i) * 3ull);
        expected += "deferred " + std::to_string(i) + " literal " + std::to_string(static_cast<unsigned long long>(i) * 3ull) + "\n";
        const std::string plain = "plain " + std::to_string(i) + "\n";
        log.Write(file, plain.data(), plain.size());
        expected += plain;
    }
    log.Flush();
    Check(ReadAll(file) == expected, "deferred and written lines changed order or content");
    std::fclose(file);
}

void LongLine() {
    std::FILE* file = TemporaryFile::Open();
    Check(file != nullptr, "temporary file");
    const std::string longText(3000, 'x');
    AgcDriver::AsyncLog::Get().Defer(file, "%s|%d\n", AgcDriver::LogLiteral{longText.c_str()}, 7);
    AgcDriver::AsyncLog::Get().Flush();
    Check(ReadAll(file) == longText + "|7\n", "a line longer than the stack buffer changed");
    std::fclose(file);
}

void Coalescing() {
    auto& log = AgcDriver::AsyncLog::Get();
    const std::string line = "[gpu] coalescing test line\n";
    int printed = 0;
    for (int i = 0; i < 50; ++i) printed += log.Admit(line) ? 1 : 0;
    Check(printed == 3, "identical lines in one window were not limited to the first three");
    Check(log.Admit("[gpu] another line\n"), "a different line was suppressed");
}

void FlushWaitsForHeldStream() {
    std::FILE* file = TemporaryFile::Open();
    Check(file != nullptr, "temporary file");
    auto& log = AgcDriver::AsyncLog::Get();
    {
        HeldStream held(file);
        log.Defer(file, "held %d %s\n", 7, AgcDriver::LogLiteral{"line"});
        std::thread releaser([&] {
            std::this_thread::sleep_for(2400ms);
            held.Release();
        });
        log.Flush();
        const bool waited = held.Released();
        releaser.join();
        Check(waited, "Flush returned while the logger still waited for the stream");
    }
    Check(ReadAll(file) == "held 7 line\n", "the line queued behind a held stream changed");
    std::fclose(file);
}

void BoundedFlushGivesUp() {
    std::FILE* file = TemporaryFile::Open();
    Check(file != nullptr, "temporary file");
    auto& log = AgcDriver::AsyncLog::Get();
    Check(log.FlushFor(0ms), "FlushFor waited with nothing queued");
    {
        HeldStream held(file);
        log.Defer(file, "bounded %d\n", 1);
        Check(!log.FlushFor(100ms), "FlushFor reported a line written while its stream was held");
        held.Release();
        Check(log.FlushFor(10s), "FlushFor missed the line written after the stream was released");
    }
    Check(ReadAll(file) == "bounded 1\n", "the line queued behind a held stream changed");
    std::fclose(file);
}

void FullQueueBlocksCaller() {
    std::FILE* file = TemporaryFile::Open();
    Check(file != nullptr, "temporary file");
    auto& log = AgcDriver::AsyncLog::Get();
    constexpr std::size_t lineBytes = 1024 * 1024;
    constexpr std::size_t lines = 2 * AgcDriver::AsyncLog::Capacity / lineBytes + 4;
    constexpr std::size_t filled = AgcDriver::AsyncLog::Capacity / lineBytes / 2;
    std::atomic<std::size_t> queued{0};
    bool blocked = false;
    {
        HeldStream held(file);
        std::thread producer([&] {
            for (std::size_t i = 0; i < lines; ++i) {
                const std::string line = LargeLine(i, lineBytes);
                log.Write(file, line.data(), line.size());
                queued.store(i + 1);
            }
        });
        auto last = queued.load();
        auto changedAt = Clock::now();
        const auto deadline = changedAt + 10s;
        while (last < lines && Clock::now() < deadline && (last < filled || Clock::now() - changedAt < 500ms)) {
            std::this_thread::sleep_for(5ms);
            const auto now = queued.load();
            if (now != last) {
                last = now;
                changedAt = Clock::now();
            }
        }
        blocked = last >= filled && last < lines && Clock::now() - changedAt >= 500ms;
        held.Release();
        producer.join();
    }
    Check(blocked, "the caller was not blocked by a full queue");
    log.Flush();
    std::fflush(file);
    std::rewind(file);
    std::string line;
    for (std::size_t i = 0; i < lines; ++i) {
        const std::string expected = LargeLine(i, lineBytes);
        line.resize(expected.size());
        Check(std::fread(line.data(), 1, line.size(), file) == line.size() && line == expected, "a line written through a full queue changed or went missing");
    }
    Check(std::fgetc(file) == EOF, "a full queue wrote extra output");
    std::fclose(file);
}

constexpr int ChildLines = 50;

[[noreturn]] void EndChild(const std::string& mode, const char* output) {
    const auto path = std::filesystem::temp_directory_path() / output;
#ifdef _WIN32
    std::FILE* file = _wfopen(path.c_str(), L"wb");
#else
    std::FILE* file = std::fopen(path.c_str(), "wb");
#endif
    Check(file != nullptr, "child output file");
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
    const rlimit noCore{0, 0};
    setrlimit(RLIMIT_CORE, &noCore);
#endif
#ifdef __linux__
    prctl(PR_SET_DUMPABLE, 0);
#endif
    std::thread([] {
        std::this_thread::sleep_for(8s);
        std::_Exit(99);
    }).detach();
    AgcDriver::InstallAsyncLog_nid_no_patch();
    auto& log = AgcDriver::AsyncLog::Get();
    HeldStream held(file);
    for (int i = 0; i < ChildLines; ++i) {
        log.Defer(file, "deferred %d\n", i);
        WriteLogLine(file, "plain %d\n", i);
    }
    std::thread releaser([&] {
        std::this_thread::sleep_for(100ms);
        if (mode != "blocked") held.Release();
    });
    if (mode == "terminate") std::terminate();
#ifdef _WIN32
    if (mode == "fault") RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr);
#else
    if (mode == "raise") {
        std::raise(SIGABRT);
        std::_Exit(0);
    }
#endif
    std::abort();
}

std::string Quote(const std::string& text) {
    return "\"" + text + "\"";
}

void LinesSurviveEnd(const std::string& self, const std::string& mode) {
    const auto output = std::filesystem::temp_directory_path() / ("aps5-async-log-" + mode + "-" + std::to_string(Clock::now().time_since_epoch().count()) + ".txt");
#ifdef _WIN32
    (void)self;
    std::wstring executable(32768, L'\0');
    executable.resize(GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size())));
    const std::wstring command = L"\"\"" + executable + L"\" child " + std::filesystem::path(mode).wstring() + L" \"" + output.filename().wstring() + L"\"\"";
    const auto start = Clock::now();
    const int status = _wsystem(command.c_str());
#else
    const std::string command = Quote(self) + " child " + mode + " " + Quote(output.filename().string());
    const auto start = Clock::now();
    const int status = std::system(command.c_str());
#endif
    const auto lived = Clock::now() - start;
    std::error_code error;
    const bool started = std::filesystem::exists(output, error);
    const std::string text = started ? ReadFile(output) : std::string();
    std::filesystem::remove(output, error);
    Check(started, "the child did not start");
    Check(status != 0, "the child did not end abnormally");
    Check(lived < 6s, "the child took too long to end");
    if (mode == "blocked") {
        Check(lived >= 1900ms, "the child ended before the flush gave up");
        Check(text.empty(), "the child wrote through a held stream");
        return;
    }
    std::string expected;
    for (int i = 0; i < ChildLines; ++i) expected += "deferred " + std::to_string(i) + "\nplain " + std::to_string(i) + "\n";
    Check(text == expected, "lines queued before the process ended were lost or changed");
}

}

int main(int argc, char** argv) {
    try {
        ClearLogEnvironment();
        if (argc == 4 && std::strcmp(argv[1], "child") == 0) EndChild(argv[2], argv[3]);
        if (argc == 2 && std::strcmp(argv[1], "fatal") == 0) {
            LinesSurviveEnd(argv[0], "abort");
            LinesSurviveEnd(argv[0], "terminate");
#ifdef _WIN32
            LinesSurviveEnd(argv[0], "fault");
#else
            LinesSurviveEnd(argv[0], "raise");
#endif
            LinesSurviveEnd(argv[0], "blocked");
            return 0;
        }
        OrderPerThread();
        LongLine();
        Coalescing();
        FlushWaitsForHeldStream();
        BoundedFlushGivesUp();
        FullQueueBlocksCaller();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
