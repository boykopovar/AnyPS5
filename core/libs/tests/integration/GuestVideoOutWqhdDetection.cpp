#include "SceTypes.hpp"
#include "GuestVideoOutFixture.hpp"

#include <Testing/Test.hpp>

#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status);
int APS5_VABI sceVideoOutAllowOutputResolutionWqhdDetection(int handle);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int neverOpenedHandle = 2;

void RequireRejected(int handle, const std::string& message) {
    Testing::RequireThrows<std::runtime_error>([handle] { sceVideoOutAllowOutputResolutionWqhdDetection(handle); }, message);
}

const Case allowKeepsStatus{"AllowWqhdDetection_OpenPortTwice_SucceedsWithoutChangingStatus", [] {
    const OpenVideoOut port;
    VideoOutOutputStatus before{};
    RequireEqual(sceVideoOutGetOutputStatus(port.handle, &before), 0, "status before");
    RequireEqual(sceVideoOutAllowOutputResolutionWqhdDetection(port.handle), 0, "first allow");
    RequireEqual(sceVideoOutAllowOutputResolutionWqhdDetection(port.handle), 0, "second allow");
    VideoOutOutputStatus after{};
    RequireEqual(sceVideoOutGetOutputStatus(port.handle, &after), 0, "status after");
    Require(std::memcmp(&before, &after, sizeof(before)) == 0, "output status unchanged");
}};

const Case invalidHandles{"AllowWqhdDetection_InvalidOrClosedHandle_Throws", [] {
    OpenVideoOut port;
    RequireRejected(0, "handle 0");
    RequireRejected(-1, "handle -1");
    RequireRejected(neverOpenedHandle, "a never opened handle");
    const int handle = port.handle;
    port.Close();
    RequireRejected(handle, "a closed handle");
}};

} // namespace

int main(int argc, char** argv) {
    return RunVideoOutTests(argc, argv);
}
