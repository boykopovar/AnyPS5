#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawAhead.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::DrawMiss;

constexpr std::byte Kept{0x40};
constexpr std::size_t Draws = 4;

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> TablePixelCode{0xf4000080u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 12> BufferPixelCode{0xf4080100u, 0xfa000000u, 0xbf8cc07fu, 0xf4200082u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 4> FetchVertexCode{0xbe8a2108u, 0xf80008cfu, 0x0b0a0908u, 0xbf810000u};
using Target = std::array<std::byte, 1024>;
alignas(256) std::array<Target, Draws> Targets{};
alignas(256) std::array<Target, Draws> References{};
alignas(256) std::array<Target, 1024> Skipped{};
alignas(256) std::array<Target, 4> TableTargets{};
alignas(256) std::array<Target, 4> SpeculationTargets{};
alignas(256) std::array<Target, 4> SpeculationReferences{};
alignas(64) std::atomic<std::uint32_t> Released{0};
alignas(64) std::uint32_t Condition = 0;
alignas(64) std::array<std::uint32_t, 4> IndirectRecord{3u, 1u, 0u, 0u};
alignas(64) std::array<std::uint32_t, 4> Table{};
alignas(64) std::array<std::uint32_t, 4> Descriptor{};
alignas(64) std::array<float, 4> BufferValue{};
alignas(64) std::array<std::uint32_t, 4> VertexBufferTable{};
alignas(64) std::array<std::uint32_t, 1> VertexAttributeTable{};
alignas(64) std::array<float, 16> Vertices{};
alignas(64) std::array<std::uint16_t, 8> RestartIndices{};
alignas(64) std::array<std::uint16_t, 4> TriangleIndices{0, 1, 2, 0};
alignas(256) constexpr std::array<std::uint32_t, 4> SilentVertexCode{0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};
alignas(8) Header TablePixelHeader{};
alignas(8) Header BufferPixelHeader{};
alignas(8) Header SilentVertexHeader{};

struct FetchHeader {
    Shader shader{};
    ShaderUserData userData{};
    std::array<std::uint16_t, 11> direct{};
    ShaderSemantic semantic{};
};

alignas(8) FetchHeader FetchVertexHeader{};

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void RegisteredFetch() {
    auto& header = FetchVertexHeader;
    header.direct.fill(0xffffu);
    header.direct[8] = 4;
    header.direct[10] = 6;
    header.semantic.hardware_mapping = 8;
    header.semantic.size_in_elements = 4;
    header.userData.direct_resource_offset = header.direct.data();
    header.userData.direct_resource_count = static_cast<std::uint16_t>(header.direct.size());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(FetchHeader);
    header.shader.shader_size = sizeof(FetchVertexCode);
    header.shader.code = FetchVertexCode.data();
    header.shader.type = 2;
    header.shader.user_data = &header.userData;
    header.shader.input_semantics = &header.semantic;
    header.shader.num_input_semantics = 1;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void AppendCustom(std::vector<std::uint32_t>& commands, std::uint32_t operation, std::initializer_list<std::uint32_t> payload) {
    Append(commands, 0x10, payload);
    commands[commands.size() - payload.size() - 1] |= operation << 2u;
}

std::uint32_t Low(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u);
}

struct Pass {
    bool hold = true;
    bool table = false;
    bool holdAfter = false;
    bool pushInSkippedCondition = false;
    bool fetch = false;
    bool restart = false;
    std::uint32_t primitive = 4;
    const std::uint32_t* vertexCode = nullptr;
    std::span<const std::uint16_t> indices;
    std::uint32_t instances = 1;
    float depthMax = 1.0f;
    const std::uint32_t* pixelCode = nullptr;
    const void* tableAddress = nullptr;
    const std::uint32_t* condition = nullptr;
    bool indirectDrawIndex = false;
    std::optional<std::uint32_t> written;
    std::vector<std::pair<Target*, std::uint32_t>> draws;
};

std::vector<std::uint32_t> Commands(const Pass& pass) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(pass.vertexCode != nullptr ? pass.vertexCode : pass.fetch ? FetchVertexCode.data() : VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(pass.pixelCode != nullptr ? pass.pixelCode : pass.table ? TablePixelCode.data() : PixelCode.data());
    const void* table = pass.tableAddress != nullptr ? pass.tableAddress : Table.data();
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(pass.depthMax)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)},
    };
    if (pass.fetch) {
        shader.insert(shader.end(), {{0x8b, 8u << 1u}, {0x8c, Low(FetchVertexCode.data())}, {0x8d, High(FetchVertexCode.data())}, {0x8e, 0}, {0x8f, 0}});
        shader.insert(shader.end(), {{0x90, Low(VertexBufferTable.data())}, {0x91, High(VertexBufferTable.data())}, {0x92, Low(VertexAttributeTable.data())}, {0x93, High(VertexAttributeTable.data())}});
    } else {
        shader.emplace_back(0x8b, 1u << 1u);
    }
    if (pass.table || pass.pixelCode != nullptr) shader.insert(shader.end(), {{0x00b, 2u << 1u}, {0x00c, Low(table)}, {0x00d, High(table)}});
    else shader.insert(shader.end(), {{0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)}});
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, pass.primitive});
    if (pass.indirectDrawIndex) {
        Append(commands, 0x79, {0x24a, 0u});
        Append(commands, 0x11, {1u, Low(IndirectRecord.data()), High(IndirectRecord.data())});
    }
    if (pass.restart) {
        Append(commands, 0x79, {0x24b, 1});
        Append(commands, 0x69, {0x103, 0xffffffffu});
    }
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2f, {pass.instances});
    if (pass.hold) Append(commands, 0x3c, {0x13u, Low(&Released), High(&Released), 1u, 0xffffffffu, 4u});
    if (pass.written) Append(commands, 0x37, {(5u << 8u) | (1u << 20u), Low(Table.data()), High(Table.data()), *pass.written});
    if (pass.pushInSkippedCondition) {
        Append(commands, 0x22, {Low(&Condition), High(&Condition), 0u, 3u});
        AppendCustom(commands, 0x1a, {1u, 0u});
        AppendCustom(commands, 0x1a, {1u, 0u});
    }
    std::vector<std::uint32_t> body;
    for (const auto& [target, word] : pass.draws) {
        const auto color = reinterpret_cast<std::uintptr_t>(target->data());
        Append(body, 0x69, {0x318, static_cast<std::uint32_t>(color >> 8u)});
        Append(body, 0x69, {0x390, static_cast<std::uint32_t>(color >> 40u)});
        if (!pass.fetch) Append(body, 0x76, {0x8c, word});
        if (!pass.indices.empty()) {
            const auto count = static_cast<std::uint32_t>(pass.indices.size());
            Append(body, 0x2a, {0u});
            Append(body, 0x27, {count, Low(pass.indices.data()), High(pass.indices.data()), count, 0u});
            continue;
        }
        if (pass.indirectDrawIndex) Append(body, 0x2c, {0u, 0u, 0u, 0x8cu | (1u << 31u), 1u, 0u, 0u, 16u, 2u});
        else Append(body, 0x2d, {3, 2});
    }
    if (pass.condition != nullptr) Append(commands, 0x22, {Low(pass.condition), High(pass.condition), 0u, static_cast<std::uint32_t>(body.size())});
    commands.insert(commands.end(), body.begin(), body.end());
    if (pass.pushInSkippedCondition) AppendCustom(commands, 0x1a, {2u, 0u});
    if (pass.holdAfter) Append(commands, 0x3c, {0x13u, Low(&Released), High(&Released), 1u, 0xffffffffu, 4u});
    return commands;
}

