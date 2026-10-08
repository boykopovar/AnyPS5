#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern "C" {
int APS5_VABI sceShareCaptureScreenshotExtended(const void* extended_param, std::int32_t* req_id);
int APS5_VABI sceShareCaptureScreenshot(const void* param, std::int32_t* req_id);
int APS5_VABI sceShareCaptureVideoClip(const void* param, std::int32_t* req_id);
int APS5_VABI sceShareGetCurrentStatus(std::uint32_t feature_flag, void* status);
int APS5_VABI sceShareOpenMenuForContent(const void* content_id);
int APS5_VABI sceShareGetRunningStatus(std::uint32_t* status);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    constexpr std::int32_t notSupported = static_cast<std::int32_t>(0x81960007);
    std::uint8_t param[64]{};

    std::int32_t reqId = 7;
    Require(sceShareCaptureScreenshotExtended(param, &reqId) == notSupported);
    Require(reqId == -1);

    reqId = 7;
    Require(sceShareCaptureScreenshotExtended(nullptr, &reqId) == notSupported);
    Require(reqId == -1);

    Require(sceShareCaptureScreenshotExtended(param, nullptr) == notSupported);
    Require(sceShareCaptureScreenshotExtended(nullptr, nullptr) == notSupported);

    using Capture = int (APS5_VABI*)(const void*, std::int32_t*);
    for (Capture capture : {sceShareCaptureScreenshot, sceShareCaptureVideoClip}) {
        reqId = 7;
        Require(capture(param, &reqId) == notSupported);
        Require(reqId == -1);
        reqId = 7;
        Require(capture(nullptr, &reqId) == notSupported);
        Require(reqId == -1);
        Require(capture(param, nullptr) == notSupported);
    }

    Require(sceShareOpenMenuForContent(param) == notSupported);
    Require(sceShareOpenMenuForContent(nullptr) == notSupported);

    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x81960002);
    std::uint8_t status[18];
    std::memset(status, 0x5a, sizeof(status));
    Require(sceShareGetCurrentStatus(1, status) == 0);
    for (std::size_t index = 0; index < sizeof(status); ++index) Require(status[index] == (index < 16 ? 0 : 0x5a));
    std::memset(status, 0x5a, sizeof(status));
    Require(sceShareGetCurrentStatus(0xffffffffu, status) == 0);
    Require(status[0] == 0 && status[15] == 0 && status[16] == 0x5a);
    std::memset(status, 0x5a, sizeof(status));
    Require(sceShareGetCurrentStatus(0, status) == invalidParam);
    Require(status[0] == 0x5a);
    Require(sceShareGetCurrentStatus(1, nullptr) == invalidParam);
    std::uint32_t running[2]{0xffffffffu, 0x5a5a5a5au};
    Require(sceShareGetRunningStatus(running) == 0);
    Require(running[0] == 0 && running[1] == 0x5a5a5a5au);
    Require(sceShareGetRunningStatus(nullptr) == invalidParam);
    return 0;
}
