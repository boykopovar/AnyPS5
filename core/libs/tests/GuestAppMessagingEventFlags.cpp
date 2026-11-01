#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstdint>
#include <cstdlib>

namespace {
void Require(bool value) { if (!value) std::abort(); }
}

int main() {
    constexpr int missingFlag = static_cast<int>(0x80c50007);
    constexpr std::uint64_t high = std::uint64_t{1} << 63;
    Require(sceAppMessagingSetEventFlag(0x7142, 1) == missingFlag);
    Require(sceAppMessagingClearEventFlag(0x7142, 1) == missingFlag);
    KernelEventFlag eventFlag = nullptr;
    Require(sceKernelCreateEventFlag(&eventFlag, "SceAppMessagingffffffff", 0, 0, nullptr) == 0);
    Require(sceAppMessagingSetEventFlag(-1, high | 1) == 0);
    std::uint64_t pattern{};
    Require(sceKernelPollEventFlag(eventFlag, high | 1, 1, &pattern) == 0);
    Require(pattern == (high | 1));
    Require(sceAppMessagingClearEventFlag(-1, high) == 0);
    Require(sceKernelPollEventFlag(eventFlag, 1, 1, &pattern) == 0 && pattern == 1);
    Require(sceKernelPollEventFlag(eventFlag, high, 1, &pattern) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceAppMessagingSetEventFlag(-1, 0) == 0);
    Require(sceAppMessagingClearEventFlag(-1, 0) == 0);
    Require(sceKernelPollEventFlag(eventFlag, 1, 1, &pattern) == 0 && pattern == 1);
    Require(sceKernelDeleteEventFlag(eventFlag) == 0);
    Require(sceAppMessagingSetEventFlag(-1, 1) == missingFlag);
}
