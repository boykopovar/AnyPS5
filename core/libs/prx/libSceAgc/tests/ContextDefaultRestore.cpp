#include "prx/libSceAgc/DcbState/include/ContextState.hpp"
#include "prx/libSceAgc/DcbState/include/Init.hpp"
#include "prx/libSceAgc/Misc/include/RegisterDefaults.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

namespace {

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Storage {
    std::array<std::uint32_t, 32> words;
    CommandBuffer buffer;
    Storage() : buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0} { words.fill(0xa5a5a5a5u); }
};

std::map<std::uint32_t, std::uint32_t> expected(std::uint32_t version) {
    std::map<std::uint32_t, std::uint32_t> result;
    for (const auto* root : {sceAgcGetRegisterDefaults2(version), sceAgcGetRegisterDefaults2Internal(version)}) {
        ShaderRegister** table = nullptr;
        std::uint32_t count = 0;
        std::memcpy(&table, root, sizeof(table));
        std::memcpy(&count, static_cast<const std::byte*>(root) + 0x20, sizeof(count));
        for (std::uint32_t i = 0; i < count; ++i) result[table[0][i].offset] = table[0][i].value;
    }
    return result;
}

void verify(const std::uint32_t* packet, std::uint32_t version) {
    check(packet != nullptr && packet[0] == 0xc0039f00u, "operation zero did not emit LOAD_CONTEXT_REG_INDEX");
    check((packet[1] & 3u) == 0 && packet[3] == 0x80000000u, "context restore changed the indexed-load control");
    const auto values = expected(version);
    check(packet[4] == values.size(), "context restore selected the wrong register count");
    const auto pairs = AgcDriver::Pm4::ReadIndirectRegisters({packet, 5});
    check(pairs.size() == values.size() * 2, "context restore has an incomplete register list");
    std::size_t i = 0;
    for (const auto& [offset, value] : values) {
        check(pairs[i++] == offset && pairs[i++] == value, "context restore changed a register or its ordering");
    }
    AgcDriver::QueueState queue;
    queue.context[0x999] = 0x11223344u;
    AgcDriver::Pm4::Validate({packet, 5}, 0);
    AgcDriver::Pm4::Execute({packet, 5}, queue);
    check(queue.context.at(0x999) == 0x11223344u, "context restore cleared unrelated context registers");
    for (const auto& [offset, value] : values) check(queue.context.at(offset) == value, "driver did not restore the requested context value");
}

struct Growth {
    Storage spare;
    std::uint32_t calls = 0;
    std::uint32_t requested = 0;
    std::uint32_t switchVersion = std::numeric_limits<std::uint32_t>::max();
    bool success = true;
    bool capacity = true;
};

bool APS5_VABI grow(CommandBuffer* buffer, std::uint32_t words, void* user) {
    auto& growth = *static_cast<Growth*>(user);
    ++growth.calls;
    growth.requested = words;
    if (growth.switchVersion < 14) check(sceAgcInit(growth.switchVersion) == 0, "callback could not initialize the other context");
    if (growth.capacity) {
        buffer->bottom = growth.spare.words.data();
        buffer->top = growth.spare.words.data() + growth.spare.words.size();
        buffer->cursor_up = buffer->bottom;
        buffer->cursor_down = buffer->top;
    }
    return growth.success;
}

