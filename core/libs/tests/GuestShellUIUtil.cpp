#include "prx/libSceShellUIUtil/libSceShellUIUtil.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int NotInitialized = static_cast<int>(0x80BA0001u);
constexpr int InvalidArgument = static_cast<int>(0x80BA0020u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ShellUIUtil: %s\n", message);
    std::abort();
}

const ShellUIUtilLaunchParameter* LaunchParameter(const std::uint32_t* words) {
    return reinterpret_cast<const ShellUIUtilLaunchParameter*>(words);
}

void CheckArguments() {
    const std::string longText(0x1000, 'a');
    const std::string longestText(0xFFF, 'a');
    char output[16];
    std::uint32_t size = 0;

    Require(sceShellUIUtilGetAppUrl(nullptr, "p", output, 16, &size) == InvalidArgument, "GetAppUrl: null application");
    Require(sceShellUIUtilGetAppUrl("a", nullptr, output, 16, &size) == InvalidArgument, "GetAppUrl: null parameter");
    Require(sceShellUIUtilGetAppUrl(longText.c_str(), "p", output, 16, &size) == InvalidArgument, "GetAppUrl: long application");
    Require(sceShellUIUtilGetAppUrl("a", longText.c_str(), output, 16, &size) == InvalidArgument, "GetAppUrl: long parameter");
    Require(sceShellUIUtilGetAppUrl("a", "p", output, 0x1001, &size) == InvalidArgument, "GetAppUrl: capacity");
    Require(sceShellUIUtilGetAppUrl("a", "p", nullptr, 16, &size) == InvalidArgument, "GetAppUrl: null output");
    Require(sceShellUIUtilGetAppUrl(longestText.c_str(), longestText.c_str(), nullptr, 0, nullptr) == NotInitialized,
        "GetAppUrl: the longest strings, no output, before Initialize");
    Require(sceShellUIUtilGetAppUrl("a", "p", output, 0x1000, nullptr) == NotInitialized, "GetAppUrl: capacity 0x1000");

    const std::uint32_t good[2] = {8, 0};
    for (const std::uint32_t bad : {0u, 7u, 9u, 16u}) {
        const std::uint32_t words[2] = {bad, 0};
        Require(sceShellUIUtilLaunchByUri("u", LaunchParameter(words)) == InvalidArgument, "LaunchByUri: parameter size");
    }
    Require(sceShellUIUtilLaunchByUri(nullptr, LaunchParameter(good)) == InvalidArgument, "LaunchByUri: null uri");
    Require(sceShellUIUtilLaunchByUri("u", nullptr) == InvalidArgument, "LaunchByUri: null parameter");
    errno = 71;
    Require(sceShellUIUtilLaunchByUri("u", reinterpret_cast<const ShellUIUtilLaunchParameter*>(0x10)) == InvalidArgument, "LaunchByUri: unreadable parameter");
    Require(errno == 71, "LaunchByUri: errno preserved");
    Require(sceShellUIUtilLaunchByUri(longText.c_str(), LaunchParameter(good)) == InvalidArgument, "LaunchByUri: long uri");
    Require(sceShellUIUtilLaunchByUri(longestText.c_str(), LaunchParameter(good)) == NotInitialized, "LaunchByUri before Initialize");

    std::vector<char> taskOutput(0x1000, 'x');
    Require(sceShellUIUtilLaunchTask(1, nullptr, taskOutput.data(), 0x1000) == InvalidArgument, "LaunchTask: null uri");
    Require(sceShellUIUtilLaunchTask(1, "u", taskOutput.data(), 0xFFF) == InvalidArgument, "LaunchTask: capacity");
    Require(sceShellUIUtilLaunchTask(1, "u", nullptr, 0x1000) == InvalidArgument, "LaunchTask: null output");
    errno = 71;
    Require(sceShellUIUtilLaunchTask(1, "u", reinterpret_cast<char*>(0x10), 0x1000) == InvalidArgument, "LaunchTask: unwritable output");
    Require(errno == 71, "LaunchTask: errno preserved");
    Require(sceShellUIUtilLaunchTask(1, longText.c_str(), taskOutput.data(), 0x1000) == InvalidArgument, "LaunchTask: long uri");
    for (const char byte : taskOutput) Require(byte == 'x', "LaunchTask: a rejected call leaves the output");
    Require(sceShellUIUtilLaunchTask(1, "u", taskOutput.data(), 0x1000) == NotInitialized, "LaunchTask before Initialize");
    for (const char byte : taskOutput) Require(byte == 0, "LaunchTask clears the output before the service call");

    Require(sceShellUIUtilAbortTask(1) == NotInitialized, "AbortTask before Initialize");
}

void CheckState() {
    Require(sceShellUIUtilTerminate() == NotInitialized, "Terminate before Initialize");
    const int first = sceShellUIUtilCreateTaskToken();
    Require(first >= 1, "CreateTaskToken starts at 1");
    Require(sceShellUIUtilCreateTaskToken() == first + 1, "CreateTaskToken counts up");
    Require(sceShellUIUtilInitialize() == 0, "Initialize");
    Require(sceShellUIUtilInitialize() == 0, "Initialize twice");
    Require(sceShellUIUtilCreateTaskToken() == first + 2, "CreateTaskToken does not depend on Initialize");
    Require(sceShellUIUtilTerminate() == 0, "Terminate");
    Require(sceShellUIUtilTerminate() == NotInitialized, "Terminate twice");
    Require(sceShellUIUtilAbortTask(1) == NotInitialized, "AbortTask after Terminate");
}

}

int main() {
    Require(sceShellUIUtilCreateTaskToken() == 1, "the first token is 1");
    CheckArguments();
    CheckState();
    std::puts("ShellUIUtil tests passed");
    return 0;
}
