#include "prx/libc/include/LogFile.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace {

using namespace log_file;

std::string g_self;
std::filesystem::path g_directory;
int g_failures = 0;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "log file test failed: %s\n", what);
    std::exit(1);
}

std::string Slurp(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string Drain(Handle read) {
    std::string text;
    char buffer[4096];
    for (;;) {
        const long count = ReadAvailable(read, buffer, sizeof(buffer));
        if (count <= 0) return text;
        text.append(buffer, static_cast<std::size_t>(count));
    }
}

struct Rig {
    Pipe output;
    Pipe errors;
    Pipe terminalOutput;
    Pipe terminalError;
    Handle file = NoHandle;
    std::filesystem::path path;
    std::unique_ptr<Tee> tee;
    std::mutex outputWrites;
    std::mutex errorWrites;

    explicit Rig(const std::string& name, bool terminals = true, bool start = true) {
        path = g_directory / name;
        std::string error;
        Require(MakePipe(output, error) && MakePipe(errors, error), "source pipes");
        Require(MakePipe(terminalOutput, error) && MakePipe(terminalError, error), "terminal pipes");
        Require(OpenAppend(path.string(), file, error), "log file");
        tee = std::make_unique<Tee>(file, terminals ? terminalOutput.write : NoHandle, terminals ? terminalError.write : NoHandle, output.read, errors.read);
        if (start) tee->Start();
    }

    void Write(int standard, const std::string& text) {
        const std::lock_guard<std::mutex> hold(standard == StandardOutput ? outputWrites : errorWrites);
        Require(WriteAll(standard == StandardOutput ? output.write : errors.write, text.data(), text.size()), "source write");
    }

    ~Rig() {
        tee.reset();
        for (const Handle handle : {output.read, output.write, errors.read, errors.write, terminalOutput.read, terminalOutput.write, terminalError.read, terminalError.write}) Close(handle);
    }
};

void OrderAndTerminal() {
    Rig rig("order.log");
    rig.Write(StandardOutput, "one\n");
    rig.Write(StandardOutput, "two\n");
    rig.Write(StandardOutput, "three\n");
    rig.tee->Stop();
    Require(Slurp(rig.path) == "one\ntwo\nthree\n", "the file lacks the lines or their order");
    Require(Drain(rig.terminalOutput.read) == "one\ntwo\nthree\n", "the terminal lacks the lines");
    Require(Drain(rig.terminalError.read).empty(), "standard error shows standard output text");
}

void BothStreams() {
    Rig rig("both.log", true, false);
    rig.Write(StandardOutput, "out one\n");
    Require(rig.tee->Flush(false), "flush");
    rig.Write(StandardError, "err one\n");
    Require(rig.tee->Flush(false), "flush");
    rig.Write(StandardOutput, "out two\n");
    Require(rig.tee->Flush(false), "flush");
    Require(Slurp(rig.path) == "out one\nerr one\nout two\n", "the streams are not in arrival order in the file");
    Require(Drain(rig.terminalOutput.read) == "out one\nout two\n", "standard output on the terminal differs");
    Require(Drain(rig.terminalError.read) == "err one\n", "standard error on the terminal differs");
}

void LinesStayWholeAcrossStreams() {
    Rig rig("whole.log", true, false);
    rig.Write(StandardOutput, "AAA");
    rig.tee->Flush(false);
    rig.Write(StandardError, "BBB\n");
    rig.tee->Flush(false);
    rig.Write(StandardOutput, "CCC\n");
    rig.tee->Flush(false);
    Require(Slurp(rig.path) == "BBB\nAAACCC\n", "a line of one stream was split by a line of the other");
    Require(Drain(rig.terminalOutput.read) == "AAACCC\n", "the terminal copy of a partial line differs");
}

void ConcurrentWriters() {
    Rig rig("concurrent.log", false);
    constexpr int Writers = 6;
    constexpr int PerWriter = 400;
    std::vector<std::thread> threads;
    for (int writer = 0; writer < Writers; ++writer) {
        threads.emplace_back([&rig, writer] {
            for (int index = 0; index < PerWriter; ++index) {
                char line[3100];
                const int head = std::snprintf(line, sizeof(line), "w%d n%d ", writer, index);
                std::memset(line + head, 'a' + writer, 3000);
                line[head + 3000] = '\n';
                rig.Write(writer % 2 == 0 ? StandardOutput : StandardError, std::string(line, static_cast<std::size_t>(head) + 3001));
            }
        });
    }
    for (auto& thread : threads) thread.join();
    rig.tee->Stop();
    std::istringstream lines(Slurp(rig.path));
    std::vector<int> next(Writers, 0);
    std::string line;
    int total = 0;
    while (std::getline(lines, line)) {
        int writer = -1;
        int index = -1;
        int head = 0;
        Require(std::sscanf(line.c_str(), "w%d n%d %n", &writer, &index, &head) == 2 && writer >= 0 && writer < Writers, "a line of the file has no writer header");
        Require(line.size() == static_cast<std::size_t>(head) + 3000 && line.find_first_not_of(static_cast<char>('a' + writer), static_cast<std::size_t>(head)) == std::string::npos, "a line of the file mixes bytes of two writers");
        Require(index == next[static_cast<std::size_t>(writer)]++, "the lines of one writer are out of order");
        ++total;
    }
    Require(total == Writers * PerWriter, "lines are missing from the file");
}

void CrashWriteReachesFile() {
    Rig rig("crash.log", true, false);
    rig.Write(StandardOutput, "before one\n");
    rig.Write(StandardError, "before two\n");
    rig.Write(StandardOutput, "unfinished");
    rig.tee->CrashWrite(StandardError, "CRASH report\n", 13);
    Require(Slurp(rig.path) == "before one\nunfinished\nbefore two\nCRASH report\n", "the pending lines or the crash report are not in the file in order");
    Require(Drain(rig.terminalError.read) == "before two\nCRASH report\n", "the crash report did not reach the terminal");
    Require(Drain(rig.terminalOutput.read) == "before one\nunfinished", "the pending output did not reach the terminal");
}

void CrashWriteWithTeeThreadsRunning() {
    Rig rig("crash-running.log");
    for (int index = 0; index < 200; ++index) rig.Write(StandardError, "pending " + std::to_string(index) + "\n");
    rig.tee->CrashWrite(StandardError, "CRASH\n", 6);
    const std::string text = Slurp(rig.path);
    Require(text.size() > 6 && text.compare(text.size() - 6, 6, "CRASH\n") == 0, "the crash line is not last in the file");
    Require(text.find("pending 199\n") != std::string::npos && text.find("pending 0\n") == 0, "pending lines are missing before the crash line");
}

void PartialLineAtExit() {
    Rig rig("partial.log");
    rig.Write(StandardOutput, "whole\nno newline at the end");
    rig.tee->Stop();
    Require(Slurp(rig.path) == "whole\nno newline at the end\n", "a line without a newline was not flushed at the end");
    Require(Drain(rig.terminalOutput.read) == "whole\nno newline at the end", "the terminal copy gained a newline");
}

void LongLines() {
    Rig rig("long.log", false);
    const std::string line(300000, 'x');
    rig.Write(StandardOutput, line + "\n");
    const std::string huge(Tee::CarryBytes + 12345, 'y');
    std::thread writer([&] { rig.Write(StandardError, huge + "\n"); });
    std::string terminal;
    writer.join();
    rig.tee->Stop();
    const std::string text = Slurp(rig.path);
    Require(text.size() == line.size() + huge.size() + 2, "a long line changed size in the file");
    Require(text.find(line + "\n") != std::string::npos && text.find(huge + "\n") != std::string::npos, "a long line is not whole in the file");
}

void AppendKeepsEarlierRuns() {
    const auto path = g_directory / "append.log";
    for (const char* text : {"first run\n", "second run\n"}) {
        Handle file = NoHandle;
        std::string error;
        Require(OpenAppend(path.string(), file, error), "open for appending");
        Require(WriteAll(file, text, std::strlen(text)), "append write");
        Close(file);
    }
    Require(Slurp(path) == "first run\nsecond run\n", "opening the file again did not append");
}

void FilePrimitives() {
    const auto path = g_directory / "primitives.log";
    const auto moved = g_directory / "primitives.log.1";
    Handle file = NoHandle;
    std::string error;
    Require(OpenAppend(path.string(), file, error), "open for the size check");
    Require(WriteAll(file, "twelve bytes", 12), "write for the size check");
    Require(FileSize(file) == 12, "the size of an open file differs");
    Require(RenameFile(path.string(), moved.string()), "rename an open file");
    Require(!std::filesystem::exists(path) && std::filesystem::exists(moved), "the rename left the old name behind");
    Require(WriteAll(file, "!", 1), "write after the rename");
    Close(file);
    Require(Slurp(moved) == "twelve bytes!", "the open file did not follow the rename");
    Require(RemoveFile(moved.string()), "remove a file");
    Require(RemoveFile(moved.string()), "removing a missing file is not success");
    Require(!std::filesystem::exists(moved), "the file is still there after removal");
}

void MaxBytesValues() {
    std::uint64_t value = 7;
    std::string error;
    Require(ParseMaxBytes(nullptr, value, error) && value == 0 && error.empty(), "an unset limit is not off");
    Require(ParseMaxBytes("", value, error) && value == 0 && error.empty(), "an empty limit is not off");
    Require(ParseMaxBytes("0", value, error) && value == 0 && error.empty(), "zero does not turn the limit off");
    Require(ParseMaxBytes("65536", value, error) && value == 65536, "the smallest limit was refused");
    Require(ParseMaxBytes("1099511627776", value, error) && value == (std::uint64_t{1} << 40), "the largest limit was refused");
    for (const char* text : {"-1", "+5", " 5", "1k", "65535", "1099511627777", "99999999999999999999999", "0x10"}) {
        error.clear();
        Require(!ParseMaxBytes(text, value, error) && error.find("ANYPS5_LOG_MAX_BYTES") != std::string::npos, text);
    }
}

void KeepValues() {
    unsigned value = 9;
    std::string error;
    Require(ParseKeep(nullptr, value, error) && value == 3 && error.empty(), "an unset count is not the default of 3");
    Require(ParseKeep("", value, error) && value == 3, "an empty count is not the default");
    Require(ParseKeep("0", value, error) && value == 0, "zero files was refused");
    Require(ParseKeep("100", value, error) && value == 100, "the largest count was refused");
    for (const char* text : {"-1", "101", "1000000000000", "x", "3 "}) {
        error.clear();
        Require(!ParseKeep(text, value, error) && error.find("ANYPS5_LOG_KEEP") != std::string::npos, text);
    }
}

void RotationPolicy() {
    Require(!RotationNeeded(1000, 1000, 0), "a zero limit rotates");
    Require(!RotationNeeded(0, 5000, 100), "an empty file rotates before its first line");
    Require(!RotationNeeded(60, 40, 100), "a write that exactly fills the limit rotates");
    Require(RotationNeeded(60, 41, 100), "a write past the limit did not rotate");
    Require(RotationNeeded(10, 5000, 100), "an oversize write into a used file did not rotate first");
    Require(RotatedName("/var/log/run.log", 2) == "/var/log/run.log.2", "rotated name differs");
}

void RotatesWhenFull() {
    const auto path = g_directory / "rotate.log";
    for (const char* leftover : {"rotate.log", "rotate.log.1", "rotate.log.2", "rotate.log.3"}) RemoveFile((g_directory / leftover).string());
    Handle file = NoHandle;
    std::string error;
    Require(OpenAppend(path.string(), file, error), "open for rotation");
    Tee tee(file, NoHandle, NoHandle, NoHandle, NoHandle);
    tee.SetRotation(path.string(), 10, 2);
    tee.Note("aaaa\n", 5);
    tee.Note("bbbb\n", 5);
    tee.Note("cccc\n", 5);
    tee.Note("dddddddddd\n", 11);
    tee.Note("e\n", 2);
    tee.Flush(true);
    Require(Slurp(path) == "e\n", "the current file does not hold the newest line");
    Require(Slurp(RotatedName(path.string(), 1)) == "dddddddddd\n", "the first rotated file lacks the last full write");
    Require(Slurp(RotatedName(path.string(), 2)) == "cccc\n", "the second rotated file lacks the older line");
    Require(!std::filesystem::exists(RotatedName(path.string(), 3)), "a file beyond the keep count was kept");
    Require(!tee.FileFailed(), "a rotation was recorded as a failure");
}

void RotatesNothingWithoutLimit() {
    const auto path = g_directory / "unlimited.log";
    Handle file = NoHandle;
    std::string error;
    Require(OpenAppend(path.string(), file, error), "open unlimited");
    Tee tee(file, NoHandle, NoHandle, NoHandle, NoHandle);
    for (int index = 0; index < 50; ++index) tee.Note("0123456789\n", 11);
    tee.Flush(true);
    Require(!std::filesystem::exists(RotatedName(path.string(), 1)), "a file with no limit was rotated");
    Require(Slurp(path).size() == 550, "a file with no limit lost lines");
}

void InvalidLimitsStopBeforeOpening() {
    const auto path = g_directory / "limits-refused.log";
    std::filesystem::remove(path);
    std::string error;
#ifdef _WIN32
    _putenv_s("ANYPS5_LOG_KEEP", "x");
#else
    ::setenv("ANYPS5_LOG_KEEP", "x", 1);
#endif
    Require(SystemFactory().Create(path.string(), error) == nullptr, "an invalid keep count started a session");
    Require(error.find("ANYPS5_LOG_KEEP") != std::string::npos, "the error does not name ANYPS5_LOG_KEEP");
    Require(!std::filesystem::exists(path), "an invalid keep count created the file");
#ifdef _WIN32
    _putenv_s("ANYPS5_LOG_KEEP", "");
    _putenv_s("ANYPS5_LOG_MAX_BYTES", "12");
#else
    ::unsetenv("ANYPS5_LOG_KEEP");
    ::setenv("ANYPS5_LOG_MAX_BYTES", "12", 1);
#endif
    error.clear();
    Require(SystemFactory().Create(path.string(), error) == nullptr && error.find("ANYPS5_LOG_MAX_BYTES") != std::string::npos, "an invalid byte limit was accepted");
    Require(!std::filesystem::exists(path), "an invalid byte limit created the file");
#ifdef _WIN32
    _putenv_s("ANYPS5_LOG_MAX_BYTES", "");
#else
    ::unsetenv("ANYPS5_LOG_MAX_BYTES");
#endif
}

void RelativePathUsesWorkingDirectory() {
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(g_directory);
    Handle file = NoHandle;
    std::string error;
    const bool opened = OpenAppend("relative.log", file, error);
    std::filesystem::current_path(previous);
    Require(opened, "open a relative path");
    Close(file);
    Require(std::filesystem::exists(g_directory / "relative.log"), "a relative path was not resolved against the working directory");
}

void UnopenablePath() {
    std::string error;
    const std::string path = (g_directory / "missing-directory" / "x.log").string();
    Require(StartFromValue(path.c_str(), SystemFactory(), error) == nullptr, "an unopenable path started a session");
    Require(error.find("ANYPS5_LOG_FILE") != std::string::npos, "the error does not name ANYPS5_LOG_FILE");
    Require(error.find(path) != std::string::npos, "the error does not name the path");
#ifdef _WIN32
    Require(error.find("(Windows error 3)") != std::string::npos, "the error does not carry the operating system error");
#else
    Require(error.find("No such file or directory") != std::string::npos && error.find("errno 2") != std::string::npos, "the error does not carry the operating system error");
#endif
    const std::string directory = g_directory.string();
    error.clear();
#ifdef _WIN32
    Require(StartFromValue(directory.c_str(), SystemFactory(), error) == nullptr && error.find("(Windows error 5)") != std::string::npos, "a directory was accepted as the log file");
#else
    Require(StartFromValue(directory.c_str(), SystemFactory(), error) == nullptr && error.find("Is a directory") != std::string::npos, "a directory was accepted as the log file");
#endif
}

class CountingFactory final : public Factory {
public:
    int created = 0;
    std::unique_ptr<Session> Create(const std::string&, std::string& error) override {
        ++created;
        error = "counted";
        return nullptr;
    }
};

int OpenDescriptors() {
#ifdef _WIN32
    DWORD count = 0;
    Require(GetProcessHandleCount(GetCurrentProcess(), &count) != 0, "count the process handles");
    return static_cast<int>(count);
#else
    int count = 0;
    for (int descriptor = 0; descriptor < 1024; ++descriptor) count += ::fcntl(descriptor, F_GETFD) != -1;
    return count;
#endif
}

void UnsetCreatesNothing() {
    CountingFactory factory;
    const int before = OpenDescriptors();
    std::string error = "stale";
    Require(StartFromValue(nullptr, factory, error) == nullptr && error.empty(), "an unset variable started a session");
    Require(StartFromValue("", factory, error) == nullptr && error.empty(), "an empty variable started a session");
    Require(factory.created == 0, "an unset or empty variable reached the factory");
    Require(OpenDescriptors() == before, "an unset variable opened a descriptor");
    Require(StartFromValue("x.log", factory, error) == nullptr && factory.created == 1 && error == "counted", "a set variable did not reach the factory");
}

void BrokenTerminalKeepsFile() {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    Rig rig("terminal-closed.log");
    Close(rig.terminalOutput.read);
    rig.terminalOutput.read = NoHandle;
    rig.Write(StandardOutput, "still logged\n");
    rig.Write(StandardOutput, "and again\n");
    rig.tee->Stop();
    Require(Slurp(rig.path) == "still logged\nand again\n", "a closed terminal stopped the file");
}

void MissingTerminalKeepsFile() {
    Rig rig("terminal-absent.log", false);
    rig.Write(StandardOutput, "file only\n");
    rig.Write(StandardError, "also file only\n");
    rig.tee->Stop();
    Require(Slurp(rig.path) == "file only\nalso file only\n", "an absent terminal stopped the file");
}

void FullDiskKeepsTerminal() {
    Pipe output;
    Pipe errors;
    Pipe terminalOutput;
    Pipe terminalError;
    std::string error;
    Require(MakePipe(output, error) && MakePipe(errors, error) && MakePipe(terminalOutput, error) && MakePipe(terminalError, error), "pipes");
#ifdef _WIN32
    const auto readOnly = g_directory / "read-only.log";
    std::ofstream(readOnly).close();
    const HANDLE opened = CreateFileW(readOnly.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (opened == INVALID_HANDLE_VALUE) return;
    const Handle full = reinterpret_cast<Handle>(opened);
#else
    const int opened = ::open("/dev/full", O_WRONLY);
    if (opened < 0) return;
    const Handle full = opened;
#endif
    {
        Tee tee(full, terminalOutput.write, terminalError.write, output.read, errors.read);
        tee.Start();
        const std::string line = "after the disk is full\n";
        Require(WriteAll(output.write, line.data(), line.size()), "source write");
        tee.Stop();
        Require(tee.FileFailed(), "a failed file write was not recorded");
    }
    Require(Drain(terminalOutput.read) == "after the disk is full\n", "the terminal lost output after the file failed");
    const std::string notice = Drain(terminalError.read);
    Require(notice.find("ANYPS5_LOG_FILE") != std::string::npos && notice.find("can no longer be written") != std::string::npos, "the failure was not reported once on the terminal");
    for (const Handle handle : {output.read, output.write, errors.read, errors.write, terminalOutput.read, terminalOutput.write, terminalError.read, terminalError.write}) Close(handle);
}

#ifdef _WIN32
constexpr const char TextNewline[] = "\r\n";
constexpr const char MissingPathError[] = "(Windows error 3)";
#else
constexpr const char TextNewline[] = "\n";
constexpr const char MissingPathError[] = "No such file or directory";
#endif

std::string Line(const char* text) {
    return std::string(text) + TextNewline;
}

struct Result {
    std::string out;
    std::string err;
    int status = -1;
};

bool StandardErrorIs(const std::string& text, const std::string& expected) {
    constexpr std::string_view WriteWatchUnavailable = "[memory] write watch unavailable: ";
    std::size_t begin = 0;
    if (text.compare(0, WriteWatchUnavailable.size(), WriteWatchUnavailable) == 0) {
        const std::size_t end = text.find('\n');
        if (end == std::string::npos) return false;
        begin = end + 1;
    }
    return text.compare(begin, std::string::npos, expected) == 0;
}

int ExitCode(int status) {
#ifdef _WIN32
    return status;
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

#ifdef _WIN32
std::wstring OwnExecutable() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    Require(length > 0 && length < path.size(), "the path of the test executable");
    path.resize(length);
    return path;
}

std::vector<wchar_t> ChildEnvironment(const std::string& variable, bool set) {
    std::vector<std::wstring> entries;
    wchar_t* const block = GetEnvironmentStringsW();
    Require(block != nullptr, "read the environment");
    for (const wchar_t* entry = block; *entry != L'\0'; entry += std::wcslen(entry) + 1) {
        if (_wcsnicmp(entry, L"ANYPS5_LOG_FILE=", 16) != 0) entries.emplace_back(entry);
    }
    FreeEnvironmentStringsW(block);
    if (set) entries.push_back(L"ANYPS5_LOG_FILE=" + std::filesystem::path(variable).wstring());
    std::sort(entries.begin(), entries.end(), [](const std::wstring& left, const std::wstring& right) { return _wcsicmp(left.c_str(), right.c_str()) < 0; });
    std::vector<wchar_t> environment;
    for (const auto& entry : entries) {
        environment.insert(environment.end(), entry.begin(), entry.end());
        environment.push_back(L'\0');
    }
    environment.push_back(L'\0');
    return environment;
}

HANDLE Spawn(const std::wstring& application, std::wstring command, wchar_t* environment, HANDLE output, HANDLE error) {
    HANDLE inherited[2] = {nullptr, nullptr};
    const HANDLE sources[2] = {output, error};
    for (int index = 0; index < 2; ++index) {
        Require(DuplicateHandle(GetCurrentProcess(), sources[index], GetCurrentProcess(), &inherited[index], 0, TRUE, DUPLICATE_SAME_ACCESS) != 0, "duplicate a standard handle for the child");
    }
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    auto* const attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    Require(InitializeProcThreadAttributeList(attributes, 1, 0, &bytes) != 0, "create the process attributes");
    Require(UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr) != 0, "list the inherited handles");
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput = inherited[0];
    startup.StartupInfo.hStdError = inherited[1];
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(application.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT, environment, nullptr, &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(attributes);
    for (const HANDLE handle : inherited) CloseHandle(handle);
    Require(created != 0, "spawn the child");
    CloseHandle(process.hThread);
    return process.hProcess;
}

Result Run(const std::string& role, const std::string& variable, bool set = true) {
    Pipe out;
    Pipe err;
    std::string error;
    Require(MakePipe(out, error) && MakePipe(err, error), "child pipes");
    std::vector<wchar_t> environment = ChildEnvironment(variable, set);
    const std::wstring self = OwnExecutable();
    const HANDLE child = Spawn(self, L"\"" + self + L"\" --child " + std::wstring(role.begin(), role.end()), environment.data(), reinterpret_cast<HANDLE>(out.write), reinterpret_cast<HANDLE>(err.write));
    Close(out.write);
    Close(err.write);
    Result result;
    bool open[2] = {true, true};
    const Handle reads[2] = {out.read, err.read};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while ((open[0] || open[1]) && std::chrono::steady_clock::now() < deadline) {
        bool idle = true;
        for (int index = 0; index < 2; ++index) {
            if (!open[index]) continue;
            char buffer[4096];
            const long count = ReadAvailable(reads[index], buffer, sizeof(buffer));
            if (count > 0) {
                (index == 0 ? result.out : result.err).append(buffer, static_cast<std::size_t>(count));
                idle = false;
            } else if (count < 0) {
                open[index] = false;
            }
        }
        if (idle) Sleep(2);
    }
    if (open[0] || open[1]) std::fprintf(stderr, "role %s open %d %d\n", role.c_str(), open[0], open[1]);
    Require(!open[0] && !open[1], "the child kept its streams open");
    DWORD code = 0;
    Require(WaitForSingleObject(child, 20000) == WAIT_OBJECT_0 && GetExitCodeProcess(child, &code) != 0, "wait for the child");
    CloseHandle(child);
    result.status = static_cast<int>(code);
    Close(out.read);
    Close(err.read);
    return result;
}
#else
Result Run(const std::string& role, const std::string& variable, bool set = true) {
    Pipe out;
    Pipe err;
    std::string error;
    Require(MakePipe(out, error) && MakePipe(err, error), "child pipes");
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, static_cast<int>(out.write), 1);
    posix_spawn_file_actions_adddup2(&actions, static_cast<int>(err.write), 2);
    std::vector<std::string> environment;
    for (char** entry = environ; *entry != nullptr; ++entry) {
        if (std::strncmp(*entry, "ANYPS5_LOG_FILE=", 16) != 0) environment.emplace_back(*entry);
    }
    if (set) environment.push_back("ANYPS5_LOG_FILE=" + variable);
    std::vector<char*> pointers;
    for (auto& entry : environment) pointers.push_back(entry.data());
    pointers.push_back(nullptr);
    std::string self = g_self;
    std::string argument = "--child";
    std::string roleCopy = role;
    char* arguments[] = {self.data(), argument.data(), roleCopy.data(), nullptr};
    pid_t child = 0;
    Require(posix_spawn(&child, g_self.c_str(), &actions, nullptr, arguments, pointers.data()) == 0, "spawn the child");
    posix_spawn_file_actions_destroy(&actions);
    Close(out.write);
    Close(err.write);
    Result result;
    bool open[2] = {true, true};
    const int descriptors[2] = {static_cast<int>(out.read), static_cast<int>(err.read)};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while ((open[0] || open[1]) && std::chrono::steady_clock::now() < deadline) {
        pollfd requests[2] = {{open[0] ? descriptors[0] : -1, POLLIN, 0}, {open[1] ? descriptors[1] : -1, POLLIN, 0}};
        ::poll(requests, 2, 100);
        for (int index = 0; index < 2; ++index) {
            if (!open[index]) continue;
            char buffer[4096];
            const ssize_t count = ::read(descriptors[index], buffer, sizeof(buffer));
            if (count > 0) (index == 0 ? result.out : result.err).append(buffer, static_cast<std::size_t>(count));
            else if (count == 0) open[index] = false;
        }
    }
    if (open[0] || open[1]) std::fprintf(stderr, "role %s open %d %d\n", role.c_str(), open[0], open[1]);
    Require(!open[0] && !open[1], "the child kept its streams open");
    ::waitpid(child, &result.status, 0);
    Close(out.read);
    Close(err.read);
    return result;
}
#endif

int ThreadCount() {
    int count = 0;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("/proc/self/task", error)) {
        static_cast<void>(entry);
        ++count;
    }
    return count;
}

