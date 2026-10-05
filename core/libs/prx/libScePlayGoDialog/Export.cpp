#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(PlayGoDialogResult) == 40);

namespace {

constexpr std::int32_t STATUS_NONE = 0;
constexpr std::int32_t STATUS_INITIALIZED = 1;
constexpr std::int32_t STATUS_RUNNING = 2;
constexpr std::int32_t STATUS_FINISHED = 3;

constexpr std::int32_t RESULT_OK = 0;

constexpr std::int32_t ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x80ED0001);
constexpr std::int32_t ERROR_ALREADY_INITIALIZED = static_cast<std::int32_t>(0x80ED0002);
constexpr std::int32_t ERROR_PARAM_INVALID = static_cast<std::int32_t>(0x80ED0003);
constexpr std::int32_t ERROR_INVALID_STATE = static_cast<std::int32_t>(0x80ED0005);

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
    if (!param) return ERROR_PARAM_INVALID;
    int expected = STATUS_INITIALIZED;
    if (!g_status.compare_exchange_strong(expected, STATUS_FINISHED)) {
        expected = STATUS_FINISHED;
        if (!g_status.compare_exchange_strong(expected, STATUS_FINISHED)) return ERROR_INVALID_STATE;
    }
    return 0;
}

int APS5_VABI scePlayGoDialogClose(void) {
    if (g_status.load() == STATUS_NONE) return ERROR_NOT_INITIALIZED;
    g_status.store(STATUS_FINISHED);
    return 0;
}

int APS5_VABI scePlayGoDialogUpdateStatus(void) {
    int expected = STATUS_RUNNING;
    g_status.compare_exchange_strong(expected, STATUS_FINISHED);
    return g_status.load();
}

int APS5_VABI scePlayGoDialogGetResult(void* result) {
    if (g_status.load() == STATUS_NONE) return ERROR_NOT_INITIALIZED;
    if (!result) return ERROR_PARAM_INVALID;
    auto* r = static_cast<PlayGoDialogResult*>(result);
    std::memset(r, 0, sizeof(*r));
    r->result = RESULT_OK;
    return 0;
}

int APS5_VABI scePlayGoDialogGetStatus(void) {
    return g_status.load();
}

}
