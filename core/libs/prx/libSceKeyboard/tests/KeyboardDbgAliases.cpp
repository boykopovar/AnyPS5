#include "prx/libSceKeyboard/libSceKeyboard.hpp"
#include "prx/libSceKeyboard/include/keyboard_structs.h"

#include <cstdio>
#include <cstdlib>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    Require(sceDbgKeyboardOpen(1, 0, 0, nullptr) == KEYBOARD_ERROR_NOT_INITIALIZED, "the debug open must answer as sceKeyboardOpen before initialization");
    Require(sceDbgKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_ERROR_NOT_INITIALIZED, "the debug close must answer as sceKeyboardClose before initialization");
    Require(sceDbgKeyboardInit() == KEYBOARD_OK, "the debug initialization must succeed as sceKeyboardInit does");
    Require(sceKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_ERROR_INVALID_HANDLE, "the debug initialization must initialize sceKeyboardClose's state");
    Require(sceDbgKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_ERROR_INVALID_HANDLE, "the debug close must check the handle as sceKeyboardClose does");
    Require(sceDbgKeyboardOpen(1, 1, 0, nullptr) == KEYBOARD_ERROR_INVALID_ARG, "the debug open must check the type as sceKeyboardOpen does");
    Require(sceKeyboardInit() == KEYBOARD_OK && sceDbgKeyboardInit() == KEYBOARD_OK, "repeated initialization must succeed through both names");
    return 0;
}
