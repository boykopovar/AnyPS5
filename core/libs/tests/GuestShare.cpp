#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>

extern "C" {
int APS5_VABI sceShareCaptureScreenshotExtended(const void* extended_param, std::int32_t* req_id);
int APS5_VABI sceShareCaptureScreenshot(const void* param, std::int32_t* req_id);
int APS5_VABI sceShareCaptureVideoClip(const void* param, std::int32_t* req_id);
int APS5_VABI sceShareCaptureVideoClipExtended(const void* extended_param, std::int32_t* req_id);
int APS5_VABI sceShareGetCurrentStatus(std::uint32_t feature_flag, void* status);
int APS5_VABI sceShareOpenMenuForContent(const void* content_id);
int APS5_VABI sceShareGetRunningStatus(std::uint32_t* status);
int APS5_VABI sceShareSetContentParamForApplicationTitle(const char* application_title);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    constexpr std::int32_t notSupported = static_cast<std::int32_t>(0x81960007);
    std::uint8_t param[64]{};

    using Capture = int (APS5_VABI*)(const void*, std::int32_t*);
    std::int32_t reqId = 7;
    constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x81960002);
    for (Capture capture : {sceShareCaptureScreenshotExtended, sceShareCaptureVideoClipExtended}) {
        std::uint64_t fields[5]{};
        const std::uint64_t size = capture == sceShareCaptureScreenshotExtended ? 24 : 40;
        reqId = 7;
        Require(capture(fields, &reqId) == invalidParam && reqId == 7);
        Require(capture(nullptr, &reqId) == invalidParam && reqId == 7);
        fields[0] = size;
        Require(capture(fields, &reqId) == notSupported && reqId == -1);
        Require(capture(fields, nullptr) == notSupported);
        if (size == 40) {
            fields[2] = 1;
            reqId = 7;
            Require(capture(fields, &reqId) == invalidParam && reqId == -1);
            fields[3] = 1;
            Require(capture(fields, nullptr) == notSupported);
            fields[3] |= std::uint64_t{1} << 32;
            Require(capture(fields, nullptr) == invalidParam);
        }
    }

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
    Require(sceShareSetContentParamForApplicationTitle("title") == 0);
    Require(sceShareSetContentParamForApplicationTitle(nullptr) == invalidParam);
    Require(sceShareSetContentParamForApplicationTitle("") == invalidParam);
    std::string title(5119, 'a');
    Require(sceShareSetContentParamForApplicationTitle(title.c_str()) == 0);
    title.push_back('a');
    Require(sceShareSetContentParamForApplicationTitle(title.c_str()) == invalidParam);
    return 0;
}
