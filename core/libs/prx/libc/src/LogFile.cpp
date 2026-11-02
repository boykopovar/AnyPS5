#include "prx/libc/include/LogFile.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace log_file {

namespace {

constexpr const char VariableName[] = "ANYPS5_LOG_FILE";
constexpr const char WriteFailure[] = "[log] ANYPS5_LOG_FILE: the file can no longer be written; it stops here and the terminal output goes on\n";

std::atomic<Session*> g_session{nullptr};

}

Tee::Tee(Handle file, Handle terminalOutput, Handle terminalError, Handle sourceOutput, Handle sourceError) : _file(file) {
    _streams[0].source = sourceOutput;
    _streams[0].terminal = terminalOutput;
    _streams[1].source = sourceError;
    _streams[1].terminal = terminalError;
    for (auto& stream : _streams) {
        stream.chunk = std::make_unique<char[]>(ChunkBytes);
        stream.carry = std::make_unique<char[]>(CarryBytes);
    }
}

Tee::~Tee() {
    Stop();
}

Tee::Stream& Tee::streamOf(int standard) {
    return _streams[standard == StandardOutput ? 0 : 1];
}

bool Tee::lock(int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    for (;;) {
        int expected = 0;
        if (_lock.compare_exchange_strong(expected, 1, std::memory_order_acquire)) return true;
        if (std::chrono::steady_clock::now() >= deadline) return false;
        PauseMicroseconds(100);
    }
}

void Tee::unlock() {
    _lock.store(0, std::memory_order_release);
}

void Tee::toTerminal(Stream& stream, const char* data, std::size_t length) {
    if (stream.terminal == NoHandle || stream.terminalBroken.load(std::memory_order_relaxed) || length == 0) return;
    if (!WriteAll(stream.terminal, data, length)) stream.terminalBroken.store(true, std::memory_order_relaxed);
}

void Tee::toFile(const char* data, std::size_t length) {
    if (length == 0 || _fileFailed.load(std::memory_order_relaxed)) return;
    if (WriteAll(_file, data, length)) return;
    _fileFailed.store(true, std::memory_order_relaxed);
    toTerminal(_streams[1], WriteFailure, sizeof(WriteFailure) - 1);
}

void Tee::flushCarry(Stream& stream) {
    if (stream.carried == 0) return;
    toFile(stream.carry.get(), stream.carried);
    toFile("\n", 1);
    stream.carried = 0;
}

void Tee::pump(Stream& stream) {
    if (stream.source == NoHandle) return;
    for (;;) {
        const long count = ReadAvailable(stream.source, stream.chunk.get(), ChunkBytes);
        if (count <= 0) return;
        const char* chunk = stream.chunk.get();
        const auto length = static_cast<std::size_t>(count);
        toTerminal(stream, chunk, length);
        std::size_t whole = length;
        while (whole > 0 && chunk[whole - 1] != '\n') --whole;
        if (whole == 0) {
            if (stream.carried + length > CarryBytes) {
                toFile(stream.carry.get(), stream.carried);
                stream.carried = 0;
                toFile(chunk, length);
            } else {
                std::memcpy(stream.carry.get() + stream.carried, chunk, length);
                stream.carried += length;
            }
            continue;
        }
        if (stream.carried != 0) {
            toFile(stream.carry.get(), stream.carried);
            stream.carried = 0;
        }
        toFile(chunk, whole);
        std::memcpy(stream.carry.get(), chunk + whole, length - whole);
        stream.carried = length - whole;
    }
}

void Tee::run(Stream& stream) {
    while (!_stop.load(std::memory_order_acquire)) {
        if (!WaitReadable(stream.source, 20)) continue;
        bool locked = false;
        while (!locked && !_stop.load(std::memory_order_acquire)) locked = lock(50);
        if (!locked) return;
        if (!_stop.load(std::memory_order_acquire)) pump(stream);
        unlock();
    }
}

void Tee::Start() {
    if (_started) return;
    _started = true;
    for (auto& stream : _streams) {
        if (stream.source != NoHandle) stream.worker = std::thread([this, &stream] { run(stream); });
    }
}

void Tee::Stop() {
    _stop.store(true, std::memory_order_release);
    for (auto& stream : _streams) {
        if (stream.worker.joinable()) stream.worker.join();
    }
    _started = false;
    Flush(true);
}

bool Tee::Flush(bool ending) {
    if (!lock(500)) return false;
    for (auto& stream : _streams) pump(stream);
    if (ending) {
        for (auto& stream : _streams) flushCarry(stream);
    }
    unlock();
    return true;
}

