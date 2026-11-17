#include "prx/libSceAgc/Acb/include/Control.hpp"
#include "prx/libSceAgc/DcbState/include/Init.hpp"
#include "prx/libSceAgc/Misc/include/RegisterDefaults.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Storage {
    std::array<std::uint32_t, 256> words;
    CommandBuffer buffer;
    Storage() : buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0} { words.fill(0xa5a5a5a5u); }
};

std::vector<ShaderRegister> expectedRegisters(std::uint32_t version) {
    std::vector<ShaderRegister> registers;
    for (const auto* root : {sceAgcGetRegisterDefaults2(version), sceAgcGetRegisterDefaults2Internal(version)}) {
        ShaderRegister** table;
        std::uint32_t count;
        const auto* bytes = static_cast<const std::byte*>(root);
        std::memcpy(&table, bytes + 8, sizeof(table));
        std::memcpy(&count, bytes + 0x24, sizeof(count));
        for (std::uint32_t i = 0; i < count; ++i) if (table[0][i].offset >= 0x201u) registers.push_back(table[0][i]);
    }
    std::sort(registers.begin(), registers.end(), [](auto left, auto right) { return left.offset < right.offset; });
    return registers;
}

std::size_t verify(Storage& storage, std::uint32_t version) {
    const auto* start = storage.buffer.cursor_up;
    check(sceAgcAcbInitializeDefaultHardwareState(&storage.buffer) == start, "initializer did not return its first packet");
    const auto expected = expectedRegisters(version);
    std::size_t index = 0;
    for (const auto* packet = start; packet < storage.buffer.cursor_up;) {
        check(packet + 2 <= storage.buffer.cursor_up, "short compute-default header");
        check((packet[0] & 0xc000ffffu) == 0xc0007600u, "initializer emitted a non-compute-default packet");
        const auto count = ((packet[0] >> 16) & 0x3fffu) + 2u;
        check(count >= 3 && count <= static_cast<std::size_t>(storage.buffer.cursor_up - packet), "compute-default packet exceeded command bounds");
        for (std::uint32_t i = 2; i < count; ++i) {
            check(index < expected.size(), "initializer emitted an extra register");
            check(packet[1] + i - 2 == expected[index].offset && packet[i] == expected[index].value, "initializer changed a compute-default register");
            ++index;
        }
        packet += count;
    }
    check(index == expected.size() && index != 0, "initializer omitted compute defaults");
    check(*storage.buffer.cursor_up == 0xa5a5a5a5u, "initializer overwrote its tail guard");
    return storage.buffer.cursor_up - start;
}

struct Growth {
    Storage spare;
    std::uint32_t requested = 0;
    std::uint32_t calls = 0;
    std::uint32_t version = 0;
    bool success = true;
    bool capacity = true;
};

bool APS5_VABI grow(CommandBuffer* buffer, std::uint32_t requested, void* user) {
    auto& state = *static_cast<Growth*>(user);
    state.requested = requested;
    ++state.calls;
    if (state.version != 0) check(sceAgcInit(state.version) == 0, "callback context initialization failed");
    if (state.capacity) {
        buffer->bottom = state.spare.words.data();
        buffer->top = state.spare.words.data() + state.spare.words.size();
        buffer->cursor_up = buffer->bottom;
        buffer->cursor_down = buffer->top;
    }
    return state.success;
}

void uninitialized() {
    Storage storage;
    const auto before = storage.words;
    check(sceAgcAcbInitializeDefaultHardwareState(&storage.buffer) == nullptr, "uninitialized defaults did not return null");
    check(storage.buffer.cursor_up == storage.words.data() && storage.words == before, "uninitialized defaults touched the command buffer");
    check(sceAgcAcbInitializeDefaultHardwareState(nullptr) == nullptr, "uninitialized initializer inspected a null command buffer");
}

void initialized(std::uint32_t version) {
    check(sceAgcInit(version) == 0, "context initialization failed");
    Storage reference;
    const auto count = verify(reference, version);
    const std::vector<std::uint32_t> commands(reference.words.begin(), reference.words.begin() + count);
    Storage exact;
    exact.buffer.cursor_down = exact.words.data() + count + 7;
    exact.buffer.reserved_dw = 7;
    check(verify(exact, version) == count, "exact-capacity initializer changed size");
    for (auto [success, capacity] : {std::pair{false, false}, std::pair{false, true}, std::pair{true, false}, std::pair{true, true}}) {
        Storage source;
        source.buffer.cursor_down = source.words.data();
        source.buffer.reserved_dw = 7;
        Growth state;
        state.success = success;
        state.capacity = capacity;
        source.buffer.callback = grow;
        source.buffer.user_data = &state;
        const auto* result = sceAgcAcbInitializeDefaultHardwareState(&source.buffer);
        check(state.calls == 1 && state.requested == count + 7, "growth request ignored reserved space or repeated the callback");
        if (success && capacity) {
            check(result == state.spare.words.data() && source.buffer.cursor_up == result + count, "growth did not use replacement storage");
            check(std::equal(commands.begin(), commands.end(), result) && result[count] == 0xa5a5a5a5u, "growth changed commands or their guard");
        } else {
            check(result == nullptr, "failed growth did not return null");
            check(source.buffer.cursor_up == (capacity ? state.spare.words.data() : source.words.data()), "failed growth advanced the cursor");
            check(std::ranges::all_of(source.words, [](auto value) { return value == 0xa5a5a5a5u; }) && std::ranges::all_of(state.spare.words, [](auto value) { return value == 0xa5a5a5a5u; }), "failed growth wrote a packet");
        }
    }
    Storage exhausted;
    exhausted.buffer.cursor_down = exhausted.words.data();
    check(sceAgcAcbInitializeDefaultHardwareState(&exhausted.buffer) == nullptr && exhausted.buffer.cursor_up == exhausted.words.data(), "missing callback did not leave an exhausted buffer unchanged");
    Growth overflow;
    exhausted.buffer.callback = grow;
    exhausted.buffer.user_data = &overflow;
    exhausted.buffer.reserved_dw = std::numeric_limits<std::uint32_t>::max();
    check(sceAgcAcbInitializeDefaultHardwareState(&exhausted.buffer) == nullptr && overflow.calls == 0, "overflowing growth request reached the callback");
    Storage changed;
    changed.buffer.cursor_down = changed.words.data();
    Growth switched;
    switched.version = version == 8 ? 13 : 8;
    changed.buffer.callback = grow;
    changed.buffer.user_data = &switched;
    const auto* result = sceAgcAcbInitializeDefaultHardwareState(&changed.buffer);
    check(result == switched.spare.words.data() && std::equal(commands.begin(), commands.end(), result), "callback context switch changed the captured defaults");
    Storage after;
    verify(after, switched.version);
}

}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected uninitialized or a context version");
        if (std::string(argv[1]) == "uninitialized") uninitialized();
        else initialized(static_cast<std::uint32_t>(std::stoul(argv[1])));
        std::cout << "ACB hardware defaults preserve context, packets, capacity and callback results\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
