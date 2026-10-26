#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceScreenShot/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"

namespace {

constexpr int ErrorArgument = static_cast<int>(0x80BE0001);
constexpr int ErrorOverlayFile = static_cast<int>(0x80BE0003);
constexpr int ErrorOverlaySize = static_cast<int>(0x80BE000E);
constexpr std::uint32_t LargeOverlaySdkVersion = 0x3500000;
constexpr std::size_t MaxOverlayPath = 0x400;
constexpr std::uint16_t FileTypeMask = 0xF000;
constexpr std::uint16_t DirectoryType = 0x4000;

std::atomic<bool> disabled{false};

std::uint32_t CompiledSdkVersion() {
    static const auto* processParameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t version;
    std::memcpy(&version, processParameters + 0x10, sizeof(version));
    return version;
}

int SetOverlayImage(const char* path, int offsetX, int offsetY) {
    const bool large = CompiledSdkVersion() >= LargeOverlaySdkVersion;
    const int maxX = large ? 3839 : 1919;
    const int maxY = large ? 2159 : 1079;
    const std::int64_t maxSize = large ? 8 * 1024 * 1024 : 2 * 1024 * 1024;
    if (!path || offsetX < 0 || offsetX > maxX || offsetY < 0 || offsetY > maxY) return ErrorArgument;
    if (strnlen(path, MaxOverlayPath) >= MaxOverlayPath) return ErrorArgument;
    const int descriptor = sceKernelOpen(path, 0, 0);
    if (descriptor < 0) return ErrorOverlayFile;
    FileStat stat{};
    int result = ErrorOverlayFile;
    if (sceKernelFstat(descriptor, &stat) == 0 && (stat.st_mode & FileTypeMask) != DirectoryType) {
        result = stat.st_size > 0 && stat.st_size <= maxSize ? 0 : ErrorOverlaySize;
    }
    sceKernelClose(descriptor);

    return result;
}

}

extern "C" {

int APS5_VABI sceScreenShotSetOverlayImage(const char* path, int offsetX, int offsetY) {
    return SetOverlayImage(path, offsetX, offsetY);
}

int APS5_VABI sceScreenShotSetOverlayImageWithOrigin(const char* path, int offsetX, int offsetY, int origin) {
    if (origin < 1 || origin > 9) return ErrorArgument;
    return SetOverlayImage(path, offsetX, offsetY);
}

int APS5_VABI sceScreenShotDisable() {
    disabled = true;
    return 0;
}

int APS5_VABI sceScreenShotEnable() {
    disabled = false;
    return 0;
}

int APS5_VABI sceScreenShotIsDisabled(int id, int* isDisabled) {
    if (id == -1 || !isDisabled) return ErrorArgument;
    *isDisabled = disabled ? 1 : 0;
    return 0;
}

int APS5_VABI sceScreenShotDisableNotification() {
    return 0;
}

int APS5_VABI sceScreenShotEnableNotification() {
    return 0;
}

int APS5_VABI sceScreenShotCapture() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceScreenShotGetAppInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceScreenShotGetDrcParam() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceScreenShotIsVshScreenCaptureDisabled() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceScreenShotSetDrcParam() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceScreenShotSetParam() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
