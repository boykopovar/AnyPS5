#include "prx/libSceScreenShot/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int ErrorArgument = static_cast<int>(0x80BE0001);

using CaptureFn = int (APS5_VABI*)(std::uint32_t, void*, void*, void*);
using GetAppInfoFn = int (APS5_VABI*)(int, void*);
using IsVshDisabledFn = int (APS5_VABI*)(int*);
using GetDrcParamFn = int (APS5_VABI*)(int, void*, std::uint64_t, void*, void*, std::uint64_t, void*);
using BlockFn = int (APS5_VABI*)(const void*);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ScreenShot checks: %s\n", message);
    std::abort();
}

template <typename Function, typename Export>
Function As(Export function) {
    return reinterpret_cast<Function>(function);
}

template <std::size_t Size>
struct Block {
    alignas(8) std::array<std::byte, Size> bytes{};

    template <typename T>
    void Set(std::size_t offset, T value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }
};

Block<0x28> Param(const char* first, const char* second, const char* third) {
    Block<0x28> param;
    param.Set<std::uint32_t>(0x00, 0x28);
    param.Set(0x08, first);
    param.Set(0x10, second);
    param.Set(0x18, third);
    return param;
}

Block<0x30> DrcParam(const void* first, const void* second) {
    Block<0x30> param;
    param.Set<std::uint32_t>(0x00, 0x30);
    param.Set(0x08, first);
    param.Set<std::uint32_t>(0x10, 0x400);
    param.Set(0x18, second);
    param.Set<std::uint32_t>(0x20, 0x8000);
    return param;
}

}

int main() {
    const auto setParam = As<BlockFn>(&sceScreenShotSetParam);
    const auto capture = As<CaptureFn>(&sceScreenShotCapture);
    const auto getAppInfo = As<GetAppInfoFn>(&sceScreenShotGetAppInfo);
    const auto isVshDisabled = As<IsVshDisabledFn>(&sceScreenShotIsVshScreenCaptureDisabled);
    const auto getDrcParam = As<GetDrcParamFn>(&sceScreenShotGetDrcParam);
    const auto setDrcParam = As<BlockFn>(&sceScreenShotSetDrcParam);
    std::array<std::byte, 0x1000> output{};

    Require(setParam(nullptr) == ErrorArgument, "SetParam took a null parameter");
    auto param = Param(nullptr, nullptr, nullptr);
    param.Set<std::uint64_t>(0x20, 1);
    Require(setParam(&param) == ErrorArgument, "SetParam took a nonzero reserved field");
    for (const std::uint32_t size : {0x00u, 0x27u, 0x29u}) {
        param = Param(nullptr, nullptr, nullptr);
        param.Set<std::uint32_t>(0x00, size);
        Require(setParam(&param) == ErrorArgument, "SetParam took a size other than 0x28");
    }
    const std::string long256(256, 'a');
    const std::string long512(512, 'a');
    param = Param(long256.c_str(), nullptr, nullptr);
    Require(setParam(&param) == ErrorArgument, "SetParam took a first string of 256 characters");
    param = Param(nullptr, long256.c_str(), nullptr);
    Require(setParam(&param) == ErrorArgument, "SetParam took a second string of 256 characters");
    param = Param(nullptr, nullptr, long512.c_str());
    Require(setParam(&param) == ErrorArgument, "SetParam took a third string of 512 characters");

    Require(capture(2, output.data(), nullptr, nullptr) == ErrorArgument && capture(0xffffffffu, output.data(), nullptr, nullptr) == ErrorArgument, "Capture took a type of 2 or more");
    Require(capture(0, nullptr, nullptr, nullptr) == ErrorArgument && capture(1, nullptr, nullptr, nullptr) == ErrorArgument, "Capture took a null output");

    Require(getAppInfo(-1, output.data()) == ErrorArgument, "GetAppInfo took the id -1");
    Require(getAppInfo(0, nullptr) == ErrorArgument, "GetAppInfo took a null output");

    Require(isVshDisabled(nullptr) == ErrorArgument, "IsVshScreenCaptureDisabled took a null output");

    void* out = output.data();
    Require(getDrcParam(-1, out, 0, out, out, 0, out) == ErrorArgument, "GetDrcParam took the id -1");
    Require(getDrcParam(0, nullptr, 0, out, out, 0, out) == ErrorArgument, "GetDrcParam took a null second argument");
    Require(getDrcParam(0, out, 0, nullptr, out, 0, out) == ErrorArgument, "GetDrcParam took a null fourth argument");
    Require(getDrcParam(0, out, 0, out, nullptr, 0, out) == ErrorArgument, "GetDrcParam took a null fifth argument");
    Require(getDrcParam(0, out, 0, out, out, 0, nullptr) == ErrorArgument, "GetDrcParam took a null seventh argument");

    Require(setDrcParam(nullptr) == ErrorArgument, "SetDrcParam took a null parameter");
    auto drc = DrcParam(out, out);
    drc.Set<std::uint64_t>(0x28, 1);
    Require(setDrcParam(&drc) == ErrorArgument, "SetDrcParam took a nonzero reserved field");
    drc = DrcParam(out, out);
    drc.Set<std::uint32_t>(0x00, 0x2f);
    Require(setDrcParam(&drc) == ErrorArgument, "SetDrcParam took a size other than 0x30");
    drc = DrcParam(nullptr, out);
    Require(setDrcParam(&drc) == ErrorArgument, "SetDrcParam took a null first buffer");
    drc = DrcParam(out, nullptr);
    Require(setDrcParam(&drc) == ErrorArgument, "SetDrcParam took a null second buffer");
    drc = DrcParam(out, out);
    drc.Set<std::uint32_t>(0x10, 0x3ff);
    Require(setDrcParam(&drc) == ErrorArgument, "SetDrcParam took a first count other than 0x400");
    drc = DrcParam(out, out);
    drc.Set<std::uint32_t>(0x20, 0x7fff);
    Require(setDrcParam(&drc) == ErrorArgument, "SetDrcParam took a second count other than 0x8000");

    std::puts("ScreenShot checks passed");
    return 0;
}
