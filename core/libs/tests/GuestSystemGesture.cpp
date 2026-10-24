#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

extern "C" {
int APS5_VABI sceSystemGestureClose(std::int32_t);
std::int32_t APS5_VABI sceSystemGestureOpen(std::int32_t, const void*);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEvents(std::int32_t, void*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(std::int32_t);
int APS5_VABI sceSystemGestureGetTouchEvents(std::int32_t, const void*, void*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(std::int32_t, std::uint32_t, void*);
int APS5_VABI sceSystemGestureGetTouchEventByIndex(std::int32_t, const void*, std::uint32_t, void*);
int APS5_VABI sceSystemGestureGetTouchEventByEventID(std::int32_t, const void*, std::uint32_t, void*);
int APS5_VABI sceSystemGestureAppendTouchRecognizer(std::int32_t, void*);
int APS5_VABI sceSystemGestureCreateTouchRecognizer(std::int32_t, void*, std::int32_t, const void*, const void*);
int APS5_VABI sceSystemGestureGetTouchRecognizerInformation(std::int32_t, const void*, void*);
int APS5_VABI sceSystemGestureRemoveTouchRecognizer(std::int32_t, void*);
int APS5_VABI sceSystemGestureResetTouchRecognizer(std::int32_t, void*);
int APS5_VABI sceSystemGestureUpdateTouchRecognizer(std::int32_t, void*);
int APS5_VABI sceSystemGestureUpdateTouchRecognizerRectangle(std::int32_t, void*, const void*);
int APS5_VABI sceSystemGestureResetPrimitiveTouchRecognizer(std::int32_t);
int APS5_VABI sceSystemGestureUpdateAllTouchRecognizer(std::int32_t);
int APS5_VABI sceSystemGestureUpdatePrimitiveTouchRecognizer(std::int32_t, const SystemGestureTouchPadData*);
int APS5_VABI sceSystemGestureInitializePrimitiveTouchRecognizer();
int APS5_VABI sceSystemGestureFinalizePrimitiveTouchRecognizer(void);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(std::int32_t, std::uint16_t, void*);
int APS5_VABI sceSystemGestureDebugGetVersion(void);
}

static void Require(bool value) { if (!value) std::abort(); }

static bool SameRectangle(const SystemGestureRectangle& left, const SystemGestureRectangle& right) {
    return std::memcmp(&left, &right, sizeof(SystemGestureRectangle)) == 0;
}

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void TestTouchRecognizerInformation(std::int32_t handle, int invalidArgument, int invalidHandle) {
    constexpr std::int32_t gestureType = 2;
    const SystemGestureRectangle area{10.0f, 20.0f, 300.0f, 400.0f, {}};
    const SystemGestureRectangle moved{40.0f, 50.0f, 60.0f, 70.0f, {}};

    SystemGestureTouchRecognizer recognizer{};
    Require(sceSystemGestureCreateTouchRecognizer(handle, &recognizer, gestureType, &area, nullptr) == 0);

    SystemGestureTouchRecognizerInformation information;
    std::memset(&information, 0x5a, sizeof(information));
    Require(sceSystemGestureGetTouchRecognizerInformation(handle, &recognizer, &information) == 0);
    Require(information.gesture_type == gestureType);
    Require(SameRectangle(information.rectangle, area));
    Require(information.updated_time == 0);
    Require(std::all_of(std::begin(information.reserve), std::end(information.reserve), [](std::uint8_t value) { return value == 0x5a; }));

    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, &recognizer, &moved) == 0);
    Require(sceSystemGestureGetTouchRecognizerInformation(handle, &recognizer, &information) == 0);
    Require(information.gesture_type == gestureType);
    Require(SameRectangle(information.rectangle, moved));

    Require(sceSystemGestureGetTouchRecognizerInformation(handle + 1, &recognizer, &information) == invalidHandle);
    Require(sceSystemGestureGetTouchRecognizerInformation(handle, nullptr, &information) == invalidArgument);
    Require(sceSystemGestureGetTouchRecognizerInformation(handle, &recognizer, nullptr) == invalidArgument);

    SystemGestureTouchRecognizer uncreated{};
    Require(Throws([&] { sceSystemGestureGetTouchRecognizerInformation(handle, &uncreated, &information); }));

    SystemGestureTouchRecognizer withoutRectangle{};
    Require(sceSystemGestureCreateTouchRecognizer(handle, &withoutRectangle, gestureType, nullptr, nullptr) == 0);
    Require(Throws([&] { sceSystemGestureGetTouchRecognizerInformation(handle, &withoutRectangle, &information); }));
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, &withoutRectangle, &area) == 0);
    Require(sceSystemGestureGetTouchRecognizerInformation(handle, &withoutRectangle, &information) == 0);
    Require(SameRectangle(information.rectangle, area));
}

