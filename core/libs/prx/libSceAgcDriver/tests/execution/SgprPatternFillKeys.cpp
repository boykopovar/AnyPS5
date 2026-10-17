#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DwordPatternFill.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::MatchSgprPatternFill;
using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 23> Code{
    0xbfa00001u, 0xd7460004u, 0x04010c09u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0x7da80808u,
    0xbf88000du, 0x360a0883u, 0xbe84047eu, 0xbe86047eu, 0x7e100505u, 0x7da40a08u, 0xbefc0308u, 0x7e0c8700u,
    0x8a867e06u, 0xbefe0406u, 0xbf85fff9u, 0xbefe0404u, 0xe0102000u, 0x80000604u, 0xbf810000u};
constexpr std::uint32_t Dwords = 1024;
constexpr std::size_t KeyBytes = Dwords * 4;
constexpr std::uint64_t SurfaceBytes = KeyBytes * 256;
alignas(256) std::array<std::uint32_t, Dwords> Keys{};

using Pattern = std::array<std::uint32_t, 4>;

constexpr Pattern Uniform(std::uint8_t key) {
    const std::uint32_t word = key * 0x01010101u;
    return {word, word, word, word};
}

std::uint64_t KeysAddress(std::size_t offset = 0) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Keys.data())) + offset;
}

std::vector<std::uint32_t> UserData(std::uint32_t records, std::uint32_t count, const Pattern& pattern, std::uint32_t format = 0x00014004u) {
    const auto address = KeysAddress();
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), records, format, pattern[0], pattern[1], pattern[2], pattern[3], count};
}

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const std::array<std::uint32_t, 5> packet{0xc0031500u, 16, 1, 1, 0x41u};
    const Pattern pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    const auto data = UserData(Dwords, Dwords, pattern);
    const auto fill = MatchSgprPatternFill(packet, Code, data, compute);
    Require(fill && fill->base == KeysAddress() && fill->bytes == KeyBytes && fill->pattern == pattern, "the SGPR pattern-fill kernel was not recognized");
    for (std::size_t word = 0; word < Code.size(); ++word) {
        auto altered = Code;
        altered[word] ^= 1u;
        Require(!MatchSgprPatternFill(packet, altered, data, compute), "a different shader matched the SGPR pattern-fill kernel");
    }
    Require(!MatchSgprPatternFill(packet, std::span(Code).first(22), data, compute), "a truncated shader matched");
    const auto bounded = MatchSgprPatternFill(packet, Code, UserData(Dwords, 100, pattern), compute);
    Require(bounded && bounded->bytes == 400, "the fill exceeded the kernel's count");
    const auto clamped = MatchSgprPatternFill(packet, Code, UserData(64, Dwords, pattern), compute);
    Require(clamped && clamped->bytes == 256, "the fill exceeded the descriptor's records");
    auto fewer = packet;
    fewer[1] = 2;
    const auto launched = MatchSgprPatternFill(fewer, Code, data, compute);
    Require(launched && launched->bytes == 512, "the fill exceeded the launched threads");
    Require(!MatchSgprPatternFill(packet, Code, UserData(Dwords, 0, pattern), compute), "an empty fill matched");
    for (const std::uint32_t format : {0x00015004u, 0x00016004u, 0x10014004u}) {
        Require(MatchSgprPatternFill(packet, Code, UserData(Dwords, Dwords, pattern, format), compute).has_value(), "a 32-bit output format with an index bound was refused");
    }
    for (const std::uint32_t format : {0x00005004u, 0x0000d004u, 0x0004b004u, 0x00814004u, 0x20014004u, 0x30014004u, 0x40014004u}) {
        Require(!MatchSgprPatternFill(packet, Code, UserData(Dwords, Dwords, pattern, format), compute), "a descriptor whose elements are not one bounded 32-bit value matched");
    }
    auto wide = data;
    wide[1] = (wide[1] & 0xffffu) | (8u << 16u);
    Require(!MatchSgprPatternFill(packet, Code, wide, compute), "a stride other than one dword matched");
    auto swizzled = data;
    swizzled[1] |= 1u << 31u;
    Require(!MatchSgprPatternFill(packet, Code, swizzled, compute), "a swizzled descriptor matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchSgprPatternFill(packet, Code, extra, compute), "a different user SGPR count matched (the group id moves)");
    auto partial = packet;
    partial[4] |= 0x20u;
    Require(!MatchSgprPatternFill(partial, Code, data, compute), "a partial dispatch matched");
    auto rows = packet;
    rows[2] = 2;
    Require(!MatchSgprPatternFill(rows, Code, data, compute), "a dispatch with more than one group row matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchSgprPatternFill(packet, Code, data, noGroup), "a dispatch without the group id matched");
}

void Submit(const std::vector<std::uint32_t>& data, std::uint32_t groups) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{64, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (1u << 7u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, groups, 1, 1, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

struct KeyRead {
    DccKeys keys;
    bool waited;
};

std::uint64_t lastNote = 0;

KeyRead ReadAfterWrite(std::uint64_t surfaceBytes = SurfaceBytes) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            auto* recorder = AgcDriver::Graphics::Recorder::Active();
            if (const auto noted = recorder != nullptr ? recorder->NewestWriteNote(KeysAddress(), KeyBytes) : 0; noted > lastNote) {
                lastNote = noted;
                const auto keys = AgcDriver::Graphics::CurrentDccKeys(KeysAddress(), surfaceBytes);
                return {keys, !recorder->PendingWriteOverlaps(KeysAddress(), KeyBytes)};
            }
        }
        Require(std::chrono::steady_clock::now() < deadline, "the submitted dispatch never became a pending write over the keys");
        std::this_thread::yield();
    }
}