void Submit(const Pass& pass) {
    auto commands = Commands(pass);
    if (pass.hold || pass.holdAfter) Released.store(0, std::memory_order_release);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
}

void Release() {
    Released.store(1, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
}

template <typename TCondition>
void Await(TCondition&& condition, const std::string& what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!condition()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            Released.store(1, std::memory_order_release);
            throw std::runtime_error("timed out waiting for " + what);
        }
        std::this_thread::yield();
    }
}

void RenderReference(const Pass& pass) {
    ShaderRecompiler::SetDebugProbeActive(true);
    struct Restore {
        ~Restore() { ShaderRecompiler::SetDebugProbeActive(false); }
    } restore;
    auto reference = pass;
    reference.hold = false;
    Submit(reference);
    AgcDriverWaitIdle_nid_postfix();
}

void DrawsCompiledAheadAreUsed(bool verify) {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    Pass reference{false};
    for (std::size_t draw = 0; draw < Draws; ++draw) reference.draws.emplace_back(&References[draw], static_cast<std::uint32_t>(draw));
    for (auto& target : References) target.fill(Kept);
    RenderReference(reference);
    for (auto& target : Targets) target.fill(Kept);
    Pass pass;
    for (std::size_t draw = 0; draw < Draws; ++draw) pass.draws.emplace_back(&Targets[draw], static_cast<std::uint32_t>(draw));
    const auto before = driver.DrawCacheCounters();
    const auto aheadBefore = driver.DrawAheadStatistics();
    const auto preparedBefore = AgcDriver::Graphics::PreparedDrawUses();
    Submit(pass);
    Await([&] { return driver.DrawCacheCounters().inserts - before.inserts >= Draws && driver.DrawAheadStatistics().published - aheadBefore.published >= Draws; }, "the draws behind the wait to be compiled ahead");
    const auto ahead = driver.DrawCacheCounters();
    Release();
    Require(ahead.inserts - before.inserts == Draws && ahead.lookups == before.lookups, "the draws behind the wait were not compiled ahead of the queue: " + std::to_string(ahead.inserts - before.inserts) + " inserts and " + std::to_string(ahead.lookups - before.lookups) + " lookups before the release");
    const auto counters = driver.DrawCacheCounters();
    const auto statistics = driver.DrawAheadStatistics();
    Require(statistics.compiled - aheadBefore.compiled == Draws, "the workers compiled " + std::to_string(statistics.compiled - aheadBefore.compiled) + " draws, expected " + std::to_string(Draws));
    if (!verify) Require(AgcDriver::Graphics::PreparedDrawUses() - preparedBefore == Draws, "the queue consumed " + std::to_string(AgcDriver::Graphics::PreparedDrawUses() - preparedBefore) + " prepared draw records, expected " + std::to_string(Draws));
    if (!verify) Require(statistics.used - aheadBefore.used == Draws, "the queue used " + std::to_string(statistics.used - aheadBefore.used) + " published draw records, expected " + std::to_string(Draws));
    const auto lookups = counters.lookups - before.lookups;
    const auto absent = counters.absent - before.absent;
    const auto hits = counters.hits - before.hits;
    const auto inserts = counters.inserts - before.inserts;
    Require(lookups == Draws && absent == 0 && hits == Draws && inserts == (verify ? 2 : 1) * Draws, "draws compiled ahead were not reused: " + std::to_string(lookups) + " lookups, " + std::to_string(absent) + " absent, " + std::to_string(hits) + " hits, " + std::to_string(inserts) + " inserts");
    for (std::size_t miss = 0; miss < counters.misses.size(); ++miss) Require(counters.misses[miss] == before.misses[miss], "a draw compiled ahead missed with reason " + std::to_string(miss));
    if (verify) Require(counters.verifyHits - before.verifyHits == Draws && counters.verifyMismatches == before.verifyMismatches, "a draw compiled ahead captured other memory than the queue's own compile");
    const auto kept = static_cast<std::size_t>(std::count(References[0].begin(), References[0].end(), Kept));
    Require(kept != 0 && kept != References[0].size(), "the reference draw covered " + std::to_string(References[0].size() - kept) + " of " + std::to_string(References[0].size()) + " bytes, expected an edge through the target");
    for (std::size_t draw = 0; draw < Draws; ++draw) Require(Targets[draw] == References[draw], "draw " + std::to_string(draw) + " compiled ahead wrote other pixels than the uncached path");
    for (std::size_t sight = 0; sight < 2; ++sight) {
        auto again = pass;
        again.hold = false;
        const auto seen = driver.DrawCacheCounters();
        Submit(again);
        AgcDriverWaitIdle_nid_postfix();
        const auto repeated = driver.DrawCacheCounters();
        Require(repeated.hits - seen.hits == Draws && repeated.absent == seen.absent && repeated.inserts - seen.inserts == (verify ? Draws : 0), "sight " + std::to_string(sight + 2) + " of the draws compiled ahead made " + std::to_string(repeated.hits - seen.hits) + " hits, " + std::to_string(repeated.absent - seen.absent) + " absent and " + std::to_string(repeated.inserts - seen.inserts) + " inserts, expected every draw to keep its entry");
    }
}

