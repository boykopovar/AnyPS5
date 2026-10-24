#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::DrawMiss;
using AgcDriver::DriverDetail::DrawKeySightings;

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{0x7e000208u, 0x7e020280u, 0x7e040280u, 0x7e0602f2u, 0xf80008cfu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> TriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 16> OffsetTriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e0c0c08u, 0x06020306u, 0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> ConstantPixelCode{0xf4000080u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> ColorMemory{};
alignas(256) std::array<float, 4> PixelConstant{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};
alignas(8) Header TriangleVertexHeader{};
alignas(8) Header ConstantPixelHeader{};
alignas(8) Header OffsetTriangleVertexHeader{};
alignas(256) std::array<std::uint32_t, 4> IndirectRecord{3, 1, 0, 0};
alignas(256) std::array<std::uint32_t, 8> OffsetRecords{3, 1, 4, 0, 3, 1, 0, 0};
alignas(256) std::array<std::uint32_t, 4> OffsetRecord{3, 1, 0, 0};

const Shader* Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    return &header.shader;
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> Draws(float scale, const std::uint32_t* vertexCode, std::uint32_t vertexUserSgprs, const std::uint32_t* pixelCode, std::uint32_t pixelUserSgprs, const std::vector<std::vector<std::uint32_t>>& pixelWords) {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(vertexCode);
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode);
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
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(scale)}, {0x110, std::bit_cast<std::uint32_t>(scale)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, vertexUserSgprs << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, pixelUserSgprs << 1u},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    for (const auto& words : pixelWords) {
        for (std::uint32_t k = 0; k < words.size(); ++k) Append(commands, 0x76, {0x00c + k, words[k]});
        Append(commands, 0x2d, {3, 2});
    }
    return commands;
}

AgcDriver::DriverDetail::DrawEntryCounters Submit(std::vector<std::uint32_t> commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    return AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
}

AgcDriver::DriverDetail::DrawEntryCounters Submit(float scale, std::initializer_list<float> pixelWords) {
    std::vector<std::vector<std::uint32_t>> words;
    for (const auto word : pixelWords) words.push_back({std::bit_cast<std::uint32_t>(word)});
    return Submit(Draws(scale, VertexCode.data(), 1, PixelCode.data(), 1, words));
}

AgcDriver::DriverDetail::DrawEntryCounters SubmitConstantDraws(std::size_t draws) {
    const auto constant = reinterpret_cast<std::uintptr_t>(PixelConstant.data());
    const std::vector<std::vector<std::uint32_t>> words(draws, {static_cast<std::uint32_t>(constant), static_cast<std::uint32_t>(constant >> 32u)});
    return Submit(Draws(32.0f, TriangleVertexCode.data(), 0, ConstantPixelCode.data(), 2, words));
}

unsigned BrightestColorByte() {
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(ColorMemory.data()), ColorMemory.size());
    unsigned brightest = 0;
    for (const auto value : ColorMemory) brightest = std::max(brightest, static_cast<unsigned>(value));
    return brightest;
}

void ThirdDrawHitsTheCache(bool libraries) {
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    const auto counters = Submit(32.0f, {1.0f, 1.0f, 1.0f});
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    const auto layout = counters.misses[static_cast<std::size_t>(DrawMiss::Layout)];
    Require(counters.lookups == 3 && counters.absent == 2, mode + ": the three draws made " + std::to_string(counters.lookups) + " draw-cache lookups (" + std::to_string(counters.absent) + " without an entry), expected 3 lookups with 2 absent");
    Require(counters.inserts == 1, mode + ": the three draws inserted " + std::to_string(counters.inserts) + " draw entries, expected 1: a key is cached the second time it is seen");
    Require(layout == 0, mode + ": the repeated draw missed the draw cache on its push offset (" + std::to_string(layout) + " layout misses)");
    Require(counters.hits == 1, mode + ": the repeated draw did not reuse the cached draw (" + std::to_string(counters.hits) + " hits)");
}

