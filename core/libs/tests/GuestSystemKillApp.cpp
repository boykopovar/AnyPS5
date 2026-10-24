#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
extern "C" int APS5_VABI sceSystemServiceGetAppIdOfRunningBigApp(void);
extern "C" int APS5_VABI sceSystemServiceKillApp(int, int, int, int);
extern "C" int APS5_VABI sceLncUtilKillApp(unsigned);
extern "C" int APS5_VABI sceLncUtilKillAppWithReason(unsigned, int);
namespace {
bool cleaned = false;
bool finished = false;
void Cleanup() {
    try { LibcAwaitExit_nid_postfix(); }
    catch (const ProcessShutdown&) { return; }
    catch (const std::runtime_error&) { cleaned = true; }
}
void VerifyExit() {
    if (!cleaned) std::_Exit(1);
    std::puts("Guest shutdown and atexit completed");
}
void UnexpectedShutdown() {
    if (!finished) std::_Exit(3);
}
void Require(bool value) { if (!value) std::abort(); }
bool Rejects(int appId, int how, int reason, int coreDump, const char* expected) {
    try { sceSystemServiceKillApp(appId, how, reason, coreDump); }
    catch (const std::runtime_error& error) { return std::strstr(error.what(), expected) != nullptr; }
    return false;
}
template <typename TCall>
bool RejectsCall(TCall call, const char* expected) {
    try { call(); }
    catch (const std::runtime_error& error) { return std::strstr(error.what(), expected) != nullptr; }
    return false;
}
}
int main(int argc, char** argv) {
    const int appId = sceSystemServiceGetAppIdOfRunningBigApp();
    Require(appId > 0 && sceSystemServiceGetAppIdOfRunningBigApp() == appId);
    if (argc > 1 && std::strcmp(argv[1], "--kill") == 0) {
        LibcRegisterShutdown_nid_postfix(Cleanup);
        Require(std::atexit(VerifyExit) == 0);
        sceSystemServiceKillApp(appId, -1, 0, 0);
        std::_Exit(4);
    }
    if (argc > 1 && (std::strcmp(argv[1], "--lnc-kill") == 0 || std::strcmp(argv[1], "--lnc-kill-reason") == 0)) {
        LibcRegisterShutdown_nid_postfix(Cleanup);
        Require(std::atexit(VerifyExit) == 0);
        if (std::strcmp(argv[1], "--lnc-kill") == 0) sceLncUtilKillApp(static_cast<unsigned>(appId));
        else sceLncUtilKillAppWithReason(static_cast<unsigned>(appId), 0);
        std::_Exit(4);
    }
    LibcRegisterShutdown_nid_postfix(UnexpectedShutdown);
    Require(std::atexit(UnexpectedShutdown) == 0);
    constexpr char otherApp[] = "sceSystemServiceKillApp: application other than the running title";
    constexpr char otherArguments[] = "sceSystemServiceKillApp: arguments other than -1, 0 and 0";
    Require(Rejects(appId + 1, -1, 0, 0, otherApp));
    Require(Rejects(-1, -1, 0, 0, otherApp));
    Require(Rejects(0, -1, 0, 0, otherApp));
    Require(Rejects(appId, 0, 0, 0, otherArguments));
    Require(Rejects(appId, -2, 0, 0, otherArguments));
    Require(Rejects(appId, -1, 1, 0, otherArguments));
    Require(Rejects(appId, -1, 0, 1, otherArguments));
    constexpr char lncOtherApp[] = "sceLncUtilKillApp: application other than the running title";
    constexpr char reasonOtherApp[] = "sceLncUtilKillAppWithReason: application other than the running title";
    constexpr char otherReason[] = "sceLncUtilKillAppWithReason: a reason other than 0";
    const auto running = static_cast<unsigned>(appId);
    for (const unsigned other : {running + 1u, 0u, 0xffffffffu, running | 0x80000000u}) {
        Require(RejectsCall([other] { sceLncUtilKillApp(other); }, lncOtherApp));
        Require(RejectsCall([other] { sceLncUtilKillAppWithReason(other, 0); }, reasonOtherApp));
        Require(RejectsCall([other] { sceLncUtilKillAppWithReason(other, 1); }, reasonOtherApp));
    }
    for (const int reason : {1, -1, 2, 0x7fffffff}) {
        Require(RejectsCall([running, reason] { sceLncUtilKillAppWithReason(running, reason); }, otherReason));
    }
    finished = true;
}
