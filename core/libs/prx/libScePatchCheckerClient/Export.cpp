#include "prx/libScePatchCheckerClient/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI scePatchCheckerCancel() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerCheckPatch() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerCreateHandler() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerDestroyHandler() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerDisableAutoDownload() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerEnableAutoDownload() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerGetApplicableTick() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerGetPackageInfo() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerInitialize() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        throw std::logic_error(std::string(__func__) + ": already initialized");
    }
    return 0;
}

int APS5_VABI scePatchCheckerRequestCheckPatch() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerRequestCheckPatchByType() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerTerminate() {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    return 0;
}

}
