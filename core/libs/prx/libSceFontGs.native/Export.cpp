#include "prx/libSceFontGs.native/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int FontGsInvalidParameter = static_cast<int>(0x80460002);
constexpr int FontGsNoCommands = static_cast<int>(0x80460001);
constexpr int FontGsInvalidSequence = static_cast<int>(0x8046009a);
constexpr int FontGsSequenceLocked = static_cast<int>(0x804600b8);
constexpr std::uint8_t FontAgcSurfaceTag = 0x58;
constexpr std::uint16_t FontAgcSequenceTag = 0xf80;
constexpr float FontAgcDrawupLimit = 16834.0f;
constexpr std::size_t FontAgcRecordStateOffset = 0x10;

struct FontAgcSurface {
    void* head;
    std::uint8_t reserved8[5];
    std::uint8_t tag;
    std::uint8_t reserved0e;
    std::uint8_t mode;
    std::uint8_t reserved10[0xc];
    std::uint32_t busy;
    std::uint8_t reserved20[8];
    const void* fillObject;
    std::uint8_t reserved30[8];
    std::uint32_t initialized;
};

static_assert(offsetof(FontAgcSurface, tag) == 0x0d);
static_assert(offsetof(FontAgcSurface, mode) == 0x0f);
static_assert(offsetof(FontAgcSurface, busy) == 0x1c);
static_assert(offsetof(FontAgcSurface, fillObject) == 0x28);
static_assert(offsetof(FontAgcSurface, initialized) == 0x38);

struct FontAgcCommands {
    std::uint8_t reserved[0x28];
    const void* first;
    std::uint32_t firstSize;
    std::uint32_t secondCount;
    const void* second;
};

static_assert(offsetof(FontAgcCommands, first) == 0x28);
static_assert(offsetof(FontAgcCommands, firstSize) == 0x30);
static_assert(offsetof(FontAgcCommands, secondCount) == 0x34);
static_assert(offsetof(FontAgcCommands, second) == 0x38);

struct FontAgcSequence {
    std::uint16_t tag;
    std::uint8_t reserved2[0x19];
    std::uint8_t lock;
    std::uint8_t reserved1c[0xc];
    FontAgcCommands* commands;
};

static_assert(offsetof(FontAgcSequence, lock) == 0x1b);
static_assert(offsetof(FontAgcSequence, commands) == 0x28);

const FontAgcSurface* ReadAgcSurface(const void* surface) {
    const auto* const agc = static_cast<const FontAgcSurface*>(surface);
    if (agc->tag != FontAgcSurfaceTag || agc->head == nullptr || agc->initialized != 1 || agc->busy != 0) {
        return nullptr;
    }
    return agc;
}

void MarkAgcRecord(void* output) {
    *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(output) + FontAgcRecordStateOffset) = 1;
}

}

extern "C" {

void* APS5_VABI sceFontSelectGraphicsAgc(std::uint32_t index) {
    return nullptr;
}

int APS5_VABI sceFontGraphicsAgcSurfaceInit(void* surface, const void* config) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsAgcDrawupFillTextureImage(FontGraphicsFrame* graphicsObject, const void* surface, void** output,
                                                     float x, float y, float width, float height) {
    if (output == nullptr) {
        return FontGsInvalidParameter;
    }
    *output = nullptr;
    if (graphicsObject == nullptr || surface == nullptr) {
        return FontGsInvalidParameter;
    }
    const auto* const agc = static_cast<const FontAgcSurface*>(surface);
    if (agc->tag != FontAgcSurfaceTag || agc->head == nullptr || agc->initialized != 1 || !(x >= 0.0f) || agc->busy != 0 ||
        !(y >= 0.0f)) {
        return FontGsInvalidParameter;
    }
    const float right = x + width;
    const float bottom = y + height;
    if (!(right >= x) || !(right <= FontAgcDrawupLimit) || !(bottom >= y) || !(bottom <= FontAgcDrawupLimit)) {
        return FontGsInvalidParameter;
    }
    const int error = sceFontGraphicsDrawupFillTextureImageObject(graphicsObject, agc->fillObject, agc->mode, output, x, y,
                                                                width, height);
    if (error != 0) {
        return error;
    }
    MarkAgcRecord(*output);
    return 0;
}

int APS5_VABI sceFontGraphicsAgcDrawupFillTexturePattern(FontGraphicsFrame* graphicsObject, const void* surface, std::uint32_t format,
                                                       std::uint32_t select, std::uint32_t dimension1, std::uint32_t option1,
                                                       std::uint32_t option2, std::uint32_t dimension2, void** output) {
    if (output == nullptr) {
        return FontGsInvalidParameter;
    }
    *output = nullptr;
    if (graphicsObject == nullptr || surface == nullptr) {
        return FontGsInvalidParameter;
    }
    const auto* const agc = ReadAgcSurface(surface);
    if (agc == nullptr) {
        return FontGsInvalidParameter;
    }
    const int error = sceFontGraphicsDrawupFillTexturePatternObject(graphicsObject, agc->fillObject, agc->mode, format,
                                                                  select, dimension1, option1, option2, dimension2, output);
    if (error != 0) {
        return error;
    }
    MarkAgcRecord(*output);
    return 0;
}

int APS5_VABI sceFontGraphicsSequenceGetAgcCommandsForJump(FontGraphicsCanvasSequence* sequence, void** commands,
                                                         std::uint32_t* numCommands, void** commands2, std::uint32_t* numCommands2) {
    if (commands != nullptr) {
        *commands = nullptr;
    }
    if (numCommands != nullptr) {
        *numCommands = 0;
    }
    if (commands2 != nullptr) {
        *commands2 = nullptr;
    }
    if (numCommands2 != nullptr) {
        *numCommands2 = 0;
    }
    const auto* const seq = reinterpret_cast<const FontAgcSequence*>(sequence);
    if (seq == nullptr || seq->tag != FontAgcSequenceTag) {
        return FontGsInvalidSequence;
    }
    if (commands == nullptr || numCommands == nullptr || commands2 == nullptr || numCommands2 == nullptr) {
        return FontGsInvalidParameter;
    }
    if ((seq->lock & 1) != 0) {
        return FontGsSequenceLocked;
    }
    const auto* const agc = seq->commands;
    if (agc == nullptr) {
        return FontGsNoCommands;
    }
    *commands = const_cast<void*>(agc->first);
    *numCommands = agc->firstSize >> 2;
    *commands2 = const_cast<void*>(agc->second);
    *numCommands2 = agc->secondCount;
    return 0;
}

int APS5_VABI sceFontGraphicsSequenceGetAgcCommandsForSubmit(FontGraphicsCanvasSequence* sequence, void** commands,
                                                           std::uint32_t* numCommands) {
    if (commands != nullptr) {
        *commands = nullptr;
    }
    if (numCommands != nullptr) {
        *numCommands = 0;
    }
    const auto* const seq = reinterpret_cast<const FontAgcSequence*>(sequence);
    if (seq == nullptr || seq->tag != FontAgcSequenceTag) {
        return FontGsInvalidSequence;
    }
    if (commands == nullptr || numCommands == nullptr) {
        return FontGsInvalidParameter;
    }
    if ((seq->lock & 1) != 0) {
        return FontGsSequenceLocked;
    }
    const auto* const agc = seq->commands;
    if (agc == nullptr) {
        return FontGsNoCommands;
    }
    *commands = const_cast<void*>(agc->first);
    *numCommands = agc->firstSize;
    return 0;
}

}
