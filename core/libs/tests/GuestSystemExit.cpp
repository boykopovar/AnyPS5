#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif
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
#ifdef _WIN32
void WINAPI UnexpectedDetach(PVOID) { TerminateProcess(GetCurrentProcess(), 4); }
#endif
}
int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--immediate") == 0) {
        LibcRegisterShutdown_nid_postfix(UnexpectedCleanup);
        Require(std::atexit(UnexpectedCleanup) == 0);
#ifdef _WIN32
        auto slot = FlsAlloc(UnexpectedDetach);
        Require(slot != FLS_OUT_OF_INDEXES && FlsSetValue(slot, &slot));
#endif
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
}
