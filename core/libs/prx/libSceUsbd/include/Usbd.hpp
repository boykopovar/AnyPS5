#ifndef CORE_LIBS_PRX_LIBSCEUSBD_INCLUDE_USBD_HPP
#define CORE_LIBS_PRX_LIBSCEUSBD_INCLUDE_USBD_HPP

#include <cstdint>

constexpr int USBD_OK = 0;
constexpr int USBD_ERROR_IO = static_cast<int>(0x80240001);
constexpr int USBD_ERROR_INVALID_ARG = static_cast<int>(0x80240002);
constexpr int USBD_ERROR_ACCESS = static_cast<int>(0x80240003);
constexpr int USBD_ERROR_NO_DEVICE = static_cast<int>(0x80240004);
constexpr int USBD_ERROR_NOT_FOUND = static_cast<int>(0x80240005);
constexpr int USBD_ERROR_BUSY = static_cast<int>(0x80240006);
constexpr int USBD_ERROR_TIMEOUT = static_cast<int>(0x80240007);
constexpr int USBD_ERROR_OVERFLOW = static_cast<int>(0x80240008);
constexpr int USBD_ERROR_PIPE = static_cast<int>(0x80240009);
constexpr int USBD_ERROR_INTERRUPTED = static_cast<int>(0x8024000A);
constexpr int USBD_ERROR_NO_MEMORY = static_cast<int>(0x8024000B);
constexpr int USBD_ERROR_NOT_SUPPORTED = static_cast<int>(0x8024000C);
constexpr int USBD_ERROR_OTHER = static_cast<int>(0x802400FF);

constexpr std::uint8_t USBD_TRANSFER_SHORT_NOT_OK = 1u << 0;
constexpr std::uint8_t USBD_TRANSFER_FREE_BUFFER = 1u << 1;
constexpr std::uint8_t USBD_TRANSFER_FREE_TRANSFER = 1u << 2;
constexpr std::uint8_t USBD_TRANSFER_ADD_ZERO_PACKET = 1u << 3;

enum class UsbdTransferStatus : std::int32_t {
    Completed = 0,
    Error = 1,
    TimedOut = 2,
    Cancelled = 3,
    Stall = 4,
    NoDevice = 5,
    Overflow = 6
};

enum class UsbdTransferType : std::int32_t {
    Control = 0,
    Isochronous = 1,
    Bulk = 2,
    Interrupt = 3
};

enum class UsbdSpeed : std::int32_t {
    Unknown = 0,
    Low = 1,
    Full = 2,
    High = 3,
    Super = 4,
    SuperPlus = 5,
    SuperPlusX2 = 6
};

enum class UsbdLogLevel : std::int32_t {
    None = 0,
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4
};

enum class UsbdOption : std::int32_t {
    LogLevel = 0,
    UseUsbDk = 1,
    UseUsbDkPrivate = 2,
    MaxTransferSize = 3,
    DebugLog = 4
};

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct UsbdDevice;
struct UsbdDeviceHandle;
struct UsbdTransfer;

using UsbdTransferCallback = void (*)(UsbdTransfer* transfer);

struct UsbdControlSetup {
    std::uint8_t requestType;
    std::uint8_t request;
    std::uint16_t value;
    std::uint16_t index;
    std::uint16_t length;
};

struct UsbdIsoPacketDescriptor {
    std::uint32_t length;
    std::uint32_t actualLength;
    UsbdTransferStatus status;
};

struct UsbdDeviceDescriptor {
    std::uint8_t length;
    std::uint8_t descriptorType;
    std::uint16_t usbVersion;
    std::uint8_t deviceClass;
    std::uint8_t deviceSubClass;
    std::uint8_t deviceProtocol;
    std::uint8_t maxPacketSize0;
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint16_t deviceVersion;
    std::uint8_t manufacturerIndex;
    std::uint8_t productIndex;
    std::uint8_t serialNumberIndex;
    std::uint8_t configurationCount;
};

struct UsbdEndpointDescriptor {
    std::uint8_t length;
    std::uint8_t descriptorType;
    std::uint8_t endpointAddress;
    std::uint8_t attributes;
    std::uint16_t maxPacketSize;
    std::uint8_t interval;
    std::uint8_t refresh;
    std::uint8_t synchAddress;
    const unsigned char* extra;
    int extraLength;
};

