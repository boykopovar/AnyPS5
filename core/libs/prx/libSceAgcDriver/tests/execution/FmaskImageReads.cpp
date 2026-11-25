#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 64;
constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Sentinel = 0xdeadbeef;
alignas(4096) std::array<std::byte, 4 * 65536> TexelStorage{};
const auto Texels = std::span(TexelStorage).subspan((0u - reinterpret_cast<std::uintptr_t>(TexelStorage.data())) & 0xffffu, 3 * 65536);
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4 + 4> Output{};
alignas(256) constexpr std::array<std::uint32_t, 10> Code{0x34020084u, 0xe0381000u, 0x80000201u, 0xbf8c3f70u, 0xf0001f08u, 0x00020602u, 0xbf8c3f70u, 0xe0781000u, 0x80010601u, 0xbf810000u};

std::array<std::uint32_t, 4> Buffer(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> Image(bool layered, std::uint32_t pipeXor, std::uint32_t swizzle = 4u, std::uint32_t format = 166u) {
    const auto address = reinterpret_cast<std::uintptr_t>(Texels.data()) + pipeXor;
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (format << 20u) | (3u << 30u), 31u | ((Height - 1u) << 14u), swizzle | (24u << 20u) | ((layered ? 13u : 9u) << 28u), layered ? 2u | (1u << 16u) : 0u, 0u, 0u, 0u};
}

std::uint32_t Mapping(std::uint32_t x, std::uint32_t y, std::uint32_t layer, std::uint32_t round) {
    if (round == 0) return 0u;
    if (round == 1) return 0x11111111u;
    if (round == 2) return 0x11111100u;
    std::uint32_t word = 0;
    for (std::uint32_t sample = 0; sample < 8; ++sample) word |= ((x + 3u * y + 5u * layer + sample + round) & 7u) << (sample * 4u);
    return word;
}

std::uint64_t EquationOffset(const TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y, std::uint32_t layer) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < 16; ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12u) & (mask & 0xfff000u)) ^ ((layer << 24u) & (mask & 0xff000000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

template<typename TValue>
void FillWith(bool layered, std::uint32_t pipeXor, std::uint32_t surfaceFormat, std::uint32_t bytes, TValue valueAt) {
    auto words = Image(layered, pipeXor);
    words[1] = (words[1] & ~(0x1ffu << 20u)) | (surfaceFormat << 20u);
    const auto resource = DecodeTextureResource(words);
    const auto geometry = DescribeSurface(resource);
    Require(geometry.guestBytes <= Texels.size(), "FMASK fixture exceeds its allocation");
    const auto& mip = geometry.mips.at(0);
    const auto block = ThinBlockLayout(resource.tileMode, bytes);
    const auto* equation = FindTextureSwizzleEquation(24u, bytes);
    Require(equation != nullptr, "FMASK fixture has no Z swizzle equation");
    std::ranges::fill(Texels, std::byte{0});
    for (std::uint32_t layer = 0; layer < geometry.layers; ++layer) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                auto offset = geometry.GuestLayerOffset(layer) + mip.tiledOffset;
                if (mip.tail) offset += EquationOffset(*equation, x + mip.tailX, y + mip.tailY, layer) ^ pipeXor;
                else offset += (static_cast<std::uint64_t>(y / block[2]) * mip.blocksPerRow + x / block[1]) * block[0] + (EquationOffset(*equation, x, y, layer) ^ pipeXor);
                const auto value = valueAt(x, y, layer);
                Require(bytes <= sizeof(value), "FMASK fixture value is narrower than its storage width");
                Require(offset + bytes <= Texels.size(), "FMASK fixture addresses outside its allocation");
                std::memcpy(Texels.data() + offset, &value, bytes);
            }
        }
    }
}