static PadData TouchReport(std::uint64_t timestamp, std::initializer_list<std::array<std::uint16_t, 3>> touches) {
    PadData report{};
    report.timestamp = timestamp;
    for (const auto& touch : touches) {
        if (report.touch_data_touch_num == 0) {
            report.touch_data_touch0_id = static_cast<std::uint8_t>(touch[0]);
            report.touch_data_touch0_x = touch[1];
            report.touch_data_touch0_y = touch[2];
        } else {
            report.touch_data_touch1_id = static_cast<std::uint8_t>(touch[0]);
            report.touch_data_touch1_x = touch[1];
            report.touch_data_touch1_y = touch[2];
        }
        ++report.touch_data_touch_num;
    }
    return report;
}

static int UpdateWithReports(std::int32_t handle, const PadData* reports, std::int32_t count) {
    SystemGestureTouchPadData input{};
    input.pad_handle = 1;
    input.report_number = count;
    input.pad_data_buffer = reports;
    return sceSystemGestureUpdatePrimitiveTouchRecognizer(handle, &input);
}

static SystemGesturePrimitiveTouchEvent SingleEvent(std::int32_t handle) {
    SystemGesturePrimitiveTouchEvent event;
    std::memset(&event, 0x5a, sizeof(event));
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle, 0, &event) == 0);
    return event;
}