void SkippedDrawsRetireWithTheirSubmission() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    Condition = 0;
    Pass pass;
    pass.condition = &Condition;
    for (std::size_t draw = 0; draw < Skipped.size(); ++draw) {
        Skipped[draw].fill(Kept);
        pass.draws.emplace_back(&Skipped[draw], static_cast<std::uint32_t>(0x100 + draw));
    }
    const auto before = driver.DrawAheadStatistics();
    Submit(pass);
    Await([&] { return driver.DrawAheadStatistics().compiled != before.compiled; }, "a skipped draw to be compiled ahead");
    Release();
    const auto retired = driver.DrawAheadStatistics();
    const auto inserts = driver.DrawCacheCounters().inserts;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto later = driver.DrawAheadStatistics();
    Require(later.compiled == retired.compiled && driver.DrawCacheCounters().inserts == inserts, "draw-ahead workers inserted " + std::to_string(later.compiled - retired.compiled) + " entries for draws of a submission that had retired");
    Require(later.published == retired.published, "the draw-ahead scan published draws of a submission that had retired");
    Require(retired.compiled - before.compiled < Skipped.size(), "every skipped draw was compiled ahead before the queue retired them");
    for (const auto& target : Skipped) Require(std::all_of(target.begin(), target.end(), [](std::byte value) { return value == Kept; }), "a draw inside a false condition wrote its target");
}

void SkippedDrawsAreNotCompiledAfterTheQueuePassesThem() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    Condition = 0;
    Pass pass{false};
    pass.holdAfter = true;
    pass.condition = &Condition;
    for (std::size_t draw = 0; draw < Skipped.size(); ++draw) {
        Skipped[draw].fill(Kept);
        pass.draws.emplace_back(&Skipped[draw], static_cast<std::uint32_t>(0x400 + draw));
    }
    const auto before = driver.DrawAheadStatistics();
    Submit(pass);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto settled = driver.DrawAheadStatistics();
    const auto inserts = driver.DrawCacheCounters().inserts;
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    const auto later = driver.DrawAheadStatistics();
    const auto laterInserts = driver.DrawCacheCounters().inserts;
    Release();
    Require(later.compiled == settled.compiled && laterInserts == inserts, "draw-ahead workers inserted " + std::to_string(later.compiled - settled.compiled) + " entries for draws the queue had skipped and was no longer reading");
    Require(later.compiled - before.compiled < Skipped.size(), "every skipped draw was compiled ahead");
    for (const auto& target : Skipped) Require(std::all_of(target.begin(), target.end(), [](std::byte value) { return value == Kept; }), "a draw inside a false condition wrote its target");
}

void TablesWrittenBehindTheWaitAreSeen() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    const auto stale = std::bit_cast<std::uint32_t>(0.0f);
    const auto fresh = std::bit_cast<std::uint32_t>(1.0f);
    std::array<Target, 2> expected{};
    for (std::size_t value = 0; value < 2; ++value) {
        Table.fill(value == 0 ? stale : fresh);
        TableTargets[3].fill(Kept);
        Pass reference{false, true};
        reference.draws.emplace_back(&TableTargets[3], 0u);
        RenderReference(reference);
        expected[value] = TableTargets[3];
    }
    Require(expected[0] != expected[1], "the table draw renders the same pixels for both table values");
    for (std::size_t mode = 0; mode < 2; ++mode) {
        Table.fill(stale);
        auto& target = TableTargets[mode];
        target.fill(Kept);
        Pass pass{true, true};
        if (mode == 1) pass.written = fresh;
        pass.draws.emplace_back(&target, static_cast<std::uint32_t>(0x200 + mode));
        const auto before = driver.DrawAheadStatistics();
        Submit(pass);
        Await([&] { return driver.DrawAheadStatistics().compiled != before.compiled; }, "the table draw to be compiled ahead");
        if (mode == 0) Table.fill(fresh);
        Release();
        Require(target == expected[1], mode == 0 ? "a draw compiled ahead ignored a CPU store to its table made while the queue waited" : "a draw compiled ahead ignored a queue write to its table before the draw");
    }
}

