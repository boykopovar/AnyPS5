#include "prx/libSceMouse/include/mouse_structs.h"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "SDL_events.h"
#include "SDL_mouse.h"

#include <Testing/Test.hpp>

extern "C" {
int APS5_VABI sceMouseInit();
int APS5_VABI sceMouseOpen(int, std::int32_t, std::int32_t, const void*);
int APS5_VABI sceMouseClose(std::int32_t);
int APS5_VABI sceMouseRead(std::int32_t, MouseData*, std::int32_t);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr unsigned windowId = 7;

class MouseSession {
public:
    explicit MouseSession(const MouseOpenParam* param = nullptr) {
        RequireEqual(sceMouseInit(), MOUSE_OK, "initialize the mouse library");
        RequireEqual(sceMouseOpen(1, 0, 0, param), MOUSE_HANDLE, "open the mouse");
    }

    ~MouseSession() {
        MouseInputEvent reconnect{};
        reconnect.connectionChange = true;
        reconnect.connected = true;
        MousePublishInput_nid_postfix(reconnect);
        sceMouseClose(MOUSE_HANDLE);
    }

    MouseSession(const MouseSession&) = delete;
    MouseSession& operator=(const MouseSession&) = delete;

    void DrainInitialSample() {
        MouseData initial{};
        RequireEqual(sceMouseRead(MOUSE_HANDLE, &initial, 1), 1, "read the initial sample");
    }
};

SDL_Event Motion(int xrel, int yrel, unsigned window = windowId) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = window;
    event.motion.xrel = xrel;
    event.motion.yrel = yrel;
    return event;
}

SDL_Event ButtonDown(Uint8 button) {
    SDL_Event event{};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = windowId;
    event.button.button = button;
    return event;
}

SDL_Event Wheel(int x, int y, Uint32 direction) {
    SDL_Event event{};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.windowID = windowId;
    event.wheel.x = x;
    event.wheel.y = y;
    event.wheel.direction = direction;
    return event;
}

SDL_Event Window(SDL_WindowEventID windowEvent) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = windowId;
    event.window.event = windowEvent;
    return event;
}

void RequireNoSample(const char* message) {
    MouseData data{};
    RequireEqual(sceMouseRead(MOUSE_HANDLE, &data, 1), 0, message);
}

MouseData ReadOne(const char* message) {
    MouseData data{};
    RequireEqual(sceMouseRead(MOUSE_HANDLE, &data, 1), 1, message);
    return data;
}

const Case openBeforeInit{"Open_BeforeInit_FailsNotInitialized", [] {
    RequireEqual(sceMouseOpen(1, 0, 0, nullptr), MOUSE_ERROR_NOT_INITIALIZED, "open before init");
}};

const Case initTwice{"Init_CalledTwice_Succeeds", [] {
    RequireEqual(sceMouseInit(), MOUSE_OK, "first init");
    RequireEqual(sceMouseInit(), MOUSE_OK, "second init");
}};

const Case openInvalidIndex{"Open_NonZeroIndex_FailsInvalidArg", [] {
    RequireEqual(sceMouseInit(), MOUSE_OK, "init");
    RequireEqual(sceMouseOpen(1, 0, 1, nullptr), MOUSE_ERROR_INVALID_ARG, "index 1");
}};

const Case openUnsupportedBehavior{"Open_UnsupportedBehaviorFlag_FailsInvalidArg", [] {
    RequireEqual(sceMouseInit(), MOUSE_OK, "init");
    MouseOpenParam unsupported{};
    unsupported.behaviorFlag = 2;
    RequireEqual(sceMouseOpen(1, 0, 0, &unsupported), MOUSE_ERROR_INVALID_ARG, "behavior flag 2");
}};

const Case openTwice{"Open_AlreadyOpened_FailsAlreadyOpened", [] {
    const MouseSession session;
    RequireEqual(sceMouseOpen(1, 0, 0, nullptr), MOUSE_ERROR_ALREADY_OPENED, "second open");
}};

const Case readArguments{"Read_InvalidHandleOrArguments_Fails", [] {
    const MouseSession session;
    MouseData data[MOUSE_MAX_DATA_NUM + 1]{};
    RequireEqual(sceMouseRead(42, data, 1), MOUSE_ERROR_INVALID_HANDLE, "unknown handle");
    RequireEqual(sceMouseRead(MOUSE_HANDLE, nullptr, 1), MOUSE_ERROR_INVALID_ARG, "null buffer");
    RequireEqual(sceMouseRead(MOUSE_HANDLE, data, MOUSE_MAX_DATA_NUM + 1), MOUSE_ERROR_INVALID_ARG, "too many samples");
}};

const Case openInitialSample{"Open_Fresh_QueuesOneConnectedSampleWithoutButtons", [] {
    const MouseSession session;
    const auto initial = ReadOne("initial sample");
    Require(initial.connected, "initial sample is connected");
    RequireEqual(initial.buttons, 0u, "initial buttons");
    RequireNoSample("queue empty after the initial sample");
}};

const Case otherWindow{"HandleEvent_OtherWindow_IsIgnored", [] {
    MouseSession session;
    session.DrainInitialSample();
    MouseInput input;
    input.HandleEvent(Motion(9, -4), 8);
    RequireNoSample("motion for window 7 while listening to window 8");
}};

