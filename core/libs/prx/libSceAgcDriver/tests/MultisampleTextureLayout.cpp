#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace AgcDriver::Graphics;

namespace {

struct ReferenceCase {
    TextureTileMode tileMode;
    std::uint32_t elementBytes;
    std::uint32_t fragments;
    std::uint32_t blockWidth;
    std::uint32_t blockHeight;
    std::uint32_t pitch;
    std::uint32_t paddedHeight;
    std::uint64_t sliceBytes;
    std::array<std::uint64_t, 7> addresses;
};

constexpr std::array<std::array<std::uint32_t, 3>, 7> kCoordinates{{
    {0u, 0u, 0u}, {128u, 66u, 16u}, {64u, 32u, 1u}, {32u, 64u, 2u},
    {63u, 31u, 7u}, {127u, 65u, 15u}, {1u, 1u, 3u}
}};

constexpr ReferenceCase kReferences[] = {
    {TextureTileMode::kR64KBX, 1u, 2u, 128u, 256u, 256u, 256u, 131072ull, {{0ull, 2201608ull, 141312ull, 300032ull, 919295ull, 2013271ull, 396305ull}}},
    {TextureTileMode::kR64KBX, 1u, 4u, 128u, 128u, 256u, 128u, 131072ull, {{0ull, 2185224ull, 174080ull, 316416ull, 919295ull, 1996887ull, 429073ull}}},
    {TextureTileMode::kR64KBX, 1u, 8u, 64u, 128u, 192u, 128u, 196608ull, {{0ull, 3291144ull, 279552ull, 422400ull, 1413119ull, 3061847ull, 640529ull}}},
    {TextureTileMode::kR64KBX, 2u, 2u, 128u, 128u, 256u, 128u, 131072ull, {{0ull, 2213920ull, 141312ull, 312320ull, 923390ull, 2025630ull, 396306ull}}},
    {TextureTileMode::kR64KBX, 2u, 4u, 128u, 64u, 256u, 128u, 262144ull, {{0ull, 4392992ull, 305152ull, 689152ull, 1840894ull, 4073630ull, 822290ull}}},
    {TextureTileMode::kR64KBX, 2u, 8u, 64u, 64u, 192u, 128u, 393216ull, {{0ull, 6621216ull, 476160ull, 999936ull, 2793470ull, 6195358ull, 1230354ull}}},
    {TextureTileMode::kR64KBX, 4u, 2u, 64u, 128u, 192u, 128u, 196608ull, {{0ull, 3328032ull, 264192ull, 443392ull, 1390332ull, 3074204ull, 592916ull}}},
    {TextureTileMode::kR64KBX, 4u, 4u, 64u, 64u, 192u, 128u, 393216ull, {{0ull, 6621216ull, 493568ull, 1016832ull, 2766588ull, 6170780ull, 1215508ull}}},
    {TextureTileMode::kR64KBX, 4u, 8u, 32u, 64u, 160u, 128u, 655360ull, {{0ull, 11077664ull, 820224ull, 1737216ull, 4667388ull, 10365084ull, 2002452ull}}},
    {TextureTileMode::kR64KBX, 8u, 2u, 64u, 64u, 192u, 128u, 393216ull, {{0ull, 6621312ull, 460800ull, 984064ull, 2782968ull, 6170744ull, 1182744ull}}},
    {TextureTileMode::kR64KBX, 8u, 4u, 64u, 32u, 192u, 96u, 589824ull, {{0ull, 9963648ull, 855040ull, 1574400ull, 4158456ull, 9317496ull, 1773080ull}}},
    {TextureTileMode::kR64KBX, 8u, 8u, 32u, 32u, 160u, 96u, 983040ull, {{0ull, 16648320ull, 1444352ull, 2689280ull, 6979576ull, 15605880ull, 2952984ull}}},
    {TextureTileMode::kR64KBX, 16u, 2u, 32u, 64u, 160u, 128u, 655360ull, {{0ull, 11075712ull, 787456ull, 1706496ull, 4718576ull, 10397808ull, 1967664ull}}},
    {TextureTileMode::kR64KBX, 16u, 4u, 32u, 32u, 160u, 96u, 983040ull, {{0ull, 16646272ull, 1443328ull, 2690304ull, 7011312ull, 15641712ull, 2950960ull}}},
    {TextureTileMode::kR64KBX, 16u, 8u, 16u, 32u, 144u, 96u, 1769472ull, {{0ull, 30017664ull, 2624000ull, 4851968ull, 12615664ull, 28201072ull, 5312304ull}}},
    {TextureTileMode::kZ64KBX, 1u, 2u, 128u, 256u, 256u, 256u, 131072ull, {{0ull, 2172945ull, 149504ull, 271361ull, 923390ull, 1992879ull, 396294ull}}},
    {TextureTileMode::kZ64KBX, 1u, 4u, 128u, 128u, 256u, 128u, 131072ull, {{0ull, 2181153ull, 165890ull, 279555ull, 931580ull, 2025565ull, 396302ull}}},
    {TextureTileMode::kZ64KBX, 1u, 8u, 64u, 128u, 192u, 128u, 196608ull, {{0ull, 3295297ull, 263170ull, 410115ull, 1425404ull, 3041469ull, 591390ull}}},
    {TextureTileMode::kZ64KBX, 2u, 2u, 128u, 128u, 256u, 128u, 131072ull, {{0ull, 2181154ull, 165888ull, 279554ull, 931580ull, 2025566ull, 396300ull}}},
    {TextureTileMode::kZ64KBX, 2u, 4u, 128u, 64u, 256u, 128u, 262144ull, {{0ull, 4425794ull, 280580ull, 689158ull, 1881848ull, 4118714ull, 789532ull}}},
    {TextureTileMode::kZ64KBX, 2u, 8u, 64u, 64u, 192u, 128u, 393216ull, {{0ull, 6637698ull, 459780ull, 999942ull, 2818040ull, 6219898ull, 1181244ull}}},
    {TextureTileMode::kZ64KBX, 4u, 2u, 64u, 128u, 192u, 128u, 196608ull, {{0ull, 3295300ull, 264192ull, 410628ull, 1423096ull, 3041468ull, 592920ull}}},
    {TextureTileMode::kZ64KBX, 4u, 4u, 64u, 64u, 192u, 128u, 393216ull, {{0ull, 6637700ull, 460808ull, 1000460ull, 2815728ull, 6219892ull, 1182776ull}}},
    {TextureTileMode::kZ64KBX, 4u, 8u, 32u, 64u, 160u, 128u, 655360ull, {{0ull, 11126788ull, 787464ull, 1720844ull, 4716528ull, 10359028ull, 1967736ull}}},
};

template<typename TFunction>
void requireRefusal(TFunction action) {
    bool refused = false;
    try { action(); } catch (const std::exception&) { refused = true; }
    Require(refused, "multisampled layout accepted an unsupported operation");
}

void checkReference(const ReferenceCase& reference) {
    MultisampleTextureLayout layout(reference.tileMode, reference.elementBytes, reference.fragments, 129u, 67u, 17u);
    Require(layout.BlockWidth() == reference.blockWidth && layout.BlockHeight() == reference.blockHeight, "multisampled block extent differs from AMD reference");
    Require(layout.Pitch() == reference.pitch && layout.PaddedHeight() == reference.paddedHeight, "multisampled padded extent differs from AMD reference");
    Require(layout.SliceBytes() == reference.sliceBytes && layout.Bytes() == reference.sliceBytes * 17u, "multisampled allocation differs from AMD reference");
    Require(layout.LinearBytes() == 129ull * 67u * 17u * reference.fragments * reference.elementBytes, "multisampled linear region omits fragments or layers");
    for (std::uint32_t n = 0; n < kCoordinates.size(); ++n) {
        const auto& coordinate = kCoordinates[n];
        const auto address = layout.Offset(coordinate[0], coordinate[1], coordinate[2], n % reference.fragments);
        Require(address == reference.addresses[n], "multisampled fragment address differs from AMD reference");
        for (auto xorBytes : {0x100u, 0x800u, 0x4000u, 0x8000u, 0xcf00u}) {
            MultisampleTextureLayout swizzled(reference.tileMode, reference.elementBytes, reference.fragments, 129u, 67u, 17u, xorBytes);
            const auto expected = (address & ~65535ull) | ((address & 65535ull) ^ xorBytes);
            Require(swizzled.Offset(coordinate[0], coordinate[1], coordinate[2], n % reference.fragments) == expected, "multisampled pipe/bank XOR moves a fragment outside its tile");
        }
    }
}

void checkTransfer(const ReferenceCase& reference) {
    MultisampleTextureLayout layout(reference.tileMode, reference.elementBytes, reference.fragments, 129u, 67u, 3u, 0x8100u);
    std::vector<std::byte> source(layout.LinearBytes() + 32u, std::byte{0xa5});
    auto linear = std::span(source).subspan(16u, layout.LinearBytes());
    for (std::size_t n = 0; n < linear.size(); ++n) linear[n] = static_cast<std::byte>((n * 13u + n / 251u) & 255u);
    const auto savedSource = source;
    std::vector<std::byte> tiled(layout.Bytes() + 32u, std::byte{0xa5});
    auto guest = std::span(tiled).subspan(16u, layout.Bytes());
    layout.Tile(linear, guest);
    Require(source == savedSource, "multisampled retile changed its source");
    std::vector<bool> covered(guest.size());
    std::size_t position = 0;
    for (std::uint32_t layer = 0; layer < 3u; ++layer) {
        for (std::uint32_t y = 0; y < 67u; ++y) {
            for (std::uint32_t x = 0; x < 129u; ++x) {
                for (std::uint32_t fragment = 0; fragment < reference.fragments; ++fragment) {
                    const auto address = layout.Offset(x, y, layer, fragment);
                    for (std::uint32_t channel = 0; channel < reference.elementBytes; ++channel) {
                        Require(address + channel < guest.size() && !covered[address + channel], "multisampled element addresses overlap or exceed the allocation");
                        covered[address + channel] = true;
                        Require(guest[address + channel] == linear[position++], "multisampled retile changed element bytes");
                    }
                }
            }
        }
    }
    for (std::size_t n = 0; n < guest.size(); ++n) Require(covered[n] || guest[n] == std::byte{0xa5}, "multisampled retile changed tile padding");
    Require(std::all_of(tiled.begin(), tiled.begin() + 16u, [](auto value) { return value == std::byte{0xa5}; }) && std::all_of(tiled.end() - 16u, tiled.end(), [](auto value) { return value == std::byte{0xa5}; }), "multisampled retile changed guard bytes");
    const auto savedTiled = tiled;
    std::vector<std::byte> restored(source.size(), std::byte{0xa5});
    layout.Detile(guest, std::span(restored).subspan(16u, layout.LinearBytes()));
    Require(restored == source && tiled == savedTiled, "multisampled detile changed element, source or guard bytes");
}

void checkInverse(const ReferenceCase& reference) {
    const auto byteBits = static_cast<std::uint32_t>(std::countr_zero(reference.elementBytes));
    const auto fragmentBits = static_cast<std::uint32_t>(std::countr_zero(reference.fragments));
    const auto widthBits = static_cast<std::uint32_t>(std::countr_zero(reference.blockWidth));
    for (const auto xorBytes : {0u, 0x4000u, 0x8000u, 0xcf00u}) {
        const MultisampleTextureLayout layout(reference.tileMode, reference.elementBytes, reference.fragments, reference.blockWidth * 4u, reference.blockHeight * 4u, 8u, xorBytes);
        for (const auto origin : {std::array{0u, 0u, 0u}, std::array{reference.blockWidth * 2u, reference.blockHeight * 3u, 7u}}) {
            const auto base = static_cast<std::uint32_t>(layout.Offset(origin[0], origin[1], origin[2], 0u) & 65535u);
            const auto step = xorBytes == 0u && origin[2] == 0u ? 1u : 257u;
            for (std::uint32_t packed = 0u; packed < 65536u; packed += step) {
                const auto byte = packed & (reference.elementBytes - 1u);
                const auto fragment = (packed >> byteBits) & (reference.fragments - 1u);
                const auto x = (packed >> (byteBits + fragmentBits)) & (reference.blockWidth - 1u);
                const auto y = packed >> (byteBits + fragmentBits + widthBits);
                const auto physical = static_cast<std::uint32_t>((layout.Offset(origin[0] + x, origin[1] + y, origin[2], fragment) + byte) & 65535u) ^ base;
                std::uint32_t restored = 0u;
                for (std::uint32_t bit = 0; bit < 16u; ++bit) restored |= static_cast<std::uint32_t>(std::popcount(physical & layout.InverseBlockEquation()[bit]) & 1u) << bit;
                Require(restored == packed, "multisampled inverse equation changed a byte, fragment or pixel coordinate");
            }
        }
    }
}

void checkRefusals() {
    for (const auto& extent : {std::array{0u, 3u, 1u}, std::array{5u, 0u, 1u}, std::array{16385u, 3u, 1u}, std::array{5u, 16385u, 1u}, std::array{5u, 3u, 0u}, std::array{5u, 3u, 8193u}}) {
        requireRefusal([&] { MultisampleTextureLayout invalid(TextureTileMode::kR64KBX, 8u, 8u, extent[0], extent[1], extent[2]); });
    }
    for (auto elementBytes : {0u, 3u, 32u}) requireRefusal([&] { MultisampleTextureLayout invalid(TextureTileMode::kR64KBX, elementBytes, 8u, 5u, 3u, 1u); });
    for (auto fragments : {0u, 1u, 3u, 16u}) requireRefusal([&] { MultisampleTextureLayout invalid(TextureTileMode::kR64KBX, 8u, fragments, 5u, 3u, 1u); });
    for (auto tileMode : {TextureTileMode::kLinear, TextureTileMode::kStandard4KB, TextureTileMode::kStandard64KB, TextureTileMode::kS64KBX, TextureTileMode::kD64KBX}) requireRefusal([&] { MultisampleTextureLayout invalid(tileMode, 8u, 8u, 5u, 3u, 1u); });
    for (auto elementBytes : {8u, 16u}) requireRefusal([&] { MultisampleTextureLayout invalid(TextureTileMode::kZ64KBX, elementBytes, 8u, 5u, 3u, 1u); });
    for (auto xorBytes : {1u, 255u, 0x1000u, 0x2000u, 0x10000u}) requireRefusal([&] { MultisampleTextureLayout invalid(TextureTileMode::kR64KBX, 8u, 8u, 5u, 3u, 1u, xorBytes); });

    MultisampleTextureLayout layout(TextureTileMode::kR64KBX, 8u, 8u, 5u, 3u, 2u);
    for (const auto& coordinate : {std::array{5u, 0u, 0u, 0u}, std::array{0u, 3u, 0u, 0u}, std::array{0u, 0u, 2u, 0u}, std::array{0u, 0u, 0u, 8u}}) requireRefusal([&] { layout.Offset(coordinate[0], coordinate[1], coordinate[2], coordinate[3]); });
    std::vector<std::byte> source(layout.Bytes(), std::byte{0xa5}), destination(layout.Bytes(), std::byte{0x5a});
    const auto savedSource = source, savedDestination = destination;
    requireRefusal([&] { layout.Detile(std::span(source).first(layout.Bytes() - 1u), destination); });
    requireRefusal([&] { layout.Detile(source, std::span(destination).first(layout.LinearBytes() - 1u)); });
    requireRefusal([&] { layout.Tile(std::span(source).first(layout.LinearBytes() - 1u), destination); });
    requireRefusal([&] { layout.Tile(source, std::span(destination).first(layout.Bytes() - 1u)); });
    requireRefusal([&] { layout.Detile(source, source); });
    requireRefusal([&] { layout.Tile(source, source); });
    requireRefusal([&] { layout.Detile(source, std::span(source).subspan(8u)); });
    Require(source == savedSource && destination == savedDestination, "refused multisampled transfer changed a buffer");
}

}

int main() {
    try {
        for (const auto& reference : kReferences) {
            checkReference(reference);
            checkTransfer(reference);
            checkInverse(reference);
        }
        checkRefusals();
        std::cout << "Multisampled texture layout reference, fragment, layer, padding and refusal tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
