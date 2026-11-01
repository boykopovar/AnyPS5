#ifndef CORE_LIBS_PRX_LIBSCECOMMONDIALOG_COMMONDIALOG_HPP
#define CORE_LIBS_PRX_LIBSCECOMMONDIALOG_COMMONDIALOG_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

struct CommonDialogBaseParam {
    std::uint64_t size;
    std::uint8_t reserved[36];
    std::uint32_t magic;
};

struct NpAuthAuthorizedAppDialogParam {
    CommonDialogBaseParam base;
    std::uint64_t size;
    std::int32_t userId;
    std::uint32_t padding60;
    const char* application;
    const char* text;
    std::uint32_t mode;
    std::uint32_t padding84;
    std::uint8_t reserved[32];
};

static_assert(sizeof(NpAuthAuthorizedAppDialogParam) == 120);
static_assert(offsetof(NpAuthAuthorizedAppDialogParam, application) == 64);
static_assert(offsetof(NpAuthAuthorizedAppDialogParam, text) == 72);
static_assert(offsetof(NpAuthAuthorizedAppDialogParam, mode) == 80);
static_assert(offsetof(NpAuthAuthorizedAppDialogParam, reserved) == 88);

struct VrSetupDialogParam {
    std::uint64_t size;
    std::uint8_t reserved8;
    std::uint8_t reserved9;
    std::uint8_t data[46];
};

static_assert(sizeof(VrSetupDialogParam) == 56);

struct UsbStorageDialogParam {
    CommonDialogBaseParam base;
    std::uint64_t size;
    char path[1024];
    std::int32_t userId;
    std::uint32_t padding;
};

struct UsbStorageDialogResult {
    std::uint64_t size;
    std::int32_t result;
    std::int32_t status;
    std::uint8_t reserved[64];
};

static_assert(offsetof(UsbStorageDialogParam, path) == 56);
static_assert(offsetof(UsbStorageDialogParam, userId) == 1080);
static_assert(sizeof(UsbStorageDialogResult) == 80);
static_assert(offsetof(UsbStorageDialogResult, result) == 8);
static_assert(offsetof(UsbStorageDialogResult, status) == 12);

struct LoginDialogParam {
    std::uint32_t size;
    std::uint32_t mode;
    std::int32_t userIds[9];
    std::uint32_t reserved[5];
};

struct LoginDialogResult {
    std::int32_t result;
    std::int32_t errorCode;
    std::uint8_t data[8];
};

static_assert(sizeof(LoginDialogParam) == 64);
static_assert(offsetof(LoginDialogParam, userIds) == 8);
static_assert(offsetof(LoginDialogParam, reserved) == 44);
static_assert(sizeof(LoginDialogResult) == 16);
static_assert(offsetof(LoginDialogResult, errorCode) == 4);

struct NetCtlApDialogParam {
    CommonDialogBaseParam base;
    std::uint64_t size;
    std::uint32_t value56;
    std::uint32_t reserved60;
    char option[9];
    std::uint8_t reserved[31];
};

struct NetCtlApDialogResult {
    std::int32_t result;
    std::uint8_t reserved[36];
};

static_assert(sizeof(NetCtlApDialogParam) == 104);
static_assert(offsetof(NetCtlApDialogParam, option) == 64);
static_assert(offsetof(NetCtlApDialogParam, reserved) == 73);
static_assert(sizeof(NetCtlApDialogResult) == 40);

struct G2PDialogParam {
    CommonDialogBaseParam base;
    std::int32_t userId;
    std::uint32_t padding;
    std::uint64_t size;
    std::uint8_t reserved[64];
};

struct PlayerReviewDialogParam {
    CommonDialogBaseParam base;
    std::uint64_t size;
    std::int32_t userId;
    std::uint8_t target[40];
    std::uint32_t mode;
    std::uint8_t reserved[64];
};

struct PlayerSelectionDialogPlayer {
    std::uint64_t accountId;
    std::uint32_t option0;
    std::uint32_t option1;
    std::uint32_t mode;
    std::uint8_t reserved[64];
    std::uint32_t padding;
};

struct PlayerSelectionDialogParam {
    CommonDialogBaseParam base;
    std::uint64_t size;
    std::int32_t userId;
    char title[64];
    std::uint32_t maxPlayers;
    std::uint32_t mode;
    std::uint32_t padding0;
    const PlayerSelectionDialogPlayer* players;
    std::uint32_t playerCount;
    std::uint8_t reserved[64];
    std::uint32_t padding1;
};

