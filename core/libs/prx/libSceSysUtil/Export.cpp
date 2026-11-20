#include "prx/libSceSysUtil/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceRegMgr/include/Api.hpp"

namespace {

constexpr std::size_t NotificationRequestSize = 0xc30;
constexpr std::int32_t TextNotificationKind = 5;
constexpr std::size_t TypeOffset = 0xc;
constexpr std::size_t TextOffset = 0x2d;
constexpr std::size_t TextLimit = 0xb3;

void StoreInt(std::array<unsigned char, NotificationRequestSize>& request, std::size_t offset, std::int32_t value) {
    std::memcpy(request.data() + offset, &value, sizeof(value));
}

void StoreLong(std::array<unsigned char, NotificationRequestSize>& request, std::size_t offset, std::int64_t value) {
    std::memcpy(request.data() + offset, &value, sizeof(value));
}

int SendNotification(std::array<unsigned char, NotificationRequestSize>& request, std::int32_t kind, std::int32_t type) {
    const std::uint64_t header = (std::uint64_t{0xffffffff} << 32) | static_cast<std::uint32_t>(kind);
    std::memcpy(request.data(), &header, sizeof(header));
    StoreInt(request, TypeOffset, type);
    return sceKernelSendNotificationRequest(0, request.data(), request.size(), 0);
}

int CopyNotificationString(std::array<unsigned char, NotificationRequestSize>& request, std::size_t offset, const char* value, std::size_t limit = TextLimit) {
    if (value == nullptr) return 0;
    const int savedErrno = errno;
    for (std::size_t index = 0; index < limit; ++index) {
        char character = 0;
        const bool copied = GuestCopyIn_nid_no_patch(&character, value + index, sizeof(character));
        errno = savedErrno;
        if (!copied) return SCE_KERNEL_ERROR_EFAULT;
        request[offset + index] = static_cast<unsigned char>(character);
        if (character == '\0') break;
    }
    return 0;
}

std::array<unsigned char, NotificationRequestSize> NotificationRequest() {
    return {};
}

int SendConditionalDebugNotification(std::int64_t setting, std::int32_t kind, const char* message, std::int32_t flags) {
    std::int32_t enabled = 0;
    if (sceRegMgrNonSysGetInt(setting, 0, &enabled) != 0 || enabled != 1) return 0;

    auto request = NotificationRequest();
    StoreInt(request, 0, kind);
    std::memset(request.data() + 4, 0xff, sizeof(std::uint32_t));
    std::memset(request.data() + TypeOffset, 0xff, 0x14);
    const int copied = CopyNotificationString(request, TextOffset, message, 0x3ff);
    if (copied != 0) return copied;
    return sceKernelSendNotificationRequest(0, request.data(), request.size(), flags);
}

void CopyParameterString(std::array<unsigned char, NotificationRequestSize>& request, std::size_t destinationOffset,
                         const std::array<unsigned char, 0xa40>& param, std::size_t sourceOffset, std::size_t limit) {
    for (std::size_t index = 0; index < limit && param[sourceOffset + index] != 0; ++index)
        request[destinationOffset + index] = param[sourceOffset + index];
}

}