struct UsbdInterfaceDescriptor {
    std::uint8_t length;
    std::uint8_t descriptorType;
    std::uint8_t interfaceNumber;
    std::uint8_t alternateSetting;
    std::uint8_t endpointCount;
    std::uint8_t interfaceClass;
    std::uint8_t interfaceSubClass;
    std::uint8_t interfaceProtocol;
    std::uint8_t interfaceIndex;
    const UsbdEndpointDescriptor* endpoints;
    const unsigned char* extra;
    int extraLength;
};

struct UsbdInterface {
    const UsbdInterfaceDescriptor* alternateSettings;
    int alternateSettingCount;
};

struct UsbdConfigDescriptor {
    std::uint8_t length;
    std::uint8_t descriptorType;
    std::uint16_t totalLength;
    std::uint8_t interfaceCount;
    std::uint8_t configurationValue;
    std::uint8_t configurationIndex;
    std::uint8_t attributes;
    std::uint8_t maxPower;
    const UsbdInterface* interfaces;
    const unsigned char* extra;
    int extraLength;
};

struct UsbdTransfer {
    void* deviceHandle;
    std::uint8_t flags;
    std::uint8_t endpoint;
    std::uint8_t type;
    unsigned int timeout;
    UsbdTransferStatus status;
    int length;
    int actualLength;
    UsbdTransferCallback callback;
    void* userData;
    unsigned char* buffer;
    int isochronousPacketCount;
    UsbdIsoPacketDescriptor isochronousPackets[];
};

inline constexpr int USBD_ENDPOINT_IN = 0x80;
inline constexpr int USBD_ENDPOINT_OUT = 0x00;
inline constexpr int USBD_TRANSFER_TYPE_MASK = 0x03;
inline constexpr int USBD_ENDPOINT_NUMBER_MASK = 0x0f;
inline constexpr int USBD_REQUEST_TYPE_DIRECTION_IN = 0x80;
inline constexpr int USBD_REQUEST_TYPE_DIRECTION_OUT = 0x00;
inline constexpr int USBD_REQUEST_TYPE_TYPE_STANDARD = 0x00;
inline constexpr int USBD_REQUEST_TYPE_TYPE_CLASS = 0x20;
inline constexpr int USBD_REQUEST_TYPE_TYPE_VENDOR = 0x40;
inline constexpr int USBD_REQUEST_TYPE_RECIPIENT_DEVICE = 0x00;
inline constexpr int USBD_REQUEST_TYPE_RECIPIENT_INTERFACE = 0x01;
inline constexpr int USBD_REQUEST_TYPE_RECIPIENT_ENDPOINT = 0x02;
inline constexpr int USBD_REQUEST_TYPE_RECIPIENT_OTHER = 0x03;
inline constexpr int USBD_REQUEST_GET_STATUS = 0x00;
inline constexpr int USBD_REQUEST_CLEAR_FEATURE = 0x01;
inline constexpr int USBD_REQUEST_SET_FEATURE = 0x03;
inline constexpr int USBD_REQUEST_GET_DESCRIPTOR = 0x06;
inline constexpr int USBD_REQUEST_SET_DESCRIPTOR = 0x07;
inline constexpr int USBD_REQUEST_GET_CONFIGURATION = 0x08;
inline constexpr int USBD_REQUEST_SET_CONFIGURATION = 0x09;
inline constexpr int USBD_DESCRIPTOR_TYPE_DEVICE = 0x01;
inline constexpr int USBD_DESCRIPTOR_TYPE_CONFIGURATION = 0x02;
inline constexpr int USBD_DESCRIPTOR_TYPE_STRING = 0x03;
inline constexpr int USBD_DESCRIPTOR_TYPE_INTERFACE = 0x04;
inline constexpr int USBD_DESCRIPTOR_TYPE_ENDPOINT = 0x05;
inline constexpr int USBD_DESCRIPTOR_TYPE_BOS = 0x0f;
inline constexpr int USBD_DESCRIPTOR_TYPE_DEVICE_QUALIFIER = 0x06;
inline constexpr int USBD_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION = 0x07;

#endif
