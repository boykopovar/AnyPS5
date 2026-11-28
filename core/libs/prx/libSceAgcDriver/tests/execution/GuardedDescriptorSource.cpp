#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "tests/TemporaryFile.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t Stride = 48;
constexpr std::uint32_t Records = 3;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Groups = 3;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32SInt = 21;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t SamplePc = 0x88u;
constexpr std::uint32_t StorePc = 0x100u;
constexpr std::uint32_t SelectorDword = 16u;
constexpr std::uint32_t OffsetDword = 17u;
constexpr std::uint32_t OtherTableDword = 20u;
constexpr std::uint32_t StorageDword = 24u;
constexpr std::uint32_t SecondStorageDword = 32u;
constexpr std::uint32_t EqualStorageDword = 40u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t SrtOffset = 16384;

alignas(256) constexpr std::array<std::uint32_t, 44> ColumnCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0xf4040500u, 0xfa000040u, 0xbf8cc07fu, 0xbf068014u, 0xbf850003u, 0xf4080100u, 0x2a000000u, 0xbf8cc07fu,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0xbf8cc07fu, 0x9312b011u, 0xf4240b02u, 0x24000008u, 0xbf8cc07fu,
    0xf40c0916u, 0xfa000020u, 0xbf8cc07fu, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u,
    0x7e0402ffu, 0x3e000000u, 0xf09c0f08u, 0x01090401u, 0xbf8c3f70u, 0x7e120202u, 0x34101286u, 0x4a101100u,
    0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 45> ColumnNoLaneCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0xf4040500u, 0xfa000040u, 0xbf8cc07fu, 0xbf068014u, 0xbf850003u, 0xf4080100u, 0x2a000000u, 0xbf8cc07fu,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0xbf8cc07fu, 0x9312b011u, 0xf4240b02u, 0x24000008u, 0xbf8cc07fu,
    0xf40c0916u, 0xfa000020u, 0xbf8cc07fu, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u,
    0x7e0402ffu, 0x3e000000u, 0xbefe0480u, 0xf09c0f08u, 0x01090401u, 0xbf8c3f70u, 0x7e120202u, 0x34101286u,
    0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 67> SpillCode{
    0xf40c1100u, 0xfa000060u, 0xf4040500u, 0xfa000040u, 0xbf8cc07fu, 0xd761001bu, 0x00011244u, 0xd761001bu,
    0x00011445u, 0xd761001bu, 0x00011646u, 0xd761001bu, 0x00011847u, 0xd761001bu, 0x00011a48u, 0xd761001bu,
    0x00011c49u, 0xd761001bu, 0x00011e4au, 0xd761001bu, 0x0001204bu, 0xbf068014u, 0xbf850025u, 0xbf068114u,
    0xbf850013u, 0xf40c1300u, 0x2a000000u, 0xbf8cc07fu, 0xd761001bu, 0x0001124cu, 0xd761001bu, 0x0001144du,
    0xd761001bu, 0x0001164eu, 0xd761001bu, 0x0001184fu, 0xd761001bu, 0x00011a50u, 0xd761001bu, 0x00011c51u,
    0xd761001bu, 0x00011e52u, 0xd761001bu, 0x00012053u, 0xd7600044u, 0x0001131bu, 0xd7600045u, 0x0001151bu,
    0xd7600046u, 0x0001171bu, 0xd7600047u, 0x0001191bu, 0xd7600048u, 0x00011b1bu, 0xd7600049u, 0x00011d1bu,
    0xd760004au, 0x00011f1bu, 0xd760004bu, 0x0001211bu, 0x4a0800ffu, 0x00001000u, 0x360200bfu, 0x7e040280u,
    0xf0201108u, 0x00110401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 68> SpillNoLaneCode{
    0xf40c1100u, 0xfa000060u, 0xf4040500u, 0xfa000040u, 0xbf8cc07fu, 0xd761001bu, 0x00011244u, 0xd761001bu,
    0x00011445u, 0xd761001bu, 0x00011646u, 0xd761001bu, 0x00011847u, 0xd761001bu, 0x00011a48u, 0xd761001bu,
    0x00011c49u, 0xd761001bu, 0x00011e4au, 0xd761001bu, 0x0001204bu, 0xbf068014u, 0xbf850025u, 0xbf068114u,
    0xbf850013u, 0xf40c1300u, 0x2a000000u, 0xbf8cc07fu, 0xd761001bu, 0x0001124cu, 0xd761001bu, 0x0001144du,
    0xd761001bu, 0x0001164eu, 0xd761001bu, 0x0001184fu, 0xd761001bu, 0x00011a50u, 0xd761001bu, 0x00011c51u,
    0xd761001bu, 0x00011e52u, 0xd761001bu, 0x00012053u, 0xd7600044u, 0x0001131bu, 0xd7600045u, 0x0001151bu,
    0xd7600046u, 0x0001171bu, 0xd7600047u, 0x0001191bu, 0xd7600048u, 0x00011b1bu, 0xd7600049u, 0x00011d1bu,
    0xd760004au, 0x00011f1bu, 0xd760004bu, 0x0001211bu, 0x4a0800ffu, 0x00001000u, 0x360200bfu, 0x7e040280u,
    0xbefe0480u, 0xf0201108u, 0x00110401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 67> TwoSrtArmsCode{
    0xf40c1100u, 0xfa000060u, 0xf4040500u, 0xfa000040u, 0xbf8cc07fu, 0xd761001bu, 0x00011244u, 0xd761001bu,
    0x00011445u, 0xd761001bu, 0x00011646u, 0xd761001bu, 0x00011847u, 0xd761001bu, 0x00011a48u, 0xd761001bu,
    0x00011c49u, 0xd761001bu, 0x00011e4au, 0xd761001bu, 0x0001204bu, 0xbf068014u, 0xbf850025u, 0xbf068114u,
    0xbf850013u, 0xf40c1300u, 0xfa000080u, 0xbf8cc07fu, 0xd761001bu, 0x0001124cu, 0xd761001bu, 0x0001144du,
    0xd761001bu, 0x0001164eu, 0xd761001bu, 0x0001184fu, 0xd761001bu, 0x00011a50u, 0xd761001bu, 0x00011c51u,
    0xd761001bu, 0x00011e52u, 0xd761001bu, 0x00012053u, 0xd7600044u, 0x0001131bu, 0xd7600045u, 0x0001151bu,
    0xd7600046u, 0x0001171bu, 0xd7600047u, 0x0001191bu, 0xd7600048u, 0x00011b1bu, 0xd7600049u, 0x00011d1bu,
    0xd760004au, 0x00011f1bu, 0xd760004bu, 0x0001211bu, 0x4a0800ffu, 0x00001000u, 0x360200bfu, 0x7e040280u,
    0xf0201108u, 0x00110401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 23> NoSrtArmCode{
    0xf4040500u, 0xfa000040u, 0xbf8cc07fu, 0xbec40380u, 0xbec50380u, 0xbec60380u, 0xbec70380u, 0xbec80380u,
    0xbec90380u, 0xbeca0380u, 0xbecb0380u, 0xbf068014u, 0xbf850003u, 0xf40c1100u, 0x2a000000u, 0xbf8cc07fu,
    0x4a0800ffu, 0x00001000u, 0x360200bfu, 0x7e040280u, 0xf0201108u, 0x00110401u, 0xbf810000u,
};

alignas(256) std::array<float, Width * Height> FloatTexels{};
alignas(256) std::array<std::uint32_t, Width * Height> UintTexels{};
alignas(256) std::array<std::int32_t, Width * Height> SintTexels{};
alignas(256) std::array<std::uint32_t, Records * Stride / 4> Table{};
alignas(256) std::array<std::uint32_t, Records * Stride / 4> OtherTable{};
alignas(256) std::array<std::uint32_t, Records * 16> Indirect{};
alignas(256) std::array<std::uint32_t, 64> Keys{};
alignas(256) std::array<std::uint32_t, Groups * Lanes * 4> Output{};
std::span<std::uint32_t> Srt;

constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = TemporaryFile::Open();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }
    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "guarded descriptor source: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> Image(const void* texels, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (Type2D << 28u)};
}

std::array<std::uint32_t, 8> StorageImage(const void* texels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Width - 1u) & 3u) << 30u), (Width - 1u) >> 2u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
}

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