void VerificationComparesWholeCompilations() {
    using AgcDriver::DriverDetail::SameCompilation;
    ShaderRecompiler::RecompileResult base;
    base.spirv = std::vector<std::uint32_t>{0x07230203u, 0x00010000u, 0u, 1u, 0u};
    ShaderRecompiler::DescriptorBinding binding{};
    binding.kind = ShaderRecompiler::DescriptorKind::StorageImage;
    binding.count = 1;
    binding.guestDescriptor = {1, 2, 3, 4, 5, 6, 7, 8};
    binding.imageWritten = {false};
    binding.imageShapes = {ShaderRecompiler::DescriptorImageShape{}};
    binding.samplerDepthCompare = {false};
    binding.bufferRead = {true};
    base.bindings.push_back(binding);
    base.vertexAttributes.push_back({0, 4, {{1, 2, 3, 4}}, 0});
    base.pushConstants = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    base.fragmentParameters.push_back({0, 0, false, false});
    Require(SameCompilation(base, base), "a compilation differed from itself");
    auto cached = base;
    cached.cacheHit = true;
    Require(SameCompilation(base, cached), "a compilation served from a cache differed from the same compilation built anew");
    const auto differs = [&](const char* what, auto&& change) {
        auto other = base;
        change(other);
        Require(!SameCompilation(base, other), std::string("a compilation with another ") + what + " was taken for the same");
    };
    differs("written image", [](auto& result) { result.bindings[0].imageWritten[0] = true; });
    differs("image shape", [](auto& result) { result.bindings[0].imageShapes.push_back(ShaderRecompiler::DescriptorImageShape{}); });
    differs("sampler depth compare", [](auto& result) { result.bindings[0].samplerDepthCompare[0] = true; });
    differs("buffer read", [](auto& result) { result.bindings[0].bufferRead[0] = false; });
    differs("guest descriptor", [](auto& result) { result.bindings[0].guestDescriptor[0] = 9; });
    differs("vertex attribute", [](auto& result) { result.vertexAttributes[0].resource.fields[0] = 5; });
    differs("push constant", [](auto& result) { result.pushConstants[0] = std::byte{9}; });
    differs("SPIR-V module", [](auto& result) { result.spirv = std::vector<std::uint32_t>{0x07230203u, 0x00010000u, 0u, 2u, 0u}; });
    differs("fragment parameter", [](auto& result) { result.fragmentParameters[0].flat = true; });
    differs("specialization", [](auto& result) { result.specialization.push_back({1, 2}); });
    differs("workgroup memory size", [](auto& result) { result.workgroupMemoryDwords = 64; });
}

void PacketDerivedCompilesStayOnTheQueue() {
    using AgcDriver::DriverDetail::DrawAhead;
    AgcDriver::Graphics::State state{};
    AgcDriver::Pm4::DrawParameters draw{0x10000, 3, 2, 1, 0};
    Require(DrawAhead::CompilesAhead(state, draw), "a direct vertex draw is not compiled ahead");
    auto indirect = draw;
    indirect.indirect.emplace();
    Require(!DrawAhead::CompilesAhead(state, indirect), "an indirect draw is compiled ahead");
    auto mesh = state;
    mesh.stages.mesh = ShaderRecompiler::MeshConfiguration{4, 1, 3, 3, 1, 64, 128, 0, 4};
    Require(!DrawAhead::CompilesAhead(mesh, draw), "a mesh draw, whose index buffer descriptor its packet supplies, is compiled ahead under a key of its registers");
    auto tessellation = state;
    tessellation.stages.tessellation = ShaderRecompiler::TessellationConfiguration{3, 3, 1, 0, 2};
    Require(DrawAhead::CompilesAhead(tessellation, draw), "a tessellated draw of one instance is not compiled ahead");
    auto instanced = draw;
    instanced.instanceCount = 2;
    Require(!DrawAhead::CompilesAhead(tessellation, instanced), "an instanced tessellated draw, whose hull patches its packet's counts number, is compiled ahead");
}

void SkippedSpeculationStaysOnProbation() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    for (auto& target : Targets) target.fill(Kept);
    Pass once{false};
    once.draws.emplace_back(&Targets[0], 0x300u);
    AgcDriver::DriverDetail::DrawAhead::SetCompileAhead(false);
    Submit(once);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::DriverDetail::DrawAhead::SetCompileAhead(true);
    const auto sighted = driver.DrawCacheState();
    const auto aheadBefore = driver.DrawAheadStatistics();
    Condition = 0;
    Pass skipped;
    skipped.condition = &Condition;
    skipped.draws.emplace_back(&Targets[0], 0x300u);
    const auto drawn = Targets[0];
    Submit(skipped);
    Await([&] { return driver.DrawAheadStatistics().compiled != aheadBefore.compiled; }, "the skipped draw to be compiled ahead");
    Release();
    const auto speculated = driver.DrawCacheState();
    Require(speculated.admitted == sighted.admitted && speculated.probation == sighted.probation + 1, "a draw seen once and compiled ahead inside a false condition was admitted without a second execution: " + std::to_string(speculated.admitted - sighted.admitted) + " admitted, " + std::to_string(speculated.probation - sighted.probation) + " on probation");
    Require(Targets[0] == drawn, "a draw inside a false condition wrote its target");
    Submit(once);
    AgcDriverWaitIdle_nid_postfix();
    const auto executed = driver.DrawCacheState();
    Require(executed.admitted == sighted.admitted + 1 && executed.probation == sighted.probation, "the second execution of a draw compiled ahead on probation left " + std::to_string(executed.admitted - sighted.admitted) + " admitted and " + std::to_string(executed.probation - sighted.probation) + " on probation, expected it admitted as without compile-ahead");
}

void UncachedExecutionsAreNoSightings() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    for (auto& target : Targets) target.fill(Kept);
    const auto before = driver.DrawCacheState();
    const auto aheadBefore = driver.DrawAheadStatistics();
    Condition = 0;
    Pass skipped;
    skipped.condition = &Condition;
    skipped.draws.emplace_back(&Targets[2], 0x340u);
    Submit(skipped);
    Await([&] { return driver.DrawAheadStatistics().compiled != aheadBefore.compiled; }, "the skipped draw to be compiled ahead");
    Release();
    const auto speculated = driver.DrawCacheState();
    Require(speculated.admitted == before.admitted && speculated.probation == before.probation + 1, "a skipped draw compiled ahead was not held on probation");
    Pass indirect{false};
    indirect.indirectDrawIndex = true;
    indirect.draws.emplace_back(&Targets[2], 0x340u);
    for (std::size_t sight = 0; sight < 2; ++sight) {
        Submit(indirect);
        AgcDriverWaitIdle_nid_postfix();
    }
    const auto indirected = driver.DrawCacheState();
    Require(indirected.admitted == before.admitted && indirected.probation == before.probation + 1, "two executions of an indirect draw the queue draws on the CPU, which the draw cache does not admit, promoted the draw compiled ahead with its registers: " + std::to_string(indirected.admitted - before.admitted) + " admitted, " + std::to_string(indirected.probation - before.probation) + " on probation");
    Pass direct{false};
    direct.draws.emplace_back(&Targets[2], 0x340u);
    Submit(direct);
    AgcDriverWaitIdle_nid_postfix();
    const auto first = driver.DrawCacheState();
    Require(first.admitted == before.admitted && first.probation == before.probation + 1, "the first execution of the direct draw admitted it");
    Submit(direct);
    AgcDriverWaitIdle_nid_postfix();
    const auto second = driver.DrawCacheState();
    Require(second.admitted == before.admitted + 1 && second.probation == before.probation, "the second execution of the direct draw did not admit it");
}