extern "C" {

int APS5_VABI sceSysUtilSendSystemNotificationWithText(int32_t type, const char* message) {
    std::array<unsigned char, NotificationRequestSize> request{};
    std::memcpy(request.data(), &TextNotificationKind, sizeof(TextNotificationKind));
    std::memcpy(request.data() + TypeOffset, &type, sizeof(type));
    if (message != nullptr) {
        const int savedErrno = errno;
        std::size_t length = 0;
        char c = 0;
        while (length < TextLimit) {
            const bool copied = GuestCopyIn_nid_no_patch(&c, message + length, sizeof(c));
            errno = savedErrno;
            if (!copied) {
                return SCE_KERNEL_ERROR_EFAULT;
            }
            if (c == '\0') break;
            request[TextOffset + length] = static_cast<unsigned char>(c);
            ++length;
        }
    }
    return sceKernelSendNotificationRequest(0, request.data(), request.size(), 0);
}

int APS5_VABI sceSysUtilSendSystemNotification(int32_t type) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreLong(request, 0x14, -1);
    StoreInt(request, 0x1c, -1);
    return SendNotification(request, 1, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithUserId(int32_t type, int32_t userId) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreInt(request, 0x14, userId);
    StoreLong(request, 0x18, -1);
    return SendNotification(request, 2, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithDeviceId(int32_t type, const void* deviceId) {
    auto request = NotificationRequest();
    StoreLong(request, 0x10, -1);
    StoreLong(request, 0x18, reinterpret_cast<std::intptr_t>(deviceId));
    return SendNotification(request, 3, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithErrorCode(int32_t type, int32_t errorCode) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreLong(request, 0x14, -1);
    StoreInt(request, 0x24, errorCode);
    return SendNotification(request, 7, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithAppId(int32_t type, int32_t appId) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreLong(request, 0x14, -1);
    StoreInt(request, 0x20, appId);
    return SendNotification(request, 8, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithAppName(int32_t type, const char* appName) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreLong(request, 0x14, -1);
    const int copied = CopyNotificationString(request, TextOffset, appName);
    if (copied != 0) return copied;
    return SendNotification(request, 9, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithAppInfo(int32_t type, const char* appName, const char* info) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreLong(request, 0x14, -1);
    const int copiedName = CopyNotificationString(request, TextOffset, appName);
    if (copiedName != 0) return copiedName;
    const int copiedInfo = CopyNotificationString(request, TextOffset + 0xb4, info);
    if (copiedInfo != 0) return copiedInfo;
    return SendNotification(request, 9, type);
}

void APS5_VABI sceSysUtilSendSystemNotificationParamInit(void* param, int32_t type) {
    constexpr std::uint32_t initializedTag = 0xd59c4b6e;
    if (param == nullptr) return;

    std::array<unsigned char, 0xa40> initialized{};
    std::memset(initialized.data() + 0xc, 0xff, 8);
    std::memcpy(initialized.data(), &type, sizeof(type));
    if (type == 2) {
        std::memset(initialized.data() + 0x818, 0xff, sizeof(std::uint64_t));
    } else if (type == 3) {
        std::memset(initialized.data() + 0x818, 0xff, sizeof(std::uint32_t));
        std::memset(initialized.data() + 0x820, 0xff, sizeof(std::uint64_t));
    } else if (type == 0 || type == 1 || (type >= 4 && type <= 8)) {
        std::memset(initialized.data() + 0x818, 0xff, sizeof(std::uint32_t));
    }
    std::memcpy(initialized.data() + 0xa38, &initializedTag, sizeof(initializedTag));
    const int savedErrno = errno;
    GuestCopyOut_nid_no_patch(param, initialized.data(), initialized.size());
    errno = savedErrno;
}

int APS5_VABI sceSysUtilSendAddressingSystemNotification(int32_t type, int32_t addressingType) {
    auto request = NotificationRequest();
    StoreLong(request, 0x14, -1);
    StoreInt(request, 0x1c, -1);
    StoreInt(request, 0x10, addressingType);
    return SendNotification(request, 14, type);
}

int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithDeviceId(int32_t type, int32_t addressingType, const void* deviceId) {
    auto request = NotificationRequest();
    StoreInt(request, 0x14, -1);
    StoreInt(request, 0x10, addressingType);
    StoreLong(request, 0x18, reinterpret_cast<std::intptr_t>(deviceId));
    return SendNotification(request, 15, type);
}

int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithUserName(int32_t type, int32_t addressingType, const char* userName) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, addressingType);
    StoreLong(request, 0x14, -1);
    StoreInt(request, 0x1c, -1);
    const int copied = CopyNotificationString(request, TextOffset, userName);
    if (copied != 0) return copied;
    return SendNotification(request, 16, type);
}

int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithUserId(int32_t type, int32_t addressingType, int32_t userId) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, addressingType);
    StoreLong(request, 0x18, -1);
    StoreInt(request, 0x14, userId);
    return SendNotification(request, 17, type);
}

int APS5_VABI sceSysUtilSendNotificationRequest(const char* message, int32_t flags) {
    auto request = NotificationRequest();
    const std::uint64_t header = std::uint64_t{0xffffffff} << 32;
    std::memcpy(request.data(), &header, sizeof(header));
    std::memset(request.data() + TypeOffset, 0xff, 0x14);
    const int copied = CopyNotificationString(request, TextOffset, message, 0x3ff);
    if (copied != 0) return copied;
    return sceKernelSendNotificationRequest(0, request.data(), request.size(), flags);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithDeviceIdRelatedToUser(int32_t type, int32_t userId, const void* deviceId) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreInt(request, 0x14, userId);
    StoreLong(request, 0x18, reinterpret_cast<std::intptr_t>(deviceId));
    return SendNotification(request, 4, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithTextRelatedToUser(int32_t type, int32_t userId, const char* message) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreInt(request, 0x14, userId);
    StoreLong(request, 0x18, -1);
    const int copied = CopyNotificationString(request, TextOffset, message);
    if (copied != 0) return copied;
    return SendNotification(request, 6, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithUserName(int32_t type, const char* userName) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreLong(request, 0x14, -1);
    const int copied = CopyNotificationString(request, TextOffset, userName);
    if (copied != 0) return copied;
    return SendNotification(request, 12, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithUserNameInfo(int32_t type, const char* userName, const char* info) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreLong(request, 0x14, -1);
    const int copiedName = CopyNotificationString(request, TextOffset, userName);
    if (copiedName != 0) return copiedName;
    const int copiedInfo = CopyNotificationString(request, TextOffset + 0xb4, info);
    if (copiedInfo != 0) return copiedInfo;
    return SendNotification(request, 13, type);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithAppNameRelatedToUser(int32_t type, int32_t userId, const char* appName) {
    auto request = NotificationRequest();
    StoreInt(request, 0x10, -1);
    StoreInt(request, 0x14, userId);
    StoreLong(request, 0x18, -1);
    const int copied = CopyNotificationString(request, TextOffset, appName);
    if (copied != 0) return copied;
    return SendNotification(request, 10, type);
}

int APS5_VABI sceSysUtilSendNpDebugNotificationRequest(const char* message, int32_t flags) {
    return SendConditionalDebugNotification(0x0d49671adfd5eb34, 0x66, message, flags);
}

int APS5_VABI sceSysUtilSendSystemNotification2(const void* param) {
    constexpr int invalidParameter = static_cast<int>(0x81670000u);
    constexpr std::uint32_t initializedTag = 0xd59c4b6e;
    if (param == nullptr) return invalidParameter;

    std::array<unsigned char, 0xa40> localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(localParam.data(), param, localParam.size());
    errno = savedErrno;
    if (!copied) return SCE_KERNEL_ERROR_EFAULT;

    std::uint32_t type = 0;
    std::uint32_t tag = 0;
    std::memcpy(&type, localParam.data(), sizeof(type));
    std::memcpy(&tag, localParam.data() + 0xa38, sizeof(tag));
    if (tag != initializedTag || type > 13) return invalidParameter;

    std::array<unsigned char, NotificationRequestSize> request{};
    const auto* requestValues = localParam.data();
    std::memcpy(request.data() + 0x8, requestValues + 0x4, sizeof(std::uint32_t));
    std::memcpy(request.data() + 0x10, requestValues + 0x10, sizeof(std::uint32_t));
    std::memcpy(request.data() + 0x28, requestValues + 0x8, sizeof(std::uint32_t));
    request[0x2c] = requestValues[0x14];
    if (requestValues[0x14] == 1) CopyParameterString(request, 0x42d, localParam, 0x15, 0x7ff);
    std::memcpy(request.data() + TypeOffset, requestValues + 0xc, sizeof(std::uint32_t));

    std::int32_t kind = 0;
    switch (type) {
    case 0: kind = 1; break;
    case 1: kind = 2; std::memcpy(request.data() + 0x14, requestValues + 0x818, sizeof(std::uint32_t)); break;
    case 2: kind = 3; std::memcpy(request.data() + 0x18, requestValues + 0x818, sizeof(std::uint64_t)); break;
    case 3:
        kind = 4;
        std::memcpy(request.data() + 0x14, requestValues + 0x818, sizeof(std::uint32_t));
        std::memcpy(request.data() + 0x18, requestValues + 0x820, sizeof(std::uint64_t));
        break;
    case 4: kind = 5; CopyParameterString(request, TextOffset, localParam, 0x818, TextLimit); break;
    case 5:
        kind = 6;
        std::memcpy(request.data() + 0x14, requestValues + 0x818, sizeof(std::uint32_t));
        CopyParameterString(request, TextOffset, localParam, 0x81c, TextLimit);
        break;
    case 6:
        kind = 8;
        StoreLong(request, 0, static_cast<std::int64_t>((std::uint64_t{0xffffffff} << 32) | 8));
        std::memcpy(request.data() + 0x20, requestValues + 0x818, sizeof(std::uint32_t));
        break;
    case 7:
        kind = 9;
        CopyParameterString(request, TextOffset, localParam, 0x818, TextLimit);
        break;
    case 8:
        kind = 10;
        std::memcpy(request.data() + 0x14, requestValues + 0x818, sizeof(std::uint32_t));
        CopyParameterString(request, TextOffset, localParam, 0x81c, TextLimit);
        break;
    case 9:
        kind = 9;
        CopyParameterString(request, TextOffset, localParam, 0x818, TextLimit);
        CopyParameterString(request, TextOffset + 0xb4, localParam, 0x8cc, TextLimit);
        break;
    case 10: kind = 7; std::memcpy(request.data() + 0x24, requestValues + 0x818, sizeof(std::uint32_t)); break;
    case 11:
        kind = 11;
        CopyParameterString(request, TextOffset, localParam, 0x818, TextLimit);
        CopyParameterString(request, TextOffset + 0xb4, localParam, 0x8cc, TextLimit);
        CopyParameterString(request, TextOffset + 0x168, localParam, 0x980, TextLimit);
        break;
    case 12: kind = 12; CopyParameterString(request, TextOffset, localParam, 0x818, TextLimit); break;
    case 13:
        kind = 13;
        CopyParameterString(request, TextOffset, localParam, 0x818, TextLimit);
        CopyParameterString(request, TextOffset + 0xb4, localParam, 0x8cc, TextLimit);
        break;
    default: return invalidParameter;
    }
    if (type != 6) StoreInt(request, 0, kind);
    return sceKernelSendNotificationRequest(0, request.data(), request.size(), 0);
}

int APS5_VABI sceSysUtilSendSystemNotificationWithParams(int32_t type, SysUtilNotificationWithParams params) {
    static_assert(sizeof(SysUtilNotificationWithParams) == 0x21c);
    auto request = NotificationRequest();
    StoreLong(request, 0, static_cast<std::int64_t>((std::uint64_t{0xffffffff} << 32) | 11));
    StoreInt(request, TypeOffset, type);
    StoreLong(request, 0x14, -1);
    StoreInt(request, 0x1c, -1);
    const auto copyField = [&](std::size_t offset, const char* field) {
        for (std::size_t index = 0; index < TextLimit; ++index) {
            const char value = field[index];
            request[offset + index] = static_cast<unsigned char>(value);
            if (value == '\0') break;
        }
    };
    copyField(TextOffset, params.first);
    copyField(TextOffset + 0xb4, params.second);
    copyField(TextOffset + 0x168, params.third);
    return sceKernelSendNotificationRequest(0, request.data(), request.size(), 0);
}

int APS5_VABI sceSysUtilSendTrcCheckNotificationRequest(const char* message, int32_t flags) {
    return SendConditionalDebugNotification(0x0d49671adfd5eb34, 0x65, message, flags);
}

int APS5_VABI sceSysUtilSendWebDebugNotificationRequest(const char* message, int32_t flags) {
    return SendConditionalDebugNotification(0x1c0b63438bdc7b40, 0x67, message, flags);
}

}
