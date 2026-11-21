#include "prx/libSceAgc/DcbFlow/include/Control.hpp"
#include "prx/libSceAgc/DcbState/include/Init.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <span>
#include <utility>
#include <vector>
#include "DrawQueueResetOracle.hpp"

namespace {

void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Storage {
    std::array<std::uint32_t, 1024> words;
    CommandBuffer buffer;
    Storage() : buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0} { words.fill(0xa5a5a5a5u); }
    std::size_t Count() const { return buffer.cursor_up - words.data(); }
};

constexpr std::array<std::uint32_t, 61> EmptyReset{
    0xffff1000u, 0xc0027904u, 0x342u, 0xce2003ffu, 0u,
    0xc0004600u, 0x407u, 0xc0004600u, 0x410u,
    0xc0039f00u, 0u, 0u, 0x80000000u, 0u,
    0xc0036300u, 0u, 0u, 0x80000000u, 0u,
    0xc0036400u, 0u, 0u, 0x80000000u, 0u,
    0xc0002f00u, 1u, 0xc0017a00u, 0x20000243u, 0x480u,
    0xc0012600u, 0u, 0u, 0xc0001300u, 0xffffffffu,
    0xc0021102u, 1u, 0u, 0u, 0xc0021100u, 1u, 0u, 0u,
    0xc0065800u, 0x86007fc0u, 0xfffffffeu, 0xffu, 1u, 0u, 0x19u, 0xc3e1u,
    0xc0004600u, 0x2eu, 0xc0004600u, 0x2cu,
    0xc0024600u, 0x138u, 0x07fffc00u, 0u,
    0xc0017904u, 0x342u, 0xcea00000u};

void ExecuteState(const Storage& storage, AgcDriver::QueueState& queue) {
    for (std::size_t at = 0; at < storage.Count();) {
        const auto count = AgcDriver::Pm4::PacketWords(storage.words[at]);
        Check(count <= storage.Count() - at, "draw reset ended inside a packet");
        const auto packet = std::span(storage.words.data() + at, count);
        AgcDriver::Pm4::Validate(packet, 0);
        const auto opcode = (packet[0] >> 8u) & 0xffu;
        if (!AgcDriver::Pm4::FillerPacket(packet[0]) && opcode != 0x46 && opcode != 0x58) AgcDriver::Pm4::Execute(packet, queue);
        at += count;
    }
}

void Empty() {
    for (const auto instance : {0u, 1u}) {
        Storage storage;
        Check(sceAgcDcbResetQueue(&storage.buffer, 0x3ff, instance) == storage.words.data(), "draw reset did not return its envelope");
        Check(storage.Count() == EmptyReset.size() && std::equal(EmptyReset.begin(), EmptyReset.end(), storage.words.begin()), "draw reset differs from the reference-derived composite sequence");
        Check(storage.words[storage.Count()] == 0xa5a5a5a5u, "draw reset changed its tail guard");
    }
    Storage storage;
    Check(sceAgcDcbResetQueue(&storage.buffer, 0xabcd0000u, 0) == storage.words.data(), "high argument bits were not ignored");
    const std::array<std::uint32_t, 8> expected{0xffff1000u, 0xc0027904u, 0x342u, 0xce200000u, 0u, 0xc0017904u, 0x342u, 0xcea00000u};
    Check(storage.Count() == expected.size() && std::equal(expected.begin(), expected.end(), storage.words.begin()), "empty draw reset differs from the reference envelope");
}

void Oracle() {
    for (const auto& reference : DrawResetOracle::Cases) {
        Storage storage;
        Check(sceAgcDcbResetQueue(&storage.buffer, reference.flags, 0) == storage.words.data(), "measured draw reset failed");
        Check(storage.Count() == reference.words.size() && std::equal(reference.words.begin(), reference.words.end(), storage.words.begin()), "draw reset differs from a measured single-bit console case");
        Check(storage.words[storage.Count()] == 0xa5a5a5a5u, "console-case reset changed its tail guard");
    }
}

void Preserved() {
    Storage storage;
    AgcDriver::QueueState queue;
    queue.context[0x206] = 0x43f;
    queue.shader[0x246] = 0x12345678;
    queue.indexBase = 0x12345000;
    queue.instanceCount = 9;
    Check(sceAgcDcbResetQueue(&storage.buffer, 0, 0) != nullptr, "empty reset failed");
    ExecuteState(storage, queue);
    Check(queue.context.at(0x206) == 0x43f && queue.shader.at(0x246) == 0x12345678 && queue.indexBase == 0x12345000 && queue.instanceCount == 9, "an empty draw reset cleared queue state");
}