std::atomic<bool> HookEntered{false};
std::atomic<bool> HookRelease{false};

void BlockFirstCompileRead(bool compile) {
    if (!compile) return;
    auto expected = false;
    if (!HookEntered.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    while (!HookRelease.load(std::memory_order_acquire)) std::this_thread::yield();
}

void PinnedSpeculationHoldsTheGateAndRefusesCommits() {
    using Speculation = AgcDriver::DriverDetail::DrawAhead::Speculation;
    constexpr std::uint64_t serial = 0x7000;
    AgcDriver::DriverDetail::DrawAhead ahead(AgcDriver::DriverDetail::Driver::Get(), 0);
    Speculation speculation(ahead, serial, 8, true);
    speculation.Enter();
    std::atomic<bool> retired{false};
    std::thread retiring([&] {
        ahead.Retire(serial);
        retired.store(true, std::memory_order_release);
    });
    try {
        Await([&] { return speculation.Passed(); }, "the retire to pass the pinned speculation");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Require(!retired.load(std::memory_order_acquire), "a retire returned while a speculative read of its submission was pinned");
        auto committed = false;
        Require(!speculation.Commit([&] { committed = true; }) && !committed, "a speculation committed after its draw had passed");
    } catch (...) {
        speculation.Leave();
        retiring.join();
        throw;
    }
    speculation.Leave();
    retiring.join();
    Require(retired.load(std::memory_order_acquire), "the retire did not return after the pinned read left");
    auto refused = false;
    try {
        speculation.Enter();
        speculation.Leave();
    } catch (const AgcDriver::GuestMemory::SpeculativeReadRefused&) {
        refused = true;
    }
    Require(refused, "a speculative read of a passed draw was not refused with SpeculativeReadRefused");
}

void RetireWaitsForAWorkerReadInProgress() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    Condition = 0;
    auto& target = TableTargets[2];
    target.fill(Kept);
    Table.fill(std::bit_cast<std::uint32_t>(1.0f));
    Pass pass{true, true};
    pass.condition = &Condition;
    pass.draws.emplace_back(&target, 0x600u);
    HookEntered.store(false, std::memory_order_release);
    HookRelease.store(false, std::memory_order_release);
    AgcDriver::DriverDetail::DrawAhead::SetPinnedReadHook(&BlockFirstCompileRead);
    struct Reset {
        ~Reset() {
            HookRelease.store(true, std::memory_order_release);
            AgcDriver::DriverDetail::DrawAhead::SetPinnedReadHook(nullptr);
        }
    } reset;
    const auto before = driver.DrawAheadStatistics();
    const auto inserts = driver.DrawCacheCounters().inserts;
    Submit(pass);
    Await([&] { return HookEntered.load(std::memory_order_acquire); }, "a worker to read guest memory for its compile");
    Released.store(1, std::memory_order_release);
    std::atomic<bool> idle{false};
    std::thread waiter([&] {
        AgcDriverWaitIdle_nid_postfix();
        idle.store(true, std::memory_order_release);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    const auto blockedIdle = idle.load(std::memory_order_acquire);
    const auto blocked = driver.DrawAheadStatistics();
    const auto blockedInserts = driver.DrawCacheCounters().inserts;
    HookRelease.store(true, std::memory_order_release);
    waiter.join();
    Require(!blockedIdle, "the queue retired a skipped draw while a worker's guest read of it was in progress");
    Require(blocked.compiled == before.compiled && blockedInserts == inserts, "a worker inserted an entry while its guest read was held");
    Await([&] { return driver.DrawAheadStatistics().dropped > before.dropped; }, "the held compile to be dropped");
    const auto after = driver.DrawAheadStatistics();
    Require(after.compiled == before.compiled && driver.DrawCacheCounters().inserts == inserts, "a worker inserted an entry for a draw the queue skipped during its read");
    Require(std::all_of(target.begin(), target.end(), [](std::byte value) { return value == Kept; }), "a draw inside a false condition wrote its target");
}

std::array<std::uint32_t, 4> BufferDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(BufferValue.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, static_cast<std::uint32_t>(sizeof(BufferValue)), 0x31016facu};
}

void ContextStackDivergenceLeavesTheDrawToTheQueue() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    Condition = 0;
    SpeculationReferences[0].fill(Kept);
    Pass reference{false};
    reference.draws.emplace_back(&SpeculationReferences[0], 0x720u);
    RenderReference(reference);
    SpeculationTargets[0].fill(Kept);
    Pass pass;
    pass.pushInSkippedCondition = true;
    pass.draws.emplace_back(&SpeculationTargets[0], 0x720u);
    const auto before = driver.DrawAheadStatistics();
    Submit(pass);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto held = driver.DrawAheadStatistics();
    Release();
    const auto after = driver.DrawAheadStatistics();
    Require(held.published == before.published && after.used == before.used, "the draw-ahead scan published a draw past a CONTEXT_STATE push it could not execute");
    Require(SpeculationTargets[0] == SpeculationReferences[0], "a draw after a skipped CONTEXT_STATE push wrote other pixels than the uncached path");
}

