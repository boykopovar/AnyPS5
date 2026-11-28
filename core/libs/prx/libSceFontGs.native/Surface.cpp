#include "prx/libSceFontGs.native/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace {

constexpr int InvalidParameter = static_cast<int>(0x80460002);
constexpr int UnsupportedSurface = static_cast<int>(0x8046005c);

struct Config {
    std::uint64_t base;
    std::uint32_t reserved08;
    std::int8_t bytesPerPixel;
    std::uint8_t tag;
    std::uint16_t reserved0e;
    std::uint32_t width, height, x, y, tileMode, flags;
    std::uint32_t reserved28[2];
    std::uint16_t view, reserved32;
    std::uint32_t user, reserved38[2];
    std::uint32_t format, numberType, swap, reserved4c[3];
    std::uint64_t maskBase;
    std::uint32_t clearWord0, clearWord1;
    std::uint64_t metadataBase;
};

struct Register {
    std::uint32_t tag, value;
};

struct Surface {
    std::uint64_t base;
    std::uint32_t pitch;
    std::uint8_t bytesPerPixel, format;
    std::uint16_t tag;
    std::uint32_t width, height, x, y, clipWidth, clipHeight, user;
    std::uint16_t view, viewCount;
    std::uint32_t reserved30, flags, reserved38;
    std::uint16_t targetWidth, targetHeight;
    std::array<Register, 16> registers;
    std::uint32_t reservedc0[16];
};

static_assert(offsetof(Config, format) == 0x40 && offsetof(Config, maskBase) == 0x58 && sizeof(Config) == 0x70);
static_assert(offsetof(Surface, registers) == 0x40 && sizeof(Surface) == 0x100);

struct Format {
    unsigned numberTypes, renderableTypes;
    std::uint8_t surfaceFormat;
};

constexpr Format LookupFormat(std::uint32_t format) {
    switch (format) {
        case 0x04: return {0x73, 0x01, 0x10};
        case 0x0c: return {0x73, 0x41, 0x20};
        case 0x20: case 0x24: return {0x33, 0x33, 0x4a};
        case 0x28: return {0x73, 0x41, 0x40};
        case 0x30: return {0xb3, 0xb3, 0x4e};
        case 0x38: return {0xb0, 0xb0, 0x4f};
        case 0x7c: return {0x80, 0x80, 0x4a};
        case 0x08: case 0x14: case 0x18: case 0x1c: return {0xb3, 0, 0};
        case 0x10: case 0x2c: return {0xb0, 0, 0};
        case 0x40: case 0x44: case 0x48: case 0x4c: return {0x01, 0, 0};
        default: return {};
    }
}

constexpr bool SupportsTile(std::uint32_t tile, std::uint32_t format) {
    switch (tile) {
        case 0: case 0x4000: case 0x8000: case 0x14000: case 0x24000: case 0x44000: case 0x6c000: return true;
        case 0x60000: return format != 0x38;
        default: return false;
    }
}

constexpr std::uint32_t ColorInfo(const Config& c, std::uint32_t swap) {
    if (c.format == 0x0c) return 0x980c;
    const std::uint32_t format = c.format | c.numberType | swap;
    switch (c.numberType) {
        case 0: case 0x100: case 0x600: return format | 0x8000;
        case 0x400: case 0x500: return format | 0x50000;
        default: return format | 0x40000;
    }
}

void EncodeAddress(std::uint64_t address, Register& low, Register& high) {
    low.value = static_cast<std::uint32_t>(address >> 8);
    high.value = static_cast<std::uint8_t>(address >> 40);
}

}

extern "C" int APS5_VABI sceFontGraphicsAgcSurfaceInit(void* output, const void* configuration) {
    Surface surface{};
    if (!GuestCopyOut_nid_no_patch(output, &surface, sizeof(surface))) return InvalidParameter;
    Config c{};
    if (!GuestCopyIn_nid_no_patch(&c, configuration, sizeof(c)) || c.tag != 0x54) return InvalidParameter;
    if (c.x || c.y || c.flags != 0x01000000) return UnsupportedSurface;
    const auto format = LookupFormat(c.format);
    const std::uint32_t swap = (c.swap & ~0x1800u) == 0 ? c.swap : 0;
    if ((c.numberType & ~0x700u) != 0 || (c.format == 0x7c && swap != 0)) return InvalidParameter;
    const unsigned numberType = 1u << (c.numberType >> 8);
    if (!(format.numberTypes & numberType)) return InvalidParameter;
    if (!(format.renderableTypes & numberType) || !SupportsTile(c.tileMode, c.format) || c.bytesPerPixel < 0 ||
        c.width == 0 || c.width > 16384 || c.height == 0 || c.height > 16384) return UnsupportedSurface;

    surface.base = c.base;
    surface.pitch = c.width * c.bytesPerPixel;
    surface.bytesPerPixel = static_cast<std::uint8_t>(c.bytesPerPixel);
    surface.format = format.surfaceFormat;
    surface.tag = 0x47e0;
    surface.width = surface.clipWidth = surface.targetWidth = static_cast<std::uint16_t>(c.width);
    surface.height = surface.clipHeight = surface.targetHeight = static_cast<std::uint16_t>(c.height);
    surface.user = c.user;
    surface.view = c.view;
    surface.viewCount = 1;
    surface.flags = 0x440100;
    auto& regs = surface.registers;
    for (auto& reg : regs) reg.tag = 0x10000000;
    EncodeAddress(c.base, regs[0], regs[10]);
    EncodeAddress(c.maskBase, regs[5], regs[11]);
    EncodeAddress(c.metadataBase, regs[6], regs[12]);
    regs[2].value = ColorInfo(c, swap);
    regs[4].value = 0x48;
    regs[7].value = c.clearWord0;
    regs[8].value = c.clearWord1;
    regs[14].value = ((c.width - 1) << 14) | (c.height - 1);
    regs[15].value = 0x45000000 | c.tileMode;
    return GuestCopyOut_nid_no_patch(output, &surface, sizeof(surface)) ? 0 : InvalidParameter;
}
