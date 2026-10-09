#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <libusb.h>

namespace {

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

using UsbdTransferCallback = void (APS5_VABI *)(libusb_transfer*);
libusb_device_handle* const mockHandle = reinterpret_cast<libusb_device_handle*>(0x30);
std::int32_t result = 0;
std::int32_t allocations = 0;
std::int32_t cancelled = 0;
std::int32_t callbackCount = 0;
libusb_transfer* callbackTransfer = nullptr;
std::vector<libusb_transfer*> pending;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBD async: %s\n", message);
        std::abort();
    }
}

}

extern "C" {
std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
libusb_transfer* APS5_VABI sceUsbdAllocTransfer(int isoPackets);
void APS5_VABI sceUsbdFreeTransfer(libusb_transfer* transfer);
void APS5_VABI sceUsbdFillInterruptTransfer(libusb_transfer* transfer, libusb_device_handle* handle, std::uint8_t endpoint, unsigned char* buffer, int length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdSubmitTransfer(libusb_transfer* transfer);
std::int32_t APS5_VABI sceUsbdCancelTransfer(libusb_transfer* transfer);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout);

libusb_transfer* LIBUSB_CALL __real_libusb_alloc_transfer(int isoPackets);
void LIBUSB_CALL __real_libusb_free_transfer(libusb_transfer* transfer);

libusb_transfer* LIBUSB_CALL __wrap_libusb_alloc_transfer(int isoPackets) {
    auto* transfer = __real_libusb_alloc_transfer(isoPackets);
    if (transfer != nullptr) ++allocations;
    return transfer;
}

void LIBUSB_CALL __wrap_libusb_free_transfer(libusb_transfer* transfer) {
    if (transfer != nullptr) --allocations;
    __real_libusb_free_transfer(transfer);
}

int LIBUSB_CALL __wrap_libusb_submit_transfer(libusb_transfer* transfer) {
    if (result != 0) return result;
    Require(transfer->dev_handle == mockHandle, "wrong asynchronous handle");
    Require(transfer->callback != nullptr, "missing native callback bridge");
    pending.push_back(transfer);
    return 0;
}

int LIBUSB_CALL __wrap_libusb_cancel_transfer(libusb_transfer* transfer) {
    if (result != 0) return result;
    Require(!pending.empty() && pending[0] == transfer, "wrong transfer cancelled");
    cancelled = 1;
    return 0;
}

int LIBUSB_CALL __wrap_libusb_handle_events_timeout(libusb_context* context, timeval* timeout) {
    Require(context != nullptr && timeout != nullptr, "missing event context or timeout");
    if (result != 0) return result;
    auto completed = std::move(pending);
    pending.clear();
    for (auto* transfer : completed) {
        transfer->status = cancelled ? LIBUSB_TRANSFER_CANCELLED : LIBUSB_TRANSFER_COMPLETED;
        cancelled = 0;
        transfer->actual_length = transfer->length;
        for (int index = 0; index < transfer->num_iso_packets; ++index) {
            transfer->iso_packet_desc[index].actual_length = transfer->iso_packet_desc[index].length;
            transfer->iso_packet_desc[index].status = transfer->status;
        }
        transfer->callback(transfer);
    }
    return 0;
}

}

namespace {

void APS5_VABI Callback(libusb_transfer* transfer) {
    Require(transfer == callbackTransfer, "callback received native transfer instead of guest transfer");
    Require(transfer->user_data == &callbackCount, "callback user data changed");
    ++callbackCount;
}

void APS5_VABI ResubmitCallback(libusb_transfer* transfer) {
    Callback(transfer);
    if (callbackCount == 1) Require(sceUsbdSubmitTransfer(transfer) == 0, "resubmission from callback failed");
}

void APS5_VABI FreeCallback(libusb_transfer* transfer) {
    Callback(transfer);
    sceUsbdFreeTransfer(transfer);
}

}

