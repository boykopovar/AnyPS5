#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

struct CameraStartParameter {
    std::uint32_t sizeThis;
    std::uint32_t formatLevel[2];
    void* startOption;
};

struct CameraFrameData {
    std::uint32_t sizeThis;
    std::uint32_t readMode;
    std::uint32_t framePosition[2][4][4];
    void* framePointerList[2][4];
    std::uint32_t frameSize[2][4];
    std::uint32_t status[2];
    std::uint8_t rest[584 - 240];
};

extern "C" {
int APS5_VABI sceCameraIsAttached(std::int32_t index);
int APS5_VABI sceCameraOpen(std::int32_t userId, std::int32_t type, std::int32_t index, void* param);
int APS5_VABI sceCameraClose(std::int32_t handle);
int APS5_VABI sceCameraCloseByHandle(std::int32_t handle);
int APS5_VABI sceCameraStart(std::int32_t handle, const CameraStartParameter* param);
int APS5_VABI sceCameraStartByHandle(std::int32_t handle, const CameraStartParameter* param);
int APS5_VABI sceCameraStop(std::int32_t handle);
int APS5_VABI sceCameraStopByHandle(std::int32_t handle);
int APS5_VABI sceCameraSetVideoSync(void);
int APS5_VABI sceCameraGetCalibData(void);
int APS5_VABI sceCameraGetFrameData(std::int32_t handle, CameraFrameData* frameData);
int APS5_VABI sceCameraIsValidFrameData(std::int32_t handle, const CameraFrameData* frameData);
}

namespace {

constexpr int ErrorParam = static_cast<int>(0x802E0000u);
constexpr int ErrorNotOpen = static_cast<int>(0x802E0004u);
constexpr int ErrorFormatUnknown = static_cast<int>(0x802E0007u);
constexpr int ErrorNotConnected = static_cast<int>(0x802E0010u);
constexpr std::int32_t SystemUser = 0xFF;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Camera: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireNotImplemented(TCall call, const std::string& name) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == name + " not implemented", "a declared camera function threw another error");
        return;
    }
    Require(false, "a declared camera function returned");
}

CameraStartParameter Start(std::uint32_t level0, std::uint32_t level1) {
    return {sizeof(CameraStartParameter), {level0, level1}, nullptr};
}

void CheckClosed() {
    const CameraStartParameter start = Start(1, 1);
    CameraFrameData frame{};
    frame.sizeThis = sizeof(CameraFrameData);
    Require(sceCameraClose(1) == ErrorNotOpen, "closing before any open must report NOT_OPEN");
    Require(sceCameraCloseByHandle(1) == ErrorNotOpen, "closing by handle before any open must report NOT_OPEN");
    Require(sceCameraStart(1, &start) == ErrorNotOpen, "starting before any open must report NOT_OPEN");
    Require(sceCameraStartByHandle(1, &start) == ErrorNotOpen, "starting by handle before any open must report NOT_OPEN");
    Require(sceCameraStop(1) == ErrorNotOpen, "stopping before any open must report NOT_OPEN");
    Require(sceCameraStopByHandle(1) == ErrorNotOpen, "stopping by handle before any open must report NOT_OPEN");
    Require(sceCameraIsValidFrameData(1, &frame) == ErrorNotOpen, "frame validity before any open must report NOT_OPEN");
}

void CheckOpen() {
    Require(sceCameraOpen(1, 0, 0, nullptr) == ErrorParam, "a user other than the system user must be rejected");
    Require(sceCameraOpen(SystemUser, 1, 0, nullptr) == ErrorParam, "a camera type other than 0 must be rejected");
    Require(sceCameraOpen(SystemUser, 0, 1, nullptr) == ErrorParam, "a camera index other than 0 must be rejected");
    Require(sceCameraOpen(SystemUser, 0, 0, nullptr) == 1, "the first open must return handle 1");
    Require(sceCameraOpen(SystemUser, 0, 0, nullptr) == 2, "the second open must return handle 2");
}