void Settle() {
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(KeysAddress(), KeyBytes);
}

void RequireStored(std::uint32_t count, const Pattern& pattern, std::uint32_t before, const std::string& what) {
    Settle();
    for (std::uint32_t i = 0; i < Dwords; ++i) {
        Require(Keys[i] == (i < count ? pattern[i % 4] : before), what + ": dword " + std::to_string(i) + " holds " + std::to_string(Keys[i]));
    }
}

std::string Describe(const char* what, const KeyRead& read) {
    return std::string(what) + ": the keys read " + AgcDriver::Graphics::DccKeysName(read.keys) + " and the read " + (read.waited ? "waited for the GPU" : "did not wait");
}

void ExecuteTests() {
    const std::array<std::pair<std::uint8_t, DccKeys>, 6> codes{{{0x00, DccKeys::Clear0000}, {0x40, DccKeys::Clear0001}, {0x80, DccKeys::Clear1110}, {0xc0, DccKeys::Clear1111}, {0x20, DccKeys::ClearRegister}, {0xff, DccKeys::Uncompressed}}};
    for (const auto& [key, keys] : codes) {
        Settle();
        Submit(UserData(Dwords, Dwords, Uniform(key)), Dwords / 64);
        const auto read = ReadAfterWrite();
        Require(read.keys == keys && !read.waited, Describe("a pending key code fill by the kernel", read));
        RequireStored(Dwords, Uniform(key), 0, "the kernel's key code fill");
    }

    Submit(UserData(Dwords, Dwords, Uniform(0x5a)), Dwords / 64);
    const auto other = ReadAfterWrite();
    Require(other.keys == DccKeys::Mixed && other.waited, Describe("a pending fill of a byte that is not a key code", other));
    RequireStored(Dwords, Uniform(0x5a), 0, "the kernel's fill of another byte");

    const Pattern several{0x40404040u, 0x40404040u, 0x40404040u, 0x80808080u};
    Submit(UserData(Dwords, Dwords, several), Dwords / 64);
    const auto mixed = ReadAfterWrite();
    Require(mixed.keys == DccKeys::Mixed && mixed.waited, Describe("a pending fill of several key codes", mixed));
    RequireStored(Dwords, several, 0, "the kernel's fill of several key codes");

    Submit(UserData(Dwords, Dwords, Uniform(0x40)), Dwords / 64);
    const auto first = ReadAfterWrite();
    Require(first.keys == DccKeys::Clear0001 && !first.waited, Describe("a pending 0001 fill by the kernel", first));
    Submit(UserData(Dwords, Dwords / 2, Uniform(0x5a)), Dwords / 64);
    const auto later = ReadAfterWrite();
    Require(later.keys == DccKeys::Mixed && later.waited, Describe("a later writer over half of a pending 0001 fill", later));
    Settle();
    for (std::uint32_t i = 0; i < Dwords; ++i) Require(Keys[i] == (i < Dwords / 2 ? 0x5a5a5a5au : 0x40404040u), "the later writer over the 0001 fill: dword " + std::to_string(i) + " holds " + std::to_string(Keys[i]));

    Submit(UserData(Dwords, Dwords / 2, Uniform(0xc0)), Dwords / 64);
    const auto half = ReadAfterWrite();
    Require(half.keys == DccKeys::Mixed, Describe("a pending 1111 fill of half the keys over 0001 keys", half));
    Settle();
    Submit(UserData(Dwords, Dwords / 2, Uniform(0xc0)), Dwords / 64);
    const auto inside = ReadAfterWrite(SurfaceBytes / 2);
    Require(inside.keys == DccKeys::Clear1111, Describe("the half of the keys a pending 1111 fill covers", inside));
    Settle();
    for (std::uint32_t i = 0; i < Dwords; ++i) Require(Keys[i] == (i < Dwords / 2 ? 0xc0c0c0c0u : 0x40404040u), "the 1111 fill of half the keys: dword " + std::to_string(i) + " holds " + std::to_string(Keys[i]));
}

}

int main() {
    try {
        MatchTests();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        ExecuteTests();
        AgcDriverShutdown_nid_postfix();
        std::puts("SGPR pattern fill key tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
