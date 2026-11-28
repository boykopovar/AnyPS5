#include "prx/libSceAgc/DcbState/include/ContextState.hpp"
#include "prx/libSceAgc/DcbState/include/Init.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Storage {
    std::array<std::uint32_t, 64> words;
    CommandBuffer buffer;
    Storage() : buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0} { words.fill(0xa5a5a5a5u); }
};

struct Growth {
    std::array<Storage, 3> replacements;
    std::array<std::uint32_t, 3> requested{};
    std::uint32_t calls = 0;
    std::uint32_t switchAt = 0;
    std::uint32_t switchVersion = 0;
    bool success = true;
    bool capacity = true;
};

bool APS5_VABI grow(CommandBuffer* buffer, std::uint32_t required, void* user) {
    auto& growth = *static_cast<Growth*>(user);
    check(growth.calls < growth.replacements.size(), "unexpected growth callback");
    auto& storage = growth.replacements[growth.calls];
    growth.requested[growth.calls++] = required;
    if (growth.calls == growth.switchAt) check(sceAgcInit(growth.switchVersion) == 0, "callback initialization failed");
    if (!growth.success) return false;
    buffer->bottom = storage.words.data();
    buffer->top = storage.words.data() + storage.words.size();
    buffer->cursor_up = buffer->bottom;
    buffer->cursor_down = buffer->bottom + required - (growth.capacity ? 0 : 1);
    return true;
}

std::array<std::uint32_t, 3> descriptor() {
    Storage storage;
    auto* packet = sceAgcDcbContextStateOp_0100(&storage.buffer, 0);
    check(packet != nullptr && packet[0] == 0xc0039f00u, "could not obtain default descriptor");
    return {packet[1], packet[2], packet[4]};
}

void checkPrefix(const std::uint32_t* words) {
    const std::array<std::uint32_t, 5> expected{0xc0012800u, 0, 0x80018001u, 0xc0001200u, 1};
    check(std::equal(expected.begin(), expected.end(), words), "default push prefix differs from the bundled function");
}

