#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#ifdef __linux__
#include <chrono>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <unistd.h>
#endif

namespace {

void Require(bool value) { if (!value) std::abort(); }

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

#ifdef __linux__
void TestHostBrowserLauncherResetsSignalMask() {
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path();
    const std::string suffix = std::to_string(::getpid());
    const std::filesystem::path scriptPath = tempDir / ("anyps5_sigmask_test_" + suffix + ".sh");
    const std::filesystem::path tempOutputPath = tempDir / ("anyps5_sigmask_test_" + suffix + ".tmp");
    const std::filesystem::path outputPath = tempDir / ("anyps5_sigmask_test_" + suffix + ".out");
    std::error_code ec;
    std::filesystem::remove(scriptPath, ec);
    std::filesystem::remove(tempOutputPath, ec);
    std::filesystem::remove(outputPath, ec);
    {
        std::ofstream script(scriptPath, std::ios::binary);
        Require(script.is_open());
        script << "#!/bin/sh\n"
               << "while IFS= read -r line; do\n"
               << "  case \"$line\" in\n"
               << "    SigBlk:*) printf '%s\\n' \"$line\" > '" << tempOutputPath.string()
               << "' && mv '" << tempOutputPath.string() << "' '" << outputPath.string() << "'; break ;;\n"
               << "  esac\n"
               << "done < /proc/self/status\n";
        Require(script.good());
    }
    std::filesystem::permissions(
        scriptPath,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);
    sigset_t blockSet;
    Require(sigemptyset(&blockSet) == 0);
    Require(sigaddset(&blockSet, SIGUSR1) == 0);
    sigset_t oldSet;
    Require(sigprocmask(SIG_BLOCK, &blockSet, &oldSet) == 0);
    HostBrowserLauncher launcher(scriptPath.string());
    const bool launched = launcher.Launch("https://example.com");
    const int restoreRc = sigprocmask(SIG_SETMASK, &oldSet, nullptr);
    Require(restoreRc == 0);
    Require(launched);
    std::string line;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream input(outputPath);
        if (input.is_open() && std::getline(input, line) && !line.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::filesystem::remove(scriptPath, ec);
    std::filesystem::remove(tempOutputPath, ec);
    std::filesystem::remove(outputPath, ec);
    Require(line.rfind("SigBlk:", 0) == 0);
    const std::size_t valueStart = line.find_first_not_of(" \t", 7);
    Require(valueStart != std::string::npos);
    const std::size_t valueEnd = line.find_last_not_of(" \t\r\n");
    Require(valueEnd != std::string::npos && valueEnd >= valueStart);
    const std::string hexMask = line.substr(valueStart, valueEnd - valueStart + 1);
    Require(!hexMask.empty());
    for (const char ch : hexMask) {
        Require(ch == '0');
    }
}
#endif

}

int main() {
    Require(!sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    Require(sceShellCoreUtilAccessibilityZoomLock() == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    Require(sceShellCoreUtilAccessibilityZoomLock() == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    Require(sceShellCoreUtilAccessibilityZoomUnlock() == SYSTEM_SERVICE_OK);
    Require(!sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    Require(sceShellCoreUtilAccessibilityZoomUnlock() == SYSTEM_SERVICE_OK);
    Require(!sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    const std::int32_t others[] = {SYSTEM_SERVICE_RUNNING_APP_ID + 1, SYSTEM_SERVICE_RUNNING_APP_ID - 1, 0, -1,
                                   std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()};
    for (const std::int32_t appId : others) {
        Require(Throws([&] { static_cast<void>(sceShellCoreUtilIsAccessibilityZoomLocked(appId)); }));
    }
    Require(sceShellCoreUtilAccessibilityZoomLock() == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
#ifdef __linux__
    TestHostBrowserLauncherResetsSignalMask();
#endif
}