static void TestPrimitiveTouchEvents(std::int32_t handle, int invalidArgument, int invalidHandle, int indexOutOfArray) {
    Require(sceSystemGestureInitializePrimitiveTouchRecognizer() == 0);

    const PadData pressed = TouchReport(1000, {{7, 480, 471}});
    Require(UpdateWithReports(handle, &pressed, 1) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 1);
    SystemGesturePrimitiveTouchEvent event = SingleEvent(handle);
    Require(event.event_state == 1);
    Require(event.primitive_id == 7);
    Require(event.is_updated == 1);
    Require(event.pressed_position.x == 480.0f && event.pressed_position.y == 471.0f);
    Require(event.current_position.x == 480.0f && event.current_position.y == 471.0f);
    Require(event.delta_vector.x == 0.0f && event.delta_vector.y == 0.0f);
    Require(event.delta_time == 0 && event.elapsed_time == 0);

    const PadData moved = TouchReport(2000, {{7, 500, 480}});
    Require(UpdateWithReports(handle, &moved, 1) == 0);
    event = SingleEvent(handle);
    Require(event.event_state == 2);
    Require(event.is_updated == 1);
    Require(event.pressed_position.x == 480.0f && event.pressed_position.y == 471.0f);
    Require(event.current_position.x == 500.0f && event.current_position.y == 480.0f);
    Require(event.delta_vector.x == 20.0f && event.delta_vector.y == 9.0f);
    Require(event.delta_time == 1000 && event.elapsed_time == 1000);

    const PadData held = TouchReport(3000, {{7, 500, 480}});
    Require(UpdateWithReports(handle, &held, 1) == 0);
    event = SingleEvent(handle);
    Require(event.event_state == 2);
    Require(event.is_updated == 0);
    Require(event.delta_vector.x == 0.0f && event.delta_vector.y == 0.0f);
    Require(event.delta_time == 1000 && event.elapsed_time == 2000);

    const PadData released = TouchReport(4000, {});
    Require(UpdateWithReports(handle, &released, 1) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 1);
    event = SingleEvent(handle);
    Require(event.event_state == 3);
    Require(event.is_updated == 1);
    Require(event.current_position.x == 500.0f && event.current_position.y == 480.0f);
    Require(event.delta_time == 1000 && event.elapsed_time == 3000);

    const PadData empty = TouchReport(5000, {});
    Require(UpdateWithReports(handle, &empty, 1) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle, 0, &event) == indexOutOfArray);

    const PadData pair = TouchReport(6000, {{3, 100, 200}, {4, 300, 400}});
    Require(UpdateWithReports(handle, &pair, 1) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 2);

    SystemGesturePrimitiveTouchEvent byId{};
    Require(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(handle, 4, &byId) == 0);
    Require(byId.primitive_id == 4);
    Require(byId.current_position.x == 300.0f && byId.current_position.y == 400.0f);
    Require(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(handle, 4, nullptr) == invalidArgument);
    Require(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(handle + 1, 4, &byId) == invalidHandle);
    Require(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(handle, 9, &byId) == static_cast<int>(0x80890005));
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle + 1, 0, nullptr) == invalidArgument);

    std::array<SystemGesturePrimitiveTouchEvent, 2> events{};
    std::uint32_t reported = 7;
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, events.data(), 1, &reported) == 0);
    Require(reported == 1);
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, events.data(), 0, &reported) == invalidArgument);
    Require(events[0].primitive_id == 3);
    reported = 7;
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, events.data(), 2, &reported) == 0);
    Require(reported == 2 && events[1].primitive_id == 4);
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, nullptr, 2, &reported) == invalidArgument);
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle, 2, &event) == indexOutOfArray);

    Require(UpdateWithReports(handle + 1, &pair, 1) == invalidHandle);
    Require(sceSystemGestureUpdatePrimitiveTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 2);

    const std::array<PadData, 2> sequence{TouchReport(7000, {{3, 110, 200}}), TouchReport(8000, {{3, 130, 210}})};
    Require(UpdateWithReports(handle, sequence.data(), 2) == invalidArgument);
    Require(UpdateWithReports(handle, sequence.data(), -1) == invalidArgument);
    Require(UpdateWithReports(handle, sequence.data(), 1) == 0);
    Require(UpdateWithReports(handle, sequence.data() + 1, 1) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 1);
    event = SingleEvent(handle);
    Require(event.primitive_id == 3);
    Require(event.current_position.x == 130.0f && event.current_position.y == 210.0f);
    Require(event.delta_vector.x == 20.0f && event.delta_vector.y == 10.0f);

    Require(sceSystemGestureResetPrimitiveTouchRecognizer(handle) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 0);

    Require(UpdateWithReports(handle, &pressed, 1) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 1);
    Require(sceSystemGestureFinalizePrimitiveTouchRecognizer() == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == static_cast<int>(0x80890003));
    Require(sceSystemGestureInitializePrimitiveTouchRecognizer() == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 1);
}