void run(std::uint32_t version) {
    check(sceAgcInit(version) == 0, "initialization failed");
    const auto initial = descriptor();
    Storage complete;
    check(sceAgcDcbContextStateOp(&complete.buffer, 3) == complete.words.data(), "default push returned the wrong first packet");
    checkPrefix(complete.words.data());
    const auto* load = complete.words.data() + 5;
    check(load[0] == 0xc0039f00u && load[3] == 0x80000000u, "default push did not load indexed context defaults");
    check(std::array{load[1], load[2], load[4]} == initial, "default push loaded the wrong descriptor");
    check(complete.buffer.cursor_up == load + 5 && load[5] == 0xa5a5a5a5u, "default push changed its length or tail guard");
    check(sceAgcDcbContextStateOpGetSize(3) == 128, "legacy size query changed");
    AgcDriver::QueueState queue;
    queue.context[0x999] = 0x11223344u;
    const auto original = queue.context;
    for (const auto packet : {std::span<const std::uint32_t>{complete.words.data(), 3}, std::span<const std::uint32_t>{complete.words.data() + 3, 2}, std::span<const std::uint32_t>{load, 5}}) {
        AgcDriver::Pm4::Validate(packet, 0);
        AgcDriver::Pm4::Execute(packet, queue);
    }
    const auto registers = AgcDriver::Pm4::ReadIndirectRegisters({load, 5});
    for (std::size_t i = 0; i < registers.size(); i += 2) check(queue.context.at(registers[i]) == registers[i + 1], "driver did not apply initialized defaults");
    check(queue.context.at(0x999) == 0x11223344u, "indexed defaults cleared an unrelated register");
    queue.context[0x999] = 0xdeadbeefu;
    const std::array<std::uint32_t, 2> pop{0xc0001200u, 2};
    AgcDriver::Pm4::Execute(pop, queue);
    check(queue.context == original, "default push did not preserve the prior context for pop");
    for (std::uint32_t capacity = 0; capacity <= 10; ++capacity) {
        Storage storage;
        storage.buffer.reserved_dw = 2;
        storage.buffer.cursor_down = storage.words.data() + capacity + 2;
        const auto* result = sceAgcDcbContextStateOp(&storage.buffer, 3);
        const auto written = capacity < 3 ? 0u : capacity < 5 ? 3u : capacity < 10 ? 5u : 10u;
        check(storage.buffer.cursor_up == storage.words.data() + written, "failed allocation rolled back or advanced prior packets");
        check((result != nullptr) == (capacity == 10), "incomplete default push returned success");
        check(std::all_of(storage.words.begin() + written, storage.words.end(), [](auto word) { return word == 0xa5a5a5a5u; }), "failed allocation modified its unwritten tail");
        for (const bool success : {false, true}) {
            for (const bool sufficient : {false, true}) {
                Storage grown;
                grown.buffer.cursor_down = grown.words.data() + capacity + 2;
                grown.buffer.reserved_dw = 2;
                Growth growth;
                growth.success = success;
                growth.capacity = sufficient;
                grown.buffer.callback = grow;
                grown.buffer.user_data = &growth;
                const auto* packet = sceAgcDcbContextStateOp(&grown.buffer, 3);
                check((packet != nullptr) == (capacity == 10 || (success && sufficient)), "growth return contract differs from the bundled function");
                if (capacity == 10) check(growth.calls == 0, "exact capacity unnecessarily requested growth");
                else check(growth.requested[0] == (capacity < 3 ? 5u : capacity < 5 ? 4u : 7u), "growth ignored the packet boundary or reserved footer");
                if (packet != nullptr) check(packet == (capacity < 3 ? growth.replacements[0].words.data() : grown.words.data()), "growth returned a later segment instead of the first packet");
            }
        }
    }
    for (const auto reserved : {0xfffffffdu, 0xfffffffeu, 0xffffffffu}) {
        Storage storage;
        Growth growth;
        storage.buffer.reserved_dw = reserved;
        storage.buffer.callback = grow;
        storage.buffer.user_data = &growth;
        check(sceAgcDcbContextStateOp(&storage.buffer, 3) == nullptr && growth.calls == 0, "reservation overflow reached its callback");
    }
    const auto other = version == 13 ? 0u : 13u;
    for (const auto switchAt : {1u, 3u}) {
        check(sceAgcInit(version) == 0, "could not restore initial version");
        Storage storage;
        storage.buffer.cursor_down = storage.words.data();
        Growth growth;
        growth.switchAt = switchAt;
        growth.switchVersion = other;
        storage.buffer.callback = grow;
        storage.buffer.user_data = &growth;
        check(sceAgcDcbContextStateOp(&storage.buffer, 3) == growth.replacements[0].words.data(), "segmented push returned the wrong first packet");
        check(growth.calls == 3 && growth.requested == std::array<std::uint32_t, 3>{3, 2, 5}, "push allocation boundaries changed");
        const auto* retained = growth.replacements[2].words.data();
        const auto current = descriptor();
        check(std::array{retained[1], retained[2], retained[4]} == (switchAt == 1 ? current : initial), "push captured its descriptor on the wrong side of growth");
        check(current != initial, "callback did not change initialized descriptor");
    }
}

void uninitialized() {
    Storage storage;
    bool rejected = false;
    try { sceAgcDcbContextStateOp(&storage.buffer, 3); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "uninitialized push accepted unavailable defaults");
    checkPrefix(storage.words.data());
    check(storage.buffer.cursor_up == storage.words.data() + 5 && storage.words[5] == 0xa5a5a5a5u, "uninitialized push changed earlier packets");
}

}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected uninitialized or defaults version");
        if (std::string(argv[1]) == "uninitialized") uninitialized();
        else run(static_cast<std::uint32_t>(std::stoul(argv[1])));
        LibcRunShutdown_nid_postfix();
        std::puts("context default push passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