bool Faulted(const std::string& log, std::uint32_t pc) {
    char instruction[32];
    std::snprintf(instruction, sizeof(instruction), "instruction=0x%x", pc);
    return log.find("BDA access failed") != std::string::npos && log.find("reason=8") != std::string::npos && log.find(instruction) != std::string::npos;
}

bool Clean(const std::string& log) {
    return log.find("BDA access failed") == std::string::npos && log.find("write-back failed") == std::string::npos;
}

std::string Recompiled(std::span<const std::uint32_t> code, std::uint32_t waveSize, bool groupIds, std::span<const ShaderRecompiler::MemoryRegion> memory, AgcDriver::VulkanDevice& device, ShaderRecompiler::RecompileResult* result) {
    const auto root = reinterpret_cast<std::uintptr_t>(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u)};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {groupIds, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    try {
        auto compiled = ShaderRecompiler::Recompile(request);
        if (result != nullptr) *result = std::move(compiled);
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

std::string Dispatch(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& result, std::span<const std::uint32_t> code, std::uint32_t groups) {
    StderrCapture capture;
    try {
        device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
    } catch (...) {
        static_cast<void>(capture.Finish());
        throw;
    }
    return capture.Finish();
}

void FillColumn(std::uint32_t selector, bool sameOther) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = y * Width + x;
            FloatTexels[index] = static_cast<float>(x) + 100.0f * static_cast<float>(y) + 0.25f;
            UintTexels[index] = 0x1000u + x + 0x100u * y;
            SintTexels[index] = -3 * static_cast<std::int32_t>(x + 1u) - 1000 * static_cast<std::int32_t>(y);
        }
    }
    const std::array<std::array<std::uint32_t, 4>, Records> images{Image(FloatTexels.data(), Format32Float), Image(UintTexels.data(), Format32UInt), Image(SintTexels.data(), Format32SInt)};
    Table = {};
    OtherTable = {};
    Indirect = {};
    Keys = {};
    for (std::uint32_t group = 0; group < Groups; ++group) Keys[group] = 2u - group;
    for (std::uint32_t record = 0; record < Records; ++record) {
        const std::uint32_t entry = 2u - record;
        std::copy(images[record].begin(), images[record].end(), Indirect.data() + entry * 16u + 8u);
        const auto pointer = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Indirect.data() + entry * 16u));
        Table[record * (Stride / 4u) + 2u] = static_cast<std::uint32_t>(pointer);
        Table[record * (Stride / 4u) + 3u] = static_cast<std::uint32_t>(pointer >> 32u);
    }
    std::fill(Srt.begin(), Srt.end(), 0u);
    const auto table = Buffer(Table.data(), static_cast<std::uint32_t>(sizeof(Table)));
    const auto other = sameOther ? table : Buffer(OtherTable.data(), static_cast<std::uint32_t>(sizeof(OtherTable)));
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(table.begin(), table.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4);
    std::copy(output.begin(), output.end(), Srt.begin() + 8);
    std::copy(PointClamp.begin(), PointClamp.end(), Srt.begin() + 12);
    Srt[SelectorDword] = selector;
    Srt[OffsetDword] = OtherTableDword * 4u;
    std::copy(other.begin(), other.end(), Srt.begin() + OtherTableDword);
    Output.fill(Untouched);
}

