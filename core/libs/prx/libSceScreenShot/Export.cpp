#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceScreenShot/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
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

template <typename T>
T Field(const void* block, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(block) + offset, sizeof(value));
    return value;
}

bool StringFits(const char* text, std::size_t maxLength) {
    return text == nullptr || strnlen(text, maxLength + 1) <= maxLength;
}

[[noreturn]] void ScreenShotService(const char* function) {
    throw std::runtime_error(std::string(function) + ": the screenshot service is not available");
}

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

int APS5_VABI sceScreenShotCapture(std::uint32_t type, void* result, int (APS5_VABI*)(void*), void*) {
    if (type >= 2 || !result) return ErrorArgument;
    ScreenShotService(__func__);
}
int APS5_VABI sceScreenShotGetAppInfo(int id, void* info) {
    if (id == -1 || !info) return ErrorArgument;
    ScreenShotService(__func__);
}
int APS5_VABI sceScreenShotGetDrcParam(int id, void* first, std::uint64_t, void* second, void* third, std::uint64_t, void* fourth) {
    if (id == -1 || !first || !second || !third || !fourth) return ErrorArgument;
    ScreenShotService(__func__);
}
int APS5_VABI sceScreenShotIsVshScreenCaptureDisabled(int* isDisabled) {
    if (!isDisabled) return ErrorArgument;
    ScreenShotService(__func__);
}
int APS5_VABI sceScreenShotSetDrcParam(const void* param) {
    if (!param || Field<std::uint64_t>(param, 0x28) != 0 || Field<std::uint32_t>(param, 0x00) != 0x30) return ErrorArgument;
    if (!Field<const void*>(param, 0x08) || !Field<const void*>(param, 0x18)) return ErrorArgument;
    if (Field<std::uint32_t>(param, 0x10) != 0x400 || Field<std::uint32_t>(param, 0x20) != 0x8000) return ErrorArgument;
    ScreenShotService(__func__);
}
int APS5_VABI sceScreenShotSetParam(const void* param) {
    if (!param || Field<std::uint64_t>(param, 0x20) != 0 || Field<std::uint32_t>(param, 0x00) != 0x28) return ErrorArgument;
    if (!StringFits(Field<const char*>(param, 0x08), 255) || !StringFits(Field<const char*>(param, 0x10), 255) || !StringFits(Field<const char*>(param, 0x18), 511)) return ErrorArgument;
    ScreenShotService(__func__);
}

}