void Tee::CrashWrite(int standard, const char* text, std::size_t length) {
    const bool locked = lock(250);
    if (locked) {
        for (auto& stream : _streams) {
            pump(stream);
            flushCarry(stream);
        }
    }
    toTerminal(streamOf(standard), text, length);
    toFile(text, length);
    if (locked) unlock();
}

void Tee::Note(const char* text, std::size_t length) {
    const bool locked = lock(250);
    toFile(text, length);
    if (locked) unlock();
}

bool Tee::FileFailed() const {
    return _fileFailed.load(std::memory_order_relaxed);
}

std::unique_ptr<Session> StartFromValue(const char* value, Factory& factory, std::string& error) {
    error.clear();
    if (value == nullptr || *value == '\0') return nullptr;
    auto session = factory.Create(value, error);
    if (session == nullptr && error.empty()) error = "no session was created";
    return session;
}

namespace {

class SystemSession final : public Session {
public:
    SystemSession(Handle file, Handle originalOutput, Handle originalError, Pipe output, Pipe error) :
        _originalOutput(originalOutput), _originalError(originalError), _output(output), _error(error),
        _tee(file, originalOutput, originalError, output.read, error.read) {}

    Tee& Output() override { return _tee; }

    void Release() override {
        _tee.Flush(true);
        RestoreStandard(StandardOutput, _originalOutput);
        RestoreStandard(StandardError, _originalError);
    }

private:
    Handle _originalOutput;
    Handle _originalError;
    Pipe _output;
    Pipe _error;
    Tee _tee;
};

class SystemFactoryImpl final : public Factory {
public:
    std::unique_ptr<Session> Create(const std::string& path, std::string& error) override {
        Handle file = NoHandle;
        std::string reason;
        if (!OpenAppend(path, file, reason)) {
            error = std::string(VariableName) + ": cannot open \"" + path + "\" for appending: " + reason;
            return nullptr;
        }
        Pipe output;
        Pipe errors;
        if (!MakePipe(output, reason) || !MakePipe(errors, reason)) {
            error = std::string(VariableName) + ": cannot create the pipes that copy the log into the file: " + reason;
            Close(file);
            return nullptr;
        }
        const Handle originalOutput = KeepStandard(StandardOutput);
        const Handle originalError = KeepStandard(StandardError);
        const bool interactive = originalOutput != NoHandle && IsInteractive(originalOutput);
        std::fflush(nullptr);
        if (!Divert(StandardOutput, output.write) || !Divert(StandardError, errors.write)) {
            RestoreStandard(StandardOutput, originalOutput);
            RestoreStandard(StandardError, originalError);
            error = std::string(VariableName) + ": cannot redirect the standard streams into the log pipes";
            Close(file);
            return nullptr;
        }
        if (interactive) {
#ifdef _WIN32
            std::setvbuf(stdout, nullptr, _IONBF, 0);
#else
            std::setvbuf(stdout, nullptr, _IOLBF, 0);
#endif
        }
        auto session = std::make_unique<SystemSession>(file, originalOutput, originalError, output, errors);
        const std::string note = std::string("[log] ANYPS5_LOG_FILE: appending to this file, process ") + std::to_string(static_cast<long>(
#ifdef _WIN32
            _getpid()
#else
            getpid()
#endif
        )) + "\n";
        session->Output().Note(note.data(), note.size());
        session->Output().Start();
        return session;
    }
};

}

Factory& SystemFactory() {
    static SystemFactoryImpl factory;
    return factory;
}

bool Active() {
    return g_session.load(std::memory_order_acquire) != nullptr;
}

void Flush() {
    if (auto* const session = g_session.load(std::memory_order_acquire)) session->Output().Flush(true);
}

void Release() {
    if (auto* const session = g_session.load(std::memory_order_acquire)) session->Release();
}

void CrashWrite(int standard, const char* text, std::size_t length) {
    if (auto* const session = g_session.load(std::memory_order_acquire)) {
        session->Output().CrashWrite(standard, text, length);
        return;
    }
    WriteStandardDirect(standard, text, length);
}

namespace {

[[maybe_unused]] const bool g_started = [] {
    std::string value;
    const bool set = ReadVariable(value);
    std::string error;
    auto session = StartFromValue(set ? value.c_str() : nullptr, SystemFactory(), error);
    if (!error.empty()) FailStartup(error + "\n");
    if (session == nullptr) return false;
    g_session.store(session.release(), std::memory_order_release);
    std::atexit([] {
        std::fflush(nullptr);
        Flush();
    });
    return true;
}();

}

}

extern "C" void LogFileFlush_nid_no_patch() {
    log_file::Flush();
}

extern "C" void LogFileRelease_nid_no_patch() {
    log_file::Release();
}
