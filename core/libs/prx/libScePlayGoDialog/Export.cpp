#include "prx/libScePlayGoDialog/include/Api.hpp"
#include <atomic>
#include <algorithm>
#include <iterator>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(PlayGoDialogResult) == 40);
static_assert(offsetof(PlayGoDialogResult, result) == 4);
static_assert(offsetof(PlayGoDialogResult, reserved) == 8);

namespace {

constexpr std::int32_t STATUS_NONE = 0;
constexpr std::int32_t STATUS_INITIALIZED = 1;
constexpr std::int32_t STATUS_RUNNING = 2;
constexpr std::int32_t STATUS_FINISHED = 3;

constexpr std::int32_t RESULT_OK = 0;
constexpr std::int32_t ERROR_ARG_NULL = static_cast<std::int32_t>(0x80B8000Du);
constexpr std::int32_t ERROR_NOT_RUNNING = static_cast<std::int32_t>(0x80B8000Bu);

constexpr std::int32_t ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x80B80003);
constexpr std::int32_t ERROR_ALREADY_INITIALIZED = static_cast<std::int32_t>(0x80B80004);
constexpr std::int32_t ERROR_PARAM_INVALID = static_cast<std::int32_t>(0x80B8000A);
constexpr std::int32_t ERROR_INVALID_STATE = static_cast<std::int32_t>(0x80B80007);
constexpr std::int32_t COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<std::int32_t>(0x80B80005u);

std::atomic<int> g_status{STATUS_NONE};

}

extern "C" {

int APS5_VABI scePlayGoDialogInitialize(void) {
    int expected = STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, STATUS_INITIALIZED)) return ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI scePlayGoDialogTerminate(void) {
    int expected = STATUS_INITIALIZED;
    if (!g_status.compare_exchange_strong(expected, STATUS_NONE)) {
        expected = STATUS_FINISHED;
        if (!g_status.compare_exchange_strong(expected, STATUS_NONE)) return ERROR_NOT_INITIALIZED;
    }
    return 0;
}

int APS5_VABI scePlayGoDialogOpen(const void* param) {
    if (!param) return ERROR_ARG_NULL;
    if (g_status.load() == STATUS_NONE) return ERROR_NOT_INITIALIZED;
    int expected = STATUS_INITIALIZED;
    if (!g_status.compare_exchange_strong(expected, STATUS_FINISHED)) {
        expected = STATUS_FINISHED;
        if (!g_status.compare_exchange_strong(expected, STATUS_FINISHED)) return ERROR_INVALID_STATE;
    }
    return 0;
}

int APS5_VABI scePlayGoDialogClose(void) {
    if (g_status.load() == STATUS_NONE) return ERROR_NOT_INITIALIZED;
    if (g_status.load() != STATUS_RUNNING) return ERROR_NOT_RUNNING;
    g_status.store(STATUS_FINISHED);
    return 0;
}

int APS5_VABI scePlayGoDialogUpdateStatus(void) {
    int expected = STATUS_RUNNING;
    g_status.compare_exchange_strong(expected, STATUS_FINISHED);
    return g_status.load();
}

int APS5_VABI scePlayGoDialogGetResult(PlayGoDialogResult* result) {
    if (result == nullptr) return ERROR_ARG_NULL;
    if (!std::all_of(std::begin(result->reserved), std::end(result->reserved), [](std::uint8_t value) { return value == 0; })) return ERROR_PARAM_INVALID;
    if (g_status.load() == STATUS_NONE) return ERROR_NOT_INITIALIZED;
    if (g_status.load() != STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    *result = {};
    result->result = RESULT_OK;
    return 0;
}

int APS5_VABI scePlayGoDialogGetStatus(void) {
    return g_status.load();
}

}
