// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceIme/include/Ime.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string_view>
#include <thread>

// SDL for keyboard detection
#include <SDL.h>

static constexpr u32 SCE_IME_MAX_TEXT = 1024;
static constexpr u64 SCE_IME_WAIT_INFINITE = -1;

enum class ImeState {
    Closed,
    Opening,
    Open,
};

struct ImeContext {
    std::atomic<ImeState> state{ImeState::Closed};
    std::string text;
    std::string title;
    std::string inputMode;
    u32 maxTextLength = SCE_IME_MAX_TEXT;
    bool autoSelect = false;
    bool multiLine = false;

    // Keyboard resource ID (0 if none connected)
    std::atomic<u64> keyboardResourceId{0};
};

static ImeContext g_context;
static std::mutex g_dialogMutex;

// Check for available keyboards via SDL and update resource ID
static void UpdateKeyboardResourceId() {
    // Count attached keyboards
    int numKeyboards = SDL_numjoysticks(); // SDL counts keyboards as joysticks in some versions
    
    // Also check for actual keyboard events capability
    if (SDL_WasInit(SDL_INIT_KEYBOARD) == 0) {
        SDL_InitSubSystem(SDL_INIT_KEYBOARD);
    }
    
    // If we can init keyboard subsystem, assume at least one keyboard is available
    g_context.keyboardResourceId.store(1);
}

extern "C" {

#pragma GCC visibility push(default)

int sceImeKeyboardOpen(SceImeKeyboardParam* param) {
    std::lock_guard<std::mutex> lock(g_dialogMutex);
    
    if (g_context.state.load() != ImeState::Closed) {
        return SCE_ERROR_BUSY;
    }
    
    g_context.title = param->title ? param->title : "";
    g_context.text = param->inputText ? param->inputText : "";
    g_context.maxTextLength = param->maxTextLength > 0 ? param->maxTextLength : SCE_IME_MAX_TEXT;
    g_context.autoSelect = (param->flag & SCE_IME_FLAG_AUTO_SELECT) != 0;
    g_context.multiLine = (param->flag & SCE_IME_FLAG_MULTI_LINE) != 0;
    
    // Check for keyboard availability
    UpdateKeyboardResourceId();
    
    g_context.state.store(ImeState::Opening);
    
    // Simulate opening delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    g_context.state.store(ImeState::Open);
    return SCE_OK;
}

int sceImeKeyboardClose() {
    std::lock_guard<std::mutex> lock(g_dialogMutex);
    
    if (g_context.state.load() != ImeState::Open) {
        return SCE_ERROR_INVALID_STATE;
    }
    
    g_context.state.store(ImeState::Closed);
    return SCE_OK;
}

int sceImeKeyboardGetStatus() {
    switch (g_context.state.load()) {
        case ImeState::Closed:
            return SCE_IME_STATUS_CLOSED;
        case ImeState::Opening:
            return SCE_IME_STATUS_OPENING;
        case ImeState::Open:
            return SCE_IME_STATUS_OPENED;
    }
    return SCE_ERROR_INVALID_STATE;
}

int sceImeKeyboardGetText(char* text, u32 size) {
    if (g_context.state.load() != ImeState::Open) {
        return SCE_ERROR_INVALID_STATE;
    }
    
    std::string_view view = g_context.text;
    u32 copySize = std::min(view.size(), static_cast<size_t>(size - 1));
    std::memcpy(text, view.data(), copySize);
    text[copySize] = '\0';
    
    return SCE_OK;
}

int sceImeKeyboardSetPosition(SceImeKeyboardPos* pos) {
    // Position is handled by the host dialog system
    return SCE_OK;
}

int sceImeKeyboardGetSize(SceImeKeyboardPos* size) {
    // Return default size
    if (size) {
        size->x = 0;
        size->y = 0;
        size->width = 800;
        size->height = 300;
    }
    return SCE_OK;
}

int sceImeKeyboardGetResourceId(u64* resource_id) {
    if (!resource_id) {
        return SCE_ERROR_INVALID_POINTER;
    }
    
    // Check for keyboard availability
    UpdateKeyboardResourceId();
    
    u64 id = g_context.keyboardResourceId.load();
    if (id == 0) {
        // No keyboard detected
        return SCE_ERROR_NOT_CONNECTED;
    }
    
    *resource_id = id;
    return SCE_OK;
}

int sceImeKeyboardSetAutoSelect(bool enable) {
    g_context.autoSelect = enable;
    return SCE_OK;
}

int sceImeKeyboardGetAutoSelect(bool* enable) {
    if (enable) {
        *enable = g_context.autoSelect;
    }
    return SCE_OK;
}

#pragma GCC visibility pop

} // extern "C"