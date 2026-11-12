#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpPartner001/include/InventoryExports.hpp"
#include "prx/libSceNpPartner001/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cerrno>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> g_initialized{false};
}

extern "C" {

int APS5_VABI sceNpEAAccessInitialize(void) {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceNpEAAccessTerminate(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    return 0;
}

int APS5_VABI sceNpHasEAAccessSubscription(int userId, bool* result) {
    (void)userId;
    if (!g_initialized.load()) throw std::logic_error(std::string(__func__) + ": not initialized");
    if (result == nullptr) throw std::logic_error(std::string(__func__) + ": null result");
    const bool subscribed = false;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(result, &subscribed, sizeof(subscribed));
    errno = savedErrno;
    if (!copied) throw std::logic_error(std::string(__func__) + ": unwritable result");
    return 0;
}

}

extern "C" {

int APS5_VABI libSceNpPartner001Unknown_pQfYTZHznMc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pQfYTZHznMc", libSceNpPartner001Unknown_pQfYTZHznMc_nid_no_patch);

}