int main() {
    const auto invalid = static_cast<std::int32_t>(0x80240002);
    const auto busy = static_cast<std::int32_t>(0x80240006);
    const auto notFound = static_cast<std::int32_t>(0x80240005);
    Require(sceUsbdInit() == 0, "initialization failed");
    Require(sceUsbdSubmitTransfer(nullptr) == invalid && sceUsbdCancelTransfer(nullptr) == invalid, "null transfer accepted");
    Require(sceUsbdAllocTransfer(-1) == nullptr, "negative packet count accepted");
    auto* transfer = sceUsbdAllocTransfer(2);
    Require(transfer != nullptr && allocations == 2 && transfer->num_iso_packets == 0, "transfer allocation did not zero public packet count");
    callbackTransfer = transfer;
    unsigned char data[8]{};
    sceUsbdFillInterruptTransfer(transfer, mockHandle, 0x81, data, 8, Callback, &callbackCount, 234);
    Require(sceUsbdSubmitTransfer(transfer) == 0 && sceUsbdSubmitTransfer(transfer) == busy, "in-flight transfer submitted twice");
    Require(pending[0] != transfer && pending[0]->buffer == data && pending[0]->length == 8 && pending[0]->timeout == 234, "native transfer identity or data changed");
    bool rejectedFree = false;
    try {
        sceUsbdFreeTransfer(transfer);
    } catch (const std::runtime_error&) {
        rejectedFree = true;
    }
    Require(rejectedFree && allocations == 2, "in-flight transfer freed");
    const UsbdTimeval timeout{0, 0};
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0, "event handling failed");
    Require(callbackCount == 1 && transfer->status == LIBUSB_TRANSFER_COMPLETED && transfer->actual_length == 8, "completion did not update guest");
    Require(sceUsbdCancelTransfer(transfer) == notFound, "inactive transfer cancelled");
    Require(sceUsbdSubmitTransfer(transfer) == 0 && sceUsbdCancelTransfer(transfer) == 0, "cancellation failed");
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0 && callbackCount == 2 && transfer->status == LIBUSB_TRANSFER_CANCELLED, "cancel callback missing");
    for (int error = -1; error >= -12; --error) {
        result = error;
        const auto expected = static_cast<std::int32_t>(0x80240000u - error);
        Require(sceUsbdSubmitTransfer(transfer) == expected, "submit error mapping failed");
    }
    result = LIBUSB_ERROR_OTHER;
    Require(sceUsbdSubmitTransfer(transfer) == static_cast<std::int32_t>(0x802400ff), "other error mapping failed");
    result = 0;
    transfer->num_iso_packets = 3;
    Require(sceUsbdSubmitTransfer(transfer) == invalid, "packet count exceeded allocation");
    transfer->num_iso_packets = 0;
    callbackCount = 0;
    transfer->callback = reinterpret_cast<libusb_transfer_cb_fn>(ResubmitCallback);
    Require(sceUsbdSubmitTransfer(transfer) == 0 && sceUsbdHandleEventsTimeout(&timeout) == 0, "callback resubmission failed");
    Require(callbackCount == 1 && pending.size() == 1, "resubmitted transfer lost");
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0 && callbackCount == 2, "resubmission did not complete");
    transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
    transfer->num_iso_packets = 2;
    transfer->iso_packet_desc[0].length = 3;
    transfer->iso_packet_desc[1].length = 5;
    Require(sceUsbdSubmitTransfer(transfer) == 0, "isochronous transfer submission failed");
    Require(pending[0]->num_iso_packets == 2 && pending[0]->iso_packet_desc[1].length == 5, "isochronous packet input lost");
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0 && transfer->iso_packet_desc[0].actual_length == 3 && transfer->iso_packet_desc[1].actual_length == 5 && transfer->iso_packet_desc[1].status == LIBUSB_TRANSFER_COMPLETED, "isochronous packet completion lost");
    transfer->callback = reinterpret_cast<libusb_transfer_cb_fn>(FreeCallback);
    Require(sceUsbdSubmitTransfer(transfer) == 0 && sceUsbdHandleEventsTimeout(&timeout) == 0 && allocations == 0, "free from callback failed");
    transfer = sceUsbdAllocTransfer(0);
    callbackTransfer = transfer;
    sceUsbdFillInterruptTransfer(transfer, mockHandle, 0x81, static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(8)), 8, Callback, &callbackCount, 0);
    transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER | LIBUSB_TRANSFER_SHORT_NOT_OK;
    Require(sceUsbdSubmitTransfer(transfer) == 0, "auto-free transfer submission failed");
    Require(pending[0]->flags == LIBUSB_TRANSFER_SHORT_NOT_OK, "native transfer retained guest ownership flags");
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0 && allocations == 0, "auto-free did not release transfer");
    sceUsbdExit();
    std::puts("USBD asynchronous transfer tests passed");
    return 0;
}
