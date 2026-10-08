// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceCdlgPlayerReview/include/PlayerReview.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

enum class PlayerReviewState {
    Closed,
    Opening,
    Open,
};

struct PlayerReviewContext {
    std::atomic<PlayerReviewState> state{PlayerReviewState::Closed};
    u32 result = SCE_CDLG_PLAYERREVIEW_RESULT_CANCEL;
    u32 rating = 0;
    bool submitted = false;
};

static PlayerReviewContext g_context;
static std::mutex g_dialogMutex;

extern "C" {

#pragma GCC visibility push(default)

int scePlayerReviewDialogOpen(SceCdlgPlayerReviewParam* param) {
    std::lock_guard<std::mutex> lock(g_dialogMutex);
    
    if (g_context.state.load() != PlayerReviewState::Closed) {
        return SCE_ERROR_BUSY;
    }
    
    g_context.result = SCE_CDLG_PLAYERREVIEW_RESULT_CANCEL;
    g_context.rating = 0;
    g_context.submitted = false;
    
    g_context.state.store(PlayerReviewState::Opening);
    
    // Simulate dialog opening delay
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    g_context.state.store(PlayerReviewState::Open);
    return SCE_OK;
}

int scePlayerReviewDialogClose() {
    std::lock_guard<std::mutex> lock(g_dialogMutex);
    
    if (g_context.state.load() != PlayerReviewState::Open) {
        return SCE_ERROR_INVALID_STATE;
    }
    
    g_context.state.store(PlayerReviewState::Closed);
    return SCE_OK;
}

int scePlayerReviewDialogGetStatus() {
    switch (g_context.state.load()) {
        case PlayerReviewState::Closed:
            return SCE_CDLG_STATUS_CLOSED;
        case PlayerReviewState::Opening:
            return SCE_CDLG_STATUS_OPENING;
        case PlayerReviewState::Open:
            return SCE_CDLG_STATUS_OPENED;
    }
    return SCE_ERROR_INVALID_STATE;
}

int scePlayerReviewDialogGetResult(SceCdlgPlayerReviewResult* result) {
    if (g_context.state.load() != PlayerReviewState::Open) {
        return SCE_ERROR_INVALID_STATE;
    }
    
    // Simulate user interaction: assume user rates and submits
    // In a real implementation, this would wait for actual user input
    
    std::memset(result, 0, sizeof(SceCdlgPlayerReviewResult));
    
    if (!g_context.submitted) {
        // First call: simulate user giving a rating
        result->result = SCE_CDLG_PLAYERREVIEW_RESULT_OK;
        result->rating = 5; // Default to 5-star rating
        g_context.rating = 5;
    } else {
        // Already submitted
        result->result = SCE_CDLG_PLAYERREVIEW_RESULT_OK;
        result->rating = g_context.rating;
    }
    
    return SCE_OK;
}

int scePlayerReviewDialogSubmit() {
    std::lock_guard<std::mutex> lock(g_dialogMutex);
    
    if (g_context.state.load() != PlayerReviewState::Open) {
        return SCE_ERROR_INVALID_STATE;
    }
    
    g_context.submitted = true;
    g_context.result = SCE_CDLG_PLAYERREVIEW_RESULT_OK;
    
    // Simulate submission delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return SCE_OK;
}

#pragma GCC visibility pop

} // extern "C"