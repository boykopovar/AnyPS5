// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceGameCustomDataDialog/include/Api.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libSceUserService/UserService.hpp"

namespace {

constexpr std::int32_t MODE_SEND = 1;
constexpr std::int32_t MODE_RECV = 2;
constexpr std::int32_t MODE_SEND_URL = 3;
constexpr std::int32_t ADDRESS_TYPE_USER_SELECT_DISABLE = 1;
constexpr std::int32_t ADDRESS_TYPE_USER_SELECT_ENABLE = 2;
constexpr std::uint32_t ADDRESS_USER_LIST_MAX_SIZE = 16;
constexpr std::size_t MAX_USER_MESSAGE_CHARACTERS = 512;
constexpr std::size_t MAX_USER_MESSAGE_BYTES = 2048;
constexpr std::size_t MAX_DATA_NAME_BYTES = 256;
constexpr std::size_t MAX_DATA_DETAIL_BYTES = 2048;
constexpr std::size_t MAX_URL_BYTES = 1024;
constexpr std::uint32_t MAX_THUMBNAIL_SIZE = 160 * 1024;
constexpr std::uint32_t MAX_DATA_SIZE = 1024 * 1024;
constexpr std::uint32_t MAX_MULTI_LANGUAGE_STRINGS = 30;
constexpr int GUEST_STRING_TOO_LONG = 63;

struct NpOnlineId {
    char data[16];
    char term;
    char dummy[3];
};

static_assert(sizeof(NpOnlineId) == 20);

union GameCustomDataDialogAddressInfo {
    struct {
        const void* ids;
        std::uint32_t idsCount;
        std::uint32_t padding;
    } userSelectDisable;
    struct {
        std::uint32_t userMaxCount;
    } userSelectEnable;
};

struct GameCustomDataDialogAddressParam {
    std::int32_t addressType;
    std::uint32_t padding;
    GameCustomDataDialogAddressInfo addressInfo;
};

struct GameCustomDataDialogPlatformInfo {
    std::uint32_t count;
    std::uint32_t padding;
    const char* const* platformName;
};

struct GameCustomDataDialogSendInfo {
    const char* userMessage;
    const char* dataName;
    const void* dataNameMultiLang;
    std::uint32_t dataNameMultiLangNum;
    std::uint32_t padding1C;
    const char* dataDetail;
    const void* dataDetailMultiLang;
    std::uint32_t dataDetailMultiLangNum;
    std::uint32_t padding34;
    const void* thumbnail;
    std::uint32_t thumbnailSize;
    std::uint32_t padding44;
    const void* payload;
    std::uint32_t dataSize;
    std::uint32_t padding54;
    std::int32_t expireMinutes;
    std::uint32_t padding5C;
    GameCustomDataDialogPlatformInfo availablePlatform;
    GameCustomDataDialogAddressParam addressParam;
};

static_assert(offsetof(GameCustomDataDialogSendInfo, thumbnailSize) == 0x40);
static_assert(offsetof(GameCustomDataDialogSendInfo, payload) == 0x48);
static_assert(offsetof(GameCustomDataDialogSendInfo, availablePlatform) == 0x60);
static_assert(offsetof(GameCustomDataDialogSendInfo, addressParam) == 0x70);
static_assert(sizeof(GameCustomDataDialogSendInfo) == 0x88);

struct GameCustomDataDialogRecvInfo {
    std::uint8_t reserved[160];
};

struct GameCustomDataDialogParam {
    CommonDialogBaseParam base;
    std::uint32_t size;
    std::int32_t mode;
    std::int32_t userId;
    std::uint32_t padding3C;
    void* callbackArg;
    const void* dataParam;
    std::uint8_t reserved[64];
};

static_assert(offsetof(GameCustomDataDialogParam, size) == 0x30);
static_assert(offsetof(GameCustomDataDialogParam, callbackArg) == 0x40);
static_assert(offsetof(GameCustomDataDialogParam, dataParam) == 0x48);
static_assert(sizeof(GameCustomDataDialogParam) == 0x90);

struct GameCustomDataDialogResult {
    void* callbackArg;
    std::int32_t errorCode;
    std::int32_t result;
    void* sent;
    std::uint8_t reserved[32];
};

static_assert(offsetof(GameCustomDataDialogResult, result) == 0x0C);
static_assert(offsetof(GameCustomDataDialogResult, reserved) == 0x18);

struct GameCustomDataDialogResultHead {
    void* callbackArg;
    std::int32_t errorCode;
    std::int32_t result;
};

struct AccountIdRecipients {
    static constexpr std::size_t idBytes = sizeof(std::uint64_t);
    static constexpr std::size_t sentListBytes = 8 + ADDRESS_USER_LIST_MAX_SIZE * 32;
    static bool Valid(const std::uint8_t* id) {
        return std::any_of(id, id + idBytes, [](std::uint8_t value) { return value != 0; });
    }
};

struct OnlineIdRecipients {
    static constexpr std::size_t idBytes = sizeof(NpOnlineId);
    static constexpr std::size_t sentListBytes = 4 + ADDRESS_USER_LIST_MAX_SIZE * sizeof(NpOnlineId);
    static bool Valid(const std::uint8_t* id) {
        return id[0] != 0;
    }
};

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};
std::atomic<void*> g_callbackArg{nullptr};