int main() {
    constexpr int invalidArgument = static_cast<int>(0x80890001);
    constexpr int invalidHandle = static_cast<int>(0x80890006);
    Require(sceSystemGestureOpen(0, nullptr) == static_cast<int>(0x80890003));
    Require(sceSystemGestureInitializePrimitiveTouchRecognizer() == 0);
    Require(sceSystemGestureOpen(1, nullptr) == invalidArgument);
    const std::int32_t handle = sceSystemGestureOpen(0, nullptr);

    std::array<unsigned char, 64> buffer;
    buffer.fill(0x5a);
    const auto original = buffer;
    std::uint32_t count = 7;
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, buffer.data(), 4, &count) == 0);
    Require(count == 0);
    Require(buffer == original);

    count = 7;
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle, buffer.data(), 4, nullptr) == invalidArgument);
    Require(sceSystemGestureGetPrimitiveTouchEvents(handle + 1, buffer.data(), 4, &count) == invalidHandle);
    Require(count == 7);

    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle + 1) == invalidHandle);

    constexpr int indexOutOfArray = static_cast<int>(0x80890004);
    SystemGestureTouchRecognizer recognizer{};
    SystemGestureRectangle rectangle{};
    count = 7;
    Require(sceSystemGestureGetTouchEvents(handle, &recognizer, buffer.data(), 4, &count) == 0 && count == 0);
    Require(sceSystemGestureGetTouchEvents(handle, &recognizer, buffer.data(), 4, nullptr) == invalidArgument);
    Require(sceSystemGestureGetTouchEvents(handle + 1, &recognizer, buffer.data(), 4, &count) == invalidHandle);
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle, 0, buffer.data()) == indexOutOfArray);
    Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle + 1, 0, buffer.data()) == invalidHandle);
    Require(sceSystemGestureGetTouchEventByIndex(handle, &recognizer, 0, buffer.data()) == indexOutOfArray);
    Require(sceSystemGestureGetTouchEventByIndex(handle + 1, &recognizer, 0, buffer.data()) == invalidHandle);
    Require(sceSystemGestureGetTouchEventByIndex(handle, nullptr, 0, buffer.data()) == invalidArgument);
    Require(sceSystemGestureGetTouchEventByEventID(handle, &recognizer, 0, buffer.data()) == indexOutOfArray);
    Require(sceSystemGestureGetTouchEventByEventID(handle + 1, &recognizer, 0, buffer.data()) == invalidHandle);
    Require(sceSystemGestureGetTouchEventByEventID(handle, nullptr, 0, buffer.data()) == invalidArgument);
    Require(buffer == original);

    Require(sceSystemGestureAppendTouchRecognizer(handle, &recognizer) == 0);
    Require(sceSystemGestureRemoveTouchRecognizer(handle, &recognizer) == 0);
    Require(sceSystemGestureResetTouchRecognizer(handle, &recognizer) == 0);
    Require(sceSystemGestureUpdateTouchRecognizer(handle, &recognizer) == 0);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, &recognizer, &rectangle) == 0);
    Require(sceSystemGestureResetPrimitiveTouchRecognizer(handle) == 0);
    Require(sceSystemGestureUpdateAllTouchRecognizer(handle) == 0);
    Require(sceSystemGestureUpdatePrimitiveTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureAppendTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, &recognizer, nullptr) == invalidArgument);
    Require(sceSystemGestureAppendTouchRecognizer(handle + 1, &recognizer) == invalidHandle);
    Require(sceSystemGestureRemoveTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureResetTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureUpdateTouchRecognizer(handle, nullptr) == invalidArgument);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle, nullptr, &rectangle) == invalidArgument);
    Require(sceSystemGestureRemoveTouchRecognizer(handle + 1, &recognizer) == invalidHandle);
    Require(sceSystemGestureResetTouchRecognizer(handle + 1, &recognizer) == invalidHandle);
    Require(sceSystemGestureUpdateTouchRecognizer(handle + 1, &recognizer) == invalidHandle);
    Require(sceSystemGestureUpdateTouchRecognizerRectangle(handle + 1, &recognizer, &rectangle) == invalidHandle);
    Require(sceSystemGestureResetPrimitiveTouchRecognizer(handle + 1) == invalidHandle);
    Require(sceSystemGestureUpdateAllTouchRecognizer(handle + 1) == invalidHandle);
    Require(sceSystemGestureUpdatePrimitiveTouchRecognizer(handle + 1, nullptr) == invalidArgument);

    Require(sceSystemGestureDebugGetVersion() == 0x6a00);
    TestTouchRecognizerInformation(handle, invalidArgument, invalidHandle);
    TestPrimitiveTouchEvents(handle, invalidArgument, invalidHandle, indexOutOfArray);
    std::array<std::int32_t, 7> otherHandles{};
    for (auto& otherHandle : otherHandles) {
        otherHandle = sceSystemGestureOpen(0, nullptr);
        Require(otherHandle > 0 && otherHandle != handle);
        Require(sceSystemGestureGetPrimitiveTouchEventsCount(otherHandle) == 0);
    }
    Require(sceSystemGestureOpen(0, nullptr) == static_cast<int>(0x8089000A));
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 1);
    Require(sceSystemGestureClose(otherHandles[0]) == 0);
    Require(sceSystemGestureClose(otherHandles[0]) == invalidHandle);
    const auto replacement = sceSystemGestureOpen(0, nullptr);
    Require(replacement > 0 && replacement != otherHandles[0]);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(otherHandles[0]) == invalidHandle);
    Require(sceSystemGestureClose(replacement) == 0);
    for (std::size_t index = 1; index < otherHandles.size(); ++index) Require(sceSystemGestureClose(otherHandles[index]) == 0);
    Require(sceSystemGestureClose(handle) == 0);
    Require(sceSystemGestureFinalizePrimitiveTouchRecognizer() == 0);
    return 0;
}
