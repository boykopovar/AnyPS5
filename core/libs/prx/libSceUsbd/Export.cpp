#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace {

constexpr std::int32_t SCE_USBD_ERROR_INVALID_ARG = static_cast<std::int32_t>(0x80240002);

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

void* g_emptyDeviceList[1] = {nullptr};

}

extern "C" {

std::int32_t APS5_VABI sceUsbdInit() {
    return 0;
}

void APS5_VABI sceUsbdExit() {
}

std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list) {
    if (list == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    *list = g_emptyDeviceList;
    return 0;
}

void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices) {
    (void)unrefDevices;
    if (list != nullptr && list != g_emptyDeviceList) throw std::runtime_error(std::string(__func__) + ": list was not returned by sceUsbdGetDeviceList");
}

std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout) {
    if (timeout == nullptr || timeout->seconds < 0 || timeout->microseconds < 0 || timeout->microseconds >= 1000000) return SCE_USBD_ERROR_INVALID_ARG;
    std::this_thread::sleep_for(std::chrono::seconds(timeout->seconds) + std::chrono::microseconds(timeout->microseconds));
    return 0;
}

int APS5_VABI sceUsbdAllocTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdAttachKernelDriver() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdCancelTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdCheckConnected() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdClaimInterface() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdClose() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdControlTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdEventHandlingOk() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdFillInterruptTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdFreeConfigDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdFreeTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetActiveConfigDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetBusNumber() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetConfigDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetDeviceAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetDeviceDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetStringDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdKernelDriverActive() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdOpen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdRefDevice() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdReleaseInterface() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdResetDevice() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdSetConfiguration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdSubmitTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdUnrefDevice() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
