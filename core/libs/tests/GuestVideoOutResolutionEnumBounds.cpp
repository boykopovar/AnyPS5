#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

constexpr std::uint32_t VideoOutResolution1080p = 1;
constexpr std::uint32_t VideoOutResolution2160p = 2;

constexpr int ErrorInvalidAddress = static_cast<int>(0x80290002u);
constexpr int ErrorInvalidHandle = static_cast<int>(0x8029000Bu);

void Require(bool value) {
    if (!value) std::abort();
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0);
#else
    Require(setenv(name, value, 1) == 0);
#endif
}

void UnsetEnvironment(const char* name) {
#ifdef _WIN32
    Require(_putenv_s(name, "") == 0);
#else
    Require(unsetenv(name) == 0);
#endif
}

void TestUnopenedHandleBounds() {
    VideoOutOutputStatus outStatus{};
    std::array<std::uint8_t, 80> vrrBuffer{};
    vrrBuffer.fill(0xAA);
    VideoOutResolutionStatus resStatus{};

    for (const int badHandle : {-1000, -100, -1, 0, 1, 2, 4, 10, 100, 9999, 0x7FFFFFFF}) {
        Require(Throws([badHandle] {
            return sceVideoOutAllowOutputResolutionWqhdDetection(badHandle);
        }));

        Require(Throws([badHandle] {
            return sceVideoOutGetOutputStatus(badHandle, nullptr);
        }));
        Require(Throws([badHandle, &outStatus] {
            return sceVideoOutGetOutputStatus(badHandle, &outStatus);
        }));

        Require(sceVideoOutGetOutputStatusWithVrr(badHandle, nullptr) == ErrorInvalidAddress);
        Require(sceVideoOutGetOutputStatusWithVrr(badHandle, vrrBuffer.data()) == ErrorInvalidHandle);

        Require(Throws([badHandle] {
            return sceVideoOutGetResolutionStatus(badHandle, nullptr);
        }));
        Require(Throws([badHandle, &resStatus] {
            return sceVideoOutGetResolutionStatus(badHandle, &resStatus);
        }));
    }
}

void TestNullPointersOnOpenHandle(int handle) {
    Require(Throws([handle] {
        return sceVideoOutGetOutputStatus(handle, nullptr);
    }));
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == ErrorInvalidAddress);
    Require(Throws([handle] {
        return sceVideoOutGetResolutionStatus(handle, nullptr);
    }));
}

