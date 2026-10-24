#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI sceMoveInit(void);
int APS5_VABI sceMoveTerm(void);
int APS5_VABI sceMoveOpen(std::int32_t userId, std::int32_t type, std::int32_t index);
int APS5_VABI sceMoveClose(std::int32_t handle);
int APS5_VABI sceMoveGetDeviceInfo(std::int32_t handle, void* info);
int APS5_VABI sceMoveGetExtensionPortInfo(std::int32_t handle, void* data);
int APS5_VABI sceMoveReadStateLatest(std::int32_t handle, void* data);
int APS5_VABI sceMoveReadStateRecent(std::int32_t handle, std::int64_t timestamp, void* data, std::int32_t* outCount);
int APS5_VABI sceMoveSetVibration(std::int32_t handle, std::uint8_t motor);
int APS5_VABI sceMoveSetLightSphere(std::int32_t handle, std::uint8_t red, std::uint8_t green, std::uint8_t blue);
int APS5_VABI sceMoveResetLightSphere(std::int32_t handle);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kOk = 0;
constexpr int kNoControllerConnected = 1;
constexpr int kNotInit = static_cast<int>(0x80EE0001);
constexpr int kAlreadyInit = static_cast<int>(0x80EE0002);
constexpr int kInvalidArg = static_cast<int>(0x80EE0003);
constexpr int kInvalidHandle = static_cast<int>(0x80EE0004);
constexpr int kAlreadyOpened = static_cast<int>(0x80EE0007);
constexpr std::int32_t kUserId = 0x10000000;
constexpr std::int32_t kFirstHandle = 0x30B0100;
constexpr std::int32_t kHandleStep = 0x100;
constexpr std::uint8_t kFill = 0x5A;

struct Buffer {
    std::uint8_t bytes[128];

    Buffer() { std::memset(bytes, kFill, sizeof(bytes)); }

    bool Untouched() const {
        for (std::uint8_t byte : bytes) {
            if (byte != kFill) return false;
        }
        return true;
    }
};

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void RequireNotInit(std::int32_t handle) {
    Buffer buffer;
    std::int32_t count = -7;
    Require(sceMoveOpen(kUserId, 0, 0) == kNotInit);
    Require(sceMoveClose(handle) == kNotInit);
    Require(sceMoveGetDeviceInfo(handle, buffer.bytes) == kNotInit);
    Require(sceMoveGetExtensionPortInfo(handle, buffer.bytes) == kNotInit);
    Require(sceMoveReadStateLatest(handle, buffer.bytes) == kNotInit);
    Require(sceMoveReadStateRecent(handle, 0, buffer.bytes, &count) == kNotInit);
    Require(sceMoveSetVibration(handle, 0xFF) == kNotInit);
    Require(sceMoveSetLightSphere(handle, 0xFF, 0, 0) == kNotInit);
    Require(sceMoveResetLightSphere(handle) == kNotInit);
    Require(sceMoveTerm() == kNotInit);
    Require(buffer.Untouched());
    Require(count == -7);
}

void RequireUnknownHandle(std::int32_t handle) {
    Buffer buffer;
    std::int32_t count = -7;
    Require(sceMoveClose(handle) == kInvalidHandle);
    Require(sceMoveGetDeviceInfo(handle, buffer.bytes) == kInvalidHandle);
    Require(sceMoveGetExtensionPortInfo(handle, buffer.bytes) == kInvalidHandle);
    Require(sceMoveReadStateLatest(handle, buffer.bytes) == kInvalidHandle);
    Require(sceMoveReadStateRecent(handle, 0, buffer.bytes, &count) == kInvalidHandle);
    Require(sceMoveSetVibration(handle, 0xFF) == kInvalidHandle);
    Require(sceMoveSetLightSphere(handle, 0xFF, 0, 0) == kInvalidHandle);
    Require(sceMoveResetLightSphere(handle) == kInvalidHandle);
    Require(sceMoveGetDeviceInfo(handle, nullptr) == kInvalidArg);
    Require(sceMoveGetExtensionPortInfo(handle, nullptr) == kInvalidArg);
    Require(sceMoveReadStateLatest(handle, nullptr) == kInvalidArg);
    Require(sceMoveReadStateRecent(handle, -1, buffer.bytes, &count) == kInvalidArg);
    Require(sceMoveReadStateRecent(handle, 0, buffer.bytes, nullptr) == kInvalidArg);
    Require(buffer.Untouched());
    Require(count == -7);
}

}

int main() {
    RequireNotInit(kFirstHandle);

    Require(sceMoveInit() == kOk);
    Require(sceMoveInit() == kAlreadyInit);

    RequireUnknownHandle(0);
    RequireUnknownHandle(-1);
    RequireUnknownHandle(kFirstHandle);

    const std::int32_t first = sceMoveOpen(kUserId, 0, 0);
    const std::int32_t second = sceMoveOpen(kUserId, 0, 1);
    Require(first == kFirstHandle);
    Require(second == kFirstHandle + kHandleStep);
    Require(sceMoveOpen(kUserId, 0, 0) == kAlreadyOpened);
    Require(sceMoveOpen(kUserId, 0, 1) == kAlreadyOpened);
    Require(Throws([] { sceMoveOpen(kUserId + 1, 0, 2); }));

    Require(sceMoveGetDeviceInfo(first, nullptr) == kInvalidArg);
    Require(sceMoveGetExtensionPortInfo(first, nullptr) == kInvalidArg);
    Require(sceMoveReadStateLatest(first, nullptr) == kInvalidArg);

    Buffer buffer;
    std::int32_t count = -7;
    Require(sceMoveReadStateRecent(first, -1, buffer.bytes, &count) == kInvalidArg);
    Require(sceMoveReadStateRecent(first, 0, nullptr, &count) == kInvalidArg);
    Require(sceMoveReadStateRecent(first, 0, buffer.bytes, nullptr) == kInvalidArg);

    Require(sceMoveGetDeviceInfo(first, buffer.bytes) == kNoControllerConnected);
    Require(sceMoveGetExtensionPortInfo(first, buffer.bytes) == kNoControllerConnected);
    Require(sceMoveReadStateLatest(first, buffer.bytes) == kNoControllerConnected);
    Require(sceMoveReadStateRecent(first, 0, buffer.bytes, &count) == kNoControllerConnected);
    Require(buffer.Untouched());
    Require(count == -7);

    Require(sceMoveSetVibration(first, 0xFF) == kNoControllerConnected);
    Require(sceMoveSetLightSphere(first, 0, 0xFF, 0) == kNoControllerConnected);
    Require(sceMoveResetLightSphere(first) == kOk);

    Require(sceMoveClose(first) == kOk);
    RequireUnknownHandle(first);
    const std::int32_t reopened = sceMoveOpen(kUserId, 0, 0);
    Require(reopened == kFirstHandle + 2 * kHandleStep);
    Require(sceMoveClose(reopened) == kOk);
    Require(sceMoveSetVibration(second, 0) == kNoControllerConnected);
    Require(sceMoveTerm() == kOk);

    RequireNotInit(second);

    Require(sceMoveInit() == kOk);
    RequireUnknownHandle(second);
    const std::int32_t third = sceMoveOpen(kUserId, 0, 0);
    Require(third == kFirstHandle + 3 * kHandleStep);
    Require(sceMoveClose(third) == kOk);
    Require(sceMoveTerm() == kOk);
}