void run(std::uint32_t version) {
    check(sceAgcInit(version) == 0, "context initialization failed");
    Storage initial;
    const auto* captured = sceAgcDcbContextStateOp_0100(&initial.buffer, 0);
    verify(captured, version);
    const std::array descriptor{captured[1], captured[2], captured[4]};
    for (const auto function : {sceAgcDcbContextStateOp, sceAgcDcbContextStateOp_0100}) {
        for (std::uint32_t capacity = 0; capacity <= 5; ++capacity) {
            Storage source;
            source.buffer.cursor_down = source.words.data() + capacity + 2;
            source.buffer.reserved_dw = 2;
            Growth growth;
            source.buffer.callback = grow;
            source.buffer.user_data = &growth;
            const auto* packet = function(&source.buffer, 0);
            check(growth.calls == (capacity < 5 ? 1u : 0u), "restore capacity boundary changed callback count");
            if (capacity < 5) check(growth.requested == 7 && packet == growth.spare.words.data(), "restore boundary growth changed size or storage");
            else check(packet == source.words.data(), "exact-capacity restore requested growth");
            verify(packet, version);
            check(source.buffer.cursor_up == packet + 5 && packet[5] == 0xa5a5a5a5u, "restore capacity boundary changed its cursor or guard");
        }
        Storage exact;
        exact.buffer.reserved_dw = 3;
        exact.buffer.cursor_down = exact.words.data() + 8;
        check(function(&exact.buffer, 0) == exact.words.data(), "restore did not return the first packet");
        verify(exact.words.data(), version);
        check(exact.buffer.cursor_up == exact.words.data() + 5 && exact.words[5] == 0xa5a5a5a5u, "restore changed its size or tail guard");
        for (auto [success, capacity] : {std::pair{false, false}, std::pair{false, true}, std::pair{true, false}, std::pair{true, true}}) {
            Storage source;
            source.buffer.cursor_down = source.words.data();
            source.buffer.reserved_dw = 7;
            Growth growth;
            growth.success = success;
            growth.capacity = capacity;
            source.buffer.callback = grow;
            source.buffer.user_data = &growth;
            const auto* packet = function(&source.buffer, 0);
            check(growth.calls == 1 && growth.requested == 12, "restore growth ignored reserved words or called twice");
            check(std::ranges::all_of(source.words, [](auto word) { return word == 0xa5a5a5a5u; }), "restore wrote into the replaced buffer");
            if (success && capacity) {
                check(packet == growth.spare.words.data() && source.buffer.cursor_up == packet + 5, "restore did not use replacement storage");
                verify(packet, version);
                check(packet[5] == 0xa5a5a5a5u, "grown restore overwrote its tail guard");
            } else {
                check(packet == nullptr && source.buffer.cursor_up == (capacity ? growth.spare.words.data() : source.words.data()), "failed growth wrote or advanced a restore");
                check(std::ranges::all_of(growth.spare.words, [](auto word) { return word == 0xa5a5a5a5u; }), "failed growth wrote a packet");
            }
        }
        Storage full;
        full.buffer.cursor_down = full.words.data();
        check(function(&full.buffer, 0) == nullptr && full.buffer.cursor_up == full.words.data(), "exhausted restore without callback did not return null");
        Growth overflow;
        full.buffer.callback = grow;
        full.buffer.user_data = &overflow;
        full.buffer.reserved_dw = std::numeric_limits<std::uint32_t>::max();
        check(function(&full.buffer, 0) == nullptr && overflow.calls == 0, "restore overflow reached its callback");
    }
    Storage source;
    source.buffer.cursor_down = source.words.data();
    Growth switched;
    switched.switchVersion = version == 13u ? 0u : 13u;
    source.buffer.callback = grow;
    source.buffer.user_data = &switched;
    const auto* retained = sceAgcDcbContextStateOp_0100(&source.buffer, 0);
    verify(retained, version);
    check(std::array{retained[1], retained[2], retained[4]} == descriptor, "growth selected a different descriptor for the current packet");
    Storage after;
    const auto* current = sceAgcDcbContextStateOp(&after.buffer, 0);
    verify(current, switched.switchVersion);
    check(current[1] != descriptor[0] || current[2] != descriptor[1], "next restore did not select the newly initialized descriptor");
}

void uninitialized() {
    for (const auto function : {sceAgcDcbContextStateOp, sceAgcDcbContextStateOp_0100}) {
        Storage storage;
        const auto before = storage.words;
        Growth growth;
        growth.switchVersion = 13;
        storage.buffer.cursor_down = storage.words.data();
        storage.buffer.callback = grow;
        storage.buffer.user_data = &growth;
        bool rejected = false;
        try { function(&storage.buffer, 0); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "operation zero accepted unavailable context defaults");
        check(growth.calls == 0, "uninitialized restore reached its growth callback");
        check(storage.words == before && storage.buffer.cursor_up == storage.words.data(), "uninitialized restore wrote a packet");
    }
}

}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected uninitialized or a defaults version");
        if (std::string(argv[1]) == "uninitialized") uninitialized();
        else run(static_cast<std::uint32_t>(std::stoul(argv[1])));
        LibcRunShutdown_nid_postfix();
        std::puts("context default restoration passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
