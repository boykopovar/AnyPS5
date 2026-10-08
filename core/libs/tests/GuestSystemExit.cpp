#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <csignal>
#include <pthread.h>
#include <unistd.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
extern "C" int APS5_VABI sceSystemServiceLoadExec(const char*, const char* const*);
extern "C" void APS5_VABI _Exit_nid_postfix(int);
extern "C" void APS5_VABI catchReturnFromMain_nid_postfix(int);
using GuestQuickExitCallback = void (APS5_VABI*)();
extern "C" int APS5_VABI at_quick_exit_nid_postfix(GuestQuickExitCallback);
extern "C" void APS5_VABI quick_exit_nid_postfix(int);
namespace {
bool cleaned = false;
void Cleanup() { cleaned = true; }
void VerifyExit() {
    if (!cleaned) std::_Exit(1);
    std::puts("Guest shutdown and atexit completed");
}
int quickOrder = 0;
void APS5_VABI QuickSecond() { quickOrder = 1; }
void APS5_VABI QuickFirst() {
    if (quickOrder != 1) std::_Exit(1);
    std::puts("Guest quick_exit handlers completed");
    std::fflush(stdout);
}
void Require(bool value) { if (!value) std::abort(); }
void UnexpectedCleanup() { std::_Exit(3); }
void ReportShutdown() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::puts("Guest shut down before the restart");
    std::fflush(stdout);
}
const std::vector<std::string> restartArguments{"--load-exec-restarted", "two words", "quote\"d", "trailing\\", "back\\\\\"slash", "\xc3\xbc", ""};
std::uint64_t ProcessId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}
std::vector<std::string> Arguments(int argc, char** argv) {
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int count = 0;
    const auto wide = CommandLineToArgvW(GetCommandLineW(), &count);
    Require(wide != nullptr);
    std::vector<std::string> result;
    for (int index = 0; index < count; ++index) {
        const auto size = WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, nullptr, 0, nullptr, nullptr);
        Require(size > 0);
        std::string text(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, text.data(), size, nullptr, nullptr);
        text.pop_back();
        result.push_back(text);
    }
    LocalFree(wide);
    return result;
#else
    return {argv, argv + argc};
#endif
}
void ChangeSignals() {
#ifndef _WIN32
    struct sigaction ignore {};
    ignore.sa_handler = SIG_IGN;
    Require(sigaction(SIGPIPE, &ignore, nullptr) == 0);
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);
    Require(pthread_sigmask(SIG_BLOCK, &blocked, nullptr) == 0);
#endif
}
bool SignalsReset() {
#ifdef _WIN32
    return true;
#else
    struct sigaction action {};
    if (sigaction(SIGPIPE, nullptr, &action) != 0 || action.sa_handler != SIG_DFL) return false;
    sigset_t blocked;
    if (pthread_sigmask(SIG_BLOCK, nullptr, &blocked) != 0) return false;
    return sigismember(&blocked, SIGUSR1) == 0;
#endif
}
bool PreviousProcessEnded(std::uint64_t id) {
#ifdef _WIN32
    if (GetEnvironmentVariableW(L"ANYPS5_LOAD_EXEC_PREVIOUS_PROCESS", nullptr, 0) != 0) return false;
    const auto process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(id));
    if (process == nullptr) return true;
    const auto ended = WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
    CloseHandle(process);
    return ended;
#else
    return id == ProcessId();
#endif
}
}
int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--load-exec") == 0) {
        LibcRegisterShutdown_nid_postfix(ReportShutdown);
        ChangeSignals();
        const auto previous = std::to_string(ProcessId());
        std::vector<const char*> arguments;
        for (const auto& argument : restartArguments) arguments.push_back(argument.c_str());
        arguments.push_back(previous.c_str());
        arguments.push_back(nullptr);
        sceSystemServiceLoadExec("/app0/eboot.bin", arguments.data());
        return 2;
    }
    if (argc > 1 && std::strcmp(argv[1], "--load-exec-restarted") == 0) {
        const auto received = Arguments(argc, argv);
        Require(received.size() == restartArguments.size() + 2);
        Require(std::vector<std::string>(received.begin() + 1, received.end() - 1) == restartArguments);
        Require(PreviousProcessEnded(std::stoull(received.back())));
        Require(SignalsReset());
        std::puts("Guest restarted with its arguments");
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "--immediate") == 0) {
        LibcRegisterShutdown_nid_postfix(UnexpectedCleanup);
        Require(std::atexit(UnexpectedCleanup) == 0);
        _Exit_nid_postfix(0);
        return 2;
    }
    if (argc > 1 && std::strcmp(argv[1], "--return-from-main") == 0) {
        catchReturnFromMain_nid_postfix(0);
        return 2;
    }
    if (argc > 1 && std::strcmp(argv[1], "--quick-exit") == 0) {
        Require(at_quick_exit_nid_postfix(QuickFirst) == 0);
        Require(at_quick_exit_nid_postfix(QuickSecond) == 0);
        quick_exit_nid_postfix(0);
        return 2;
    }
    if (argc > 1) {
        LibcRegisterShutdown_nid_postfix(Cleanup);
        Require(std::atexit(VerifyExit) == 0);
        sceSystemServiceLoadExec("exit", nullptr);
        return 2;
    }
    Require(sceSystemServiceLoadExec(nullptr, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceLoadExec("", nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    bool rejected = false;
    try { sceSystemServiceLoadExec("/app0/another.bin", nullptr); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
#ifdef _WIN32
    const char* const invalid[] = {"\xff", nullptr};
    bool invalidRejected = false;
    try { sceSystemServiceLoadExec("/app0/eboot.bin", invalid); }
    catch (const std::invalid_argument&) { invalidRejected = true; }
    Require(invalidRejected);
#endif
}
