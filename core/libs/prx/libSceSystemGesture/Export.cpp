#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

// No touch input is emulated: recognizers exist but never report events.
static constexpr int32_t GESTURE_HANDLE = 1;
static constexpr int SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80D10002);
static constexpr int SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE = static_cast<int>(0x80D10003);
static constexpr int SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY = static_cast<int>(0x80D10005);

namespace {

constexpr std::uint64_t RECOGNIZER_STATE_MAGIC = 0x41505335'47455354ULL;

struct RecognizerState {
    std::uint64_t magic;
    std::int32_t gestureType;
    std::uint32_t hasRectangle;
    SystemGestureRectangle rectangle;
};

static_assert(sizeof(RecognizerState) <= sizeof(SystemGestureTouchRecognizer));

void storeRecognizerState(SystemGestureTouchRecognizer* recognizer, const RecognizerState& state) {
    std::memcpy(recognizer->reserve, &state, sizeof(state));
}

bool loadRecognizerState(const SystemGestureTouchRecognizer* recognizer, RecognizerState& state) {
    std::memcpy(&state, recognizer->reserve, sizeof(state));
    return state.magic == RECOGNIZER_STATE_MAGIC;
}

}

extern "C" {

int APS5_VABI sceSystemGestureAppendTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureClose(int32_t gesture_handle) {
    return gesture_handle == GESTURE_HANDLE ? 0 : SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceSystemGestureCreateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, int32_t type, const SystemGestureRectangle* rectangle, const void* param) {
    (void)param;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    RecognizerState state{};
    state.magic = RECOGNIZER_STATE_MAGIC;
    state.gestureType = type;
    state.hasRectangle = rectangle ? 1 : 0;
    if (rectangle) state.rectangle = *rectangle;
    storeRecognizerState(recognizer, state);
    return 0;
}

int APS5_VABI sceSystemGestureFinalizePrimitiveTouchRecognizer(void) {
    return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(int32_t gesture_handle, uint32_t index, SystemGesturePrimitiveTouchEvent* event) {
    (void)index;
    (void)event;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(int32_t gesture_handle, uint16_t primitiveId, SystemGesturePrimitiveTouchEvent* event) {
 (void)primitiveId;
 (void)event;
 if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
 return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEvents(int32_t gesture_handle, SystemGesturePrimitiveTouchEvent* event_buffer, uint32_t capacity_of_buffer, uint32_t* number_of_event) {
    (void)event_buffer;
    (void)capacity_of_buffer;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!number_of_event) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    *number_of_event = 0;
    return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(int32_t gesture_handle) {
    return gesture_handle == GESTURE_HANDLE ? 0 : SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceSystemGestureGetTouchEventByEventID(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t eventId, SystemGestureTouchEvent* event) {
 (void)recognizer;
 (void)eventId;
 (void)event;
 if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
 return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetTouchEventByIndex(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t index, SystemGestureTouchEvent* event) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    (void)recognizer;
    (void)index;
    (void)event;
    return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetTouchEvents(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchEvent* event_buffer, uint32_t capacity_of_buffer, uint32_t* number_of_event) {
    (void)recognizer;
    (void)event_buffer;
    (void)capacity_of_buffer;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!number_of_event) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    *number_of_event = 0;
    return 0;
}

int APS5_VABI sceSystemGestureGetTouchEventsCount(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer) {
    (void)recognizer;
    return gesture_handle == GESTURE_HANDLE ? 0 : SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceSystemGestureGetTouchRecognizerInformation(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchRecognizerInformation* information) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer || !information) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    RecognizerState state{};
    if (!loadRecognizerState(recognizer, state))
        throw std::runtime_error(std::string(__func__) + ": recognizer was not created by sceSystemGestureCreateTouchRecognizer");
    if (!state.hasRectangle)
        throw std::runtime_error(std::string(__func__) + ": recognizer was created without a rectangle");
    information->gesture_type = state.gestureType;
    information->rectangle = state.rectangle;
    information->updated_time = 0;
    return 0;
}

int APS5_VABI sceSystemGestureInitializePrimitiveTouchRecognizer(const void* param) {
    (void)param;
    return 0;
}

int32_t APS5_VABI sceSystemGestureOpen(int32_t input_type, const void* param) {
    (void)input_type;
    (void)param;
    return GESTURE_HANDLE;
}

int APS5_VABI sceSystemGestureRemoveTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureResetPrimitiveTouchRecognizer(int32_t gesture_handle) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceSystemGestureResetTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureUpdateAllTouchRecognizer(int32_t gesture_handle) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceSystemGestureUpdatePrimitiveTouchRecognizer(int32_t gesture_handle, const void* param) {
    (void)param;
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceSystemGestureUpdateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureUpdateTouchRecognizerRectangle(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, const SystemGestureRectangle* rectangle) {
    if (gesture_handle != GESTURE_HANDLE) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    if (!rectangle) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    RecognizerState state{};
    if (loadRecognizerState(recognizer, state)) {
        state.hasRectangle = 1;
        state.rectangle = *rectangle;
        storeRecognizerState(recognizer, state);
    }
    return 0;
}

}
