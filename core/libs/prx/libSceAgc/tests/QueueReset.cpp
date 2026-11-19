#include "prx/libSceAgc/Acb/include/Control.hpp"
#include "prx/libSceAgc/DcbState/include/Init.hpp"
#include "prx/libSceAgc/Misc/include/RegisterDefaults.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool value, const char* text) {
    if (!value) throw std::runtime_error(text);
}

struct Storage {
    std::array<std::uint32_t, 256> words;
    CommandBuffer buffer;
    Storage() : buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0} { words.fill(0xa5a5a5a5u); }
    std::size_t count() const { return buffer.cursor_up - words.data(); }
};

void prefix(const Storage& storage, std::uint32_t tag, std::uint32_t version) {
    const std::array<std::uint32_t, 4> expected{0xc0027900u, 0x342u, tag, version};
    check(std::equal(expected.begin(), expected.end(), storage.words.begin()), "queue reset prefix differs from the SDK protocol");
}

void suffix(const Storage& storage, std::uint32_t tag) {
    check(storage.count() >= 7, "queue reset has no complete marker envelope");
    const auto* end = storage.words.data() + storage.count();
    check(end[-3] == 0xc0017900u && end[-2] == 0x342u && end[-1] == tag, "queue reset suffix differs from the SDK protocol");
    check(*end == 0xa5a5a5a5u, "queue reset overwrote its tail guard");
}

void uninitialized() {
    for (const auto instance : {0u, 1u}) {
        for (const auto raw : {0u, 1u, 4u, 0x20u, 0x200u, 0x4000u, 0xabcd0000u, 0xabcd0001u, 2u, 3u, 0x80u, 0x81u, 0x82u}) {
            Storage storage;
            const auto flags = raw & 0xffffu;
            const auto* result = sceAgcAcbResetQueue(&storage.buffer, raw, instance);
            prefix(storage, 0xce200000u | flags, 0);
            auto count = (flags & 0x80u) != 0 ? 6u : 4u;
            if ((flags & 0x80u) != 0) check(storage.words[4] == 0xc0004600u && storage.words[5] == 0x407u, "queue reset event packet differs");
            if ((flags & 2u) != 0) check(result == nullptr, "uninitialized restore unexpectedly succeeded");
            else {
                check(result == storage.words.data(), "queue reset did not return its first packet");
                count += 3;
                suffix(storage, 0xcea00000u);
            }
            check(storage.count() == count && storage.words[count] == 0xa5a5a5a5u, "queue reset advanced or wrote beyond the expected packets");
        }
    }
}

void invalidSelection() {
    for (const auto flags : {0u, 0x8000u}) {
        Storage storage;
        const auto before = storage.words;
        bool rejected = false;
        const auto missing = NotImplementedCalls_nid_no_patch();
        try { sceAgcAcbResetQueue(&storage.buffer, flags, 2); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected && storage.count() == 0 && storage.words == before, "invalid or unavailable context selection wrote a packet");
        if (flags == 0) check(NotImplementedCalls_nid_no_patch() == missing + 1, "unsupported retail instance did not use the standard missing-function path");
    }
}

std::vector<ShaderRegister> expectedRegisters(std::uint32_t version) {
    std::vector<ShaderRegister> result;
    for (const auto* root : {sceAgcGetRegisterDefaults2(version), sceAgcGetRegisterDefaults2Internal(version)}) {
        ShaderRegister** table;
        std::uint32_t count;
        const auto* bytes = static_cast<const std::byte*>(root);
        std::memcpy(&table, bytes + 8, sizeof(table));
        std::memcpy(&count, bytes + 0x24, sizeof(count));
        if (table == nullptr || count == 0) continue;
        for (std::uint32_t index = 0; index < count; ++index) if (table[0][index].offset >= 0x201u) result.push_back(table[0][index]);
    }
    return result;
}

void initialized() {
    for (const auto version : {8u, 13u}) {
        check(sceAgcInit(version) == 0, "context initialization failed");
        const auto instance = version == 8 ? 0u : 1u;
        Storage storage;
        check(sceAgcAcbResetQueue(&storage.buffer, 3, instance) == storage.words.data(), "initialized reset failed");
        prefix(storage, 0xce000003u, version);
        suffix(storage, 0xce800000u);
        std::vector<ShaderRegister> actual;
        for (std::size_t cursor = 4; cursor < storage.count() - 3;) {
            const auto header = storage.words[cursor];
            const auto size = ((header >> 16u) & 0x3fffu) + 2;
            check((header & 0xc000ffffu) == 0xc0007600u && size >= 3 && cursor + size <= storage.count() - 3, "restored shader state contains a malformed or unexpected packet");
            for (std::uint32_t offset = 0; offset < size - 2; ++offset) actual.push_back({storage.words[cursor + 1] + offset, storage.words[cursor + 2 + offset]});
            cursor += size;
        }
        auto expected = expectedRegisters(version);
        const auto less = [](const auto& left, const auto& right) { return left.offset < right.offset || (left.offset == right.offset && left.value < right.value); };
        std::sort(expected.begin(), expected.end(), less);
        std::sort(actual.begin(), actual.end(), less);
        check(!expected.empty() && actual.size() == expected.size(), "queue reset restored the wrong number of shader registers");
        for (std::size_t index = 0; index < actual.size(); ++index) check(actual[index].offset == expected[index].offset && actual[index].value == expected[index].value, "queue reset restored a wrong shader register");
        Storage active;
        check(sceAgcAcbResetQueue(&active.buffer, 0x8001, 0xffffffffu) == active.words.data(), "active context override failed");
        prefix(active, 0xce008001u, version);
        suffix(active, 0xce800000u);
    }
    check(sceAgcInit(8) == 0, "context reuse failed");
    Storage reuse;
    sceAgcAcbResetQueue(&reuse.buffer, 0x8000, 1);
    prefix(reuse, 0xce008000u, 8);
}

