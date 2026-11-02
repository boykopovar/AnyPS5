#include "prx/libSceGameUpdate/include/Api.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80412801);
static constexpr int SCE_GAME_UPDATE_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80412802);
static constexpr int SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80412803);
static constexpr int SCE_GAME_UPDATE_ERROR_INVALID_SIZE = static_cast<int>(0x80412804);
static constexpr int SCE_GAME_UPDATE_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x80412805);
static constexpr int SCE_GAME_UPDATE_ERROR_TOO_MANY_REQUESTS = static_cast<int>(0x80412806);
static constexpr int SCE_GAME_UPDATE_ERROR_ABORTED = static_cast<int>(0x80412807);
static constexpr int FIRST_REQUEST_ID = 0x20000000;
static constexpr int LAST_REQUEST_ID = 0x2FFFFFFF;

static_assert(sizeof(GameUpdateCheckParam) == 0x30);
static_assert(sizeof(GameUpdateCheckResult) == 0x30);

struct Request {
    int id;
    bool aborted;
};

static std::mutex stateMutex;
static bool initialized = false;
static int nextRequestId = FIRST_REQUEST_ID;
static std::array<Request, 8> requests{};

static Request* findRequest(int requestId) {
    for (Request& request : requests) {
        if (request.id != 0 && request.id == requestId) {
            return &request;
        }
    }
    return nullptr;
}

static int followingRequestId(int requestId) {
    return requestId == LAST_REQUEST_ID ? FIRST_REQUEST_ID : requestId + 1;
}

static int takeRequestId() {
    int requestId = nextRequestId;
    while (findRequest(requestId) != nullptr) {
        requestId = followingRequestId(requestId);
    }
    nextRequestId = followingRequestId(requestId);
    return requestId;
}

static int checkRequestId(int requestId, Request** request) {
    if (requestId <= 0) {
        return SCE_GAME_UPDATE_ERROR_REQUEST_NOT_FOUND;
    }
    if (!initialized) {
        return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    }
    *request = findRequest(requestId);
    if (*request == nullptr) {
        return SCE_GAME_UPDATE_ERROR_REQUEST_NOT_FOUND;
    }
    return 0;
}

static bool hasNonZeroReserved(const GameUpdateCheckParam* param) {
    for (std::uint32_t word : param->reserved) {
        if (word != 0) {
            return true;
        }
    }
    return false;
}

extern "C" {

int APS5_VABI sceGameUpdateAbortRequest(int requestId) {
    std::lock_guard lock(stateMutex);
    Request* request = nullptr;
    if (int error = checkRequestId(requestId, &request); error != 0) {
        return error;
    }
    request->aborted = true;
    return 0;
}

int APS5_VABI sceGameUpdateCheck(int requestId, const GameUpdateCheckParam* param, GameUpdateCheckResult* result) {
    if (requestId <= 0) {
        return SCE_GAME_UPDATE_ERROR_REQUEST_NOT_FOUND;
    }
    if (param == nullptr || result == nullptr) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    if (param->size != sizeof(GameUpdateCheckParam) || result->size != sizeof(GameUpdateCheckResult)) {
        return SCE_GAME_UPDATE_ERROR_INVALID_SIZE;
    }
    if (param->option != 0 || hasNonZeroReserved(param)) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    std::lock_guard lock(stateMutex);
    Request* request = nullptr;
    if (int error = checkRequestId(requestId, &request); error != 0) {
        return error;
    }
    if (request->aborted) {
        return SCE_GAME_UPDATE_ERROR_ABORTED;
    }
    result->found = false;
    result->addcont_found = false;
    return 0;
}

int APS5_VABI sceGameUpdateCreateRequest(void) {
    std::lock_guard lock(stateMutex);
    if (!initialized) {
        return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    }
    for (Request& request : requests) {
        if (request.id != 0) {
            continue;
        }
        const int requestId = takeRequestId();
        request = {requestId, false};
        return requestId;
    }
    return SCE_GAME_UPDATE_ERROR_TOO_MANY_REQUESTS;
}

int APS5_VABI sceGameUpdateDeleteRequest(int requestId) {
    std::lock_guard lock(stateMutex);
    Request* request = nullptr;
    if (int error = checkRequestId(requestId, &request); error != 0) {
        return error;
    }
    *request = {};
    return 0;
}

int APS5_VABI sceGameUpdateGetAddcontLatestVersion(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, GameUpdateAddcontVersionInfo* info) {
    (void)serviceLabel;
    if (info == nullptr) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    if (entitlementLabel == nullptr) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    {
        std::lock_guard lock(stateMutex);
        if (!initialized) {
            return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
        }
    }
    throw std::runtime_error("sceGameUpdateGetAddcontLatestVersion: entitlement version lookup is not implemented");
}

int APS5_VABI sceGameUpdateInitialize(void) {
    std::lock_guard lock(stateMutex);
    if (initialized) {
        return SCE_GAME_UPDATE_ERROR_ALREADY_INITIALIZED;
    }
    initialized = true;
    return 0;
}

int APS5_VABI sceGameUpdateTerminate(void) {
    std::lock_guard lock(stateMutex);
    if (!initialized) {
        return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    }
    initialized = false;
    requests = {};
    return 0;
}

int APS5_VABI sceGameUpdateCheckTitle() {
    NotImplemented_nid_no_patch(__func__);
}

}
