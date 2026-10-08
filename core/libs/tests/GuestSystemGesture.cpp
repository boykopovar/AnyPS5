#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
std::int32_t APS5_VABI sceSystemGestureOpen(std::int32_t input_type, const void* param);
int APS5_VABI sceSystemGestureAppendTouchRecognizer(std::int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer);
int APS5_VABI sceSystemGestureRemoveTouchRecognizer(std::int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer);
int APS5_VABI sceSystemGestureResetTouchRecognizer(std::int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer);
int APS5_VABI sceSystemGestureUpdateTouchRecognizerRectangle(std::int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, const SystemGestureRectangle* rectangle);
int APS5_VABI sceSystemGestureResetPrimitiveTouchRecognizer(std::int32_t gesture_handle);
int APS5_VABI sceSystemGestureUpdateAllTouchRecognizer(std::int32_t gesture_handle);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(std::int32_t gesture_handle);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(std::int32_t gesture_handle, std::uint32_t index, SystemGesturePrimitiveTouchEvent* event);
int APS5_VABI sceSystemGestureGetTouchEvents(std::int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchEvent* event_buffer, std::uint32_t capacity_of_buffer, std::uint32_t* number_of_event);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEvents(std::int32_t gesture_handle, SystemGesturePrimitiveTouchEvent* event_buffer, std::uint32_t capacity_of_buffer, std::uint32_t* number_of_event);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::int32_t kBadHandle = 999;
constexpr int kErrInvalidArgument = static_cast<int>(0x80D10002);
constexpr int kErrInvalidHandle = static_cast<int>(0x80D10003);
constexpr int kErrIndexOutOfArray = static_cast<int>(0x80D10005);

}

int main() {
    const std::int32_t handle = sceSystemGestureOpen(0, nullptr);
    Require(handle > 0);

    static SystemGestureTouchRecognizer recognizer{};
    SystemGestureRectangle rectangle{};

    Require(sceSystemGestureAppendTouchRecognizer(handle, &recognizer) == 0);
    Require(sceSystemGestureAppendTouchRecognizer(kBadHandle, &recognizer) == kErrInvalidHandle);
    Require(sceSystemGestureAppendTouchRecognizer(handle, nullptr) == kErrInvalidArgument);
    Require(sceSystemGestureRemoveTouchRecognizer(handle, &recognizer) == 0);
    Require(sceSystemGestureResetTouchRecognizer(handle, &recognizer) == 0);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, &recognizer, &rectangle) == 0);

    Require(sceSystemGestureResetPrimitiveTouchRecognizer(handle) == 0);
    Require(sceSystemGestureResetPrimitiveTouchRecognizer(kBadHandle) == kErrInvalidHandle);
    Require(sceSystemGestureUpdateAllTouchRecognizer(handle) == 0);

    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(kBadHandle) == kErrInvalidHandle);

    SystemGesturePrimitiveTouchEvent primitive{};
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle, 0, &primitive) == kErrIndexOutOfArray);

    SystemGestureTouchEvent touchEvents[4]{};
    std::uint32_t count = 7;
    Require(sceSystemGestureGetTouchEvents(handle, &recognizer, touchEvents, 4, &count) == 0);
    Require(count == 0);

    SystemGesturePrimitiveTouchEvent primitives[4]{};
    count = 7;
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, primitives, 4, &count) == 0);
    Require(count == 0);
}