void DescriptorsRepairedBeforeTheDrawAreSeen() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    BufferValue.fill(1.0f);
    Descriptor = BufferDescriptor();
    SpeculationReferences[1].fill(Kept);
    Pass reference{false, true};
    reference.pixelCode = BufferPixelCode.data();
    reference.tableAddress = Descriptor.data();
    reference.draws.emplace_back(&SpeculationReferences[1], 0x730u);
    RenderReference(reference);
    Require(std::any_of(SpeculationReferences[1].begin(), SpeculationReferences[1].end(), [](std::byte value) { return value != Kept; }), "the buffer draw wrote nothing");
    Descriptor[3] |= 3u << 30u;
    SpeculationTargets[1].fill(Kept);
    Pass pass{true, true};
    pass.pixelCode = BufferPixelCode.data();
    pass.tableAddress = Descriptor.data();
    pass.draws.emplace_back(&SpeculationTargets[1], 0x731u);
    const auto before = driver.DrawAheadStatistics();
    Submit(pass);
    Await([&] { const auto now = driver.DrawAheadStatistics(); return now.compiled != before.compiled || now.dropped != before.dropped; }, "the compile ahead of a draw with an invalid buffer descriptor to end");
    Descriptor = BufferDescriptor();
    Release();
    Require(SpeculationTargets[1] == SpeculationReferences[1], "a draw whose buffer descriptor was repaired while the queue waited wrote other pixels than the uncached path");
}

class GuestPage {
public:
    GuestPage() {
#ifdef _WIN32
        address = VirtualAlloc(nullptr, Bytes, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
        Require(address != nullptr, "could not reserve an inaccessible page");
#else
        address = mmap(nullptr, Bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(address != MAP_FAILED, "could not map an inaccessible page");
#endif
    }
    ~GuestPage() {
#ifdef _WIN32
        VirtualFree(address, 0, MEM_RELEASE);
#else
        munmap(address, Bytes);
#endif
    }
    GuestPage(const GuestPage&) = delete;
    GuestPage& operator=(const GuestPage&) = delete;
    void MakeWritable() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(address, Bytes, PAGE_READWRITE, &previous) != 0, "could not make the page accessible");
#else
        Require(mprotect(address, Bytes, PROT_READ | PROT_WRITE) == 0, "could not make the page accessible");
#endif
    }
    std::uint32_t* Words() const { return static_cast<std::uint32_t*>(address); }

private:
    static constexpr std::size_t Bytes = 65536;
    void* address = nullptr;
};

void UnmappedSpeculativeReadsAreRefused() {
    AgcDriver::DriverDetail::DrawAhead ahead(AgcDriver::DriverDetail::Driver::Get(), 0);
    AgcDriver::DriverDetail::DrawAhead::Speculation speculation(ahead, 0x7100, 8, true);
    const GuestPage page;
    std::array<std::byte, 4> bytes{};
    auto refused = false;
    {
        const AgcDriver::GuestMemory::SpeculativeReadScope speculative(speculation);
        try {
            AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(page.Words()), bytes, 4);
        } catch (const AgcDriver::GuestMemory::SpeculativeReadRefused&) {
            refused = true;
        }
    }
    Require(refused, "a speculative read of an inaccessible range was not refused with SpeculativeReadRefused");
}

void InputsMappedBeforeTheDrawAreSeen() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    const auto value = std::bit_cast<std::uint32_t>(1.0f);
    Table.fill(value);
    SpeculationReferences[2].fill(Kept);
    Pass reference{false, true};
    reference.draws.emplace_back(&SpeculationReferences[2], 0x740u);
    RenderReference(reference);
    GuestPage page;
    SpeculationTargets[2].fill(Kept);
    Pass pass{true, true};
    pass.tableAddress = page.Words();
    pass.draws.emplace_back(&SpeculationTargets[2], 0x741u);
    const auto before = driver.DrawAheadStatistics();
    Submit(pass);
    Await([&] { return driver.DrawAheadStatistics().dropped != before.dropped; }, "the compile ahead of a draw reading an inaccessible table to be refused");
    page.MakeWritable();
    std::fill_n(page.Words(), Table.size(), value);
    Release();
    Require(driver.DrawAheadStatistics().compiled == before.compiled, "a draw reading an inaccessible table was compiled ahead");
    Require(SpeculationTargets[2] == SpeculationReferences[2], "a draw whose table was mapped while the queue waited wrote other pixels than the uncached path");
}

void RestartOnlyDrawsBuildNoVertexLayout() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    constexpr std::uint32_t stride = 16380;
    const auto vertices = reinterpret_cast<std::uintptr_t>(Vertices.data());
    VertexBufferTable = {static_cast<std::uint32_t>(vertices), (static_cast<std::uint32_t>(vertices >> 32u) & 0xffffu) | (stride << 16u), 1u, 0x0104dfacu};
    VertexAttributeTable = {0u};
    RestartIndices.fill(0xffffu);
    SpeculationTargets[3].fill(Kept);
    Pass pass;
    pass.fetch = true;
    pass.restart = true;
    pass.primitive = 6;
    pass.indices = RestartIndices;
    pass.draws.emplace_back(&SpeculationTargets[3], 0u);
    const auto before = driver.DrawAheadStatistics();
    Submit(pass);
    Await([&] { const auto now = driver.DrawAheadStatistics(); return now.compiled != before.compiled || now.dropped != before.dropped; }, "the restart-only draw to be compiled ahead");
    const auto ahead = driver.DrawAheadStatistics();
    Release();
    Require(ahead.compiled - before.compiled == 1 && ahead.dropped == before.dropped, "the restart-only draw with a vertex stride past the device limit was not compiled ahead");
    Require(std::all_of(SpeculationTargets[3].begin(), SpeculationTargets[3].end(), [](std::byte value) { return value == Kept; }), "a draw of restart indices only wrote its target");
}