void CheckStart() {
    CameraStartParameter start = Start(1, 1);
    Require(sceCameraStart(0, &start) == ErrorParam, "handle 0 must be rejected");
    Require(sceCameraStart(1, nullptr) == ErrorParam, "a null start parameter must be rejected");
    start.sizeThis = 16;
    Require(sceCameraStart(1, &start) == ErrorParam, "a start parameter of the wrong size must be rejected");
    start = Start(0, 0);
    Require(sceCameraStart(1, &start) == ErrorFormatUnknown, "two empty format levels must be rejected");
    start = Start(0x10, 1);
    Require(sceCameraStart(1, &start) == ErrorFormatUnknown, "a format level above 0xF must be rejected");
    start = Start(1, 0);
    Require(sceCameraStart(1, &start) == ErrorNotConnected, "starting without a device must report NOT_CONNECTED");
    Require(sceCameraStartByHandle(0, &start) == ErrorParam, "starting by handle 0 must be rejected");
    Require(sceCameraStartByHandle(1, &start) == 0, "starting by handle must succeed while a handle is open");
    Require(sceCameraStop(0) == ErrorParam, "stopping handle 0 must be rejected");
    Require(sceCameraStop(1) == 0, "stopping an open handle must succeed");
    Require(sceCameraStopByHandle(1) == 0, "stopping by handle must succeed");
}

void CheckFrames() {
    CameraFrameData frame{};
    frame.sizeThis = sizeof(CameraFrameData);
    frame.status[0] = 1;
    frame.status[1] = 1;
    Require(sceCameraIsValidFrameData(1, &frame) == 1, "frame data with both statuses at 1 must be valid");
    Require(sceCameraGetFrameData(1, nullptr) == ErrorParam, "a null frame must be rejected");
    Require(sceCameraGetFrameData(1, &frame) == ErrorNotOpen, "no frame may be read without a device");
    Require(frame.status[0] == UINT32_MAX && frame.status[1] == UINT32_MAX, "a failed read must mark both statuses -1");
    Require(sceCameraIsValidFrameData(1, &frame) == 0, "frame data from a failed read must be invalid");
    frame.status[0] = 1;
    Require(sceCameraIsValidFrameData(1, &frame) == 0, "frame data with only the first status at 1 must be invalid");
    frame.sizeThis = sizeof(CameraFrameData) + 1;
    Require(sceCameraGetFrameData(1, &frame) == ErrorParam, "an oversized frame must be rejected");
    Require(frame.status[0] == UINT32_MAX, "an oversized frame must still have its statuses reset");
    Require(sceCameraIsValidFrameData(1, &frame) == ErrorParam, "validating an oversized frame must be rejected");
    frame.sizeThis = sizeof(CameraFrameData);
    Require(sceCameraGetFrameData(0, &frame) == ErrorParam, "reading handle 0 must be rejected");
    Require(sceCameraIsValidFrameData(0, &frame) == ErrorParam, "validating handle 0 must be rejected");
    Require(sceCameraIsValidFrameData(1, nullptr) == ErrorParam, "validating a null frame must be rejected");
}

void CheckClose() {
    Require(sceCameraClose(0) == ErrorParam, "closing handle 0 must be rejected");
    Require(sceCameraClose(2) == 0, "closing the second handle must succeed");
    Require(sceCameraCloseByHandle(1) == 0, "closing the first handle by handle must succeed");
    Require(sceCameraClose(1) == ErrorNotOpen, "closing after every handle is closed must report NOT_OPEN");
    Require(sceCameraOpen(SystemUser, 0, 0, nullptr) == 1, "reopening must start again from handle 1");
    Require(sceCameraClose(1) == 0, "closing the reopened handle must succeed");
}

}

int main() {
    Require(sceCameraIsAttached(0) == 0, "camera 0 must be reported as not attached");
    Require(sceCameraIsAttached(1) == ErrorParam, "camera index 1 must be rejected");
    Require(sceCameraIsAttached(-1) == ErrorParam, "a negative camera index must be rejected");
    CheckClosed();
    CheckOpen();
    CheckStart();
    CheckFrames();
    CheckClose();
    RequireNotImplemented([] { sceCameraSetVideoSync(); }, "sceCameraSetVideoSync");
    RequireNotImplemented([] { sceCameraGetCalibData(); }, "sceCameraGetCalibData");
    return 0;
}
