#include "Export.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <cstring>

namespace AnyPS5::Libs::PatchCheckerClient {

static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_autoDownloadEnabled{false};

constexpr size_t MAX_HANDLES = 64;
struct HandlerInfo {
    bool active = false;
    int userId = 0;
};

static HandlerInfo g_handles[MAX_HANDLES];
static std::mutex g_handleMutex;

constexpr int SCE_OK = 0;
constexpr int SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED = 0x80020001;
constexpr int SCE_PATCH_CHECKER_ERROR_INVALID_ARG     = 0x80020016;
constexpr int SCE_PATCH_CHECKER_ERROR_BAD_FILDES      = 0x80020009;
constexpr int SCE_PATCH_CHECKER_ERROR_OUT_OF_MEMORY   = 0x8002000c;

int scePatchCheckerInitialize() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        return SCE_OK;
    }
    return SCE_OK;
}

int scePatchCheckerTerminate() {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }

    std::lock_guard<std::mutex> lock(g_handleMutex);
    for (size_t i = 0; i < MAX_HANDLES; ++i) {
        g_handles[i].active = false;
    }

    return SCE_OK;
}

int scePatchCheckerEnableAutoDownload() {
    if (!g_initialized.load()) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }
    g_autoDownloadEnabled.store(true);
    return SCE_OK;
}

int scePatchCheckerDisableAutoDownload() {
    if (!g_initialized.load()) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }
    g_autoDownloadEnabled.store(false);
    return SCE_OK;
}

int scePatchCheckerCreateHandler(int userId) {
    if (!g_initialized.load()) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }

    std::lock_guard<std::mutex> lock(g_handleMutex);
    for (size_t i = 0; i < MAX_HANDLES; ++i) {
        if (!g_handles[i].active) {
            g_handles[i].active = true;
            g_handles[i].userId = userId;
            return static_cast<int>(i + 1);
        }
    }

    return SCE_PATCH_CHECKER_ERROR_OUT_OF_MEMORY;
}

int scePatchCheckerDestroyHandler(int handle) {
    if (!g_initialized.load()) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }

    if (handle <= 0 || static_cast<size_t>(handle) > MAX_HANDLES) {
        return SCE_PATCH_CHECKER_ERROR_BAD_FILDES;
    }

    std::lock_guard<std::mutex> lock(g_handleMutex);
    size_t index = static_cast<size_t>(handle - 1);
    if (!g_handles[index].active) {
        return SCE_PATCH_CHECKER_ERROR_BAD_FILDES;
    }

    g_handles[index].active = false;
    return SCE_OK;
}

int scePatchCheckerCancel(int handle) {
    if (!g_initialized.load()) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }

    if (handle <= 0 || static_cast<size_t>(handle) > MAX_HANDLES) {
        return SCE_PATCH_CHECKER_ERROR_BAD_FILDES;
    }

    std::lock_guard<std::mutex> lock(g_handleMutex);
    size_t index = static_cast<size_t>(handle - 1);
    if (!g_handles[index].active) {
        return SCE_PATCH_CHECKER_ERROR_BAD_FILDES;
    }

    return SCE_OK;
}

int scePatchCheckerCheckPatch(int handle) {
    if (!g_initialized.load()) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }
    if (handle <= 0 || static_cast<size_t>(handle) > MAX_HANDLES) {
        return SCE_PATCH_CHECKER_ERROR_BAD_FILDES;
    }
    return SCE_OK;
}

int scePatchCheckerRequestCheckPatch(int handle) {
    if (!g_initialized.load()) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }
    if (handle <= 0 || static_cast<size_t>(handle) > MAX_HANDLES) {
        return SCE_PATCH_CHECKER_ERROR_BAD_FILDES;
    }
    return SCE_OK;
}

int scePatchCheckerGetPackageInfo(int handle, void* outInfo) {
    if (!g_initialized.load()) {
        return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    }
    if (!outInfo) {
        return SCE_PATCH_CHECKER_ERROR_INVALID_ARG;
    }
    if (handle <= 0 || static_cast<size_t>(handle) > MAX_HANDLES) {
        return SCE_PATCH_CHECKER_ERROR_BAD_FILDES;
    }

    std::memset(outInfo, 0, sizeof(void*)); 
    return SCE_OK;
}

}