void PreparedVertexInputDrawsAsTheQueue(bool verify) {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    constexpr std::uint32_t stride = 16;
    const auto vertices = reinterpret_cast<std::uintptr_t>(Vertices.data());
    VertexBufferTable = {static_cast<std::uint32_t>(vertices), (static_cast<std::uint32_t>(vertices >> 32u) & 0xffffu) | (stride << 16u), 3u, 0x0104dfacu};
    VertexAttributeTable = {0u};
    Vertices = {0.0f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    Pass reference{false};
    reference.fetch = true;
    reference.indices = std::span<const std::uint16_t>(TriangleIndices.data(), 3);
    reference.draws.emplace_back(&SpeculationReferences[0], 0u);
    SpeculationReferences[0].fill(Kept);
    RenderReference(reference);
    const auto kept = static_cast<std::size_t>(std::count(SpeculationReferences[0].begin(), SpeculationReferences[0].end(), Kept));
    Require(kept != 0 && kept != SpeculationReferences[0].size(), "the reference draw of fetched vertices covered " + std::to_string(SpeculationReferences[0].size() - kept) + " of " + std::to_string(SpeculationReferences[0].size()) + " bytes, expected a triangle edge through the target");
    auto pass = reference;
    pass.hold = true;
    pass.draws = {{&SpeculationTargets[0], 0u}};
    SpeculationTargets[0].fill(Kept);
    const auto before = driver.DrawAheadStatistics();
    const auto preparedBefore = AgcDriver::Graphics::PreparedDrawUses();
    Submit(pass);
    Await([&] { const auto now = driver.DrawAheadStatistics(); return now.compiled != before.compiled || now.dropped != before.dropped; }, "the draw of fetched vertices to be compiled ahead");
    const auto ahead = driver.DrawAheadStatistics();
    Release();
    Require(ahead.compiled - before.compiled == 1 && ahead.dropped == before.dropped, "the draw of fetched vertices was not compiled ahead");
    if (!verify) Require(AgcDriver::Graphics::PreparedDrawUses() - preparedBefore == 1, "the queue did not draw the fetched vertices with the prepared vertex input layout");
    Require(SpeculationTargets[0] == SpeculationReferences[0], "the draw of fetched vertices through its prepared vertex input layout wrote other pixels than the uncached path");
}

void SpeculativeValidationFailuresStayPrivate() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    Pass pass;
    pass.vertexCode = SilentVertexCode.data();
    pass.indices = std::span<const std::uint16_t>(TriangleIndices.data(), 3);
    pass.instances = 0u;
    pass.draws.emplace_back(&SpeculationTargets[1], 0x600u);
    SpeculationTargets[1].fill(Kept);
    const auto before = driver.DrawAheadStatistics();
    Submit(pass);
    Await([&] { const auto now = driver.DrawAheadStatistics(); return now.compiled != before.compiled || now.dropped != before.dropped; }, "the zero-instance draw of a vertex program without a position to be compiled ahead");
    const auto ahead = driver.DrawAheadStatistics();
    Release();
    Require(ahead.compiled - before.compiled == 1, "the zero-instance draw of a vertex program without a position was not compiled ahead");
    Require(std::all_of(SpeculationTargets[1].begin(), SpeculationTargets[1].end(), [](std::byte value) { return value == Kept; }), "a draw of zero instances wrote its target");
}

void PreparedDrawStaysOnItsDevice() {
    AgcDriver::Graphics::Context preparing{};
    preparing.device = reinterpret_cast<VkDevice>(std::uintptr_t{0x1000});
    AgcDriver::Graphics::Context replacement{};
    replacement.device = reinterpret_cast<VkDevice>(std::uintptr_t{0x2000});
    AgcDriver::Graphics::PreparedDraw prepared;
    prepared.device = preparing.device;
    Require(AgcDriver::Graphics::PreparedDrawFor(&prepared, preparing) == &prepared, "a prepared draw was not served on the device that prepared it");
    Require(AgcDriver::Graphics::PreparedDrawFor(&prepared, replacement) == nullptr, "a prepared draw, whose content key names its device's cached resources, was served on a replacement device");
    Require(AgcDriver::Graphics::PreparedDrawFor(nullptr, preparing) == nullptr, "a missing prepared draw was served");
}

void PreparedDrawFollowsItsVariants() {
    using AgcDriver::DriverDetail::DispatchVariant;
    using AgcDriver::DriverDetail::PreparedDrawRecord;
    std::vector<std::shared_ptr<DispatchVariant>> variants{std::make_shared<DispatchVariant>(), nullptr, std::make_shared<DispatchVariant>()};
    auto prepared = std::make_shared<const AgcDriver::Graphics::PreparedDraw>();
    const PreparedDrawRecord record{{variants.begin(), variants.end()}, 2, prepared};
    Require(record.For(variants, 2) == prepared.get(), "a prepared draw was not served for the variants it was prepared with");
    Require(record.For(variants, 3) == nullptr, "a prepared draw was served for another stage count");
    auto replaced = variants;
    replaced[2] = std::make_shared<DispatchVariant>();
    Require(record.For(replaced, 2) == nullptr, "a prepared draw was served for another variant");
    replaced = variants;
    replaced[1] = std::make_shared<DispatchVariant>();
    Require(record.For(replaced, 2) == nullptr, "a prepared draw was served with a variant where it had none");
    replaced.clear();
    std::weak_ptr<DispatchVariant> released = variants[0];
    variants[0] = std::make_shared<DispatchVariant>();
    Require(released.expired() && record.For(variants, 2) == nullptr, "a prepared draw was served after one of its variants was released");
}

struct Admitted {
    std::size_t admitted;
    std::size_t probation;
};

