#include "prx/libSceKeyboard/libSceKeyboard.hpp"
#include "prx/libSceKeyboard/include/keyboard_structs.h"

#include <cstdio>
#include <initializer_list>
#include <cstdlib>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    Require(sceDbgKeyboardOpen(1, 0, 0, nullptr) == KEYBOARD_ERROR_NOT_INITIALIZED, "the debug open must answer as sceKeyboardOpen before initialization");
    Require(sceDbgKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_ERROR_NOT_INITIALIZED, "the debug close must answer as sceKeyboardClose before initialization");
    KeyboardData data[2]{};
    KeyboardCharData charData{};
    Require(sceDbgKeyboardRead(KEYBOARD_HANDLE, data, 1) == sceKeyboardRead(KEYBOARD_HANDLE, data, 1), "the debug read must answer as sceKeyboardRead before initialization");
    Require(sceDbgKeyboardReadState(KEYBOARD_HANDLE, data) == sceKeyboardReadState(KEYBOARD_HANDLE, data), "the debug read state must answer as sceKeyboardReadState before initialization");
    Require(sceDbgKeyboardGetHandle(1, 0, 0) == sceKeyboardGetHandle(1, 0, 0), "the debug handle lookup must answer as sceKeyboardGetHandle before initialization");
    Require(sceDbgKeyboardInit() == KEYBOARD_OK, "the debug initialization must succeed as sceKeyboardInit does");
    Require(sceKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_ERROR_INVALID_HANDLE, "the debug initialization must initialize sceKeyboardClose's state");
    Require(sceDbgKeyboardClose(KEYBOARD_HANDLE) == KEYBOARD_ERROR_INVALID_HANDLE, "the debug close must check the handle as sceKeyboardClose does");
    Require(sceDbgKeyboardOpen(1, 1, 0, nullptr) == KEYBOARD_ERROR_INVALID_ARG, "the debug open must check the type as sceKeyboardOpen does");
    for (const int num : {0, 1, 16, 17}) {
        Require(sceDbgKeyboardRead(KEYBOARD_HANDLE, data, num) == sceKeyboardRead(KEYBOARD_HANDLE, data, num), "the debug read must check as sceKeyboardRead does");
    }
    Require(sceDbgKeyboardRead(KEYBOARD_HANDLE, nullptr, 1) == sceKeyboardRead(KEYBOARD_HANDLE, nullptr, 1), "the debug read must check a null buffer as sceKeyboardRead does");
    Require(sceDbgKeyboardReadState(KEYBOARD_HANDLE, nullptr) == sceKeyboardReadState(KEYBOARD_HANDLE, nullptr), "the debug read state must check a null buffer as sceKeyboardReadState does");
    Require(sceDbgKeyboardReadState(KEYBOARD_HANDLE, data) == sceKeyboardReadState(KEYBOARD_HANDLE, data), "the debug read state must answer as sceKeyboardReadState");
    Require(sceDbgKeyboardGetHandle(1, 0, 0) == sceKeyboardGetHandle(1, 0, 0), "the debug handle lookup must answer as sceKeyboardGetHandle");
    Require(sceDbgKeyboardGetHandle(1, 1, 0) == sceKeyboardGetHandle(1, 1, 0), "the debug handle lookup must check the type as sceKeyboardGetHandle does");
    Require(sceDbgKeyboardGetKey2Char(KEYBOARD_HANDLE, 0, 0, 0, 4, &charData) == sceKeyboardGetKey2Char(KEYBOARD_HANDLE, 0, 0, 0, 4, &charData), "the debug key conversion must answer as sceKeyboardGetKey2Char");
    Require(sceDbgKeyboardGetKey2Char(KEYBOARD_HANDLE, 0, 0, 0, 4, nullptr) == sceKeyboardGetKey2Char(KEYBOARD_HANDLE, 0, 0, 0, 4, nullptr), "the debug key conversion must check its output as sceKeyboardGetKey2Char does");
    Require(sceKeyboardInit() == KEYBOARD_OK && sceDbgKeyboardInit() == KEYBOARD_OK, "repeated initialization must succeed through both names");
    return 0;
}