int ChildMain(const std::string& role) {
    if (role == "threads") {
        std::printf("threads=%d\n", ThreadCount());
        return 0;
    }
    if (role == "plain") {
        std::printf("stdout line\n");
        std::fprintf(stderr, "stderr line\n");
        std::fflush(stdout);
        return 0;
    }
    if (role == "partial") {
        std::printf("stdout without a newline");
        return 0;
    }
    if (role == "hard") {
        std::fprintf(stderr, "written just before the end\n");
        LogFileFlush_nid_no_patch();
        _exit(3);
    }
    if (role == "crash") {
        std::fprintf(stderr, "line before the crash\n");
        const char report[] = "FATAL: simulated crash report\n";
        log_file::CrashWrite(StandardError, report, sizeof(report) - 1);
        _exit(4);
    }
    if (role == "exec") {
        std::fprintf(stderr, "line before the exec\n");
        LogFileRelease_nid_no_patch();
#ifdef _WIN32
        wchar_t shell[MAX_PATH];
        const DWORD length = GetEnvironmentVariableW(L"ComSpec", shell, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return 9;
        CloseHandle(Spawn(shell, L"cmd /d /c \"echo from the replacement& (echo from the replacement on stderr)1>&2\"", nullptr, GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE)));
        return 0;
#else
        execl("/bin/sh", "sh", "-c", "echo from the replacement; echo from the replacement on stderr >&2", static_cast<char*>(nullptr));
        return 9;
#endif
    }
    return 2;
}

