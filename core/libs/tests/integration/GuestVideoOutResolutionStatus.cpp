#include "SceTypes.hpp"
#include "GuestVideoOutFixture.hpp"

#include <Testing/Test.hpp>

#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status);
int APS5_VABI sceVideoOutGetResolutionStatus(int handle, VideoOutResolutionStatus* status);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int neverOpenedHandle = 2;

void RequireRejected(int handle, VideoOutResolutionStatus* status, const std::string& message) {
    Testing::RequireThrows<std::runtime_error>([handle, status] { sceVideoOutGetResolutionStatus(handle, status); }, message);
}

const Case resolution{"GetResolutionStatus_MainPort_Reports1080pWithZeroedReservedFields", [] {
    const OpenVideoOut port;
    VideoOutResolutionStatus status;
    std::memset(&status, 0xff, sizeof(status));
    RequireEqual(sceVideoOutGetResolutionStatus(port.handle, &status), 0, "get the resolution status");
    RequireEqual(status.fullWidth, 1920u, "full width");
    RequireEqual(status.fullHeight, 1080u, "full height");
    RequireEqual(status.paneWidth, status.fullWidth, "pane width");
    RequireEqual(status.paneHeight, status.fullHeight, "pane height");
    RequireEqual(status.screenSizeInInch, 0.0f, "screen size");
    RequireEqual(status.flags, 0u, "flags");
    RequireEqual(status.reserved0, 0u, "reserved0");
    for (int i = 0; i < 3; ++i) RequireEqual(status.reserved1[i], 0u, "reserved1[" + std::to_string(i) + "]");
}};

const Case refreshRate{"GetResolutionStatus_MainPort_ReportsOutputRefreshRate", [] {
    const OpenVideoOut port;
    VideoOutResolutionStatus status{};
    RequireEqual(sceVideoOutGetResolutionStatus(port.handle, &status), 0, "get the resolution status");
    VideoOutOutputStatus output{};
    RequireEqual(sceVideoOutGetOutputStatus(port.handle, &output), 0, "get the output status");
    RequireEqual(status.refreshRate, output.refreshRate, "refresh rate");
}};

const Case invalid{"GetResolutionStatus_NullOutputOrInvalidHandle_Throws", [] {
    OpenVideoOut port;
    VideoOutResolutionStatus status{};
    RequireRejected(port.handle, nullptr, "null output");
    RequireRejected(0, &status, "handle 0");
    RequireRejected(-1, &status, "handle -1");
    RequireRejected(neverOpenedHandle, &status, "a never opened handle");
    const int handle = port.handle;
    port.Close();
    RequireRejected(handle, &status, "a closed handle");
}};

} // namespace

int main(int argc, char** argv) {
    return RunVideoOutTests(argc, argv);
}
