#include "prx/libSceUsbd/include/UsbdTransfer.hpp"

#include <libusb.h>

#include <mutex>
#include <unordered_map>

namespace {

struct TransferContext {
    UsbdTransferCallback callback;
    void* userData;
};

std::mutex g_transferMutex;
std::unordered_map<libusb_transfer*, TransferContext> g_transferContexts;

void TransferCompleted(libusb_transfer* transfer) {
    TransferContext context{};
    {
        std::lock_guard<std::mutex> lock(g_transferMutex);
        const auto entry = g_transferContexts.find(transfer);
        if (entry == g_transferContexts.end()) return;
        context = entry->second;
        g_transferContexts.erase(entry);
    }
    transfer->user_data = context.userData;
    context.callback(reinterpret_cast<UsbdTransfer*>(transfer));
}

}

namespace Usbd {

void RegisterTransferCallback(UsbdTransfer* transfer, UsbdTransferCallback callback, void* userData) {
    auto* hostTransfer = reinterpret_cast<libusb_transfer*>(transfer);
    transfer->userData = userData;
    {
        std::lock_guard<std::mutex> lock(g_transferMutex);
        g_transferContexts[hostTransfer] = TransferContext{callback, userData};
    }
    hostTransfer->callback = &TransferCompleted;
    transfer->callback = callback;
}

void UnregisterTransferCallback(UsbdTransfer* transfer) {
    std::lock_guard<std::mutex> lock(g_transferMutex);
    g_transferContexts.erase(reinterpret_cast<libusb_transfer*>(transfer));
}

}