void TestResolutionEnumEvaluation(int handle) {
    Require(sceVideoOutAllowOutputResolutionWqhdDetection(handle) == 0);

    VideoOutOutputStatus baseStatus{};
    Require(sceVideoOutGetOutputStatus(handle, &baseStatus) == 0);
    Require(baseStatus.resolution == VideoOutResolution1080p || baseStatus.resolution == VideoOutResolution2160p);
    Require(baseStatus.refreshRate == 3 || baseStatus.refreshRate == 13);
    Require(baseStatus.dynamicRange == 1 || baseStatus.dynamicRange == 2);
    Require(baseStatus.flags == 0);

    std::array<std::uint8_t, 80> vrrBuffer{};
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == 0);
    VideoOutOutputStatus vrrOutput{};
    std::memcpy(&vrrOutput, vrrBuffer.data(), sizeof(vrrOutput));
    Require(vrrOutput.resolution == baseStatus.resolution);
    Require(vrrOutput.dynamicRange == baseStatus.dynamicRange);
    Require(vrrOutput.refreshRate == baseStatus.refreshRate);

    SetEnvironment("APS5_OUTPUT_RESOLUTION", "1920x1080");
    VideoOutOutputStatus status1080{};
    Require(sceVideoOutGetOutputStatus(handle, &status1080) == 0);
    Require(status1080.resolution == VideoOutResolution1080p);

    SetEnvironment("APS5_OUTPUT_RESOLUTION", "3840x2160");
    VideoOutOutputStatus status2160{};
    Require(sceVideoOutGetOutputStatus(handle, &status2160) == 0);
    Require(status2160.resolution == VideoOutResolution2160p);

    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == 0);
    std::memcpy(&vrrOutput, vrrBuffer.data(), sizeof(vrrOutput));
    Require(vrrOutput.resolution == VideoOutResolution2160p);

    const char* const resolutions1080p[] = {
        "1280x720",
        "1920x1080",
        "2560x1440",
        "3839x2159",
        "1x1",
        "3839x1080",
        "1920x2159"
    };
    for (const char* res : resolutions1080p) {
        SetEnvironment("APS5_OUTPUT_RESOLUTION", res);
        VideoOutOutputStatus s{};
        Require(sceVideoOutGetOutputStatus(handle, &s) == 0);
        Require(s.resolution == VideoOutResolution1080p);
    }

    const char* const resolutions2160p[] = {
        "3840x2160",
        "4096x2160",
        "3840x1080",
        "1920x2160",
        "7680x4320",
        "3840x2159",
        "3839x2160"
    };
    for (const char* res : resolutions2160p) {
        SetEnvironment("APS5_OUTPUT_RESOLUTION", res);
        VideoOutOutputStatus s{};
        Require(sceVideoOutGetOutputStatus(handle, &s) == 0);
        Require(s.resolution == VideoOutResolution2160p);
    }

    const char* const invalidResolutions[] = {
        "invalid",
        "0x0",
        "0x1080",
        "1920x0",
        "1920x",
        "x1080",
        "-1920x1080",
        "1920x-1080",
        "1920 1080",
        "1920x1080p",
        "1920x1080x60"
    };
    for (const char* badRes : invalidResolutions) {
        SetEnvironment("APS5_OUTPUT_RESOLUTION", badRes);
        VideoOutOutputStatus s{};
        Require(Throws([handle, &s] {
            return sceVideoOutGetOutputStatus(handle, &s);
        }));
        Require(Throws([handle, &vrrBuffer] {
            return sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data());
        }));
    }

    UnsetEnvironment("APS5_OUTPUT_RESOLUTION");
    VideoOutOutputStatus restoredStatus{};
    Require(sceVideoOutGetOutputStatus(handle, &restoredStatus) == 0);
    Require(restoredStatus.resolution == baseStatus.resolution);

    VideoOutResolutionStatus resStatus{};
    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(resStatus.fullWidth > 0);
    Require(resStatus.fullHeight > 0);
    Require(resStatus.paneWidth == resStatus.fullWidth);
    Require(resStatus.paneHeight == resStatus.fullHeight);
    Require(resStatus.screenSizeInInch == 0.0f);
    Require(resStatus.flags == 0);
    Require(resStatus.reserved0 == 0);
    for (std::size_t i = 0; i < 3; ++i) {
        Require(resStatus.reserved1[i] == 0);
    }
}

void TestClosedHandleBounds(int handle) {
    VideoOutOutputStatus outStatus{};
    std::array<std::uint8_t, 80> vrrBuffer{};
    VideoOutResolutionStatus resStatus{};

    Require(Throws([handle] {
        return sceVideoOutAllowOutputResolutionWqhdDetection(handle);
    }));

    Require(Throws([handle, &outStatus] {
        return sceVideoOutGetOutputStatus(handle, &outStatus);
    }));
    Require(Throws([handle] {
        return sceVideoOutGetOutputStatus(handle, nullptr);
    }));

    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == ErrorInvalidHandle);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == ErrorInvalidAddress);

    Require(Throws([handle, &resStatus] {
        return sceVideoOutGetResolutionStatus(handle, &resStatus);
    }));
    Require(Throws([handle] {
        return sceVideoOutGetResolutionStatus(handle, nullptr);
    }));
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestUnopenedHandleBounds();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        return 77;
    }

    Require(handle >= 0);

    TestNullPointersOnOpenHandle(handle);
    TestResolutionEnumEvaluation(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestClosedHandleBounds(handle);

    LibcRunShutdown_nid_postfix();
    return 0;
}
