#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceShareCaptureScreenshotExtended(const void* extended_param, std::int32_t* req_id);
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
    return 0;
}
