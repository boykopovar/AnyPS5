#include "Hmd.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>

namespace {

std::atomic<bool> initialized{false};

std::int32_t Initialize(const Hmd::InitializeParam* param) {
    if (initialized.load()) return Hmd::AlreadyInitialized;
    if (!param) return Hmd::ParameterNull;
    if (param->reserved0) return Hmd::UnsupportedFeature;
    return initialized.exchange(true) ? Hmd::AlreadyInitialized : 0;
}

}

extern "C" {

std::int32_t APS5_VABI sceHmdInitialize(const Hmd::InitializeParam* param) {
    return Initialize(param);
}

std::int32_t APS5_VABI sceHmdInitialize315(const Hmd::InitializeParam* param) {
    return Initialize(param);
}

std::int32_t APS5_VABI sceHmdTerminate() {
    return initialized.exchange(false) ? 0 : Hmd::NotInitialized;
}

std::int32_t APS5_VABI sceHmdOpen(std::int32_t userId, std::int32_t type, std::int32_t index,
                                Hmd::OpenParam* param) {
    if (!initialized.load()) return Hmd::NotInitialized;
    if (type != 0 || index != 0 || param || userId == -1 || userId == 0xff)
        return Hmd::ParameterInvalid;
    return Hmd::DeviceDisconnected;
}

std::int32_t APS5_VABI sceHmdClose(std::int32_t handle) {
    (void)handle;
    return initialized.load() ? Hmd::InvalidHandle : Hmd::NotInitialized;
}

std::int32_t APS5_VABI sceHmdGetDeviceInformation(Hmd::DeviceInformation* info) {
    if (!info) return Hmd::ParameterNull;
    if (!initialized.load()) return Hmd::NotInitialized;
    *info = {};
    info->status = Hmd::DeviceStatus::NotDetected;
    info->userId = -1;
    return 0;
}

std::int32_t APS5_VABI sceHmdGetDeviceInformationByHandle(std::int32_t handle,
                                                        Hmd::DeviceInformation* info) {
    (void)handle;
    (void)info;
    return initialized.load() ? Hmd::InvalidHandle : Hmd::NotInitialized;
}

std::int32_t APS5_VABI sceHmdInternalGetDeviceStatus(Hmd::DeviceStatus* status) {
    if (!status) return Hmd::ParameterNull;
    *status = Hmd::DeviceStatus::NotDetected;
    return 0;
}

std::int32_t APS5_VABI sceHmdInternalMmapIsConnect() {
    return 0;
}

int APS5_VABI sceHmdDistortionInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGet2dVrCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGetCorrectionCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGetWorkMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGetWorkMemoryAlign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdFillDistortionBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGet2DEyeOffset() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGet2dDistortionMap() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGet2dVrCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionCorrectionCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionMap() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionParams() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionWorkMemoryAlign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionWorkMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetEyeStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetFieldOfView() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