const Case motionButtonWheel{"HandleEvent_MotionButtonAndWheel_QueueSamplesInOrder", [] {
    MouseSession session;
    session.DrainInitialSample();
    MouseInput input;
    input.HandleEvent(Motion(9, -4), windowId);
    input.HandleEvent(ButtonDown(SDL_BUTTON_RIGHT), windowId);
    input.HandleEvent(Wheel(2, -3, SDL_MOUSEWHEEL_FLIPPED), windowId);
    MouseData data[3]{};
    RequireEqual(sceMouseRead(MOUSE_HANDLE, data, 3), 3, "three samples");
    RequireEqual(data[0].x_axis, 9, "motion x");
    RequireEqual(data[0].y_axis, -4, "motion y");
    RequireEqual(data[0].buttons, 0u, "motion buttons");
    RequireEqual(data[1].buttons, 2u, "right button down");
    RequireEqual(data[2].buttons, 2u, "wheel keeps the right button");
    RequireEqual(data[2].wheel, 3, "flipped wheel");
    RequireEqual(data[2].tilt, -2, "flipped tilt");
    Require(data[0].timestamp <= data[2].timestamp, "timestamps are monotonic");
}};

const Case unsupportedButton{"HandleEvent_UnsupportedButton_IsIgnored", [] {
    MouseSession session;
    session.DrainInitialSample();
    MouseInput input;
    input.HandleEvent(ButtonDown(SDL_BUTTON_X1), windowId);
    RequireNoSample("X1 button");
}};

const Case focusLost{"HandleEvent_FocusLost_ReleasesButtonsAndDropsInput", [] {
    MouseSession session;
    session.DrainInitialSample();
    MouseInput input;
    input.HandleEvent(ButtonDown(SDL_BUTTON_RIGHT), windowId);
    ReadOne("right button down");
    input.HandleEvent(Window(SDL_WINDOWEVENT_FOCUS_LOST), windowId);
    const auto released = ReadOne("focus lost sample");
    Require(released.connected, "still connected after focus loss");
    RequireEqual(released.buttons, 0u, "buttons released on focus loss");
    input.HandleEvent(Motion(5, 0), windowId);
    RequireNoSample("motion while unfocused");
}};

const Case focusGained{"HandleEvent_FocusGainedWhileConnected_QueuesNothing", [] {
    MouseSession session;
    session.DrainInitialSample();
    MouseInput input;
    input.HandleEvent(Window(SDL_WINDOWEVENT_FOCUS_LOST), windowId);
    ReadOne("focus lost sample");
    input.HandleEvent(Window(SDL_WINDOWEVENT_FOCUS_GAINED), windowId);
    RequireNoSample("focus regained while connected");
}};

const Case closeAndReconnect{"HandleEvent_WindowCloseThenFocus_DisconnectsThenReconnects", [] {
    MouseSession session;
    session.DrainInitialSample();
    MouseInput input;
    input.HandleEvent(Window(SDL_WINDOWEVENT_CLOSE), windowId);
    Require(!ReadOne("close sample").connected, "disconnected after window close");
    input.HandleEvent(Window(SDL_WINDOWEVENT_FOCUS_GAINED), windowId);
    Require(ReadOne("reconnect sample").connected, "connected after focus gained");
}};

const Case overflow{"Publish_MoreThanCapacity_KeepsNewestSamples", [] {
    MouseSession session;
    session.DrainInitialSample();
    for (int i = 0; i < 70; ++i) {
        MouseInputEvent motion{};
        motion.x = i;
        MousePublishInput_nid_postfix(motion);
    }
    MouseData data[MOUSE_MAX_DATA_NUM]{};
    RequireEqual(sceMouseRead(MOUSE_HANDLE, data, MOUSE_MAX_DATA_NUM), MOUSE_MAX_DATA_NUM, "full queue");
    RequireEqual(data[0].x_axis, 6, "oldest kept sample");
    RequireEqual(data[MOUSE_MAX_DATA_NUM - 1].x_axis, 69, "newest sample");
    RequireNoSample("queue drained");
}};

const Case closeHandle{"Close_OpenHandle_InvalidatesHandle", [] {
    const MouseSession session;
    RequireEqual(sceMouseClose(MOUSE_HANDLE), MOUSE_OK, "close");
    MouseData data{};
    RequireEqual(sceMouseRead(MOUSE_HANDLE, &data, 1), MOUSE_ERROR_INVALID_HANDLE, "read after close");
    RequireEqual(sceMouseClose(MOUSE_HANDLE), MOUSE_ERROR_INVALID_HANDLE, "second close");
}};

const Case reopenMerged{"Open_MergedAfterClose_QueuesFreshSample", [] {
    {
        MouseSession first;
        MouseInput input;
        input.HandleEvent(ButtonDown(SDL_BUTTON_LEFT), windowId);
    }
    MouseOpenParam merged{};
    merged.behaviorFlag = MOUSE_OPEN_PARAM_MERGED;
    const MouseSession session(&merged);
    RequireEqual(ReadOne("initial sample after reopen").buttons, 0u, "buttons reset by reopen");
}};

} // namespace
