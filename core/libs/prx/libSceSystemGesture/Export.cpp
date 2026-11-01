#include "prx/libSceSystemGesture/include/InventoryExports.hpp"
#include "prx/libSceSystemGesture/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

static constexpr int SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80890001);
static constexpr int SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE = static_cast<int>(0x80890006);
static constexpr int SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY = static_cast<int>(0x80890004);

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

constexpr std::int32_t TOUCH_STATE_BEGIN = 1;
constexpr std::int32_t TOUCH_STATE_ACTIVE = 2;
constexpr std::int32_t TOUCH_STATE_END = 3;

struct TrackedTouch {
    std::uint16_t primitiveId;
    std::int32_t eventState;
    bool isUpdated;
    bool sampled;
    SystemGestureVector2 pressedPosition;
    SystemGestureVector2 currentPosition;
    SystemGestureVector2 deltaVector;
    std::uint64_t pressedTime;
    std::uint64_t updatedTime;
    std::uint64_t deltaTime;
};

std::mutex primitiveTouchMutex;
struct GestureContext {
    std::int32_t handle = 0;
    std::vector<TrackedTouch> touches;
};

std::array<GestureContext, 8> gestureContexts;
bool gestureInitialized = false;
std::uint16_t gestureGeneration = 0;

GestureContext* findGestureContext(std::int32_t gestureHandle) {
    const auto found = std::find_if(gestureContexts.begin(), gestureContexts.end(), [gestureHandle](const GestureContext& context) { return context.handle != 0 && context.handle == gestureHandle; });
    return found == gestureContexts.end() ? nullptr : &*found;
}

struct TouchSample {
    std::uint16_t primitiveId;
    SystemGestureVector2 position;
};

std::vector<TouchSample> reportSamples(const PadData& report) {
    std::vector<TouchSample> samples;
    const std::uint8_t count = report.touch_data_touch_num;
    if (count > 0)
        samples.push_back(TouchSample{report.touch_data_touch0_id,
            SystemGestureVector2{static_cast<float>(report.touch_data_touch0_x), static_cast<float>(report.touch_data_touch0_y)}});
    if (count > 1)
        samples.push_back(TouchSample{report.touch_data_touch1_id,
            SystemGestureVector2{static_cast<float>(report.touch_data_touch1_x), static_cast<float>(report.touch_data_touch1_y)}});
    return samples;
}

void sampleReport(std::vector<TrackedTouch>& primitiveTouches, const PadData& report) {
    std::erase_if(primitiveTouches, [](const TrackedTouch& touch) { return touch.eventState == TOUCH_STATE_END; });
    for (TrackedTouch& touch : primitiveTouches) {
        touch.sampled = false;
        touch.isUpdated = false;
    }
    const std::uint64_t time = report.timestamp;
    for (const TouchSample& sample : reportSamples(report)) {
        const auto tracked = std::find_if(primitiveTouches.begin(), primitiveTouches.end(),
            [&](const TrackedTouch& touch) { return touch.primitiveId == sample.primitiveId; });
        if (tracked == primitiveTouches.end()) {
            TrackedTouch touch{};
            touch.primitiveId = sample.primitiveId;
            touch.eventState = TOUCH_STATE_BEGIN;
            touch.isUpdated = true;
            touch.sampled = true;
            touch.pressedPosition = sample.position;
            touch.currentPosition = sample.position;
            touch.pressedTime = time;
            touch.updatedTime = time;
            primitiveTouches.push_back(touch);
            continue;
        }
        const SystemGestureVector2 previous = tracked->currentPosition;
        tracked->deltaVector = SystemGestureVector2{sample.position.x - previous.x, sample.position.y - previous.y};
        tracked->currentPosition = sample.position;
        tracked->deltaTime = time >= tracked->updatedTime ? time - tracked->updatedTime : 0;
        tracked->isUpdated = tracked->eventState != TOUCH_STATE_ACTIVE || tracked->deltaVector.x != 0.0f || tracked->deltaVector.y != 0.0f;
        tracked->eventState = TOUCH_STATE_ACTIVE;
        tracked->sampled = true;
        tracked->updatedTime = time;
    }
    for (TrackedTouch& touch : primitiveTouches) {
        if (touch.sampled) continue;
        touch.deltaVector = SystemGestureVector2{0.0f, 0.0f};
        touch.deltaTime = time >= touch.updatedTime ? time - touch.updatedTime : 0;
        touch.eventState = TOUCH_STATE_END;
        touch.isUpdated = true;
        touch.updatedTime = time;
    }
}

SystemGesturePrimitiveTouchEvent buildPrimitiveTouchEvent(const TrackedTouch& touch) {
    SystemGesturePrimitiveTouchEvent event{};
    event.event_state = touch.eventState;
    event.primitive_id = touch.primitiveId;
    event.is_updated = touch.isUpdated ? 1 : 0;
    event.pressed_position = touch.pressedPosition;
    event.current_position = touch.currentPosition;
    event.delta_vector = touch.deltaVector;
    event.delta_time = touch.deltaTime;
    event.elapsed_time = touch.updatedTime - touch.pressedTime;
    return event;
}

}