void Fill(bool layered, std::uint32_t pipeXor, std::uint32_t round) {
    FillWith(layered, pipeXor, 20u, 4u, [round](std::uint32_t x, std::uint32_t y, std::uint32_t layer) { return Mapping(x, y, layer, round); });
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> image, bool registration) {
    std::vector<std::uint32_t> userData(16u);
    const auto input = Buffer(Input.data(), sizeof(Input));
    const auto output = Buffer(Output.data(), Threads * 16u);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4u);
    std::copy(image.begin(), image.end(), userData.begin() + 8u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    auto target = device.Target();
    target.registrationModules = registration;
    ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}}, {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory}, target, {0u, 0u, 0u, 128u}};
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, bool layered, bool registration, std::uint32_t pipeXor) {
    auto code = Code;
    if (layered) code[4] = 0xf0001f28u;
    ShaderRecompiler::CompiledShaderArtifact first;
    for (std::uint32_t round = 0; round < 5; ++round) {
        Fill(layered, pipeXor, round);
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            Input[lane * 4u] = (lane * 37u + 11u) % Width;
            Input[lane * 4u + 1u] = (lane * 23u + 5u) % Height;
            Input[lane * 4u + 2u] = layered ? lane & 1u : 0u;
            Input[lane * 4u + 3u] = 0u;
        }
        Output.fill(Sentinel);
        const auto swizzle = round == 4u ? 4u | (1u << 3u) | (4u << 6u) : 4u;
        const auto shader = Compile(device, code, Image(layered, pipeXor, swizzle), registration);
        if (first.variantId == 0u) first = shader;
        else Require(shader.cacheHit && shader.variantId == first.variantId, "FMASK data or channel selectors changed the compiled shader artifact");
        device.Dispatch(shader, 1u, 1u, 1u);
        device.WaitIdle();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto value = Mapping(Input[lane * 4u], Input[lane * 4u + 1u], layered ? Input[lane * 4u + 2u] + 1u : 0u, round);
            const std::array expected{value, round == 4u ? 1u : 0u, round == 4u ? value : 0u, 0u};
            for (std::uint32_t component = 0; component < 4u; ++component) Require(Output[lane * 4u + component] == expected[component], "FMASK load did not read its mapping: round " + std::to_string(round) + ", lane " + std::to_string(lane) + ", component " + std::to_string(component));
        }
        Require(std::all_of(Output.begin() + Threads * 4u, Output.end(), [](auto value) { return value == Sentinel; }), "FMASK load wrote outside its output");
        if (round == 3u) {
            auto compressed = Image(layered, pipeXor);
            compressed[6] = 1u << 21u;
            Output.fill(Sentinel);
            bool rejected = false;
            try {
                const auto invalid = Compile(device, code, compressed, registration);
                device.Dispatch(invalid, 1u, 1u, 1u);
                device.WaitIdle();
            } catch (const std::runtime_error& error) {
                Require(std::string(error.what()).find("FMASK CMASK compression is not implemented") != std::string::npos, "compressed FMASK failed for another reason: " + std::string(error.what()));
                rejected = true;
            }
            Require(rejected, "a cached FMASK shader accepted compression without decoding CMASK");
            Require(std::all_of(Output.begin(), Output.end(), [](auto value) { return value == Sentinel; }), "the rejected compressed FMASK load wrote output");
        }
    }
}

struct NarrowFmask {
    std::uint32_t format;
    std::uint32_t surfaceFormat;
    std::uint32_t bytes;
    std::uint32_t samples;
    std::uint32_t fragments;
    std::uint32_t sampleBits;
    std::uint64_t identity;
    std::uint64_t expandedIdentity;
};

struct FmaskWord {
    std::uint32_t low;
    std::uint32_t high;
};

