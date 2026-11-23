#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFontGs.native/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

int APS5_VABI Capture(void* dispatch, const FontGraphicsBackendList* list) {
    *static_cast<const FontGraphicsBackendList**>(dispatch) = list;
    return -1;
}

void CheckTexture(std::uint64_t function) {
    using Structure = int (APS5_VABI*)(void*, void*, std::uint64_t);
    const auto structure = reinterpret_cast<Structure>(function);
    Require(structure(nullptr, nullptr, 0) == 0);
    std::array<std::uint32_t, 32> target{};
    for (std::size_t i = 0; i < target.size(); i += 2) target[i] = 0x10000000;
    target[5] = 0x8028;
    target[9] = 0x48;
    target[29] = (1919u << 14) | 1079u;
    constexpr std::uint64_t base = 0x4a0123400000;
    for (const auto tile : {0u, 1u, 2u, 5u, 9u, 17u, 24u, 27u}) {
        target[31] = 0x45000000 | (tile << 14);
        std::array<std::uint32_t, 26> object;
        object.fill(0xe5e5e5e5);
        const auto before = target;
        Require(structure(object.data(), target.data(), base) == 32);
        const std::array<std::uint32_t, 8> expected = {
            0x01234000, 0xc380004a, 0x010dc1df, 0x90000b24 | (tile << 20), 0, 0x00700000, 0, 0
        };
        Require(std::equal(expected.begin(), expected.end(), object.begin() + 16));
        Require(object[15] == 0xe5e5e5e5 && object[24] == 0xe5e5e5e5);
        auto expectedTarget = before;
        expectedTarget[1] = 0x01234000;
        expectedTarget[21] = 0x4a;
        Require(target == expectedTarget);
        if (tile >= 5) {
            object.fill(0xe5e5e5e5);
            Require(structure(object.data(), target.data(), base + 0x100) == 0);
            Require(std::all_of(object.begin(), object.end(), [](auto word) { return word == 0xe5e5e5e5; }));
            Require(target[1] == 0x01234001);
        }
    }
    target[5] = 0x9004;
    target[31] = 0x45000000;
    std::array<std::uint32_t, 24> object{};
    Require(structure(object.data(), target.data(), 0) == 32);
    Require((object[19] & 0xfff) == 0xc49);
}

}

int main() {
    const auto* header = static_cast<const FontGraphicsServiceParam*>(sceFontSelectGraphicsAgc(0));
    Require(header && header == sceFontSelectGraphicsAgc(0));
    Require(sceFontSelectGraphicsAgc(1) == nullptr && sceFontSelectGraphicsAgc(0xffffffffu) == nullptr);
    Require(header->tag == 0xe08 && header->revision == 5 && header->level == 0x200);
    Require(header->instanceSlot && *header->instanceSlot == nullptr && header->instanceSize == 0x20000 && header->reserved14 == 0);
    const FontGraphicsBackendList* list = nullptr;
    Require(header->enumerate(nullptr, Capture) == 0);
    Require(header->enumerate(&list, Capture) == 0x400);
    constexpr std::array<unsigned, 21> opcodes = {
        0x01, 0x02, 0x2d, 0x2e, 0xc0, 0xcb, 0xce, 0xd1, 0xd2, 0xd4, 0xe0, 0xe1, 0xec, 0xd0, 0xe2, 0xe3, 0xf0, 0x03, 0x04, 0x0b, 0x05
    };
    Require(list && list->reserved == 0 && list->count == static_cast<std::int32_t>(opcodes.size()));
    for (std::size_t i = 0; i < opcodes.size(); ++i) {
        const auto& entry = list->entries[i];
        Require(entry.opcode == (0x47000000 | opcodes[i]) && entry.reserved == 0 && entry.value != 0);
    }
    Require(list->entries[17].value == 0x1000000040 && list->entries[18].value == 0x8000000020);
    std::array<std::uint32_t, 8> instanceState;
    instanceState.fill(0xabcdef01);
    const auto before = instanceState;
    reinterpret_cast<void (APS5_VABI*)(void*)>(list->entries[1].value)(instanceState.data());
    Require(instanceState == before);
    CheckTexture(list->entries[20].value);
}
