#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceVideoOut/include/DialogInput.hpp"
#include "SDL.h"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#undef main

namespace {

constexpr unsigned Window = 7;

void Require(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "dialog input: %s\n", reason);
        std::abort();
    }
}

SDL_Event Key(SDL_Scancode scancode, unsigned window = Window, std::uint8_t repeat = 0, std::uint16_t mod = 0) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.windowID = window;
    event.key.repeat = repeat;
    event.key.keysym.scancode = scancode;
    event.key.keysym.mod = mod;
    return event;
}

SDL_Event Wheel(int y, unsigned window = Window) {
    SDL_Event event{};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.windowID = window;
    event.wheel.y = y;
    return event;
}

SDL_Event Button(std::uint8_t button, unsigned window = Window) {
    SDL_Event event{};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = window;
    event.button.button = button;
    return event;
}

std::size_t Focus() {
    ModalDialogView view;
    Require(ModalDialogReadView_nid_postfix(view), "no dialog is shown");
    for (std::size_t index = 0; index < view.buttons.size(); ++index) {
        if (view.buttons[index].selected) return index;
    }
    return ModalDialogNoButton;
}

ModalDialogState State(std::size_t* chosen = nullptr) {
    std::size_t value = 0;
    const auto state = ModalDialogPoll_nid_postfix(value);
    if (chosen != nullptr) *chosen = value;
    return state;
}

void Open() {
    ModalDialogRequest request;
    request.message = u"Choose";
    request.buttons = {u"A", u"B", u"C"};
    request.focus = 0;
    request.cancel = 2;
    Require(ModalDialogOpen_nid_postfix(request), "the dialog did not open");
}

}

int main() {
    DialogInput input;
    ModalDialogSetPresenter_nid_postfix(true);

    input.HandleEvent(Key(SDL_SCANCODE_RETURN), Window);
    Require(State() == ModalDialogState::Idle, "a key answered a dialog that is not open");

    Open();
    input.HandleEvent(Key(SDL_SCANCODE_RIGHT), Window);
    Require(Focus() == 1, "Right did not focus the next button");
    input.HandleEvent(Key(SDL_SCANCODE_TAB), Window);
    Require(Focus() == 2, "Tab did not focus the next button");
    input.HandleEvent(Key(SDL_SCANCODE_TAB, Window, 0, KMOD_LSHIFT), Window);
    Require(Focus() == 1, "Shift+Tab did not focus the previous button");
    input.HandleEvent(Key(SDL_SCANCODE_UP), Window);
    Require(Focus() == 0, "Up did not focus the previous button");
    input.HandleEvent(Key(SDL_SCANCODE_DOWN), Window);
    Require(Focus() == 1, "Down did not focus the next button");
    input.HandleEvent(Key(SDL_SCANCODE_LEFT), Window);
    Require(Focus() == 0, "Left did not focus the previous button");
    input.HandleEvent(Wheel(-1), Window);
    Require(Focus() == 1, "the wheel down did not focus the next button");
    input.HandleEvent(Wheel(1), Window);
    Require(Focus() == 0, "the wheel up did not focus the previous button");
    input.HandleEvent(Wheel(0), Window);
    Require(Focus() == 0, "a zero wheel step moved the focus");

    input.HandleEvent(Key(SDL_SCANCODE_RIGHT, Window + 1), Window);
    input.HandleEvent(Wheel(-1, Window + 1), Window);
    input.HandleEvent(Button(SDL_BUTTON_LEFT, Window + 1), Window);
    input.HandleEvent(Key(SDL_SCANCODE_A), Window);
    input.HandleEvent(Button(SDL_BUTTON_MIDDLE), Window);
    Require(Focus() == 0 && State() == ModalDialogState::Showing, "an event for another window or an unmapped event reached the dialog");

    input.HandleEvent(Key(SDL_SCANCODE_RETURN, Window, 1), Window);
    input.HandleEvent(Key(SDL_SCANCODE_ESCAPE, Window, 1), Window);
    Require(State() == ModalDialogState::Showing, "a repeated Enter or Escape answered the dialog");

    input.HandleEvent(Key(SDL_SCANCODE_RETURN), Window);
    std::size_t chosen = 9;
    Require(State(&chosen) == ModalDialogState::Answered && chosen == 0, "Enter did not choose the focused button");

    Open();
    input.HandleEvent(Key(SDL_SCANCODE_RIGHT), Window);
    input.HandleEvent(Key(SDL_SCANCODE_SPACE), Window);
    Require(State(&chosen) == ModalDialogState::Answered && chosen == 1, "Space did not choose the focused button");

    Open();
    input.HandleEvent(Key(SDL_SCANCODE_ESCAPE), Window);
    Require(State(&chosen) == ModalDialogState::Answered && chosen == 2, "Escape did not choose the cancel button");

    Open();
    input.HandleEvent(Button(SDL_BUTTON_RIGHT), Window);
    Require(State(&chosen) == ModalDialogState::Answered && chosen == 2, "a right click did not choose the cancel button");

    Open();
    input.HandleEvent(Wheel(-1), Window);
    input.HandleEvent(Button(SDL_BUTTON_LEFT), Window);
    Require(State(&chosen) == ModalDialogState::Answered && chosen == 1, "a left click did not choose the focused button");

    Open();
    input.HandleEvent(Key(SDL_SCANCODE_KP_ENTER), Window);
    Require(State(&chosen) == ModalDialogState::Answered && chosen == 0, "the keypad Enter did not choose the focused button");
    ModalDialogSetPresenter_nid_postfix(false);
}