void UserDataDrawsReuseTheirShape(bool libraries) {
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    const bool verify = std::getenv("APS5_VERIFY_DRAW_DATA_HITS") != nullptr;
    const auto before = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    const auto known = Submit(32.0f, {2.0f, 3.0f, 4.0f});
    Require(known.lookups - before.lookups == 3 && known.absent - before.absent == 3, mode + ": three draws with new user words did not miss the draw key three times");
    Require(known.dataHits - before.dataHits == 3, mode + ": " + std::to_string(known.dataHits - before.dataHits) + " of three draws that differ from a cached draw only in user words reused its decode, expected 3");
    Require(known.inserts == before.inserts, mode + ": a new key of a shape that has an entry was cached at its first sight");
    const auto stages = known.dataStagesReused + known.dataStagesCompiled - before.dataStagesReused - before.dataStagesCompiled;
    Require(stages == 6, mode + ": the three data-only hits accounted " + std::to_string(stages) + " stages, expected 6");
    Require(verify ? known.dataVerified - before.dataVerified == 3 : known.dataStagesReused - before.dataStagesReused == 3, mode + ": the data-only hits reused " + std::to_string(known.dataStagesReused - before.dataStagesReused) + " stages and verified " + std::to_string(known.dataVerified - before.dataVerified) + ", expected the 3 vertex stages, whose user words did not change");
    const auto fresh = Submit(16.0f, {5.0f, 6.0f, 7.0f});
    Require(fresh.inserts - known.inserts == 1, mode + ": three keys of a new shape inserted " + std::to_string(fresh.inserts - known.inserts) + " draw entries, expected 1: the shape's second key");
    Require(fresh.dataHits - known.dataHits == 1, mode + ": the third key of a new shape made " + std::to_string(fresh.dataHits - known.dataHits) + " data-only hits, expected 1");
}