constexpr std::array<NarrowFmask, 12> NarrowFormats{{
    {156u, 5u, 1u, 2u, 1u, 1u, 0x02u, 0x80808080u},
    {157u, 5u, 1u, 4u, 1u, 1u, 0x0eu, 0x88808880u},
    {158u, 5u, 1u, 8u, 1u, 1u, 0xfeu, 0x88888880u},
    {159u, 5u, 1u, 2u, 2u, 1u, 0x02u, 0x10u},
    {160u, 5u, 1u, 4u, 2u, 2u, 0xa4u, 0x88108810u},
    {161u, 5u, 1u, 4u, 4u, 2u, 0xe4u, 0x32103210u},
    {163u, 11u, 2u, 8u, 2u, 2u, 0xaaa4u, 0x88888810u},
    {165u, 20u, 4u, 8u, 4u, 4u, 0x44443210u, 0x88883210u},
    {162u, 11u, 2u, 16u, 1u, 1u, 0xfffeull, 0x8888888888888880ull},
    {164u, 20u, 4u, 16u, 2u, 2u, 0xaaaaaaa4ull, 0x8888888888888810ull},
    {167u, 62u, 8u, 16u, 4u, 4u, 0x4444444444443210ull, 0x8888888888883210ull},
    {168u, 62u, 8u, 16u, 8u, 4u, 0x8888888876543210ull, 0x8888888876543210ull},
}};

FmaskWord Stored(const NarrowFmask& fmask, std::uint32_t x, std::uint32_t y, std::uint32_t layer, std::uint32_t round) {
    if (round == 0u) return {static_cast<std::uint32_t>(fmask.identity), static_cast<std::uint32_t>(fmask.identity >> 32u)};
    const auto value = (x * 0x9e3779b1u) ^ (y * 0x85ebca77u) ^ (layer * 0xc2b2ae3du) ^ (fmask.format * 0x27d4eb2fu);
    if (fmask.bytes == 8u) return {value, (value * 0x9e3779b1u) ^ (y * 0x27d4eb2fu) ^ 0x85ebca77u};
    return {fmask.bytes == 4u ? value : value & ((1u << (fmask.bytes * 8u)) - 1u), 0u};
}

std::uint64_t Expanded(const NarrowFmask& fmask, FmaskWord stored) {
    std::uint64_t expanded = 0;
    for (std::uint32_t sample = 0; sample < fmask.samples; ++sample) {
        const auto offset = sample * fmask.sampleBits;
        const auto word = offset < 32u ? stored.low : stored.high;
        const auto fragment = (word >> (offset & 31u)) & ((1u << fmask.sampleBits) - 1u);
        const auto index = fragment & (fmask.fragments - 1u);
        const auto unknown = (fragment & fmask.fragments) != 0u;
        const auto nibble = index | (unknown ? 8u : 0u);
        expanded |= static_cast<std::uint64_t>(nibble) << (sample * 4u);
    }
    auto low = static_cast<std::uint32_t>(expanded);
    const auto high = static_cast<std::uint32_t>(expanded >> 32u);
    if (fmask.samples < 8u && fmask.format != 159u) {
        low |= low << ((fmask.samples * 4u) & 31u);
        low |= low << ((fmask.samples * 8u) & 31u);
    }
    return static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32u);
}

