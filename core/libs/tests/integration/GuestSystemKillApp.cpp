#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceSystemServiceGetAppIdOfRunningBigApp(void);
extern "C" int APS5_VABI sceSystemServiceKillApp(int, int, int, int);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

bool cleaned = false;
bool finished = false;

void Cleanup() {
    try {
        LibcAwaitExit_nid_postfix();
    } catch (const ProcessShutdown&) {
        return;
    } catch (const std::runtime_error&) {
        cleaned = true;
    }
}

void VerifyExit() {
    if (!cleaned) std::_Exit(1);
    std::puts("Guest shutdown and atexit completed");
}

void UnexpectedShutdown() {
    if (!finished) std::_Exit(3);
}

int RunningAppId() {
    const int appId = sceSystemServiceGetAppIdOfRunningBigApp();
    Require(appId > 0, "running app id is positive: " + std::to_string(appId));
    return appId;
}

void RequireRejected(int appId, int how, int reason, int coreDump, const char* expected) {
    const auto input = "KillApp(" + std::to_string(appId) + ", " + std::to_string(how) + ", " + std::to_string(reason) + ", " +
                       std::to_string(coreDump) + ")";
    const auto error = Testing::RequireThrows<std::runtime_error>([&] { sceSystemServiceKillApp(appId, how, reason, coreDump); }, input);
    Require(std::strstr(error.what(), expected) != nullptr, input + " message: " + error.what());
}

constexpr char otherApp[] = "sceSystemServiceKillApp: application other than the running title";
constexpr char otherArguments[] = "sceSystemServiceKillApp: arguments other than -1, 0 and 0";

const Case appId{"GetAppIdOfRunningBigApp_CalledTwice_ReturnsSamePositiveId", [] {
    const int first = RunningAppId();
    RequireEqual(sceSystemServiceGetAppIdOfRunningBigApp(), first, "second call");
}};

const Case otherApps{"KillApp_OtherApplication_ThrowsWithoutShutdown", [] {
    const int running = RunningAppId();
    finished = false;
    LibcRegisterShutdown_nid_postfix(UnexpectedShutdown);
    RequireEqual(std::atexit(UnexpectedShutdown), 0, "register the unexpected shutdown check");
    RequireRejected(running + 1, -1, 0, 0, otherApp);
    RequireRejected(-1, -1, 0, 0, otherApp);
    RequireRejected(0, -1, 0, 0, otherApp);
    finished = true;
}};

const Case otherArgumentsCase{"KillApp_UnsupportedArguments_ThrowWithoutShutdown", [] {
    const int running = RunningAppId();
    finished = false;
    LibcRegisterShutdown_nid_postfix(UnexpectedShutdown);
    RequireEqual(std::atexit(UnexpectedShutdown), 0, "register the unexpected shutdown check");
    RequireRejected(running, 0, 0, 0, otherArguments);
    RequireRejected(running, -2, 0, 0, otherArguments);
    RequireRejected(running, -1, 1, 0, otherArguments);
    RequireRejected(running, -1, 0, 1, otherArguments);
    finished = true;
}};

const Case kill{"KillApp_RunningTitle_RunsShutdownAndAtexitHandlers", [] {
    const auto& arguments = Testing::Arguments();
    if (std::find(arguments.begin(), arguments.end(), "kill") == arguments.end()) {
        Testing::Skip("ends the process, runs only when the kill argument is given");
    }
    const int running = RunningAppId();
    LibcRegisterShutdown_nid_postfix(Cleanup);
    RequireEqual(std::atexit(VerifyExit), 0, "register the atexit check");
    std::fflush(stdout);
    sceSystemServiceKillApp(running, -1, 0, 0);
    std::_Exit(4);
}};

} // namespace