void StageDataHitsRefreshTheirWords(bool libraries) {
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    const bool verify = std::getenv("APS5_VERIFY_DRAW_DATA_HITS") != nullptr;
    const bool enabled = std::getenv("APS5_NO_DRAW_STAGE_DATA_HITS") == nullptr;
    Registered(TriangleVertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
    Registered(ConstantPixelHeader, ConstantPixelCode.data(), sizeof(ConstantPixelCode), 1);
    ColorMemory.fill(std::byte{0});
    PixelConstant[0] = 0.25f;
    const auto before = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    const auto cached = SubmitConstantDraws(3);
    Require(cached.inserts - before.inserts == 1 && cached.hits - before.hits == 1, mode + ": three draws of a pixel shader reading a constant through its user words did not insert once and hit once");
    Require(!enabled || cached.stageDataInserts - before.stageDataInserts == 1, mode + ": the inserted draw recorded " + std::to_string(cached.stageDataInserts - before.stageDataInserts) + " stages with data positions, expected the fragment stage");
    Require(BrightestColorByte() == 64, mode + ": the draws of constant 0.25 wrote " + std::to_string(BrightestColorByte()) + ", expected 64");
    PixelConstant[0] = 1.0f;
    const auto refreshed = SubmitConstantDraws(1);
    const auto fragmentMisses = refreshed.misses[static_cast<std::size_t>(DrawMiss::FragmentDiffering)] - cached.misses[static_cast<std::size_t>(DrawMiss::FragmentDiffering)];
    if (!enabled) {
        Require(fragmentMisses == 1, mode + ": with stage data hits off, a changed constant did not miss on the fragment stage");
    } else if (verify) {
        Require(refreshed.stageDataVerified - cached.stageDataVerified == 1, mode + ": the stage data hit was verified " + std::to_string(refreshed.stageDataVerified - cached.stageDataVerified) + " times, expected once");
    } else {
        Require(refreshed.hits - cached.hits == 1 && fragmentMisses == 0, mode + ": a draw whose constant changed missed the cached draw");
        Require(refreshed.stageDataHits - cached.stageDataHits == 1 && refreshed.stageDataWords - cached.stageDataWords == 1, mode + ": the hit refreshed " + std::to_string(refreshed.stageDataWords - cached.stageDataWords) + " words in " + std::to_string(refreshed.stageDataHits - cached.stageDataHits) + " stages, expected 1 word in the fragment stage");
        Require(refreshed.inserts == cached.inserts, mode + ": a stage data hit inserted a draw entry");
    }
    Require(BrightestColorByte() == 255, mode + ": the draw of constant 1.0 wrote " + std::to_string(BrightestColorByte()) + ", expected 255");
}

void RefreshedStageDataVerifies() {
    Registered(TriangleVertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
    Registered(ConstantPixelHeader, ConstantPixelCode.data(), sizeof(ConstantPixelCode), 1);
    ColorMemory.fill(std::byte{0});
    PixelConstant[0] = 0.25f;
    const auto cached = SubmitConstantDraws(3);
    Require(BrightestColorByte() == 64, "draw-entry verification: the initial constant did not render");
    PixelConstant[0] = 1.0f;
    const auto refreshed = SubmitConstantDraws(1);
    Require(refreshed.verifyHits - cached.verifyHits == 1 && refreshed.stageDataHits - cached.stageDataHits == 1, "draw-entry verification: the refreshed constant did not exercise a verified stage data hit");
    Require(refreshed.verifyMismatches == cached.verifyMismatches, "draw-entry verification: refreshed stage data differed from a fresh capture");
    Require(BrightestColorByte() == 255, "draw-entry verification: the refreshed constant did not render");
}

void RetainedPixelCacheTransitions() {
    auto commands = Draws(32.0f, VertexCode.data(), 1, PixelCode.data(), 1, {});
    Append(commands, 0x76, {0x00c, std::bit_cast<std::uint32_t>(1.0f)});
    const auto draw = [&] {
        for (unsigned i = 0; i < 3; ++i) Append(commands, 0x2d, {3, 2});
    };
    const auto skipped = [&] {
        Append(commands, 0x69, {0x8e, 0, 0});
        Append(commands, 0x69, {0x203, 0});
        Append(commands, 0x69, {0x2dc, 0xaa00});
        draw();
    };
    Append(commands, 0x69, {0x1c4, 1});
    skipped();
    Append(commands, 0x69, {0x8e, 0xf, 0xf});
    draw();
    skipped();
    Append(commands, 0x69, {0x203, 0x400});
    draw();
    skipped();
    Append(commands, 0x69, {0x8e, 0xf, 0xf});
    Append(commands, 0x69, {0x2dc, 0xaa01});
    draw();
    skipped();
    const auto before = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "retained pixel cache transitions failed to submit");
    AgcDriverWaitIdle_nid_postfix();
    const auto after = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    Require(after.lookups - before.lookups == 21 && after.absent - before.absent == 8 && after.inserts - before.inserts == 4 && after.hits - before.hits == 13, "color, execution or alpha-to-coverage transitions reused the wrong cached draw");
    if (std::getenv("APS5_VERIFY_DRAW_RECIPE") != nullptr) Require(after.verifyDecodes > before.verifyDecodes, "pixel cache transitions did not verify any cached decode");
    Require(after.verifyDecodeMismatches == before.verifyDecodeMismatches && after.facadeMismatches == before.facadeMismatches, "cached pixel transitions disagreed with fresh decoding");
}

void CpuPathIndirectDrawsAreCachedWhenAsked(bool libraries) {
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    const bool cached = std::getenv("APS5_DRAW_CACHE_INDIRECT") != nullptr;
    auto commands = Draws(8.0f, VertexCode.data(), 1, PixelCode.data(), 1, {});
    Append(commands, 0x76, {0x00c, std::bit_cast<std::uint32_t>(1.0f)});
    Append(commands, 0x79, {0x24a, 0});
    const auto records = reinterpret_cast<std::uintptr_t>(IndirectRecord.data());
    Append(commands, 0x11, {1, static_cast<std::uint32_t>(records), static_cast<std::uint32_t>(records >> 32u)});
    for (unsigned i = 0; i < 3; ++i) Append(commands, 0x24, {0, 0x8c, 0, 2});
    const auto notFolded = static_cast<std::size_t>(AgcDriver::Graphics::IndirectDrawPath::NotFolded);
    const auto pathsBefore = AgcDriver::Graphics::IndirectDrawCounts();
    const auto before = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    const auto after = Submit(commands);
    const auto pathsAfter = AgcDriver::Graphics::IndirectDrawCounts();
    if (std::getenv("APS5_PROFILE_DRAW") != nullptr) Require(pathsAfter[notFolded] - pathsBefore[notFolded] == 3, mode + ": " + std::to_string(pathsAfter[notFolded] - pathsBefore[notFolded]) + " of three indirect draws whose packet patches a user word the vertex program reads as a value took the CPU path for an unfolded SGPR, expected 3");
    Require(after.lookups - before.lookups == 3, mode + ": three CPU-path indirect draws made " + std::to_string(after.lookups - before.lookups) + " draw-cache lookups, expected 3");
    if (cached) {
        Require(after.inserts - before.inserts == 1 && after.absent - before.absent == 2, mode + ": with APS5_DRAW_CACHE_INDIRECT, three CPU-path indirect draws inserted " + std::to_string(after.inserts - before.inserts) + " draw entries with " + std::to_string(after.absent - before.absent) + " lookups absent, expected 1 and 2");
        Require(after.hits - before.hits == 1, mode + ": with APS5_DRAW_CACHE_INDIRECT, the third CPU-path indirect draw did not reuse the cached draw (" + std::to_string(after.hits - before.hits) + " hits)");
    } else {
        Require(after.inserts == before.inserts && after.hits == before.hits && after.absent - before.absent == 3, mode + ": without APS5_DRAW_CACHE_INDIRECT, CPU-path indirect draws inserted " + std::to_string(after.inserts - before.inserts) + " draw entries and hit " + std::to_string(after.hits - before.hits) + " times, expected none");
    }
}

void RecordsKeepThePacketsFragmentWord(bool libraries) {
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    const bool cached = std::getenv("APS5_DRAW_CACHE_INDIRECT") != nullptr;
    Registered(OffsetTriangleVertexHeader, OffsetTriangleVertexCode.data(), sizeof(OffsetTriangleVertexCode), 2);
    const auto submitRecords = [&](float fragmentWord) {
        ColorMemory.fill(std::byte{0});
        auto commands = Draws(32.0f, OffsetTriangleVertexCode.data(), 1, PixelCode.data(), 1, {});
        Append(commands, 0x76, {0x00c, std::bit_cast<std::uint32_t>(fragmentWord)});
        Append(commands, 0x79, {0x24a, 0});
        const auto records = reinterpret_cast<std::uintptr_t>(OffsetRecords.data());
        Append(commands, 0x11, {1, static_cast<std::uint32_t>(records), static_cast<std::uint32_t>(records >> 32u)});
        Append(commands, 0x2c, {0, 0x8c, 0, 0, 2, 0, 0, 16, 2});
        return Submit(commands);
    };
    const auto wrote = [&](unsigned expected, const std::string& what) {
        const auto brightest = BrightestColorByte();
        Require(brightest == expected, mode + ": " + what + " wrote " + std::to_string(brightest) + ", expected " + std::to_string(expected));
    };
    const auto before = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    submitRecords(0.75f);
    wrote(191, "two records of a CPU-path indirect draw, the first moving the triangle off screen through the vertex word the packet names, with the fragment stage reading its own word 0 of 0.75 as its color,");
    submitRecords(0.75f);
    const auto seen = submitRecords(0.75f);
    wrote(191, "the same two records on a cached draw");
    const auto changed = submitRecords(0.25f);
    wrote(64, "the same two records with the fragment stage's word changed to 0.25");
    submitRecords(0.25f);
    const auto second = submitRecords(0.25f);
    wrote(64, "the same two records on the cached draw with the fragment word 0.25");
    const auto after = submitRecords(0.75f);
    wrote(191, "the same two records back on the cached draw with the fragment word 0.75");
    Require(after.lookups - before.lookups == 7, mode + ": seven CPU-path indirect draws made " + std::to_string(after.lookups - before.lookups) + " draw-cache lookups, expected 7");
    if (cached) {
        Require(seen.inserts - before.inserts == 1 && seen.hits - before.hits == 1, mode + ": with APS5_DRAW_CACHE_INDIRECT, three CPU-path indirect draws inserted " + std::to_string(seen.inserts - before.inserts) + " entries and hit " + std::to_string(seen.hits - before.hits) + " times, expected 1 and 1");
        Require(changed.hits == seen.hits, mode + ": a CPU-path indirect draw whose fragment word changed reused the cached draw (" + std::to_string(changed.hits - seen.hits) + " hits)");
        Require(second.inserts - seen.inserts == 1 && second.hits - changed.hits == 1, mode + ": with APS5_DRAW_CACHE_INDIRECT, the draws with the fragment word 0.25 inserted " + std::to_string(second.inserts - seen.inserts) + " entries and hit " + std::to_string(second.hits - changed.hits) + " times, expected 1 and 1");
        Require(after.hits - second.hits == 1 && after.inserts == second.inserts, mode + ": with APS5_DRAW_CACHE_INDIRECT, the draw back with the fragment word 0.75 hit " + std::to_string(after.hits - second.hits) + " times and inserted " + std::to_string(after.inserts - second.inserts) + " entries, expected 1 and 0");
    } else {
        Require(after.inserts == before.inserts && after.hits == before.hits, mode + ": without APS5_DRAW_CACHE_INDIRECT, CPU-path indirect draws inserted " + std::to_string(after.inserts - before.inserts) + " draw entries and hit " + std::to_string(after.hits - before.hits) + " times, expected none");
    }
}

void RecordWordsReachTheShader(bool libraries) {
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    const bool cached = std::getenv("APS5_DRAW_CACHE_INDIRECT") != nullptr;
    const bool forwarded = std::getenv("APS5_DRAW_CACHE_PUSH_WORD") != nullptr;
    const bool verify = forwarded && std::getenv("APS5_VERIFY_DRAW_PUSH_WORD") != nullptr;
    Registered(OffsetTriangleVertexHeader, OffsetTriangleVertexCode.data(), sizeof(OffsetTriangleVertexCode), 2);
    ColorMemory.fill(std::byte{0});
    const auto submitRecord = [&](std::uint32_t firstVertex) {
        OffsetRecord[2] = firstVertex;
        auto commands = Draws(32.0f, OffsetTriangleVertexCode.data(), 1, PixelCode.data(), 1, {});
        Append(commands, 0x76, {0x00c, std::bit_cast<std::uint32_t>(1.0f)});
        Append(commands, 0x79, {0x24a, 0});
        const auto records = reinterpret_cast<std::uintptr_t>(OffsetRecord.data());
        Append(commands, 0x11, {1, static_cast<std::uint32_t>(records), static_cast<std::uint32_t>(records >> 32u)});
        Append(commands, 0x24, {0, 0x8c, 0, 2});
        return Submit(commands);
    };
    const auto before = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    submitRecord(4);
    Require(BrightestColorByte() == 0, mode + ": a CPU-path indirect draw whose record moves the triangle off screen through the patched user word wrote " + std::to_string(BrightestColorByte()) + ", expected nothing");
    submitRecord(4);
    const auto seen = submitRecord(4);
    Require(BrightestColorByte() == 0, mode + ": a repeated CPU-path indirect draw whose record moves the triangle off screen wrote " + std::to_string(BrightestColorByte()) + ", expected nothing: the record's word must reach the shader, not the packet's");
    const auto after = submitRecord(0);
    Require(BrightestColorByte() == 255, mode + ": a CPU-path indirect draw whose record puts the triangle on screen wrote " + std::to_string(BrightestColorByte()) + ", expected 255");
    Require(after.lookups - before.lookups == 4, mode + ": four CPU-path indirect draws made " + std::to_string(after.lookups - before.lookups) + " draw-cache lookups, expected 4");
    if (cached) Require(after.inserts - before.inserts == 1 && seen.hits - before.hits == 1 && after.hits - before.hits == 2, mode + ": with APS5_DRAW_CACHE_INDIRECT, four CPU-path indirect draws inserted " + std::to_string(after.inserts - before.inserts) + " entries and hit " + std::to_string(after.hits - before.hits) + " times, expected 1 and 2");
    else Require(after.inserts == before.inserts && after.hits == before.hits, mode + ": without APS5_DRAW_CACHE_INDIRECT, CPU-path indirect draws were cached");
    const auto wordsForwarded = after.recordWordsForwarded - before.recordWordsForwarded;
    const auto stagesRecompiled = after.recordStagesRecompiled - before.recordStagesRecompiled;
    const auto refused = after.recordWordsRefused - before.recordWordsRefused;
    const auto verified = after.recordWordsVerified - before.recordWordsVerified;
    if (forwarded) Require(wordsForwarded == 4 && stagesRecompiled == 0 && refused == 0, mode + ": with APS5_DRAW_CACHE_PUSH_WORD, four records forwarded " + std::to_string(wordsForwarded) + " patched words as push data, re-materialized " + std::to_string(stagesRecompiled) + " stages and refused " + std::to_string(refused) + " words, expected 4, 0 and 0");
    else Require(wordsForwarded == 0 && stagesRecompiled == 4, mode + ": without APS5_DRAW_CACHE_PUSH_WORD, four records forwarded " + std::to_string(wordsForwarded) + " words and re-materialized " + std::to_string(stagesRecompiled) + " stages, expected 0 and 4");
    Require(verified == (verify ? 4u : 0u), mode + ": " + std::to_string(verified) + " forwarded words were verified against a full build, expected " + std::to_string(verify ? 4 : 0));
}

void SightingsKeepCollidingKeys() {
    DrawKeySightings sightings(DrawKeySightings::Ways);
    Require(!sightings.SeenBefore(0) && sightings.SeenBefore(0), "draw key 0 was not admitted at its second sight");
    Require(!sightings.SeenBefore(11) && !sightings.SeenBefore(12), "an unseen draw key was admitted");
    Require(sightings.SeenBefore(11) && sightings.SeenBefore(12) && sightings.SeenBefore(11), "two keys in one bucket, drawn in turn, were not admitted at their second sight");
    Require(!sightings.SeenBefore(13) && !sightings.SeenBefore(14), "an unseen draw key was admitted");
    Require(!sightings.SeenBefore(0), "the oldest key of a full bucket was kept over the newest");
    Require(sightings.SeenBefore(12) && sightings.SeenBefore(13) && sightings.SeenBefore(14), "a full bucket lost a key newer than the one it replaced");
}

}

int main() {
    try {
        SightingsKeepCollidingKeys();
        bool libraries = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            libraries = device->GraphicsPipelineLibraries();
        }
        if (!libraries && std::getenv("APS5_NO_GPL") == nullptr) return VulkanTestSkipped;
        if (std::getenv("APS5_VERIFY_DRAW_ENTRIES") != nullptr) {
            RefreshedStageDataVerifies();
        } else {
            ThirdDrawHitsTheCache(libraries);
            UserDataDrawsReuseTheirShape(libraries);
            StageDataHitsRefreshTheirWords(libraries);
            RetainedPixelCacheTransitions();
            CpuPathIndirectDrawsAreCachedWhenAsked(libraries);
            RecordsKeepThePacketsFragmentWord(libraries);
            RecordWordsReachTheShader(libraries);
        }
        AgcDriverShutdown_nid_postfix();
        Require(AgcDriver::Graphics::PipelineLibraryWorkers() == 0, "the driver's shutdown left the pipeline library worker running");
        std::cout << "A repeated draw reused the cached draw " << (libraries ? "with" : "without") << " graphics pipeline libraries\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