void RunNarrow(AgcDriver::VulkanDevice& device, const NarrowFmask& fmask, bool layered, bool registration) {
    auto code = Code;
    if (layered) code[4] = 0xf0001f28u;
    Require(Expanded(fmask, FmaskWord{static_cast<std::uint32_t>(fmask.identity), static_cast<std::uint32_t>(fmask.identity >> 32u)}) == fmask.expandedIdentity, "FMASK " + std::to_string(fmask.format) + " reference does not expand PAL's identity value");
    for (std::uint32_t round = 0; round < 2u; ++round) {
        FillWith(layered, 0u, fmask.surfaceFormat, fmask.bytes, [&](std::uint32_t x, std::uint32_t y, std::uint32_t layer) { return Stored(fmask, x, y, layer, round); });
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            Input[lane * 4u] = (lane * 37u + 11u) % Width;
            Input[lane * 4u + 1u] = (lane * 23u + 5u) % Height;
            Input[lane * 4u + 2u] = layered ? lane & 1u : 0u;
            Input[lane * 4u + 3u] = 0u;
        }
        Output.fill(Sentinel);
        const auto shader = Compile(device, code, Image(layered, 0u, fmask.samples > 8u ? 0xfacu : 4u, fmask.format), registration);
        device.Dispatch(shader, 1u, 1u, 1u);
        device.WaitIdle();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto stored = Stored(fmask, Input[lane * 4u], Input[lane * 4u + 1u], layered ? Input[lane * 4u + 2u] + 1u : 0u, round);
            const auto expanded = Expanded(fmask, stored);
            const auto low = static_cast<std::uint32_t>(expanded);
            const auto high = static_cast<std::uint32_t>(expanded >> 32u);
            const bool wide = fmask.samples > 8u;
            const std::array<std::uint32_t, 4> expected{low, high, wide ? low : 0u, wide ? high : 0u};
            for (std::uint32_t component = 0; component < 4u; ++component) Require(Output[lane * 4u + component] == expected[component], "FMASK " + std::to_string(fmask.format) + " load did not expand its mapping: round " + std::to_string(round) + ", lane " + std::to_string(lane) + ", component " + std::to_string(component) + ", read " + std::to_string(Output[lane * 4u + component]) + ", expected " + std::to_string(expected[component]));
        }
        Require(std::all_of(Output.begin() + Threads * 4u, Output.end(), [](auto value) { return value == Sentinel; }), "FMASK load wrote outside its output");
    }
}

struct StandalonePlane {
    StandalonePlane(std::size_t size, bool watch) : bytes((size + 65535u) & ~std::size_t{65535u}), watched(watch) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536u));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else data = static_cast<std::byte*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        data = static_cast<std::byte*>(std::aligned_alloc(65536u, bytes));
        if (watched && data != nullptr) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
#endif
        Require(data != nullptr, "cannot allocate standalone FMASK fixture");
        std::memset(data, 0, bytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "standalone FMASK fixture is not watched");
    }
    ~StandalonePlane() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else VirtualFree(data, 0, MEM_RELEASE);
#else
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        std::free(data);
#endif
    }
    StandalonePlane(const StandalonePlane&) = delete;
    StandalonePlane& operator=(const StandalonePlane&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    bool watched;
    std::byte* data = nullptr;
};

struct FlushProbe {
    static inline FlushProbe* active = nullptr;
    FlushProbe(StandalonePlane& metadata, std::byte* changed, bool race) : metadata(metadata), changed(changed), race(race), previous(AgcDriver::GuestMemory::CurrentFlushHook()) {
        active = this;
        AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t address, std::size_t bytes) {
            auto& probe = *active;
            if (probe.previous != nullptr) probe.previous(address, bytes);
            if (address != probe.metadata.Address()) return;
            ++probe.calls;
            if (!probe.race && probe.calls == 1u) std::memset(probe.metadata.data, 0xee, probe.metadata.bytes);
            if (probe.race && probe.calls == 2u) *probe.changed ^= std::byte{0xff};
        });
    }
    ~FlushProbe() {
        AgcDriver::GuestMemory::SetFlushHook(previous);
        active = nullptr;
    }
    StandalonePlane& metadata;
    std::byte* changed;
    bool race;
    void (*previous)(std::uint64_t, std::size_t);
    std::uint32_t calls = 0;
};

