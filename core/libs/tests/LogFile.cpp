#include "prx/libc/include/LogFile.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <atomic>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

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
        for (const Handle handle : {output.read, output.write, errors.read, errors.write, terminalOutput.read, terminalOutput.write, terminalError.read, terminalError.write, file}) Close(handle);
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
    Require(error.find("No such file or directory") != std::string::npos && error.find("errno 2") != std::string::npos, "the error does not carry the operating system error");
    const std::string directory = g_directory.string();
    error.clear();
    Require(StartFromValue(directory.c_str(), SystemFactory(), error) == nullptr && error.find("Is a directory") != std::string::npos, "a directory was accepted as the log file");
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
    int count = 0;
    for (int descriptor = 0; descriptor < 1024; ++descriptor) count += ::fcntl(descriptor, F_GETFD) != -1;
    return count;
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
    std::signal(SIGPIPE, SIG_IGN);
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
    const int full = ::open("/dev/full", O_WRONLY);
    if (full < 0) return;
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
    ::close(full);
}

struct Result {
    std::string out;
    std::string err;
    int status = -1;
};

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
        execl("/bin/sh", "sh", "-c", "echo from the replacement; echo from the replacement on stderr >&2", static_cast<char*>(nullptr));
        return 9;
    }
    return 2;
}

void ChildProcesses() {
    const auto path = g_directory / "child.log";
    const auto clear = [&] { std::filesystem::remove(path); };

    clear();
    Result plain = Run("plain", path.string());
    Require(plain.status == 0, "the child failed");
    Require(plain.out == "stdout line\n" && plain.err == "stderr line\n", "the terminal output of a child changed");
    const std::string logged = Slurp(path);
    Require(logged.find("stdout line\n") != std::string::npos && logged.find("stderr line\n") != std::string::npos, "the child's lines are not in the file");

    clear();
    Result partial = Run("partial", path.string());
    Require(partial.status == 0 && partial.out == "stdout without a newline", "the partial line changed on the terminal");
    Require(Slurp(path).find("stdout without a newline\n") != std::string::npos, "a partial line was lost at a normal exit");

    clear();
    Result hard = Run("hard", path.string());
    Require(WIFEXITED(hard.status) && WEXITSTATUS(hard.status) == 3 && hard.err == "written just before the end\n", "the abrupt child differs");
    Require(Slurp(path).find("written just before the end\n") != std::string::npos, "a line written before _exit is not in the file");

    clear();
    Result crash = Run("crash", path.string());
    Require(WIFEXITED(crash.status) && WEXITSTATUS(crash.status) == 4, "the crashing child differs");
    Require(crash.err == "line before the crash\nFATAL: simulated crash report\n", "the crash text did not reach the terminal in order");
    Require(Slurp(path).find("line before the crash\nFATAL: simulated crash report\n") != std::string::npos, "the crash text did not reach the file in order");

    clear();
    Result replaced = Run("exec", path.string());
    Require(replaced.status == 0, "the replacement failed or was killed by a broken pipe");
    Require(replaced.out == "from the replacement\n" && replaced.err == "line before the exec\nfrom the replacement on stderr\n", "the output around the replacement did not reach the terminal");
    const std::string execLog = Slurp(path);
    Require(execLog.find("line before the exec\n") != std::string::npos && execLog.find("from the replacement") == std::string::npos, "the file holds the replacement's output or lacks the earlier line");

    clear();
    Result bad = Run("plain", (g_directory / "no-such-directory" / "x.log").string());
    Require(WIFEXITED(bad.status) && WEXITSTATUS(bad.status) == 1, "an unopenable path did not stop the process");
    Require(bad.err.find("ANYPS5_LOG_FILE") != std::string::npos && bad.err.find("No such file or directory") != std::string::npos && bad.out.empty(), "the startup error differs");

    clear();
    Result unset = Run("plain", "", false);
    Require(unset.out == "stdout line\n" && unset.err == "stderr line\n" && !std::filesystem::exists(path), "an unset variable changed the output or created a file");
    Result emptied = Run("plain", "", true);
    Require(emptied.out == "stdout line\n" && emptied.err == "stderr line\n", "an empty variable changed the output");

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
    char pattern[] = "/tmp/anyps5-log-file-XXXXXX";
    Require(::mkdtemp(pattern) != nullptr, "temporary directory");
    g_directory = pattern;
    OrderAndTerminal();
    BothStreams();
    LinesStayWholeAcrossStreams();
    ConcurrentWriters();
    CrashWriteReachesFile();
    CrashWriteWithTeeThreadsRunning();
    PartialLineAtExit();
    LongLines();
    AppendKeepsEarlierRuns();
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