Admitted RunSameShapePair(bool ahead, float shape, std::uint32_t firstWord) {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    for (auto& target : Targets) target.fill(Kept);
    Pass pass;
    pass.depthMax = shape;
    pass.draws.emplace_back(&Targets[0], firstWord);
    pass.draws.emplace_back(&Targets[0], firstWord + 1);
    const auto before = driver.DrawCacheState();
    const auto aheadBefore = driver.DrawAheadStatistics();
    pass.hold = ahead;
    AgcDriver::DriverDetail::DrawAhead::SetCompileAhead(ahead);
    Submit(pass);
    if (ahead) {
        Await([&] { return driver.DrawAheadStatistics().compiled - aheadBefore.compiled >= 2; }, "both same-shape draws to be compiled ahead");
        const auto compiled = driver.DrawCacheState();
        Require(compiled.probation - before.probation == 2 && compiled.admitted == before.admitted, "draws of a shape seen once were not held on probation: " + std::to_string(compiled.probation - before.probation) + " on probation and " + std::to_string(compiled.admitted - before.admitted) + " admitted");
        Release();
    } else {
        AgcDriverWaitIdle_nid_postfix();
    }
    AgcDriver::DriverDetail::DrawAhead::SetCompileAhead(true);
    const auto after = driver.DrawCacheState();
    return {after.admitted - before.admitted, ahead ? after.probation - before.probation : 0};
}

void ProbationDoesNotChangeAdmission() {
    const auto upstream = RunSameShapePair(false, 0.5f, 0x800);
    Require(upstream.admitted == 1, "without compile-ahead the second key of a shape was " + std::string(upstream.admitted == 0 ? "not admitted" : "admitted twice"));
    const auto ahead = RunSameShapePair(true, 0.25f, 0x900);
    Require(ahead.admitted == upstream.admitted && ahead.probation == 2 - upstream.admitted, "compile-ahead admitted " + std::to_string(ahead.admitted) + " of two same-shape keys and left " + std::to_string(ahead.probation) + " on probation, upstream's rule admits " + std::to_string(upstream.admitted));
}

void PromotionVictimsAreReleased() {
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    AgcDriver::DriverDetail::DrawAhead::SetCompileAhead(false);
    for (std::size_t shape = 0; shape < 2; ++shape) {
        for (std::size_t sight = 0; sight < 2; ++sight) {
            Pass pass{false};
            pass.depthMax = 0.5f + 0.125f * static_cast<float>(shape);
            pass.draws.emplace_back(&Targets[0], static_cast<std::uint32_t>(0xa00 + shape));
            Submit(pass);
            AgcDriverWaitIdle_nid_postfix();
        }
    }
    AgcDriver::DriverDetail::DrawAhead::SetCompileAhead(true);
    const auto full = driver.DrawCacheState();
    Require(full.admitted == 2 && full.probation == 0 && full.released == 0, "the draw cache held " + std::to_string(full.admitted) + " admitted entries, expected it full with 2");
    const auto aheadBefore = driver.DrawAheadStatistics();
    Pass pass;
    pass.depthMax = 0.875f;
    pass.draws.emplace_back(&Targets[0], 0xb00);
    pass.draws.emplace_back(&Targets[0], 0xb01);
    Submit(pass);
    Await([&] { return driver.DrawAheadStatistics().compiled - aheadBefore.compiled >= 2; }, "both draws to be compiled ahead");
    Release();
    const auto promoted = driver.DrawCacheState();
    Require(promoted.admitted == 2 && promoted.probation == 1, "after the promotion the cache held " + std::to_string(promoted.admitted) + " admitted and " + std::to_string(promoted.probation) + " probation entries, expected 2 and 1");
    Require(promoted.released == 0, "promotions that evicted entries left " + std::to_string(promoted.released) + " of them owned by the cache");
}

}

int main(int argc, char** argv) {
    try {
        PreparedDrawFollowsItsVariants();
        PreparedDrawStaysOnItsDevice();
        PacketDerivedCompilesStayOnTheQueue();
        VerificationComparesWholeCompilations();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        PinnedSpeculationHoldsTheGateAndRefusesCommits();
        UnmappedSpeculativeReadsAreRefused();
        if (!AgcDriver::GuestMemory::SpeculativeReadsSupported()) {
            std::puts("skipped the compile-ahead checks, speculative guest reads are unavailable so the draw-ahead workers are off");
            return VulkanTestSkipped;
        }
        const bool verify = std::getenv("APS5_VERIFY_DRAW_ENTRIES") != nullptr;
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        Registered(TablePixelHeader, TablePixelCode.data(), sizeof(TablePixelCode), 1);
        Registered(BufferPixelHeader, BufferPixelCode.data(), sizeof(BufferPixelCode), 1);
        RegisteredFetch();
        Registered(SilentVertexHeader, SilentVertexCode.data(), sizeof(SilentVertexCode), 2);
        if (argc > 1 && std::string(argv[1]) == "small-cache") {
            PromotionVictimsAreReleased();
            AgcDriverShutdown_nid_postfix();
            std::cout << "Entries evicted by a promotion were released\n";
            return 0;
        }
        DrawsCompiledAheadAreUsed(verify);
        SkippedDrawsRetireWithTheirSubmission();
        SkippedDrawsAreNotCompiledAfterTheQueuePassesThem();
        RetireWaitsForAWorkerReadInProgress();
        TablesWrittenBehindTheWaitAreSeen();
        SkippedSpeculationStaysOnProbation();
        UncachedExecutionsAreNoSightings();
        ContextStackDivergenceLeavesTheDrawToTheQueue();
        DescriptorsRepairedBeforeTheDrawAreSeen();
        InputsMappedBeforeTheDrawAreSeen();
        RestartOnlyDrawsBuildNoVertexLayout();
        PreparedVertexInputDrawsAsTheQueue(verify);
        SpeculativeValidationFailuresStayPrivate();
        ProbationDoesNotChangeAdmission();
        AgcDriverShutdown_nid_postfix();
        std::cout << "Draws compiled ahead of the queue were reused" << (verify ? " and matched the queue's own compile" : "") << ", retired with their submission, held their retire for reads in progress, followed table writes and left draws whose speculation failed to the queue\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        Released.store(1, std::memory_order_release);
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