void RunStandalone(AgcDriver::VulkanDevice& device, bool tiled, bool watched, bool registration, std::uint32_t pipeXor) {
    const std::uint32_t width = tiled ? 1152u : 128u;
    const std::uint32_t height = tiled ? 576u : 64u;
    StandalonePlane fmask(2u * 1024u * 1024u, watched);
    StandalonePlane cmask(65536u, watched);
    const auto base = (fmask.Address() | pipeXor) >> 8u;
    const auto meta = cmask.Address() >> 8u;
    const std::array<std::uint32_t, 8> image{
        static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u) | (161u << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u), 4u | (1u << 3u) | (4u << 6u) | (24u << 20u) | (9u << 28u),
        0u, 0u, (1u << 19u) | (1u << 21u) | (static_cast<std::uint32_t>(meta) << 24u), static_cast<std::uint32_t>(meta >> 8u)};
    const auto resource = DecodeTextureResource(image);
    const auto geometry = DescribeSurface(resource);
    const auto& mip = geometry.mips.front();
    Require(geometry.guestBytes <= fmask.bytes && (tiled ? geometry.guestBytes > 65536u : geometry.guestBytes == 65536u), "standalone FMASK fixture does not cover its intended layout");
    const auto block = ThinBlockLayout(resource.tileMode, 1u);
    const auto* equation = FindTextureSwizzleEquation(24u, 1u);
    Require(equation != nullptr, "standalone FMASK fixture lacks its equation");
    const auto offset = [&](std::uint32_t x, std::uint32_t y) {
        const auto tile = mip.tail ? 0u : (static_cast<std::uint64_t>(y / block[2]) * mip.blocksPerRow + x / block[1]) * block[0];
        return mip.tiledOffset + tile + (EquationOffset(*equation, x + (mip.tail ? mip.tailX : 0u), y + (mip.tail ? mip.tailY : 0u), 0u) ^ pipeXor);
    };
    const auto mapping = [](std::uint32_t x, std::uint32_t y) { return static_cast<std::byte>((x * 37u + y * 19u) & 255u); };
    for (std::uint32_t y = 0; y < height; ++y) for (std::uint32_t x = 0; x < width; ++x) fmask.data[offset(x, y)] = mapping(x, y);
    const CmaskLayout layout(width, height);
    const auto nibble = [&](std::uint32_t x, std::uint32_t y) { return layout.Nibble(x / 8u, y / 8u) ^ ((pipeXor & 0xf00u) << 1u); };
    const auto setCode = [&](std::uint32_t x, std::uint32_t y, std::uint32_t code) {
        const auto n = nibble(x, y);
        const auto old = std::to_integer<std::uint32_t>(cmask.data[n / 2u]);
        cmask.data[n / 2u] = static_cast<std::byte>((old & ~(15u << ((n & 1u) * 4u))) | (code << ((n & 1u) * 4u)));
    };
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Input[lane * 4u] = lane == 0u ? width - 1u : (lane * 79u) % width;
        Input[lane * 4u + 1u] = lane == 0u ? height - 1u : (lane * 43u) % height;
        Input[lane * 4u + 2u] = Input[lane * 4u + 3u] = 0u;
    }
    const auto program = Compile(device, Code, image, registration);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(Code.data());
    const auto check = [&] {
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto x = Input[lane * 4u], y = Input[lane * 4u + 1u];
            const auto n = nibble(x, y);
            const auto code = (std::to_integer<std::uint32_t>(cmask.data[n / 2u]) >> ((n & 1u) * 4u)) & 15u;
            const auto packed = code == 12u ? 0u : std::to_integer<std::uint32_t>(fmask.data[offset(x, y)]);
            const auto expected = Expanded(NarrowFormats[5], FmaskWord{packed, 0u});
            Require(Output[lane * 4u] == expected && Output[lane * 4u + 1u] == 1u && Output[lane * 4u + 2u] == expected && Output[lane * 4u + 3u] == 0u, "standalone FMASK returned the wrong mapping or selectors");
        }
        Require(std::all_of(Output.begin() + Threads * 4u, Output.end(), [](auto word) { return word == Sentinel; }), "standalone FMASK overran its output");
    };
    for (const auto code : {0xccu, 0xeeu}) {
        std::memset(cmask.data, code, cmask.bytes);
        Output.fill(Sentinel);
        std::shared_ptr<const AgcDriver::Recipe> recipe;
        device.Dispatch(program, 1u, 1u, 1u, {}, codeAddress, nullptr, &recipe);
        device.WaitIdle();
        check();
        Require(recipe == nullptr, "standalone FMASK emitted a recipe without a two-plane proof");
    }
    for (std::uint32_t y = 0; y < height; y += 8u) for (std::uint32_t x = 0; x < width; x += 8u) setCode(x, y, ((x / 8u + 3u * (y / 8u)) & 1u) ? 12u : 14u);
    setCode(Input[0], Input[1], 14u);
    const auto prepared = device.PrepareDispatch(program, {}, Threads, codeAddress);
    Require(prepared != nullptr, "standalone FMASK did not prepare a deferred dispatch");
    Output.fill(Sentinel);
    device.Dispatch(program, 1u, 1u, 1u, {}, codeAddress, prepared);
    device.WaitIdle();
    check();
    fmask.data[offset(Input[0], Input[1])] = std::byte{0xe4};
    Output.fill(Sentinel);
    device.Dispatch(program, 1u, 1u, 1u, {}, codeAddress, prepared);
    device.WaitIdle();
    check();
    Require(Output[0] == 0x32103210u, "prepared standalone FMASK reused stale packed data");
    const auto preparedMetadata = device.PrepareDispatch(program, {}, Threads, codeAddress);
    setCode(Input[0], Input[1], 12u);
    Output.fill(Sentinel);
    device.Dispatch(program, 1u, 1u, 1u, {}, codeAddress, preparedMetadata);
    device.WaitIdle();
    check();
    Require(Output[0] == 0u, "prepared standalone FMASK reused stale CMASK");
    const auto preparedInvalid = device.PrepareDispatch(program, {}, Threads, codeAddress);
    setCode(Input[0], Input[1], 15u);
    Output.fill(Sentinel);
    std::string refusal;
    try {
        device.Dispatch(program, 1u, 1u, 1u, {}, codeAddress, preparedInvalid);
        device.WaitIdle();
    } catch (const std::runtime_error& error) { refusal = error.what(); }
    Require(refusal.find("standalone FMASK CMASK compression code is not implemented") != std::string::npos, "unsupported standalone CMASK was not refused: " + refusal);
    Require(std::all_of(Output.begin(), Output.end(), [](auto word) { return word == Sentinel; }), "rejected standalone FMASK wrote output");
    setCode(Input[0], Input[1], 12u);
    {
        FlushProbe pending(cmask, nullptr, false);
        Output.fill(Sentinel);
        device.Dispatch(program, 1u, 1u, 1u, {}, codeAddress);
        device.WaitIdle();
        Require(pending.calls != 0u && Output[0] == 0x32103210u, "standalone FMASK captured CMASK before its pending store landed");
        check();
    }
    {
        FlushProbe race(cmask, fmask.data + offset(Input[0], Input[1]), true);
        Output.fill(Sentinel);
        refusal.clear();
        try {
            device.Dispatch(program, 1u, 1u, 1u, {}, codeAddress);
            device.WaitIdle();
        } catch (const std::runtime_error& error) { refusal = error.what(); }
        Require(race.calls >= 2u && refusal.find("guest writes raced standalone FMASK import") != std::string::npos, "standalone FMASK missed a write during capture: " + refusal);
        Require(std::all_of(Output.begin(), Output.end(), [](auto word) { return word == Sentinel; }), "raced standalone FMASK wrote output");
    }
    std::cout << "standalone four-sample FMASK passed: " << width << 'x' << height << ", watched=" << watched << ", registration=" << registration << ", XOR=" << pipeXor << '\n';
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 2) {
            const std::string mode = argv[1];
            Require(mode == "--standalone4-small" || mode == "--standalone4-tiled" || mode == "--standalone4-watched", "unknown FMASK test mode");
            for (const bool registration : {false, true}) for (const auto pipeXor : {0u, 0xcf00u}) RunStandalone(*device, mode != "--standalone4-small", mode == "--standalone4-watched", registration, pipeXor);
            return 0;
        }
        for (const bool registration : {false, true}) {
            for (const bool layered : {false, true}) {
                for (const auto pipeXor : {0u, 0xf00u}) Run(*device, layered, registration, pipeXor);
                for (const auto& fmask : NarrowFormats) RunNarrow(*device, fmask, layered, registration);
            }
        }
        std::cout << "FMASK image reads passed: 136 dispatches, 17408 component checks, 8 compression refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
