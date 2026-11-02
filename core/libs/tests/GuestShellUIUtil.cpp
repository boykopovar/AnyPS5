#include "prx/libSceShellUIUtil/include/Api.hpp"
#include <cstdint>
#include <cstdlib>

struct ShellUIUtilLaunchParameter {
    std::uint32_t size;
    std::uint32_t flags;
};

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

constexpr int kErrInvalidArgument = static_cast<int>(0x80B8000Au);

}

#ifndef _WIN32
extern "C" [[noreturn, gnu::weak]] void NotImplemented_nid_no_patch(const char*) {
    std::abort();
}
#endif

int main() {
    ShellUIUtilLaunchParameter validParam{sizeof(ShellUIUtilLaunchParameter), 0};
    ShellUIUtilLaunchParameter undersizedParam{sizeof(ShellUIUtilLaunchParameter) - 1, 0};
    ShellUIUtilLaunchParameter zeroSizeParam{0, 0};

    Require(sceShellUIUtilLaunchByUri(nullptr, &validParam) == kErrInvalidArgument);
    Require(sceShellUIUtilLaunchByUri("ps5:uri", nullptr) == kErrInvalidArgument);
    Require(sceShellUIUtilLaunchByUri(nullptr, nullptr) == kErrInvalidArgument);
    Require(sceShellUIUtilLaunchByUri("ps5:uri", &undersizedParam) == kErrInvalidArgument);
    Require(sceShellUIUtilLaunchByUri("ps5:uri", &zeroSizeParam) == kErrInvalidArgument);

    char output[64] = {};
    std::uint32_t outputSize = 0;

    Require(sceShellUIUtilGetAppUrl(nullptr, "param", output, sizeof(output), &outputSize) == kErrInvalidArgument);
    Require(sceShellUIUtilGetAppUrl("app", "param", nullptr, sizeof(output), &outputSize) == kErrInvalidArgument);
    Require(sceShellUIUtilGetAppUrl("app", "param", output, 0, &outputSize) == kErrInvalidArgument);
    Require(sceShellUIUtilGetAppUrl(nullptr, nullptr, nullptr, 0, nullptr) == kErrInvalidArgument);

    Require(sceShellUIUtilLaunchTask(-1, "ps5:uri", output, sizeof(output)) == kErrInvalidArgument);
    Require(sceShellUIUtilLaunchTask(0, nullptr, output, sizeof(output)) == kErrInvalidArgument);
    Require(sceShellUIUtilLaunchTask(-1, nullptr, nullptr, 0) == kErrInvalidArgument);
}