extern "C" {

int APS5_VABI sceSystemGestureAppendTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureClose(int32_t gestureHandle) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    auto* context = findGestureContext(gestureHandle);
    if (!context) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    context->touches.clear();
    context->handle = 0;
    return 0;
}

int APS5_VABI sceSystemGestureCreateTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer, int32_t type, const SystemGestureRectangle* rectangle, const void* param) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    (void)param;
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
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
    std::lock_guard lock(primitiveTouchMutex);
    gestureInitialized = false;
    return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(int32_t gestureHandle, uint32_t index, SystemGesturePrimitiveTouchEvent* event) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!event) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (index >= findGestureContext(gestureHandle)->touches.size()) return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
    *event = buildPrimitiveTouchEvent(findGestureContext(gestureHandle)->touches[index]);
    return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(int32_t gestureHandle, uint16_t primitiveId, SystemGesturePrimitiveTouchEvent* event) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!event) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    const auto tracked = std::find_if(findGestureContext(gestureHandle)->touches.begin(), findGestureContext(gestureHandle)->touches.end(),
        [&](const TrackedTouch& touch) { return touch.primitiveId == primitiveId; });
    if (tracked == findGestureContext(gestureHandle)->touches.end()) return static_cast<int>(0x80890005);
    *event = buildPrimitiveTouchEvent(*tracked);
    return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEvents(int32_t gestureHandle, SystemGesturePrimitiveTouchEvent* eventBuffer, uint32_t capacityOfBuffer, uint32_t* numberOfEvent) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!eventBuffer || !capacityOfBuffer || !numberOfEvent) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    const std::uint32_t reported = static_cast<std::uint32_t>(std::min<std::size_t>(findGestureContext(gestureHandle)->touches.size(), capacityOfBuffer));
    for (std::uint32_t i = 0; i < reported; ++i) eventBuffer[i] = buildPrimitiveTouchEvent(findGestureContext(gestureHandle)->touches[i]);
    *numberOfEvent = reported;
    return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(int32_t gestureHandle) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return static_cast<int>(findGestureContext(gestureHandle)->touches.size());
}

int APS5_VABI sceSystemGestureGetTouchEventByEventID(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer, uint32_t eventId, SystemGestureTouchEvent* event) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    (void)eventId;
    (void)event;
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetTouchEventByIndex(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer, uint32_t index, SystemGestureTouchEvent* event) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    (void)index;
    (void)event;
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
}

int APS5_VABI sceSystemGestureGetTouchEvents(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchEvent* eventBuffer, uint32_t capacityOfBuffer, uint32_t* numberOfEvent) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    (void)recognizer;
    (void)eventBuffer;
    (void)capacityOfBuffer;
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!numberOfEvent) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    *numberOfEvent = 0;
    return 0;
}

int APS5_VABI sceSystemGestureGetTouchEventsCount(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    (void)recognizer;
    return findGestureContext(gestureHandle) != nullptr ? 0 : SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceSystemGestureGetTouchRecognizerInformation(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchRecognizerInformation* information) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
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

int APS5_VABI sceSystemGestureInitializePrimitiveTouchRecognizer() {
    std::lock_guard lock(primitiveTouchMutex);
    gestureInitialized = true;
    return 0;
}

int32_t APS5_VABI sceSystemGestureOpen(int32_t inputType, const void* param) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (inputType != 0 || param) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    for (std::size_t index = 0; index < gestureContexts.size(); ++index) {
        auto& context = gestureContexts[index];
        if (context.handle != 0) continue;
        ++gestureGeneration;
        if (gestureGeneration == 0) gestureGeneration = 1;
        context.touches.clear();
        context.handle = static_cast<std::int32_t>(1191182336u + (static_cast<std::uint32_t>(gestureGeneration) << 8) + index);
        return context.handle;
    }
    return static_cast<int>(0x8089000A);
}

int APS5_VABI sceSystemGestureRemoveTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureResetPrimitiveTouchRecognizer(int32_t gestureHandle) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    findGestureContext(gestureHandle)->touches.clear();
    return 0;
}

int APS5_VABI sceSystemGestureResetTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureUpdateAllTouchRecognizer(int32_t gestureHandle) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceSystemGestureUpdatePrimitiveTouchRecognizer(int32_t gestureHandle, const SystemGestureTouchPadData* input) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!input || !input->pad_data_buffer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    auto* context = findGestureContext(gestureHandle);
    if (!context) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (input->report_number < 0 || input->report_number > 1) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    if (input->report_number == 1) sampleReport(context->touches, *input->pad_data_buffer);
    return 0;
}

int APS5_VABI sceSystemGestureUpdateTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
    if (!recognizer) return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemGestureUpdateTouchRecognizerRectangle(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer, const SystemGestureRectangle* rectangle) {
    std::lock_guard lock(primitiveTouchMutex);
    if (!gestureInitialized) return static_cast<int>(0x80890003);
    if (!findGestureContext(gestureHandle)) return SCE_SYSTEM_GESTURE_ERROR_INVALID_HANDLE;
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

int APS5_VABI sceSystemGestureDebugGetVersion() {
    return 0x6a00;
}

}

extern "C" {

int APS5_VABI sceSystemGestureSetTouchAreaInformation() {
    NotImplemented_nid_no_patch(__func__);
}

}