void Initialized(std::uint32_t version) {
    Check(sceAgcInit(version) == 0, "initializing draw defaults failed");
    Storage storage;
    Check(sceAgcDcbResetQueue(&storage.buffer, 0x3ff, 0) != nullptr, "initialized draw reset failed");
    Check(storage.words[3] == 0xce0003ffu && storage.words[4] == version, "draw reset omitted its initialized context identity");
    AgcDriver::QueueState queue;
    queue.context[0x206] = 0;
    queue.shader[0x216] = 0;
    queue.indexBase = 0x12345000;
    queue.indexBufferSize = 2;
    queue.instanceCount = 19;
    queue.drawIndirectBase = 0x12340000;
    queue.dispatchIndirectBase = 0x56780000;
    ExecuteState(storage, queue);
    Check(queue.context.at(0x206) == 0x43f && queue.context.at(0x8e) == 0 && queue.context.at(0x8f) == 0, "draw reset did not restore public context defaults");
    Check(queue.shader.at(0x216) == 0xffffffffu, "draw reset did not restore shader defaults");
    Check(queue.indexBase == 0 && queue.indexBufferSize == 0xffffffffu && queue.indexType == 0 && queue.instanceCount == 1 && queue.drawIndirectBase == 0 && queue.dispatchIndirectBase == 0, "draw reset omitted index, instance or indirect state");
    queue.userConfig[0x35] = 0x12345678;
    Storage normal;
    sceAgcDcbResetQueue(&normal.buffer, 4, 0);
    ExecuteState(normal, queue);
    Check(queue.userConfig.at(0x35) == 0x12345678, "ordinary user-config reset changed excluded counter registers");
    Storage counters;
    sceAgcDcbResetQueue(&counters.buffer, 0x800, 0);
    ExecuteState(counters, queue);
    Check(queue.userConfig.at(0x35) == 0, "counter-only reset did not restore counter register defaults");
    Storage active;
    Check(sceAgcDcbResetQueue(&active.buffer, 0x8000, 0xffffffffu) != nullptr && active.words[4] == version, "active draw context selection used the ignored instance");
}

void Capacity() {
    for (std::size_t capacity = 0; capacity <= EmptyReset.size() + 7; ++capacity) {
        Storage storage;
        storage.buffer.cursor_down = storage.words.data() + capacity;
        storage.buffer.top = storage.buffer.cursor_down;
        storage.buffer.reserved_dw = 7;
        const auto* result = sceAgcDcbResetQueue(&storage.buffer, 0x3ff, 0);
        Check(storage.Count() + 7 <= std::max(capacity, std::size_t{7}), "draw reset consumed reserved or unavailable words");
        Check(storage.words[storage.Count()] == 0xa5a5a5a5u, "partial draw reset wrote beyond its cursor");
        Check((result != nullptr) == (capacity >= EmptyReset.size() + 7), "draw reset reported the wrong capacity result");
    }
    for (const auto pair : {std::pair{0x400u, 0u}, std::pair{0u, 2u}}) {
        Storage storage;
        const auto before = storage.words;
        const auto missing = NotImplementedCalls_nid_no_patch();
        bool refused = false;
        try { sceAgcDcbResetQueue(&storage.buffer, pair.first, pair.second); }
        catch (const std::runtime_error&) { refused = true; }
        Check(refused && storage.Count() == 0 && storage.words == before && NotImplementedCalls_nid_no_patch() == missing + 1, "unsupported reset selection did not refuse without writes");
    }
}

}

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "expected one draw reset mode");
        const std::string mode = argv[1];
        if (mode == "oracle") Oracle();
        else if (mode == "empty") Empty();
        else if (mode == "preserved") Preserved();
        else if (mode == "capacity") Capacity();
        else if (mode.starts_with("initialized-")) Initialized(static_cast<std::uint32_t>(std::stoul(mode.substr(12))));
        else throw std::runtime_error("unknown draw reset mode");
        LibcRunShutdown_nid_postfix();
        std::puts("draw reset passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
