#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "Recompiler.hpp"
#include "BdaAbi.hpp"
#include "ImageTableAbi.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
namespace Abi = ShaderRecompiler::ImageTableAbi;

constexpr std::uint32_t PageBytes = 4096;
constexpr std::uint32_t ReservedBytes = 65536;
constexpr std::uint32_t Stride = 48;
constexpr std::uint32_t Records = 4;
constexpr std::uint32_t TableBytes = Records * Stride;
constexpr std::uint32_t TableOffset = PageBytes - 2u * Stride;
constexpr std::uint32_t ImageOffset = 16;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;

struct alignas(256) Texture {
    std::array<std::uint8_t, 256> bytes{};
};

Texture Texels;
alignas(256) std::array<std::uint32_t, 16> Srt{};
alignas(256) std::array<std::uint32_t, 4> Output{};

const std::vector<std::uint32_t> TableCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x9310b010u,
    0xf4280502u, 0x20000010u, 0xf09c8f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};

std::uint64_t Address(const void* pointer) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::array<std::uint32_t, 4> Image(const void* base) {
    const auto address = Address(base);
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u)};
}

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t bytes) {
    const auto address = Address(base);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::byte* Reserve() {
#ifdef _WIN32
    auto* base = static_cast<std::byte*>(VirtualAlloc(nullptr, ReservedBytes, MEM_RESERVE, PAGE_NOACCESS));
#else
    auto* mapped = mmap(nullptr, ReservedBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    auto* base = mapped == MAP_FAILED ? nullptr : static_cast<std::byte*>(mapped);
#endif
    Require(base != nullptr, "image table cache: cannot reserve an inaccessible range");
    return base;
}

void Commit(std::byte* page) {
#ifdef _WIN32
    const bool committed = VirtualAlloc(page, PageBytes, MEM_COMMIT, PAGE_READWRITE) == page;
#else
    const bool committed = mprotect(page, PageBytes, PROT_READ | PROT_WRITE) == 0;
#endif
    Require(committed, "image table cache: cannot commit a page");
}

void Release(std::byte* base) {
#ifdef _WIN32
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, ReservedBytes);
#endif
}

struct Captured {
    std::unique_ptr<AgcDriver::ShaderMemory> memory;
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
    std::vector<ShaderRecompiler::MemoryRegion> regions;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> compiled;
};

Captured Capture(std::byte* table) {
    using namespace ShaderRecompiler;
    static const std::array<std::uint32_t, 3> capabilities{29u, 5302u, 1u};
    static const std::array<std::string_view, 1> extensions{"SPV_EXT_descriptor_indexing"};
    const auto srtAddress = Address(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x40000u + TableCode.size() * 4u, TableCode, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    const auto tableV = Buffer(table, TableBytes);
    const auto outputV = Buffer(Output.data(), sizeof(Output));
    Srt = {};
    std::copy(tableV.begin(), tableV.end(), Srt.begin());
    std::copy(outputV.begin(), outputV.end(), Srt.begin() + 12);

    Captured result;
    result.memory = std::make_unique<AgcDriver::ShaderMemory>(std::span<const MemoryRegion>{});
    result.capture = result.memory->Capture(request);
    result.regions = result.memory->TakeRecentRegions();
    request.context.memory = result.memory->Regions();
    result.compiled = Recompile(request, *result.capture);
    return result;
}

std::uint32_t Code(const Captured& captured, std::uint32_t record) {
    const auto& bindings = captured.compiled->bindings;
    const auto binding = std::find_if(bindings.begin(), bindings.end(), [](const ShaderRecompiler::DescriptorBinding& candidate) { return candidate.role == ShaderRecompiler::DescriptorRole::ImageTableMap; });
    Require(binding != bindings.end(), "image table cache: the result has no image table map");
    const auto& map = binding->guestDescriptor;
    Require(record < map.at(Abi::TableHeader(0) + Abi::TableKeyCount), "image table cache: the record is outside the map");
    return map.at(map.at(Abi::TableHeader(0) + Abi::TableMapStart) + record);
}

void SetRecord(std::byte* table, std::uint32_t record) {
    const auto words = Image(Texels.bytes.data());
    std::memcpy(table + record * Stride + ImageOffset, words.data(), sizeof(words));
}

void RunTests() {
    using AgcDriver::DriverDetail::CacheableResult;
    auto* reserved = Reserve();
    Commit(reserved);
    auto* table = reserved + TableOffset;
    SetRecord(table, 0);
    SetRecord(table, 1);

    const auto unmapped = Capture(table);
    const auto& poison = unmapped.compiled->imageTablePoison;
    const auto unmappedCode = Code(unmapped, 2);
    Require((unmappedCode & Abi::PoisonFlag) != 0u && poison.at(unmappedCode & ~Abi::PoisonFlag).reason == static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped) && Code(unmapped, 3) == unmappedCode, "image table cache: records on an uncommitted page are not Unmapped poison");
    Require((Code(unmapped, 0) & Abi::PoisonFlag) == 0u && Code(unmapped, 0) == Code(unmapped, 1), "image table cache: a record on the committed page is poison");
    Require(!CacheableResult(*unmapped.compiled), "image table cache: a result with Unmapped entries is cacheable");

    Commit(reserved + PageBytes);
    SetRecord(table, 2);
    SetRecord(table, 3);
    for (const auto& region : unmapped.regions) {
        Require(std::memcmp(reinterpret_cast<const void*>(region.guestAddress), region.bytes.data(), region.bytes.size()) == 0, "image table cache: a word the Unmapped capture read changed");
    }

    const auto mapped = Capture(table);
    Require(CacheableResult(*mapped.compiled), "image table cache: a result without Unmapped entries is not cacheable");
    Require(Code(mapped, 2) == Code(mapped, 0) && (Code(mapped, 2) & Abi::PoisonFlag) == 0u, "image table cache: a record on a newly committed page is not an image");
    Require(mapped.compiled->variantId == unmapped.compiled->variantId && mapped.compiled->PipelineVariantId() == unmapped.compiled->PipelineVariantId(), "image table cache: the committed records changed the compiled module");
    const auto& ranges = mapped.compiled->imageTableRanges;
    Require(std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.first == Address(table) && range.second == TableBytes; }), "image table cache: the result does not name the table range");
    Release(reserved);
}

}

int main() {
    try {
        RunTests();
        std::puts("image table cache tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
