#define main frameReplayToolMain
#include "../tools/FrameReplay.cpp"
#undef main
#include <windows.h>

namespace {

using AgcDriver::FrameCaptureWriter;

constexpr int Skip = 77;
constexpr DWORD ChildLimit = 60000;

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

void append(const std::filesystem::path& log, const std::string& line) {
    std::ofstream file(log, std::ios::app);
    file << line << '\n';
    check(file.good(), "cannot write the relaunch log");
}

int child(const std::filesystem::path& directory, int exitCode) {
    const int local = 0;
    const auto address = reinterpret_cast<std::uintptr_t>(&local) & ~std::uintptr_t{0xfff};
    if (!GuestArena::GuestArenaHostRegionOverlaps_nid_postfix(address, 4096)) return Skip;
    const char* const attempt = std::getenv("APS5_FRAME_REPLAY_ATTEMPT");
    append(directory / "log.txt", attempt == nullptr ? "none" : attempt);
    const auto capture = directory / std::to_string(GetCurrentProcessId());
    FrameCaptureWriter writer(capture);
    writer.WriteRange(address, 4096, FrameRangeKind::Watched, 3, AgcDriver::FrameRangeAccess::Readable | AgcDriver::FrameRangeAccess::Gpu);
    writer.Finish("{}");
    const FrameCaptureReader reader(capture);
    relaunchIfHostRegionBlocks(reader);
    append(directory / "log.txt", "continued");
    return exitCode;
}

int start(const std::filesystem::path& directory, const char* attempt, int exitCode) {
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    check(_putenv_s("APS5_FRAME_REPLAY_ATTEMPT", attempt == nullptr ? "" : attempt) == 0, "cannot set the relaunch count");
    std::wstring self(32768, L'\0');
    self.resize(GetModuleFileNameW(nullptr, self.data(), static_cast<DWORD>(self.size())));
    std::wstring commandLine = L"\"" + self + L"\" child \"" + directory.wstring() + L"\" " + std::to_wstring(exitCode);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    check(CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process) != 0, "cannot start the replay child");
    CloseHandle(process.hThread);
    const bool exited = WaitForSingleObject(process.hProcess, ChildLimit) == WAIT_OBJECT_0;
    if (!exited) TerminateProcess(process.hProcess, 1);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    check(exited, "the replay child did not exit");
    return static_cast<int>(code);
}

std::vector<std::string> lines(const std::filesystem::path& path) {
    std::vector<std::string> result;
    std::ifstream file(path);
    for (std::string line; std::getline(file, line);) result.push_back(line);
    return result;
}

bool expectRun(const std::filesystem::path& directory, const char* attempt, int exitCode, int expected, const std::vector<std::string>& log) {
    const int code = start(directory, attempt, exitCode);
    if (code == Skip) return false;
    const std::string name = attempt == nullptr ? "no count" : attempt;
    check(code == expected, name + ": exited with " + std::to_string(code));
    check(lines(directory / "log.txt") == log, name + ": wrong process sequence");
    return true;
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string_view(argv[1]) == "child") return child(argv[2], std::atoi(argv[3]));
        check(argc == 2, "usage: agc_driver_frame_replay_relaunch_tests <directory>");
        const std::filesystem::path directory = argv[1];
        const std::vector<std::string> full{"none", "1", "2", "3", "4", "5", "6", "7", "8", "continued"};
        if (!expectRun(directory, nullptr, 0, 0, full)) {
            std::puts("skipped: the stack is not an arena host region");
            return Skip;
        }
        check(expectRun(directory, nullptr, 42, 42, full), "the failing run skipped");
        check(expectRun(directory, "7", 0, 0, {"7", "8", "continued"}), "the last relaunch skipped");
        check(expectRun(directory, "8", 5, 5, {"8", "continued"}), "the exhausted count skipped");
        for (const char* const malformed : {"-1", "-2147483648", "9", "99999999999", "abc", "1x", " 1", "+1"}) check(expectRun(directory, malformed, 0, 2, {malformed}), "a malformed count skipped");
        std::puts("frame replay relaunch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        return argc == 4 ? 2 : 1;
    }
}
