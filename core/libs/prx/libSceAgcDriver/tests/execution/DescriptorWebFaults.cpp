#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t UndefinedFormat = 300;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ValidSelector = 0;
constexpr std::uint32_t FoldedSelector = 1;
constexpr std::uint32_t SecondSelector = 2;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t FirstTexel = 0xffu;
constexpr std::uint32_t SecondTexel = 0x80u;

alignas(256) std::array<std::uint8_t, 256> FirstTexture{};
alignas(256) std::array<std::uint8_t, 256> SecondTexture{};
alignas(256) std::array<std::uint32_t, 4> Output{};
alignas(256) std::array<std::uint32_t, 64> Root{};

constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TableStride = 48;
constexpr std::uint32_t TableTexel = 0x1234u;

alignas(256) std::array<std::uint32_t, 64 * 4> TableTexels{};
alignas(256) std::array<std::uint32_t, TableStride / 4> Table{};
alignas(256) std::array<std::uint32_t, 8> Keys{};

alignas(256) constexpr std::array<std::uint32_t, 24> WebCode{
    0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf40c0200u, 0xfa000000u,
    0xbf8cc07fu, 0xbf068010u, 0xbf850007u, 0xbf068110u, 0xbf850004u, 0xf40c0200u, 0xfa000060u, 0xbf8cc07fu,
    0xbf820001u, 0xbe8e1f00u, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 25> NoLaneWebCode{
    0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf40c0200u, 0xfa000000u,
    0xbf8cc07fu, 0xbf068010u, 0xbf850007u, 0xbf068110u, 0xbf850004u, 0xf40c0200u, 0xfa000060u, 0xbf8cc07fu,
    0xbf820001u, 0xbe8e1f00u, 0xbefe0380u, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 50> TableWebCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x7e200500u, 0x90108510u,
    0x8f118202u, 0x81101110u, 0x8f108210u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u,
    0x360200bfu, 0x7e040280u, 0xf0008f08u, 0x00090401u, 0x7e120202u, 0x34101287u, 0x4a101100u, 0x34101084u,
    0xd8340000u, 0x00000409u,
    0xf4000400u, 0xfa0000d0u, 0xf4080500u, 0xfa0000a0u, 0xf4080600u, 0xfa0000c0u, 0xf40c0200u, 0xfa000080u,
    0xbf8cc07fu, 0xbf068010u, 0xbf850007u, 0xbf068110u, 0xbf850004u, 0xf40c0200u, 0xfa0000e0u, 0xbf8cc07fu,
    0xbf820001u, 0xbe8e1f00u, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xd8340000u, 0x00000009u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 51> TableNoLaneWebCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x7e200500u, 0x90108510u,
    0x8f118202u, 0x81101110u, 0x8f108210u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u,
    0x360200bfu, 0x7e040280u, 0xf0008f08u, 0x00090401u, 0x7e120202u, 0x34101287u, 0x4a101100u, 0x34101084u,
    0xd8340000u, 0x00000409u,
    0xf4000400u, 0xfa0000d0u, 0xf4080500u, 0xfa0000a0u, 0xf4080600u, 0xfa0000c0u, 0xf40c0200u, 0xfa000080u,
    0xbf8cc07fu, 0xbf068010u, 0xbf850007u, 0xbf068110u, 0xbf850004u, 0xf40c0200u, 0xfa0000e0u, 0xbf8cc07fu,
    0xbf820001u, 0xbe8e1f00u, 0xbefe0380u, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xd8340000u, 0x00000009u,
    0xbf810000u,
};

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
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

struct Outcome {
    std::string log;
    std::uint32_t word = 0;
    bool imageTable = false;
    bool addressTable = false;
};

std::array<std::uint32_t, 8> ImageDescriptor(const std::array<std::uint8_t, 256>& texture, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.data()));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* base, std::uint32_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x30027facu};
}

void FillWeb(std::uint32_t base, std::uint32_t second, std::uint32_t selector) {
    const auto first = ImageDescriptor(FirstTexture, Format8888UNorm);
    const auto other = ImageDescriptor(SecondTexture, second);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    FirstTexture.fill(static_cast<std::uint8_t>(FirstTexel));
    SecondTexture.fill(static_cast<std::uint8_t>(SecondTexel));
    Output.fill(Untouched);
    std::copy(first.begin(), first.end(), Root.begin() + base);
    std::copy(output.begin(), output.end(), Root.begin() + base + 16);
    Root[base + 20] = selector;
    std::copy(other.begin(), other.end(), Root.begin() + base + 24);
}

void FillTable() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(TableTexels.data()));
    TableTexels.fill(TableTexel);
    Table = {};
    Table[4] = static_cast<std::uint32_t>(address >> 8u);
    Table[5] = static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (3u << 30u);
    Table[6] = 15u | (3u << 14u);
    Table[7] = 0xfacu | (Type2D << 28u);
    Keys = {};
    const auto table = BufferDescriptor(Table.data(), static_cast<std::uint32_t>(sizeof(Table)));
    const auto keys = BufferDescriptor(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    std::copy(table.begin(), table.end(), Root.begin());
    std::copy(keys.begin(), keys.end(), Root.begin() + 4);
}