void zeroVersion() {
    check(sceAgcInit(0) == 0, "version-zero context initialization failed");
    std::uint32_t viewport = 0xdeadbeefu;
    check(sceAgcGetDefaultCxStateFlat(&viewport, 0x206, 0x207, 0) == 0 && viewport == 0x43fu, "version-zero context defaults were not built");
    Storage first;
    check(sceAgcAcbResetQueue(&first.buffer, 3, 0) == first.words.data(), "version-zero shader defaults were not restored");
    prefix(first, 0xce000003u, 0);
    suffix(first, 0xce800000u);
    check(first.count() > 7, "version-zero restore omitted shader defaults");
    check(sceAgcInit(13) == 0, "second context initialization failed after version zero");
    Storage second;
    check(sceAgcAcbResetQueue(&second.buffer, 3, 1) == second.words.data(), "second context did not occupy the other slot");
    prefix(second, 0xce000003u, 13);
    suffix(second, 0xce800000u);
    check(sceAgcInit(8) == static_cast<int>(0x8a6c0004u), "version zero did not consume its context slot");
    check(sceAgcInit(0) == 0, "version-zero context could not be reused");
    Storage active;
    check(sceAgcAcbResetQueue(&active.buffer, 0x8003, 0xffffffffu) == active.words.data(), "version-zero active context could not be restored");
    prefix(active, 0xce008003u, 0);
    suffix(active, 0xce800000u);
    check(active.count() == first.count() && std::equal(first.words.begin() + 4, first.words.begin() + first.count(), active.words.begin() + 4), "initializing another version changed version-zero defaults");
}

struct Growth {
    std::uint32_t calls = 0;
    std::uint32_t requested = 0;
    bool result = false;
};

bool APS5_VABI growReset(CommandBuffer* buffer, std::uint32_t requested, void* data) {
    auto& growth = *static_cast<Growth*>(data);
    ++growth.calls;
    growth.requested = requested;
    buffer->cursor_down = buffer->top;
    return growth.result;
}

void growth() {
    for (const auto initial : {0u, 5u}) {
        Storage storage;
        Growth grow;
        storage.buffer.cursor_down = storage.words.data() + initial;
        storage.buffer.reserved_dw = 1;
        storage.buffer.callback = growReset;
        storage.buffer.user_data = &grow;
        check(sceAgcAcbResetQueue(&storage.buffer, 1, 0) == storage.words.data(), "marker allocation incorrectly required a true callback result");
        check(grow.calls == 1 && grow.requested == (initial == 0 ? 5u : 4u), "marker allocation requested an incorrect reserved size");
        check(storage.count() == 7, "grown reset has an incorrect size");
    }
    Storage partial;
    partial.buffer.cursor_down = partial.words.data() + 6;
    check(sceAgcAcbResetQueue(&partial.buffer, 0, 0) == nullptr && partial.count() == 4, "failed suffix did not retain exactly the written prefix");
    Storage event;
    Growth grow;
    event.buffer.cursor_down = event.words.data() + 4;
    event.buffer.callback = growReset;
    event.buffer.user_data = &grow;
    check(sceAgcAcbResetQueue(&event.buffer, 0x80, 0) == nullptr && event.count() == 4 && grow.requested == 2, "event allocation ignored its false callback result or changed the prefix");
    check(event.words[4] == 0xa5a5a5a5u, "failed event allocation wrote a packet");
}

void cache() {
    for (const auto flags : {0x40u, 0x100u, 0x140u}) {
        Storage storage;
        check(sceAgcAcbResetQueue(&storage.buffer, flags, 0) == storage.words.data(), "cache reset failed");
        check(storage.count() == 15 && storage.words[4] == 0xc0065800u, "cache reset did not encode one ACQUIRE_MEM packet");
        check(storage.words[5] == 0 && storage.words[6] == 0 && storage.words[7] == 0 && storage.words[8] == 0 && storage.words[9] == 0 && storage.words[10] == 0, "cache reset changed the acquire range or engine");
        check(storage.words[11] == (flags == 0x40 ? 0xc1u : 0xc3a1u), "cache reset selected wrong cache operations");
        suffix(storage, 0xcea00000u);
    }
}

}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected one queue-reset test mode");
        const std::string mode = argv[1];
        if (mode == "uninitialized") { uninitialized(); invalidSelection(); }
        else if (mode == "initialized") initialized();
        else if (mode == "zero") zeroVersion();
        else if (mode == "growth") growth();
        else if (mode == "cache") cache();
        else throw std::runtime_error("unknown queue-reset test mode");
        LibcRunShutdown_nid_postfix();
        std::puts("queue reset passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
