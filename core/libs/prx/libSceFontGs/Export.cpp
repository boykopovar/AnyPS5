#include "prx/libSceFontGs/include/InventoryExports.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int FontGsFatal = static_cast<int>(0x80460001);
constexpr int FontGsInvalidParameter = static_cast<int>(0x80460002);
constexpr int FontGsInvalidSequence = static_cast<int>(0x8046009a);
constexpr int FontGsSequenceLocked = static_cast<int>(0x804600b8);
constexpr std::uint8_t FontGnmSurfaceTag = 0x48;
constexpr std::uint16_t FontGnmSequenceTag = 0xf80;
constexpr float FontGnmDrawupLimit = 16834.0f;
constexpr std::size_t FontGnmRecordStateOffset = 0x10;

struct FontGnmSurface {
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

static_assert(offsetof(FontGnmSurface, tag) == 0x0d);
static_assert(offsetof(FontGnmSurface, mode) == 0x0f);
static_assert(offsetof(FontGnmSurface, busy) == 0x1c);
static_assert(offsetof(FontGnmSurface, fillObject) == 0x28);
static_assert(offsetof(FontGnmSurface, initialized) == 0x38);

struct FontGnmCommands {
    std::uint8_t reserved[0x28];
    const void* first;
    std::uint32_t firstSize;
    std::uint32_t secondCount;
    const void* second;
};

static_assert(offsetof(FontGnmCommands, first) == 0x28);
static_assert(offsetof(FontGnmCommands, firstSize) == 0x30);
static_assert(offsetof(FontGnmCommands, secondCount) == 0x34);
static_assert(offsetof(FontGnmCommands, second) == 0x38);

struct FontGnmSequence {
    std::uint16_t tag;
    std::uint8_t reserved2[0x19];
    std::uint8_t lock;
    std::uint8_t reserved1c[0xc];
    FontGnmCommands* commands;
};

static_assert(offsetof(FontGnmSequence, lock) == 0x1b);
static_assert(offsetof(FontGnmSequence, commands) == 0x28);

const FontGnmSurface* ReadGnmSurface(const void* surface) {
    const auto* const gnm = static_cast<const FontGnmSurface*>(surface);
    if (gnm->tag != FontGnmSurfaceTag || gnm->head == nullptr || gnm->initialized != 1 || gnm->busy != 0) return nullptr;
    return gnm;
}

void MarkGnmRecord(void* output) {
    *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(output) + FontGnmRecordStateOffset) = 1;
}

const FontGnmCommands* CheckSequence(const FontGraphicsCanvasSequence* sequence, bool outputs, int& error) {
    const auto* const seq = reinterpret_cast<const FontGnmSequence*>(sequence);
    if (seq == nullptr || seq->tag != FontGnmSequenceTag) error = FontGsInvalidSequence;
    else if (!outputs) error = FontGsInvalidParameter;
    else if ((seq->lock & 1) != 0) error = FontGsSequenceLocked;
    else if (seq->commands == nullptr) error = FontGsFatal;
    else return seq->commands;
    sceFontGraphicsReportError(error);
    return nullptr;
}

}

extern "C" {

void* APS5_VABI sceFontSelectGraphicsGnm(std::uint32_t index) {
    if (index == 0) NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

int APS5_VABI sceFontGraphicsGnmSurfaceInit(void* surface, const void* config) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsGnmDrawupFillTextureImage(FontGraphicsFrame* graphicsObject, const void* surface, void** output,
                                                     float x, float y, float width, float height) {
    if (output == nullptr) return FontGsInvalidParameter;
    *output = nullptr;
    if (graphicsObject == nullptr || surface == nullptr) return FontGsInvalidParameter;
    const auto* const gnm = ReadGnmSurface(surface);
    if (gnm == nullptr || !(x >= 0.0f)) return FontGsInvalidParameter;
    const float right = x + width;
    if (!(right >= x) || !(right <= FontGnmDrawupLimit) || !(y >= 0.0f)) return FontGsInvalidParameter;
    const float bottom = y + height;
    if (!(bottom >= y) || !(bottom <= FontGnmDrawupLimit)) return FontGsInvalidParameter;
    const int error = sceFontGraphicsDrawupFillTextureImageObject(graphicsObject, gnm->fillObject, gnm->mode, output, x, y, width, height);
    if (error != 0) return error;
    MarkGnmRecord(*output);
    return 0;
}

int APS5_VABI sceFontGraphicsGnmDrawupFillTexturePattern(FontGraphicsFrame* graphicsObject, const void* surface, std::uint32_t format,
                                                       std::uint32_t select, std::uint32_t dimension1, std::uint32_t option1,
                                                       std::uint32_t option2, std::uint32_t dimension2, void** output) {
    if (output == nullptr) return FontGsInvalidParameter;
    *output = nullptr;
    if (graphicsObject == nullptr || surface == nullptr) return FontGsInvalidParameter;
    const auto* const gnm = ReadGnmSurface(surface);
    if (gnm == nullptr) return FontGsInvalidParameter;
    const int error = sceFontGraphicsDrawupFillTexturePatternObject(graphicsObject, gnm->fillObject, gnm->mode, format, select,
                                                                  dimension1, option1, option2, dimension2, output);
    if (error != 0) return error;
    MarkGnmRecord(*output);
    return 0;
}

int APS5_VABI sceFontGraphicsSequenceGetGnmCommandsForChain(FontGraphicsCanvasSequence* sequence, void** commands,
                                                          std::uint32_t* numCommands, void** commands2, std::uint32_t* numCommands2) {
    if (commands != nullptr) *commands = nullptr;
    if (numCommands != nullptr) *numCommands = 0;
    if (commands2 != nullptr) *commands2 = nullptr;
    if (numCommands2 != nullptr) *numCommands2 = 0;
    int error = 0;
    const bool outputs = commands != nullptr && numCommands != nullptr && commands2 != nullptr && numCommands2 != nullptr;
    const auto* const gnm = CheckSequence(sequence, outputs, error);
    if (gnm == nullptr) return error;
    *commands = const_cast<void*>(gnm->first);
    *numCommands = gnm->firstSize >> 2;
    *commands2 = const_cast<void*>(gnm->second);
    *numCommands2 = gnm->secondCount;
    return 0;
}

int APS5_VABI sceFontGraphicsSequenceGetGnmCommandsForSubmit(FontGraphicsCanvasSequence* sequence, void** commands,
                                                           std::uint32_t* numCommands) {
    if (commands != nullptr) *commands = nullptr;
    if (numCommands != nullptr) *numCommands = 0;
    int error = 0;
    const auto* const gnm = CheckSequence(sequence, commands != nullptr && numCommands != nullptr, error);
    if (gnm == nullptr) return error;
    *commands = const_cast<void*>(gnm->first);
    *numCommands = gnm->firstSize;
    return 0;
}

}