void ChildProcesses() {
    const auto path = g_directory / "child.log";
    const auto clear = [&] { std::filesystem::remove(path); };

    clear();
    Result plain = Run("plain", path.string());
    Require(plain.status == 0, "the child failed");
    Require(plain.out == Line("stdout line") && StandardErrorIs(plain.err, Line("stderr line")), "the terminal output of a child changed");
    const std::string logged = Slurp(path);
    Require(logged.find(Line("stdout line")) != std::string::npos && logged.find(Line("stderr line")) != std::string::npos, "the child's lines are not in the file");

    clear();
    Result partial = Run("partial", path.string());
    Require(partial.status == 0 && partial.out == "stdout without a newline", "the partial line changed on the terminal");
    Require(Slurp(path).find("stdout without a newline\n") != std::string::npos, "a partial line was lost at a normal exit");

    clear();
    Result hard = Run("hard", path.string());
    Require(ExitCode(hard.status) == 3 && StandardErrorIs(hard.err, Line("written just before the end")), "the abrupt child differs");
    Require(Slurp(path).find(Line("written just before the end")) != std::string::npos, "a line written before _exit is not in the file");

    clear();
    Result crash = Run("crash", path.string());
    Require(ExitCode(crash.status) == 4, "the crashing child differs");
    Require(StandardErrorIs(crash.err, Line("line before the crash") + "FATAL: simulated crash report\n"), "the crash text did not reach the terminal in order");
    Require(Slurp(path).find(Line("line before the crash") + "FATAL: simulated crash report\n") != std::string::npos, "the crash text did not reach the file in order");

    clear();
    Result replaced = Run("exec", path.string());
    Require(replaced.status == 0, "the replacement failed or was killed by a broken pipe");
    Require(replaced.out == Line("from the replacement") && StandardErrorIs(replaced.err, Line("line before the exec") + Line("from the replacement on stderr")), "the output around the replacement did not reach the terminal");
    const std::string execLog = Slurp(path);
    Require(execLog.find(Line("line before the exec")) != std::string::npos && execLog.find("from the replacement") == std::string::npos, "the file holds the replacement's output or lacks the earlier line");

    clear();
    Result bad = Run("plain", (g_directory / "no-such-directory" / "x.log").string());
    Require(ExitCode(bad.status) == 1, "an unopenable path did not stop the process");
    Require(bad.err.find("ANYPS5_LOG_FILE") != std::string::npos && bad.err.find(MissingPathError) != std::string::npos && bad.out.empty(), "the startup error differs");

    clear();
    Result unset = Run("plain", "", false);
    Require(unset.out == Line("stdout line") && StandardErrorIs(unset.err, Line("stderr line")) && !std::filesystem::exists(path), "an unset variable changed the output or created a file");
    Result emptied = Run("plain", "", true);
    Require(emptied.out == Line("stdout line") && StandardErrorIs(emptied.err, Line("stderr line")), "an empty variable changed the output");

    if (std::filesystem::exists("/proc/self/task")) {
        const auto count = [&](bool set) {
            const Result result = Run("threads", path.string(), set);
            int threads = -1;
            Require(std::sscanf(result.out.c_str(), "threads=%d", &threads) == 1, "the thread count line");
            return threads;
        };
        const int without = count(false);
        const int with = count(true);
        Require(with == without + 2, "enabling the log did not add exactly the two copy threads");
        clear();
    }
}

}

