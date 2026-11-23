#include "prx/libSceFont/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

struct ColorRegister {
    std::uint32_t tag;
    std::uint32_t value;
};
using ColorTarget = std::array<ColorRegister, 16>;
static_assert(sizeof(ColorTarget) == 0x80);

std::uint32_t TextureFormat(std::uint32_t format, std::uint32_t number) {
    const bool normalizedOrInteger = number == 0 || number == 1 || number == 4 || number == 5;
    switch (format) {
    case 0x04: if (number == 0) return 1; break;
    case 0x0c: if (number == 0) return 14; break;
    case 0x20: if (normalizedOrInteger) return 44 + number; break;
    case 0x24: if (normalizedOrInteger) return 50 + number; break;
    case 0x28:
        if (number == 0) return 56;
        if (number == 6) return 130;
        break;
    case 0x30:
        if (normalizedOrInteger) return 65 + number;
        if (number == 7) return 71;
        break;
    case 0x38:
        if (number == 4 || number == 5) return 71 + number;
        if (number == 7) return 77;
        break;
    case 0x7c: if (number == 7) return 131; break;
    }
    NotImplemented_nid_no_patch("sceFontSelectGraphicsAgc_texture_format");
}

std::uint32_t AlignmentInBlocks(std::uint32_t tile) {
    switch (tile) {
    case 0: case 1: case 2: return 1;
    case 5: return 16;
    case 9: case 17: case 24: case 27: return 256;
    default: NotImplemented_nid_no_patch("sceFontSelectGraphicsAgc_texture_tile");
    }
}

int APS5_VABI StructureTexture(void* object, ColorRegister* source, std::uint64_t address) {
    if (object == nullptr) return 0;
    ColorTarget regs;
    if (!GuestCopyIn_nid_no_patch(regs.data(), source, sizeof(regs)))
        throw std::invalid_argument("unreadable AGC color target");

    const auto block = address >> 8;
    const auto low = static_cast<std::uint32_t>(block);
    const auto high = static_cast<std::uint8_t>(block >> 32);
    if (!GuestCopyOut_nid_no_patch(&source[0].value, &low, sizeof(low)) ||
        !GuestCopyOut_nid_no_patch(&source[10].value, &high, sizeof(high)))
        throw std::invalid_argument("unwritable AGC color target address");

    const auto info = regs[2].value;
    const auto attributes = info & ~(0x1800u | 0x700u | 0x7cu);
    if (regs[1].value || regs[3].value || regs[4].value != 0x48 || regs[9].value || regs[13].value ||
        (regs[15].value & ~0x7c000u) != 0x45000000 || (regs[14].value >> 28) ||
        (attributes != 0x8000 && attributes != 0x40000 && attributes != 0x50000))
        NotImplemented_nid_no_patch("sceFontSelectGraphicsAgc_texture_color_target");

    const auto tile = (regs[15].value >> 14) & 31;
    const auto format = info & 0x7c;
    const auto image = TextureFormat(format, (info >> 8) & 7);
    if (block & (AlignmentInBlocks(tile) - 1)) return 0;
    const auto swap = (info >> 11) & 3;
    const auto channels = format == 0x04 ? 0x849u | (swap << 9) : 0xb24u;
    const auto widthMinusOne = regs[14].value >> 14;
    const auto heightMinusOne = regs[14].value & 0x3fff;
    const std::array<std::uint32_t, 8> texture = {
        low, high | (image << 20) | ((widthMinusOne & 3) << 30),
        (widthMinusOne >> 2) | (heightMinusOne << 14), 0x90000000 | (tile << 20) | channels,
        0, 0x00700000, 0, 0
    };
    if (!GuestCopyOut_nid_no_patch(static_cast<std::uint8_t*>(object) + 0x40, texture.data(), sizeof(texture)))
        throw std::invalid_argument("unwritable AGC font texture");
    return sizeof(texture);
}

int APS5_VABI Initialize(void*, void*, std::uint64_t) {
    NotImplemented_nid_no_patch("sceFontSelectGraphicsAgc_backend_47000001");
}

void APS5_VABI Terminate(void*) {}

template<std::uint32_t Opcode>
void APS5_VABI Unported() {
    char name[48];
    std::snprintf(name, sizeof(name), "sceFontSelectGraphicsAgc_backend_%08x", Opcode);
    NotImplemented_nid_no_patch(name);
}

template<std::uint32_t Opcode>
FontGraphicsBackendEntry PendingEntry() {
    return {Opcode, 0, reinterpret_cast<std::uint64_t>(&Unported<Opcode>)};
}

struct Backend {
    FontGraphicsServiceParam header;
    FontGraphicsBackendEntry entries[21];
    FontGraphicsBackendList list;
};
static_assert(offsetof(Backend, entries) == 0x20 && offsetof(Backend, list) == 0x170);

void* instance = nullptr;
int APS5_VABI Enumerate(void* dispatch, FontGraphicsBackendSink sink);

const Backend backend = {
    {0x0e08, 5, 0x200, &instance, 0x20000, 0, Enumerate},
    {{0x47000001, 0, reinterpret_cast<std::uint64_t>(&Initialize)}, {0x47000002, 0, reinterpret_cast<std::uint64_t>(&Terminate)},
     PendingEntry<0x4700002d>(), PendingEntry<0x4700002e>(), PendingEntry<0x470000c0>(),
     PendingEntry<0x470000cb>(), PendingEntry<0x470000ce>(), PendingEntry<0x470000d1>(),
     PendingEntry<0x470000d2>(), PendingEntry<0x470000d4>(), PendingEntry<0x470000e0>(),
     PendingEntry<0x470000e1>(), PendingEntry<0x470000ec>(), PendingEntry<0x470000d0>(),
     PendingEntry<0x470000e2>(), PendingEntry<0x470000e3>(), PendingEntry<0x470000f0>(),
     {0x47000003, 0, 0x1000000040}, {0x47000004, 0, 0x8000000020}, PendingEntry<0x4700000b>(),
     {0x47000005, 0, reinterpret_cast<std::uint64_t>(&StructureTexture)}},
    {0, 21, backend.entries}
};

int APS5_VABI Enumerate(void* dispatch, FontGraphicsBackendSink sink) {
    if (!dispatch) return 0;
    if (!sink) throw std::invalid_argument("missing AGC font backend sink");
    sink(dispatch, &backend.list);
    return 0x400;
}

}

void* AgcFontBackend_nid_no_patch(std::uint32_t index) {
    return index == 0 ? const_cast<Backend*>(&backend) : nullptr;
}