struct ColumnOutcome {
    std::string refusal;
    std::string log;
};

template <std::size_t CodeWords>
ColumnOutcome RunColumn(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, std::uint32_t waveSize, std::uint32_t selector, bool sameOther) {
    FillColumn(selector, sameOther);
    const std::span<const std::uint32_t> span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 6> memory{{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span<const std::uint32_t>(Srt))},
        {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
        {reinterpret_cast<std::uintptr_t>(OtherTable.data()), std::as_bytes(std::span(OtherTable))},
        {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
        {reinterpret_cast<std::uintptr_t>(Indirect.data()), std::as_bytes(std::span(Indirect))},
    }};
    ShaderRecompiler::RecompileResult result;
    ColumnOutcome outcome;
    outcome.refusal = Recompiled(span, waveSize, true, memory, device, &result);
    if (outcome.refusal.empty()) outcome.log = Dispatch(device, result, span, Groups);
    return outcome;
}

std::uint32_t ColumnTexel(std::uint32_t group, std::uint32_t x) {
    const auto record = 2u - group;
    if (record == 0u) return std::bit_cast<std::uint32_t>(FloatTexels[x]);
    if (record == 1u) return UintTexels[x];
    return static_cast<std::uint32_t>(SintTexels[x]);
}

void RequireColumnTexels(const std::string& what) {
    for (std::uint32_t group = 0; group < Groups; ++group) {
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            const auto actual = Output[(group * Lanes + lane) * 4u];
            Require(actual == ColumnTexel(group, lane % Width), what + ": group " + std::to_string(group) + " lane " + std::to_string(lane) + " is " + Hex(actual) + ", expected " + Hex(ColumnTexel(group, lane % Width)));
        }
    }
}

