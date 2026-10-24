#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cstring>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <vector>
#endif
extern "C" int APS5_VABI sceSystemServiceLoadExec(const char*, const char* const*);
extern "C" int APS5_VABI __cxa_atexit_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dsoHandle);
extern "C" void APS5_VABI _Exit_nid_postfix(int);
extern "C" void APS5_VABI exit_nid_postfix(int);
extern "C" void APS5_VABI catchReturnFromMain_nid_postfix(int);
using GuestQuickExitCallback = void (APS5_VABI*)();
extern "C" int APS5_VABI at_quick_exit_nid_postfix(GuestQuickExitCallback);
extern "C" void APS5_VABI quick_exit_nid_postfix(int);
namespace {
std::atomic<bool> awaiting{false};
void AwaitExit() {
    awaiting = true;
    awaiting.notify_one();
    try {
        LibcAwaitExit_nid_postfix();
    } catch (const ProcessShutdown&) {
        std::puts("Guest thread resumed during exit");
        std::fflush(stdout);
        std::_Exit(6);
    }
}
void Cleanup() {
    std::thread(AwaitExit).detach();
    awaiting.wait(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::puts("Guest shutdown completed");
    std::fflush(stdout);
}
void ExitHandlerRan() {
    std::puts("Guest exit handler ran");
    std::fflush(stdout);
    std::_Exit(5);
}
void APS5_VABI UnexpectedFinalizer(void*) { ExitHandlerRan(); }
int quickOrder = 0;
void APS5_VABI QuickSecond() { quickOrder = 1; }
void APS5_VABI QuickFirst() {
    if (quickOrder != 1) std::_Exit(1);
    std::puts("Guest quick_exit handlers completed");
    std::fflush(stdout);
}
void Require(bool value) { if (!value) std::abort(); }
void UnexpectedCleanup() { std::_Exit(3); }
#ifdef _WIN32
void WINAPI UnexpectedDetach(PVOID) { TerminateProcess(GetCurrentProcess(), 4); }
#endif
constexpr char RelaunchMarker[] = "ANYPS5_TEST_RELAUNCH_MARKER";
constexpr char MainExecutable[] = "ANYPS5_TEST_MAIN_EXECUTABLE";
void SetVariable(const char* name, const std::filesystem::path& value) {
#ifdef _WIN32
    const std::wstring wideName = std::filesystem::path(name).wstring();
    Require(SetEnvironmentVariableW(wideName.c_str(), value.c_str()) != 0);
    Require(_wputenv_s(wideName.c_str(), value.c_str()) == 0);
#else
    Require(setenv(name, value.c_str(), 1) == 0);
#endif
}
std::filesystem::path PathVariable(const char* name) {
#ifdef _WIN32
    const wchar_t* value = _wgetenv(std::filesystem::path(name).wstring().c_str());
#else
    const char* value = std::getenv(name);
#endif
    Require(value != nullptr);
    return value;
}
std::filesystem::path OwnExecutable() {
#ifdef _WIN32
    std::wstring executable(32768, L'\0');
    executable.resize(GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size())));
    return executable;
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    Require(_NSGetExecutablePath(executable.data(), &size) == 0);
    return std::filesystem::canonical(executable.data());
#else
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}
void AppendMarker(const std::string& line) {
    std::ofstream(PathVariable(RelaunchMarker), std::ios::app) << line << '\n';
}
void MainShutdown() {
    AppendMarker("shutdown");
    std::thread(AwaitExit).detach();
    awaiting.wait(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
void SecondaryShutdown() { AppendMarker("secondary shutdown"); }
void APS5_VABI RemainingHandler(void*) { AppendMarker("remaining handler"); }
void APS5_VABI ExitingHandler(void*) { exit_nid_postfix(7); }
void GuestExitShutdown() { AppendMarker("shutdown"); }
void DelayedExitDrain() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    AppendMarker("drain");
}
void ExitFlushAfterDrain() { AppendMarker("flush"); }
void ThrowingExitDrain() { throw std::runtime_error("exit drain test failure"); }
void BlockedExitDrain() {
    for (;;) std::this_thread::sleep_for(std::chrono::seconds(1));
}
void APS5_VABI CheckShutdownStillRunning(void*) {
    if (LibcShutdownToken_nid_postfix().stop_requested()) std::_Exit(8);
    AppendMarker("guest finalizer");
}
int RunChild(const std::filesystem::path& self, const char* mode) {
#ifdef _WIN32
    std::wstring commandLine = L"\"" + self.wstring() + L"\" " + std::wstring(mode, mode + std::strlen(mode));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    Require(CreateProcessW(self.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process) != 0);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
#else
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        execl(self.c_str(), self.c_str(), mode, static_cast<char*>(nullptr));
        std::_Exit(127);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}
int DriveNestedExit() {
    const auto self = OwnExecutable();
    const auto marker = std::filesystem::temp_directory_path() / ("anyps5-nested-exit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    SetVariable(RelaunchMarker, marker);
    const int code = RunChild(self, "--nested-exit");
    std::string text;
    {
        std::ifstream file(marker);
        text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    Require(code == 7);
    Require(text == "guest finalizer\nremaining handler\nshutdown\n");
    std::puts("Guest nested exit completed");
    return 0;
}
int DriveExitOrder() {
    const auto self = OwnExecutable();
    const auto marker = std::filesystem::temp_directory_path() / ("anyps5-exit-order-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    SetVariable(RelaunchMarker, marker);
    const int code = RunChild(self, "--exit-order");
    std::string text;
    {
        std::ifstream file(marker);
        text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    Require(code == 0);
    Require(text == "guest finalizer\nshutdown\n");
    std::puts("Guest finalizers ran before shutdown");
    return 0;
}
int DriveRelaunch() {
    const auto self = OwnExecutable();
    const auto secondary = self.parent_path() / ("picker.picker.bin" + self.extension().string());
    std::filesystem::copy_file(self, secondary, std::filesystem::copy_options::overwrite_existing);
    const auto marker = std::filesystem::temp_directory_path() / ("anyps5-relaunch-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    SetVariable(RelaunchMarker, marker);
    SetVariable(MainExecutable, self);
    Require(RunChild(self, "--relaunch") == 0);
    std::string text;
    for (int attempt = 0; attempt < 200 && text.find("relaunched") == std::string::npos; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::ifstream file(marker);
        text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    for (int attempt = 0; attempt < 100 && std::filesystem::exists(secondary) && !std::filesystem::remove(secondary, ignored); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(text == "shutdown\narguments: --secondary|two words|quote\"d\\|\nsecondary shutdown\nrelaunched\n");
    std::puts("Guest relaunch completed");
    return 0;
}

int DriveExitDrain() {
    const auto self = OwnExecutable();
    const auto marker = std::filesystem::temp_directory_path() / ("anyps5-exit-drain-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    SetVariable(RelaunchMarker, marker.string().c_str());
    Require(RunChild(self, "--exit-drain") == 7);
    std::ifstream file(marker);
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    file.close();
    std::filesystem::remove(marker);
    Require(text == "drain\nflush\n");
    Require(RunChild(self, "--exit-drain-throws") == 7);
    const auto started = std::chrono::steady_clock::now();
    Require(RunChild(self, "--exit-drain-blocked") == 7);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    Require(elapsed >= std::chrono::seconds(8) && elapsed < std::chrono::seconds(12));
    std::puts("Host exit drain completed before immediate termination and stayed bounded");
    return 0;
}
}
int main(int argc, char** argv) {
    const int firstArgument = argc > 0 && std::strncmp(argv[0], "--", 2) == 0 ? 0 : 1;
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--exit-drain-driver") == 0) return DriveExitDrain();
    if (argc > firstArgument && std::strncmp(argv[firstArgument], "--exit-drain", 12) == 0) {
        LibcRegisterShutdown_nid_postfix(UnexpectedCleanup);
        Require(std::atexit(UnexpectedCleanup) == 0);
        if (std::strcmp(argv[firstArgument], "--exit-drain-throws") == 0) LibcSetExitDrain_nid_no_patch(ThrowingExitDrain);
        else if (std::strcmp(argv[firstArgument], "--exit-drain-blocked") == 0) LibcSetExitDrain_nid_no_patch(BlockedExitDrain);
        else {
            LibcSetExitDrain_nid_no_patch(DelayedExitDrain);
            LibcSetEndFlush_nid_no_patch(ExitFlushAfterDrain);
        }
        _Exit_nid_postfix(7);
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--requested-exit-driver") == 0) {
        Require(RunChild(OwnExecutable(), "--requested-exit") == 0);
        return 0;
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--immediate") == 0) {
        LibcRegisterShutdown_nid_postfix(UnexpectedCleanup);
        Require(std::atexit(UnexpectedCleanup) == 0);
#ifdef _WIN32
        auto slot = FlsAlloc(UnexpectedDetach);
        Require(slot != FLS_OUT_OF_INDEXES && FlsSetValue(slot, &slot));
#endif
        _Exit_nid_postfix(0);
        return 2;
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--return-from-main") == 0) {
        catchReturnFromMain_nid_postfix(0);
        return 2;
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--quick-exit") == 0) {
        Require(at_quick_exit_nid_postfix(QuickFirst) == 0);
        Require(at_quick_exit_nid_postfix(QuickSecond) == 0);
        quick_exit_nid_postfix(0);
        return 2;
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--nested-exit-driver") == 0) {
        return DriveNestedExit();
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--nested-exit") == 0) {
        LibcRegisterShutdown_nid_postfix(GuestExitShutdown);
        Require(__cxa_atexit_nid_postfix(RemainingHandler, nullptr, nullptr) == 0);
        Require(__cxa_atexit_nid_postfix(CheckShutdownStillRunning, nullptr, nullptr) == 0);
        Require(__cxa_atexit_nid_postfix(ExitingHandler, nullptr, nullptr) == 0);
        exit_nid_postfix(0);
        return 2;
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--exit-order-driver") == 0) {
        return DriveExitOrder();
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--exit-order") == 0) {
        LibcRegisterShutdown_nid_postfix(GuestExitShutdown);
        Require(__cxa_atexit_nid_postfix(CheckShutdownStillRunning, nullptr, nullptr) == 0);
        exit_nid_postfix(0);
        return 2;
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--relaunch-driver") == 0) {
        return DriveRelaunch();
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--relaunch") == 0) {
        LibcRegisterShutdown_nid_postfix(MainShutdown);
        const char* const arguments[] = {"--secondary", "two words", "quote\"d\\", "", nullptr};
        sceSystemServiceLoadExec("/app0/picker/picker.bin", arguments);
        return 2;
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--secondary") == 0) {
        Require(OwnExecutable().filename().string().starts_with("picker.picker.bin"));
        std::string received = "arguments:";
        for (int index = firstArgument; index < argc; ++index) received += std::string(index == firstArgument ? " " : "|") + argv[index];
        AppendMarker(received);
        LibcRegisterShutdown_nid_postfix(SecondaryShutdown);
        const char* const arguments[] = {"--relaunched", nullptr};
        sceSystemServiceLoadExec("/app0/eboot.bin", arguments);
        return 2;
    }
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--relaunched") == 0) {
        Require(std::filesystem::equivalent(OwnExecutable(), PathVariable(MainExecutable)));
        AppendMarker("relaunched");
        return 0;
    }
    if (argc > firstArgument) {
        LibcRegisterShutdown_nid_postfix(Cleanup);
        Require(__cxa_atexit_nid_postfix(UnexpectedFinalizer, nullptr, nullptr) == 0);
        Require(std::atexit(ExitHandlerRan) == 0);
        if (std::strcmp(argv[firstArgument], "--requested-exit") == 0) {
            LibcRequestExit_nid_postfix(0);
            LibcRequestExit_nid_postfix(7);
            LibcAwaitExit_nid_postfix();
        }
        sceSystemServiceLoadExec("exit", nullptr);
        return 2;
    }
    Require(sceSystemServiceLoadExec(nullptr, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceLoadExec("", nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    for (const char* path : {"/app0/another.bin", "/data/eboot.bin", "/app0/../eboot.bin", "/app0//eboot.bin"}) {
        bool rejected = false;
        try { sceSystemServiceLoadExec(path, nullptr); }
        catch (const std::runtime_error& error) { rejected = std::string(error.what()).find(path) != std::string::npos; }
        Require(rejected);
    }
#ifdef _WIN32
    Require(GetEnvironmentVariableA("APS5_EBOOT", nullptr, 0) == 0);
#else
    Require(std::getenv("APS5_EBOOT") == nullptr);
#endif
}
