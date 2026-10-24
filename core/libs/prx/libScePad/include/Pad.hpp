#ifndef CORE_LIBS_PRX_LIBSCEPAD_PAD_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PAD_HPP

#include <cstdint>

constexpr int PAD_OK = 0;
constexpr int PAD_ERROR_INVALID_ARG = -2137915391;
constexpr int PAD_ERROR_INVALID_HANDLE = -2137915389;
constexpr int PAD_ERROR_NOT_INITIALIZED = static_cast<int>(0x80920005u);

constexpr int PAD_PORT_TYPE_STANDARD = 0;
constexpr int PAD_PORT_TYPE_SPECIAL = 2;
constexpr int PAD_PORT_TYPE_REMOTE = 16;
constexpr int PAD_USER_ID_SYSTEM = 0xff;
constexpr int PAD_MAX_CONTROLLERS = 4;
constexpr int PAD_HANDLE_BASE = 1;
constexpr int PAD_HANDLE = 1;
constexpr int PAD_REMOTE_HANDLE = 0x10;
constexpr int PAD_SPECIAL_HANDLE = 0x20;
constexpr int PAD_SPECIAL_HANDLE_COUNT = 16;

constexpr int PAD_CONNECTION_TYPE_LOCAL = 0;
constexpr int PAD_DEVICE_CLASS_STANDARD = 0;

constexpr int PAD_DEVICE_TYPE_NONE = 0;
constexpr int PAD_DEVICE_TYPE_NAVIGATOR = 1;
constexpr int PAD_DEVICE_TYPE_DUAL_SHOCK_4 = 2;
constexpr int PAD_DEVICE_TYPE_DUAL_SENSE = 3;
constexpr int PAD_DEVICE_TYPE_DUAL_SENSE_EDGE = 4;

constexpr int PAD_CONTROLLER_TYPE_DUAL_SENSE = 2;

constexpr int PAD_CONNECT_TYPE_UNREGISTERED = 0;
constexpr int PAD_CONNECT_TYPE_USB = 1;
constexpr int PAD_CONNECT_TYPE_BLUETOOTH = 2;

inline constexpr std::uint8_t PAD_BD_ADDRESS[6] = {0xDC, 0x8C, 0x37, 0x51, 0x01, 0xF4};

struct PadInfo {
    std::uint8_t maxConnectCount;
    std::uint8_t connectedCount[2];
    std::uint8_t reserved0[2];
    struct PadTypeInfo {
        std::uint8_t connectPort;
        std::uint8_t status;
        std::uint8_t deviceType;
        std::uint8_t connectType;
        std::uint8_t bluetoothMacAddress[6];
    } padTypeInfo[8];
    std::uint8_t reserved1[4];
};
static_assert(sizeof(PadInfo) == 0x59);

#endif