void RunColumnTests(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    const std::string wave = " (wave" + std::to_string(waveSize) + ")";
    const auto planned = RunColumn(device, ColumnCode, waveSize, 0u, false);
    Require(planned.refusal.empty(), "a table record V# merged from an SRT read and another read was refused" + wave + ": " + planned.refusal);
    Require(Clean(planned.log), "the SRT arm of a merged table record V# faulted" + wave + ":\n" + planned.log);
    RequireColumnTexels("texels through the SRT arm of a merged table record V#" + wave);

    const auto equal = RunColumn(device, ColumnCode, waveSize, 1u, true);
    Require(Clean(equal.log), "the other arm of a merged table record V# faulted although it holds the planned words" + wave + ":\n" + equal.log);
    RequireColumnTexels("texels through an equal other arm of a merged table record V#" + wave);

    const auto other = RunColumn(device, ColumnCode, waveSize, 1u, false);
    Require(Faulted(other.log, SamplePc), "the other arm of a merged table record V# did not fault at the sample" + wave + ":\n" + other.log);

    const auto noLanes = RunColumn(device, ColumnNoLaneCode, waveSize, 1u, false);
    Require(noLanes.refusal.empty(), "a merged table record V# with no lane enabled was refused" + wave + ": " + noLanes.refusal);
    Require(Clean(noLanes.log), "the other arm of a merged table record V# faulted with no lane enabled" + wave + ":\n" + noLanes.log);
    Require(std::all_of(Output.begin(), Output.end(), [](std::uint32_t word) { return word == Untouched; }), "a lane stored with no lane enabled" + wave);
}

AgcDriver::Graphics::TileMipLayout Mip() {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format32UInt, Width, 1u, 1u).at(0);
}

struct SpillOutcome {
    std::string refusal;
    std::string log;
};

template <std::size_t CodeWords>
SpillOutcome RunSpill(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const std::array<std::uint32_t, CodeWords>& code, std::uint32_t waveSize, std::uint32_t selector, std::uint32_t otherDword) {
    std::memset(texels, 0xee, 4096u);
    std::fill(Srt.begin(), Srt.end(), 0u);
    const auto storage = StorageImage(texels + Mip().tiledOffset);
    const auto second = StorageImage(texels + 2048u);
    std::copy(storage.begin(), storage.end(), Srt.begin() + StorageDword);
    std::copy(second.begin(), second.end(), Srt.begin() + SecondStorageDword);
    std::copy(storage.begin(), storage.end(), Srt.begin() + EqualStorageDword);
    Srt[SelectorDword] = selector;
    Srt[OffsetDword] = otherDword * 4u;
    const std::span<const std::uint32_t> span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span<const std::uint32_t>(Srt))},
    }};
    ShaderRecompiler::RecompileResult result;
    SpillOutcome outcome;
    outcome.refusal = Recompiled(span, waveSize, false, memory, device, &result);
    if (!outcome.refusal.empty()) return outcome;
    outcome.log = Dispatch(device, result, span, 1u);
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), 4096u, nullptr, "test");
    device.WaitIdle();
    return outcome;
}

