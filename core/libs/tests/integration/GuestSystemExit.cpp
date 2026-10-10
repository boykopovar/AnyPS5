#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceSystemService/SystemService.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceSystemServiceLoadExec(const char*, const char* const*);
extern "C" void APS5_VABI _Exit_nid_postfix(int);
extern "C" void APS5_VABI catchReturnFromMain_nid_postfix(int);
using GuestQuickExitCallback = void (APS5_VABI*)();
extern "C" int APS5_VABI at_quick_exit_nid_postfix(GuestQuickExitCallback);
extern "C" void APS5_VABI quick_exit_nid_postfix(int);

namespace {

using Testing::Case;
using Testing::RequireEqual;

bool cleaned = false;
int quickOrder = 0;

void Cleanup() {
    cleaned = true;
}

void VerifyExit() {
    if (!cleaned) std::_Exit(1);
    std::puts("Guest shutdown and atexit completed");
}

void APS5_VABI QuickSecond() {
    quickOrder = 1;
}

void APS5_VABI QuickFirst() {
    if (quickOrder != 1) std::_Exit(1);
    std::puts("Guest quick_exit handlers completed");
    std::fflush(stdout);
}

void UnexpectedCleanup() {
    std::_Exit(3);
}

void RequireMode(const std::string& mode) {
    const auto& arguments = Testing::Arguments();
    if (std::find(arguments.begin(), arguments.end(), mode) == arguments.end()) {
        Testing::Skip("ends the process, runs only when the " + mode + " argument is given");
    }
    std::fflush(stdout);
}

const Case nullPath{"LoadExec_NullOrEmptyPath_FailsParameter", [] {
    RequireEqual(sceSystemServiceLoadExec(nullptr, nullptr), SYSTEM_SERVICE_ERROR_PARAMETER, "null path");
    RequireEqual(sceSystemServiceLoadExec("", nullptr), SYSTEM_SERVICE_ERROR_PARAMETER, "empty path");
}};

const Case otherProgram{"LoadExec_OtherProgram_Throws", [] {
    Testing::RequireThrows<std::runtime_error>([] { sceSystemServiceLoadExec("/app0/another.bin", nullptr); }, "another program");
}};

const Case loadExecExit{"LoadExec_Exit_RunsShutdownAndAtexitHandlers", [] {
    RequireMode("exit");
    LibcRegisterShutdown_nid_postfix(Cleanup);
    RequireEqual(std::atexit(VerifyExit), 0, "register the atexit check");
    sceSystemServiceLoadExec("exit", nullptr);
    Testing::Fail("LoadExec(\"exit\") returned");
}};

const Case quickExit{"QuickExit_TwoHandlers_RunInReverseOrder", [] {
    RequireMode("quick-exit");
    RequireEqual(at_quick_exit_nid_postfix(QuickFirst), 0, "register the first handler");
    RequireEqual(at_quick_exit_nid_postfix(QuickSecond), 0, "register the second handler");
    quick_exit_nid_postfix(0);
    Testing::Fail("quick_exit returned");
}};

const Case immediateExit{"Exit_Immediate_SkipsShutdownAndAtexitHandlers", [] {
    RequireMode("immediate");
    LibcRegisterShutdown_nid_postfix(UnexpectedCleanup);
    RequireEqual(std::atexit(UnexpectedCleanup), 0, "register the unexpected atexit handler");
    _Exit_nid_postfix(0);
    Testing::Fail("_Exit returned");
}};

const Case returnFromMain{"CatchReturnFromMain_ZeroStatus_EndsProcessSuccessfully", [] {
    RequireMode("return-from-main");
    catchReturnFromMain_nid_postfix(0);
    Testing::Fail("catchReturnFromMain returned");
}};

} // namespace
