#include "prx/libSceAgc/include/Agc.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

const Shader* registered = nullptr;
unsigned registrations = 0;
int soc = 0;
using Pattern = std::array<std::uint32_t, 4>;
constexpr Pattern First{0x7fc01234u, 0x80000000u, 0xabcdef01u, 0xffffffffu};
constexpr Pattern Second{11u, 22u, 33u, 44u};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void inspect(std::span<const std::uint32_t> words, std::uint64_t address, std::uint64_t size, const Pattern& pattern, std::uint32_t mask) {
    std::map<std::uint32_t, std::uint32_t> registers;
    unsigned dispatches = 0;
    for (std::size_t i = 0; i < words.size();) {
        const auto count = ((words[i] >> 16u) & 0x3fffu) + 2u;
        check((words[i] >> 30u) == 3u && count <= words.size() - i, "malformed command packet");
        const auto opcode = (words[i] >> 8u) & 0xffu;
        if (opcode == 0x76u) {
            for (std::size_t j = 2; j < count; ++j) registers[words[i + 1] + static_cast<std::uint32_t>(j - 2)] = words[i + j];
        } else {
            check(opcode == 0x15u && count == 5u, "unexpected packet");
            check(words[i + 1] == 16u && words[i + 2] == 1u && words[i + 3] == 1u && words[i + 4] == 0x41u, "wrong dispatch shape or initiator");
            check(registers[0x207] == 256u && registers[0x208] == 1u && registers[0x209] == 1u, "wrong threadgroup shape");
            check(registers[0x216] == mask && registers[0x217] == mask, "wrong exclusive masks");
            check(registers[0x240] == static_cast<std::uint32_t>(address), "wrong destination low word");
            check(registers[0x241] == (((address >> 32u) & 0xffffu) | 0x100000u), "wrong destination high word or stride");
            check(registers[0x242] == static_cast<std::uint32_t>(size >> 4u) && registers[0x243] == 0x4bfacu, "wrong record count or descriptor format");
            for (unsigned j = 0; j < 4; ++j) check(registers[0x244 + j] == pattern[j], "pattern was not snapshotted at the required time");
            check(registers[0x248] == static_cast<std::uint32_t>((size + 0xffffu) >> 16u), "wrong wrapping chunk count");
            ++dispatches;
        }
        i += count;
    }
    check(dispatches == 1 && registers[0x216] == 0xffffffffu && registers[0x217] == 0xffffffffu, "dispatch missing or thread masks not restored");
}

void boundaries() {
    constexpr std::array<std::uint64_t, 13> sizes{0, 1, 15, 16, 17, 65535, 65536, 65537, 196624, 1ull << 36u, 1ull << 48u, 0xffffffffffff0000ull, std::numeric_limits<std::uint64_t>::max()};
    for (const int id : {0, 0x840fc0, 0x840fdf, 0x840fe0}) {
        soc = id;
        for (auto size : sizes) {
            std::array<std::uint32_t, 256> storage;
            storage.fill(0xfeedfaceu);
            CommandBuffer buffer{storage.data(), storage.data() + storage.size(), storage.data(), storage.data() + storage.size(), nullptr, nullptr, 3};
            const std::uint64_t address = 0x9876543210abcdefull;
            std::array<std::byte, 17> unaligned{};
            std::memcpy(unaligned.data() + 1, First.data(), 16);
            check(sceAgcCbMemsetExclusive(&buffer, reinterpret_cast<void*>(address), unaligned.data() + 1, size) == storage.data(), "wrong returned packet pointer");
            inspect({storage.data(), static_cast<std::size_t>(buffer.cursor_up - storage.data())}, address, size, First, (id & ~0x1f) == 0x840fc0 ? 0xff00ffu : 0x3fcffu);
            for (auto* tail = buffer.cursor_up; tail != storage.data() + storage.size(); ++tail) check(*tail == 0xfeedfaceu, "command recording wrote past its cursor");
        }
    }
    Pattern destination = First;
    std::array<std::uint32_t, 256> storage{};
    CommandBuffer buffer{storage.data(), storage.data() + storage.size(), storage.data(), storage.data() + storage.size(), nullptr, nullptr, 0};
    sceAgcCbMemsetExclusive(&buffer, destination.data(), destination.data(), 16);
    check(destination == First, "command recording changed the destination");
    check(registrations == 1 && registered != nullptr && registered->user_data->srt_size_dw == 9 && registered->shader_size != 0, "fill shader was not registered exactly once");
}

struct Segments {
    std::deque<std::vector<std::uint32_t>> packets;
    Pattern pattern{};
    bool userData = false;
};

bool APS5_VABI allocate(CommandBuffer* buffer, std::uint32_t count, void* opaque) {
    auto& state = *static_cast<Segments*>(opaque);
    check(count > buffer->reserved_dw, "callback request omitted packet space");
    if (state.packets.empty()) state.pattern = First;
    if (count - buffer->reserved_dw == 11u) {
        state.pattern = Second;
        state.userData = true;
    }
    state.packets.emplace_back(count, 0xfeedfaceu);
    auto& packet = state.packets.back();
    buffer->bottom = buffer->cursor_up = packet.data();
    buffer->top = buffer->cursor_down = packet.data() + packet.size();
    return true;
}

void callbacks() {
    soc = 0;
    Segments state;
    CommandBuffer buffer{nullptr, nullptr, nullptr, nullptr, allocate, &state, 3};
    const auto address = reinterpret_cast<std::uint64_t>(state.pattern.data());
    auto* first = sceAgcCbMemsetExclusive(&buffer, state.pattern.data(), state.pattern.data(), 65537);
    check(first == state.packets.front().data() && state.userData && state.pattern == Second, "allocation callbacks were not exercised");
    std::vector<std::uint32_t> words;
    for (const auto& packet : state.packets) {
        words.insert(words.end(), packet.begin(), packet.end() - 3);
        for (auto i = packet.size() - 3; i < packet.size(); ++i) check(packet[i] == 0xfeedfaceu, "reserved callback words overwritten");
    }
    inspect(words, address, 65537, First, 0x3fcffu);
}

} // namespace

extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader) {
    registered = shader;
    ++registrations;
}

extern "C" int APS5_VABI sceKernelGetMainSocId() {
    return soc;
}

int main() {
    try {
        boundaries();
        callbacks();
        std::puts("AGC memset command boundary and callback tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