template <std::size_t CodeWords>
Outcome Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, std::uint32_t second, std::uint32_t selector, bool table = false) {
    Root = {};
    if (table) FillTable();
    FillWeb(table ? 32u : 0u, second, selector);
    const auto root = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Root.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u)};
    const std::span<const std::uint32_t> span(code);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, table ? 64u : 0u, {table, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(span.data()), span, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, {}},
        device.Target(),
        {0, 0, 0, 128}
    };
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const auto regions = memory.Regions();
    request.context.memory = regions;
    const auto result = ShaderRecompiler::Recompile(request, *capture);
    Outcome outcome;
    for (const auto& binding : result->bindings) {
        outcome.imageTable = outcome.imageTable || binding.role == ShaderRecompiler::DescriptorRole::ImageTableMap;
        outcome.addressTable = outcome.addressTable || binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable;
    }
    StderrCapture log;
    try {
        device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(span.data()));
        device.WaitIdle();
    } catch (...) {
        static_cast<void>(log.Finish());
        throw;
    }
    outcome.log = log.Finish();
    outcome.word = Output[0];
    return outcome;
}

bool Reads(std::uint32_t word, std::uint32_t texel) {
    return std::fabs(std::bit_cast<float>(word) - static_cast<float>(texel) / 255.0f) < 0.001f;
}

bool Faulted(const std::string& log, std::uint32_t pc) {
    char instruction[32];
    std::snprintf(instruction, sizeof(instruction), "instruction=0x%x", pc);
    return log.find("BDA access failed") != std::string::npos && log.find("reason=8") != std::string::npos && log.find(instruction) != std::string::npos;
}

bool Clean(const std::string& log) {
    return log.find("BDA access failed") == std::string::npos && log.find("write-back failed") == std::string::npos;
}

void RunTests(AgcDriver::VulkanDevice& device) {
    constexpr std::uint32_t SamplePc = 0x48u;
    const auto valid = Run(device, WebCode, Format8888UNorm, ValidSelector);
    Require(Clean(valid.log) && Reads(valid.word, FirstTexel), "descriptor web faults: a lane selecting the first arm faulted or did not read its texture:\n" + valid.log);
    const auto other = Run(device, WebCode, Format8888UNorm, SecondSelector);
    Require(Clean(other.log) && Reads(other.word, SecondTexel), "descriptor web faults: a lane selecting the second arm faulted or did not read its texture:\n" + other.log);
    const auto folded = Run(device, WebCode, Format8888UNorm, FoldedSelector);
    Require(Faulted(folded.log, SamplePc), "descriptor web faults: a lane on the folded path did not fault at the read:\n" + folded.log);
    const auto foldedNoLanes = Run(device, NoLaneWebCode, Format8888UNorm, FoldedSelector);
    Require(Clean(foldedNoLanes.log) && foldedNoLanes.word == Untouched, "descriptor web faults: the folded path faulted or wrote with no lane enabled:\n" + foldedNoLanes.log);

    const auto unselected = Run(device, WebCode, UndefinedFormat, ValidSelector);
    Require(Clean(unselected.log) && Reads(unselected.word, FirstTexel), "descriptor web faults: an undefined arm that no lane selects faulted or changed the selected arm's read:\n" + unselected.log);
    const auto selected = Run(device, WebCode, UndefinedFormat, SecondSelector);
    Require(Faulted(selected.log, SamplePc), "descriptor web faults: a lane selecting an undefined arm did not fault at the read:\n" + selected.log);
    const auto selectedNoLanes = Run(device, NoLaneWebCode, UndefinedFormat, SecondSelector);
    Require(Clean(selectedNoLanes.log) && selectedNoLanes.word == Untouched, "descriptor web faults: an undefined arm faulted or wrote with no lane enabled:\n" + selectedNoLanes.log);

    constexpr std::uint32_t TableSamplePc = 0xb0u;
    const auto tableValid = Run(device, TableWebCode, Format8888UNorm, ValidSelector, true);
    Require(tableValid.imageTable && !tableValid.addressTable, "descriptor web faults: the mixed shader does not bind an image table without a BDA address table");
    Require(Clean(tableValid.log) && tableValid.word == Untouched, "descriptor web faults: with an image table and no guest write, a lane selecting the first arm faulted:\n" + tableValid.log);
    const auto tableFolded = Run(device, TableWebCode, Format8888UNorm, FoldedSelector, true);
    Require(Faulted(tableFolded.log, TableSamplePc), "descriptor web faults: with an image table and no poisoned record, a lane on the folded path did not fault at the read:\n" + tableFolded.log);
    const auto tableUnselected = Run(device, TableWebCode, UndefinedFormat, ValidSelector, true);
    Require(Clean(tableUnselected.log) && tableUnselected.word == Untouched, "descriptor web faults: with an image table and no guest write, an undefined arm that no lane selects faulted:\n" + tableUnselected.log);
    const auto tableSelected = Run(device, TableWebCode, UndefinedFormat, SecondSelector, true);
    Require(Faulted(tableSelected.log, TableSamplePc), "descriptor web faults: with an image table and no poisoned record, a lane selecting an undefined arm did not fault at the read:\n" + tableSelected.log);
    const auto tableNoLanes = Run(device, TableNoLaneWebCode, UndefinedFormat, SecondSelector, true);
    Require(Clean(tableNoLanes.log) && tableNoLanes.word == Untouched, "descriptor web faults: with an image table, an undefined arm faulted with no lane enabled:\n" + tableNoLanes.log);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunTests(*device);
        std::puts("descriptor web fault tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
