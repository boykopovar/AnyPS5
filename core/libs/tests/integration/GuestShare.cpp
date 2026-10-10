#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" {
int APS5_VABI sceShareCaptureScreenshotExtended(const void* extended_param, std::int32_t* req_id);
int APS5_VABI sceShareCaptureScreenshot(const void* param, std::int32_t* req_id);
int APS5_VABI sceShareCaptureVideoClip(const void* param, std::int32_t* req_id);
int APS5_VABI sceShareGetCurrentStatus(std::uint32_t feature_flag, void* status);
int APS5_VABI sceShareOpenMenuForContent(const void* content_id);
int APS5_VABI sceShareGetRunningStatus(std::uint32_t* status);
int APS5_VABI sceShareSetContentParamForApplicationTitle(const char* application_title);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr std::int32_t notSupported = static_cast<std::int32_t>(0x81960007);
constexpr std::int32_t invalidParam = static_cast<std::int32_t>(0x81960002);
constexpr std::uint8_t filler = 0x5a;

using Capture = int(APS5_VABI*)(const void*, std::int32_t*);

struct NamedCapture {
    const char* name;
    Capture capture;
};

const std::array<NamedCapture, 3> captures{{
    {"sceShareCaptureScreenshotExtended", sceShareCaptureScreenshotExtended},
    {"sceShareCaptureScreenshot", sceShareCaptureScreenshot},
    {"sceShareCaptureVideoClip", sceShareCaptureVideoClip},
}};

std::array<std::uint8_t, 18> FilledStatus() {
    std::array<std::uint8_t, 18> status{};
    status.fill(filler);
    return status;
}

const Case captureWithRequestId{"Capture_WithOrWithoutParam_ReturnsNotSupportedAndInvalidatesRequestId", [] {
    const std::uint8_t param[64]{};
    for (const auto& entry : captures) {
        for (const void* argument : {static_cast<const void*>(param), static_cast<const void*>(nullptr)}) {
            std::int32_t reqId = 7;
            const std::string label = std::string(entry.name) + (argument ? " with a param" : " without a param");
            RequireEqual(entry.capture(argument, &reqId), notSupported, label);
            RequireEqual(reqId, -1, label + ": request id");
        }
    }
}};

const Case captureWithoutRequestId{"Capture_NullRequestId_ReturnsNotSupported", [] {
    const std::uint8_t param[64]{};
    for (const auto& entry : captures) {
        RequireEqual(entry.capture(param, nullptr), notSupported, std::string(entry.name) + " with a null request id");
    }
    RequireEqual(sceShareCaptureScreenshotExtended(nullptr, nullptr), notSupported, "extended screenshot with null arguments");
}};

const Case openMenu{"OpenMenuForContent_AnyContent_ReturnsNotSupported", [] {
    const std::uint8_t param[64]{};
    RequireEqual(sceShareOpenMenuForContent(param), notSupported, "open menu with a content id");
    RequireEqual(sceShareOpenMenuForContent(nullptr), notSupported, "open menu without a content id");
}};

const Case currentStatus{"GetCurrentStatus_SingleFeature_ZeroesSixteenBytesOnly", [] {
    auto status = FilledStatus();
    RequireEqual(sceShareGetCurrentStatus(1, status.data()), 0, "get current status");
    for (std::size_t index = 0; index < status.size(); ++index) {
        RequireEqual(status[index], index < 16 ? std::uint8_t{0} : filler, "status byte " + std::to_string(index));
    }
}};

const Case currentStatusAllFeatures{"GetCurrentStatus_AllFeatures_ZeroesSixteenBytesOnly", [] {
    auto status = FilledStatus();
    RequireEqual(sceShareGetCurrentStatus(0xffffffffu, status.data()), 0, "get current status");
    RequireEqual(status[0], std::uint8_t{0}, "status byte 0");
    RequireEqual(status[15], std::uint8_t{0}, "status byte 15");
    RequireEqual(status[16], filler, "status byte 16");
}};

const Case currentStatusNoFeature{"GetCurrentStatus_NoFeature_ReturnsInvalidParamWithoutWriting", [] {
    auto status = FilledStatus();
    RequireEqual(sceShareGetCurrentStatus(0, status.data()), invalidParam, "get current status");
    RequireEqual(status[0], filler, "status byte 0");
}};

const Case currentStatusNull{"GetCurrentStatus_NullOutput_ReturnsInvalidParam", [] {
    RequireEqual(sceShareGetCurrentStatus(1, nullptr), invalidParam, "get current status");
}};

const Case runningStatus{"GetRunningStatus_ValidOutput_ZeroesFirstWordOnly", [] {
    std::uint32_t running[2]{0xffffffffu, 0x5a5a5a5au};
    RequireEqual(sceShareGetRunningStatus(running), 0, "get running status");
    RequireEqual(running[0], 0u, "running status");
    RequireEqual(running[1], 0x5a5a5a5au, "word after the running status");
}};

const Case runningStatusNull{"GetRunningStatus_NullOutput_ReturnsInvalidParam", [] {
    RequireEqual(sceShareGetRunningStatus(nullptr), invalidParam, "get running status");
}};

const Case applicationTitle{"SetContentParamForApplicationTitle_TitleOrNull_SucceedsOrReturnsInvalidParam", [] {
    RequireEqual(sceShareSetContentParamForApplicationTitle("title"), 0, "set a title");
    RequireEqual(sceShareSetContentParamForApplicationTitle(nullptr), invalidParam, "set a null title");
}};

} // namespace