template <typename T>
bool CopyIn(T& destination, const void* source) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&destination, source, sizeof(destination));
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

template <std::size_t N>
bool AllZero(const std::uint8_t (&bytes)[N]) {
    return std::all_of(std::begin(bytes), std::end(bytes), [](std::uint8_t value) { return value == 0; });
}

int CheckString(const char* text, std::size_t minimum, std::size_t maximum, std::string& copy) {
    if (text == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    const int savedErrno = errno;
    const int copied = CopyGuestString_nid_no_patch(text, maximum + 1, copy);
    errno = savedErrno;
    if (copied == GUEST_STRING_TOO_LONG) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (copied != 0) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (copy.size() < minimum) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    return COMMON_DIALOG_OK;
}

int CheckCommonSend(const GameCustomDataDialogSendInfo& send) {
    std::string text;
    if (const int error = CheckString(send.userMessage, 0, MAX_USER_MESSAGE_BYTES, text); error != COMMON_DIALOG_OK) return error;
    const auto characters = std::count_if(text.begin(), text.end(), [](char value) { return (static_cast<unsigned char>(value) & 0xC0) != 0x80; });
    if (static_cast<std::size_t>(characters) > MAX_USER_MESSAGE_CHARACTERS) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (const int error = CheckString(send.dataName, 1, MAX_DATA_NAME_BYTES, text); error != COMMON_DIALOG_OK) return error;
    if (const int error = CheckString(send.dataDetail, 1, MAX_DATA_DETAIL_BYTES, text); error != COMMON_DIALOG_OK) return error;
    if (send.thumbnail == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (send.thumbnailSize == 0 || send.thumbnailSize > MAX_THUMBNAIL_SIZE) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    return COMMON_DIALOG_OK;
}

template <typename Recipients>
int CheckPlatformsAndRecipients(const GameCustomDataDialogSendInfo& send) {
    const auto& address = send.addressParam;
    if (address.addressType != ADDRESS_TYPE_USER_SELECT_DISABLE && address.addressType != ADDRESS_TYPE_USER_SELECT_ENABLE) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    const auto& platforms = send.availablePlatform;
    if (platforms.count == 0) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t i = 0; i < platforms.count; ++i) {
        const char* name = nullptr;
        if (!CopyIn(name, platforms.platformName + i)) return COMMON_DIALOG_ERROR_ARG_NULL;
        if (name == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
        char first = 0;
        if (!CopyIn(first, name)) return COMMON_DIALOG_ERROR_ARG_NULL;
        if (first == 0) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    }
    if (address.addressType == ADDRESS_TYPE_USER_SELECT_ENABLE) {
        const std::uint32_t maximum = address.addressInfo.userSelectEnable.userMaxCount;
        if (maximum == 0 || maximum > ADDRESS_USER_LIST_MAX_SIZE) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    } else {
        const auto& fixed = address.addressInfo.userSelectDisable;
        if (fixed.idsCount == 0 || fixed.idsCount > ADDRESS_USER_LIST_MAX_SIZE) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
        if (fixed.ids == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
        const auto* ids = static_cast<const std::uint8_t*>(fixed.ids);
        for (std::uint32_t i = 0; i < fixed.idsCount; ++i) {
            std::uint8_t id[Recipients::idBytes]{};
            if (!CopyIn(id, ids + i * Recipients::idBytes)) return COMMON_DIALOG_ERROR_ARG_NULL;
            if (!Recipients::Valid(id)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
        }
    }
    if (send.dataNameMultiLangNum > MAX_MULTI_LANGUAGE_STRINGS || send.dataDetailMultiLangNum > MAX_MULTI_LANGUAGE_STRINGS) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    return COMMON_DIALOG_OK;
}

bool IsLoggedIn(std::int32_t userId) {
    if (userId == USER_SERVICE_USER_ID_INVALID) return false;
    UserServiceLoginUserIdList users{};
    if (sceUserServiceGetLoginUserIdList(&users) != 0) return false;
    return std::find(std::begin(users.user_id), std::end(users.user_id), userId) != std::end(users.user_id);
}

template <typename Recipients>
int CheckParam(const void* guestParam, GameCustomDataDialogParam& param) {
    if (guestParam == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (!CopyIn(param, guestParam)) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (param.size != sizeof(GameCustomDataDialogParam)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (!AllZero(param.reserved)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (param.dataParam == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (!IsLoggedIn(param.userId)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (param.mode == MODE_RECV) {
        GameCustomDataDialogRecvInfo recv{};
        if (!CopyIn(recv, param.dataParam)) return COMMON_DIALOG_ERROR_ARG_NULL;
        return AllZero(recv.reserved) ? COMMON_DIALOG_OK : COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    }
    if (param.mode != MODE_SEND && param.mode != MODE_SEND_URL) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    GameCustomDataDialogSendInfo send{};
    if (!CopyIn(send, param.dataParam)) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (const int error = CheckCommonSend(send); error != COMMON_DIALOG_OK) return error;
    if (param.mode == MODE_SEND) {
        if (send.payload == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
        if (send.dataSize == 0 || send.dataSize > MAX_DATA_SIZE) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    } else {
        std::string url;
        if (const int error = CheckString(static_cast<const char*>(send.payload), 1, MAX_URL_BYTES, url); error != COMMON_DIALOG_OK) return error;
    }
    return CheckPlatformsAndRecipients<Recipients>(send);
}

template <typename Recipients>
int Open(const void* guestParam) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    GameCustomDataDialogParam param{};
    if (const int error = CheckParam<Recipients>(guestParam, param); error != COMMON_DIALOG_OK) return error;
    g_callbackArg = param.callbackArg;
    int status = g_status.load();
    do {
        if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    } while (!g_status.compare_exchange_weak(status, COMMON_DIALOG_STATUS_FINISHED));
    return COMMON_DIALOG_OK;
}

template <typename Recipients>
int GetResult(void* guestResult) {
    if (guestResult == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    GameCustomDataDialogResult result{};
    if (!CopyIn(result, guestResult)) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (!AllZero(result.reserved)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    const GameCustomDataDialogResultHead head{g_callbackArg.load(), COMMON_DIALOG_OK, COMMON_DIALOG_RESULT_USER_CANCELED};
    if (!CopyOut(guestResult, &head, sizeof(head))) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (result.sent != nullptr) {
        static const std::uint8_t zeros[Recipients::sentListBytes] = {};
        if (!CopyOut(result.sent, zeros, sizeof(zeros))) return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    return COMMON_DIALOG_RESULT_USER_CANCELED;
}

}

extern "C" {

int APS5_VABI sceGameCustomDataDialogInitialize(void) {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceGameCustomDataDialogOpenA(const void* param) {
    return Open<AccountIdRecipients>(param);
}

int APS5_VABI sceGameCustomDataDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI sceGameCustomDataDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceGameCustomDataDialogGetResultA(void* result) {
    return GetResult<AccountIdRecipients>(result);
}

int APS5_VABI sceGameCustomDataDialogClose(void) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return COMMON_DIALOG_ERROR_NOT_RUNNING;
}

int APS5_VABI sceGameCustomDataDialogTerminate(void) {
    if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceGameCustomDataDialogGetResult(void* result) {
    return GetResult<OnlineIdRecipients>(result);
}

int APS5_VABI sceGameCustomDataDialogOpen(const void* param) {
    return Open<OnlineIdRecipients>(param);
}

}