void RequireStored(const std::uint8_t* texels, bool written, const std::string& what) {
    const auto offset = Mip().tiledOffset;
    for (std::uint32_t x = 0; x < Width; ++x) {
        std::uint32_t actual = 0;
        std::memcpy(&actual, texels + offset + x * 4u, 4u);
        const auto wanted = written ? 0x1000u + x : 0xeeeeeeeeu;
        Require(actual == wanted, what + ": texel " + std::to_string(x) + " is " + Hex(actual) + ", expected " + Hex(wanted));
    }
}

void RunSpillTests(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t waveSize) {
    const std::string wave = " (wave" + std::to_string(waveSize) + ")";
    const auto planned = RunSpill(device, texels, SpillCode, waveSize, 0u, SecondStorageDword);
    Require(planned.refusal.empty(), "a T# merged from an SRT read and a lane-spill restore was refused" + wave + ": " + planned.refusal);
    Require(Clean(planned.log), "the SRT arm of a lane-spilled T# faulted" + wave + ":\n" + planned.log);
    RequireStored(texels, true, "image_store through the SRT arm of a lane-spilled T#" + wave);

    const auto restored = RunSpill(device, texels, SpillCode, waveSize, 1u, SecondStorageDword);
    Require(Clean(restored.log), "a lane-spill restore of the SRT T# faulted" + wave + ":\n" + restored.log);
    RequireStored(texels, true, "image_store through a lane-spill restore of the SRT T#" + wave);

    const auto equal = RunSpill(device, texels, SpillCode, waveSize, 2u, EqualStorageDword);
    Require(Clean(equal.log), "a lane-spill restore of another read that holds the planned T# faulted" + wave + ":\n" + equal.log);
    RequireStored(texels, true, "image_store through a lane-spill restore of an equal T#" + wave);

    const auto other = RunSpill(device, texels, SpillCode, waveSize, 2u, SecondStorageDword);
    Require(Faulted(other.log, StorePc), "a lane-spill restore of a different T# did not fault at the store" + wave + ":\n" + other.log);

    const auto noLanes = RunSpill(device, texels, SpillNoLaneCode, waveSize, 2u, SecondStorageDword);
    Require(noLanes.refusal.empty(), "a lane-spilled T# with no lane enabled was refused" + wave + ": " + noLanes.refusal);
    Require(Clean(noLanes.log), "a lane-spill restore of a different T# faulted with no lane enabled" + wave + ":\n" + noLanes.log);
    RequireStored(texels, false, "image_store with no lane enabled" + wave);

    const auto twoArms = RunSpill(device, texels, TwoSrtArmsCode, waveSize, 2u, SecondStorageDword);
    Require(twoArms.refusal.find("GetImageResource dword 0 is not a valid runtime value") != std::string::npos, "a T# merged from two different SRT reads was not refused" + wave + ": " + twoArms.refusal);
    const auto noArm = RunSpill(device, texels, NoSrtArmCode, waveSize, 1u, StorageDword);
    Require(noArm.refusal.find("GetImageResource dword 0 is not a valid runtime value") != std::string::npos, "a T# merged from immediates and a register-offset read was not refused" + wave + ": " + noArm.refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        Srt = std::span<std::uint32_t>(reinterpret_cast<std::uint32_t*>(block.Data() + SrtOffset), 64);
        for (const std::uint32_t waveSize : {32u, 64u}) {
            RunColumnTests(*device, waveSize);
            RunSpillTests(*device, block.Data(), waveSize);
        }
        std::puts("guarded descriptor source tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