struct PlayerSelectionDialogResult {
    std::int32_t result;
    std::uint32_t option;
    std::uint64_t* accountIds;
    std::uint32_t count;
    std::uint32_t padding;
};

static_assert(sizeof(PlayerSelectionDialogPlayer) == 88);
static_assert(offsetof(PlayerSelectionDialogPlayer, reserved) == 20);
static_assert(sizeof(PlayerSelectionDialogParam) == 216);
static_assert(offsetof(PlayerSelectionDialogParam, maxPlayers) == 124);
static_assert(offsetof(PlayerSelectionDialogParam, players) == 136);
static_assert(offsetof(PlayerSelectionDialogParam, reserved) == 148);
static_assert(sizeof(PlayerSelectionDialogResult) == 24);
static_assert(offsetof(PlayerSelectionDialogResult, accountIds) == 8);

struct NpCommerceDialogResult {
    std::int32_t result;
    std::uint8_t authorized;
    std::uint8_t padding[3];
    std::uint64_t userData;
    std::uint8_t reserved[32];
};

static_assert(sizeof(NpCommerceDialogResult) == 48);
static_assert(offsetof(NpCommerceDialogResult, userData) == 8);

struct NpCommerceDialogParam {
    CommonDialogBaseParam base;
    std::uint32_t size;
    std::int32_t userId;
    std::uint32_t mode;
    std::uint32_t option;
    const char* const* products;
    std::uint32_t productCount;
    std::uint32_t padding;
    std::uint64_t argument;
    std::uint64_t userData;
    std::uint8_t reserved[32];
};

static_assert(sizeof(NpCommerceDialogParam) == 128);
static_assert(offsetof(NpCommerceDialogParam, products) == 64);
static_assert(offsetof(NpCommerceDialogParam, userData) == 88);

struct NpCommerceDialogParam2 {
    CommonDialogBaseParam base;
    std::uint32_t size;
    std::int32_t userId;
    std::uint32_t mode;
    std::uint32_t option;
    std::uint32_t contentMode;
    std::uint32_t padding0;
    const char* const* products;
    std::uint32_t productCount;
    std::uint32_t padding1;
    std::uint64_t argument;
    std::uint64_t userData;
    std::uint8_t reserved[32];
};

static_assert(sizeof(NpCommerceDialogParam2) == 136);
static_assert(offsetof(NpCommerceDialogParam2, products) == 72);
static_assert(offsetof(NpCommerceDialogParam2, userData) == 96);

struct WebBrowserDialogExtendedResult {
    std::uint64_t size;
    std::uint32_t type;
    char content[2048];
    std::uint8_t reserved0[4];
    char* extendedContent;
    std::uint32_t extendedCapacity;
    std::uint8_t reserved1[20];
};

struct WebBrowserDialogResult {
    std::int32_t result;
    std::uint32_t padding;
    WebBrowserDialogExtendedResult* extended;
    std::uint8_t reserved[240];
};

static_assert(sizeof(WebBrowserDialogExtendedResult) == 2096);
static_assert(offsetof(WebBrowserDialogExtendedResult, extendedContent) == 2064);
static_assert(sizeof(WebBrowserDialogResult) == 256);

struct WebBrowserDialogAllowList {
    std::uint64_t size;
    const char* addresses[20];
    std::uint8_t reserved[256];
};

static_assert(sizeof(WebBrowserDialogAllowList) == 424);

struct CommonDialogResultPrefix {
    std::uint8_t untouched[8];
    std::int32_t result;
};

static_assert(sizeof(CommonDialogBaseParam) == 48);
static_assert(sizeof(G2PDialogParam) == 128);
static_assert(sizeof(PlayerReviewDialogParam) == 168);
static_assert(offsetof(CommonDialogResultPrefix, result) == 8);

inline bool ValidateCommonDialogBaseParam(const CommonDialogBaseParam& param) {
    return param.size == sizeof(param) && param.magic == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u && std::all_of(std::begin(param.reserved), std::end(param.reserved), [](std::uint8_t value) { return value == 0; });
}

constexpr int COMMON_DIALOG_OK = 0;
constexpr int COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED = static_cast<int>(0x80B80002);
constexpr int COMMON_DIALOG_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80B80004);

#endif
