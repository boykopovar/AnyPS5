#include "prx/libSceSysUtil/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

constexpr std::size_t NotificationRequestSize = 0xc30;
constexpr std::int32_t TextNotificationKind = 5;
constexpr std::size_t TypeOffset = 0xc;
constexpr std::size_t TextOffset = 0x2d;
constexpr std::size_t TextLimit = 0xb3;

}

extern "C" {

int APS5_VABI sceSysUtilSendSystemNotificationWithText(int32_t type, const char* message) {
    std::array<unsigned char, NotificationRequestSize> request{};
    std::memcpy(request.data(), &TextNotificationKind, sizeof(TextNotificationKind));
    std::memcpy(request.data() + TypeOffset, &type, sizeof(type));
    if (message != nullptr) {
        std::size_t length = 0;
        while (length < TextLimit && message[length] != '\0') ++length;
        std::memcpy(request.data() + TextOffset, message, length);
    }
    return sceKernelSendNotificationRequest(0, request.data(), request.size(), 0);
}

int APS5_VABI sceSysUtilSendAddressingSystemNotification() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithDeviceId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithUserId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithUserName() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendNotificationRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendNpDebugNotificationRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotification() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotification2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationParamInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithAppId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithAppInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithAppName() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithAppNameRelatedToUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithDeviceId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithDeviceIdRelatedToUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithErrorCode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithParams() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithTextRelatedToUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithUserId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithUserName() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendSystemNotificationWithUserNameInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendTrcCheckNotificationRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSysUtilSendWebDebugNotificationRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