int main(int argc, char** argv) {
    g_self = argv[0];
    if (argc == 3 && std::string(argv[1]) == "--child") return ChildMain(argv[2]);
    Require(argc == 1, "unexpected argument");
#ifdef _WIN32
    g_directory = std::filesystem::temp_directory_path() / ("anyps5-log-file-" + std::to_string(GetCurrentProcessId()));
    std::error_code stale;
    std::filesystem::remove_all(g_directory, stale);
    Require(std::filesystem::create_directory(g_directory), "temporary directory");
#else
    char pattern[] = "/tmp/anyps5-log-file-XXXXXX";
    Require(::mkdtemp(pattern) != nullptr, "temporary directory");
    g_directory = pattern;
#endif
    OrderAndTerminal();
    BothStreams();
    LinesStayWholeAcrossStreams();
    ConcurrentWriters();
    CrashWriteReachesFile();
    CrashWriteWithTeeThreadsRunning();
    PartialLineAtExit();
    LongLines();
    AppendKeepsEarlierRuns();
    FilePrimitives();
    MaxBytesValues();
    KeepValues();
    RotationPolicy();
    RotatesWhenFull();
    RotatesNothingWithoutLimit();
    InvalidLimitsStopBeforeOpening();
    RelativePathUsesWorkingDirectory();
    UnopenablePath();
    UnsetCreatesNothing();
    BrokenTerminalKeepsFile();
    MissingTerminalKeepsFile();
    FullDiskKeepsTerminal();
    ChildProcesses();
    std::error_code ignored;
    std::filesystem::remove_all(g_directory, ignored);
    return g_failures;
}
