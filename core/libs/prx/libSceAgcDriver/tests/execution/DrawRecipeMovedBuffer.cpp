#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <span>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

constexpr std::byte Kept{0x40};
constexpr std::array<std::uint8_t, 4> Texel{0x10u, 0x80u, 0xc0u, 0xffu};

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 12> PixelCode{0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u,
                                                               0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u,
                                                               0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> ConstantSamplerPixelCode{
    0xbe8803ffu, 6u | (6u << 3u) | (6u << 6u), 0xbe8903ffu, 0u, 0xbe8a03ffu, 0u, 0xbe8b03ffu, 2u << 30u,
    0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> BorderSamplerPixelCode{
    0xbe8803ffu, 6u | (6u << 3u) | (6u << 6u), 0xbe8903ffu, 0u, 0xbe8a03ffu, 0u, 0xbe8b03ffu, 2u << 30u,
    0x7e0002f3u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> WrapSamplerPixelCode{
    0xbe8803ffu, 0u, 0xbe8903ffu, 0u, 0xbe8a03ffu, 0u, 0xbe8b03ffu, 2u << 30u,
    0x7e0002f3u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> BufferPixelCode{
    0x7e0002f0u, 0x7e0202f0u, 0xf4200506u, 0xfa000000u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0x7e060214u,
    0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> GdsPixelCode{
    0xbefc03ffu, 0x0000ffffu, 0x7e0002f0u, 0x7e0202f0u, 0xd8fa0010u, 0x04000000u, 0xf09c0f08u, 0x00400000u,
    0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> StoragePixelCode{
    0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0x7e100280u, 0x7e120280u, 0xf0201f08u,
    0x00040008u, 0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> LdsPixelCode{
    0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0x7e080280u, 0xd8344400u, 0x00000004u,
    0xbf8cc07fu, 0xd8d84400u, 0x00000004u, 0xbf8cc07fu, 0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(4096) std::array<std::uint8_t, 4096> StorageMemory{};
alignas(256) constexpr std::array<std::uint32_t, 24> SamplingVertexCode{
    0x7e1402f0u, 0x7e1602f0u, 0xf09c0f08u, 0x00820c0au, 0xbf8c0000u, 0xf800020fu, 0x0f0e0d0cu, 0x7e000d05u,
    0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u, 0x7e060280u,
    0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> InterpolantPixelCode{0xc8020002u, 0xc8060102u, 0xc80a0202u, 0xc80e0302u,
                                                                         0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> BdaPixelCode{
    0x7e0002f0u, 0x7e0202f0u, 0x7e08020cu, 0x7e0a020du, 0xdc308000u, 0x067d0004u, 0xf09c0f08u, 0x00400000u,
    0xbf8c0000u, 0x7e060306u, 0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> TablePixelCode{
    0xf4200404u, 0xfa000000u, 0xbf8cc07fu, 0x8f108510u, 0xf42c0500u, 0x20000000u, 0xbf8cc07fu, 0x7e0002f0u,
    0x7e0202f0u, 0xf09c0f08u, 0x00250000u, 0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 28> TableVertexCode{
    0xf4200508u, 0xfa000000u, 0xbf8cc07fu, 0x8f148514u, 0xf42c0604u, 0x28000000u, 0xbf8cc07fu, 0x7e1402f0u, 0x7e1602f0u, 0xf09c0f08u,
    0x00660c0au, 0xbf8c0000u, 0xf800020fu, 0x0f0e0d0cu, 0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u,
    0x10040500u, 0x060404f3u, 0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, 16> TableMemory{};
alignas(256) std::array<std::uint32_t, 24> FreshVertexCode{};
alignas(256) std::array<std::uint32_t, 8> FreshPixelCode{};
alignas(256) std::array<std::uint32_t, 4> MaterialMemory{};
alignas(256) std::array<std::uint8_t, 1024> OtherTextureMemory{};
constexpr float BufferAlpha = 0.2f;
constexpr std::uint8_t BufferAlphaByte = 51u;
constexpr float SecondAlpha = 0.6f;
constexpr std::uint8_t SecondAlphaByte = 153u;
constexpr float FilledAlpha = 0.8f;
constexpr std::uint8_t FilledAlphaByte = 204u;
constexpr std::size_t FillSource = 512;
alignas(4096) std::array<float, 1024> BufferMemory{};
alignas(4096) std::array<float, 1024> OtherMemory{};
constexpr std::size_t OtherSlices = 1024;

float* SliceAt(std::size_t slice) {
    return slice >= OtherSlices ? OtherMemory.data() + (slice - OtherSlices) : BufferMemory.data() + slice;
}

std::size_t SliceBytes(std::size_t slice) {
    return (slice >= OtherSlices ? sizeof(OtherMemory) - (slice - OtherSlices) * sizeof(float) : sizeof(BufferMemory) - slice * sizeof(float));
}
alignas(256) std::array<std::byte, 1024> ColorMemory{};

std::span<std::byte> MergedTarget(std::size_t index) {
    constexpr std::size_t Bytes = 65536;
    static std::array<std::byte*, 2> data{};
    if (data.at(index) == nullptr) data[index] = [] {
        std::byte* block = nullptr;
#ifdef _WIN32
        block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Bytes, Bytes));
        GuestArena::GuestArenaCommit_nid_postfix(block, Bytes, PAGE_READWRITE, Bytes);
#else
        void* raw = mmap(nullptr, 2u * Bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "cannot map the merged color target");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        block = reinterpret_cast<std::byte*>((begin + Bytes - 1u) & ~static_cast<std::uintptr_t>(Bytes - 1u));
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, Bytes);
#endif
        GuestAllocations::Mutation mutation;
        mutation.Add(block, Bytes, true, true, true);
        return block;
    }();
    return {data[index], ColorMemory.size()};
}
alignas(256) std::array<std::uint8_t, 1024> TextureMemory{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

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

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

enum class Mode { Texture, Buffer, Storage, Gds, Vertex, Bda, Table, VertexTable, MovedBuffer, MovedTexture, PixelLds, ConstantSampler, BorderSampler, WrapSampler, MergedBlend, MergedBlendOther };

const std::uint32_t* VertexProgram(Mode mode) {
    if (mode == Mode::VertexTable) return TableVertexCode.data();
    if (mode == Mode::Vertex) return SamplingVertexCode.data();
    return VertexCode.data();
}

std::uint32_t PixelUserWords(Mode mode, bool vertexSampling) {
    switch (mode) {
    case Mode::Storage:
        return 24u;
    case Mode::Buffer:
    case Mode::MovedBuffer:
        return 16u;
    case Mode::Bda:
        return 14u;
    default:
        return vertexSampling ? 0u : 12u;
    }
}

unsigned ExpectedChannel(Mode mode, bool buffer, std::size_t channel) {
    if (mode == Mode::MovedTexture) return 0x33u;
    if ((mode == Mode::Bda || mode == Mode::MovedBuffer) && channel == 3) return SecondAlphaByte;
    if (buffer && channel == 3) return BufferAlphaByte;
    if (mode == Mode::BorderSampler) return 0xffu;
    if (mode == Mode::MergedBlend || mode == Mode::MergedBlendOther) return std::min(2u * Texel[channel] + static_cast<unsigned>(Kept), 0xffu);
    return Texel[channel];
}

const char* DrawnWhat(Mode mode, bool buffer) {
    if (mode == Mode::Bda) return " (texture and BDA)";
    if (mode == Mode::MovedBuffer) return " (texture and a buffer moved between two draws)";
    if (mode == Mode::MovedTexture) return " (a texture changed between two draws)";
    if (buffer) return " (texture and buffer)";
    if (mode == Mode::Table) return " (image table)";
    if (mode == Mode::ConstantSampler) return " (texture with a constant sampler)";
    if (mode == Mode::BorderSampler) return " (constant border-clamp sampler outside the texture)";
    if (mode == Mode::WrapSampler) return " (constant wrapping sampler outside the texture, after a border-clamp draw of the same shader)";
    if (mode == Mode::MergedBlend) return " (texture blended unclamped into the color target)";
    if (mode == Mode::MergedBlendOther) return " (texture blended unclamped into a second color target)";
    return " (texture)";
}

const char* TakenWhat(Mode mode, bool buffer) {
    if (buffer) return " reading a buffer";
    if (mode == Mode::Vertex) return " (vertex stage)";
    if (mode == Mode::Bda) return " loading through BDA";
    if (mode == Mode::Table) return " reading an image table";
    if (mode == Mode::VertexTable) return " (vertex stage reading an image table)";
    if (mode == Mode::ConstantSampler || mode == Mode::BorderSampler || mode == Mode::WrapSampler) return " sampling with a constant sampler";
    if (mode == Mode::MergedBlend || mode == Mode::MergedBlendOther) return " merging into its color target";
    return "";
}

std::array<std::uint32_t, 8> ImageDescriptor(const void* memory) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(memory));
    return {static_cast<std::uint32_t>(address >> 8u),
            static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (3u << 30u),
            3u << 14u,
            0xfacu | (9u << 28u),
            0u,
            0u,
            0u,
            0u};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* memory, std::size_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(memory));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(bytes),
            0xfacu};
}


alignas(256) std::array<std::uint8_t, 1024> SampledTexture{};

std::vector<std::uint32_t> Frame(std::span<const std::size_t> slices, std::optional<std::size_t> filled = std::nullopt) {
    const auto color = reinterpret_cast<std::uintptr_t>(MergedTarget(0).data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0},
        {0x207, 0},
        {0x200, 0},
        {0x203, 0x800},
        {0x2dc, 0xaa00},
        {0x2f8, 0},
        {0x293, 0},
        {0x80, 0},
        {0x8d, 0},
        {0x83, 0xffff},
        {0x8c, 0xa},
        {0x2f9, 0x2d},
        {0x313, 0x6000},
        {0x30e, 0xffffffff},
        {0x30f, 0xffffffff},
        {0x206, 0x43f},
        {0x204, 0x80000},
        {0x205, 0x240},
        {0x8e, 0xf},
        {0x8f, 0xf},
        {0x202, 0xcc0010},
        {0x1c4, 0},
        {0x1c5, 9},
        {0x1c3, 4},
        {0x31c, 0x28028},
        {0x31b, 0},
        {0x31d, 0},
        {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000},
        {0x1e0, 0},
        {0xc, 0},
        {0xd, 0x40040},
        {0x81, 0x80000000},
        {0x82, 0x40040},
        {0x90, 0x80000000},
        {0x91, 0x40040},
        {0x94, 0x80000000},
        {0x95, 0x40040},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)},
        {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)},
        {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)},
        {0x114, 0},
        {0xb4, 0},
        {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0},
        {0x302, 0},
        {0x306, 0},
        {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(color >> 8u)},
        {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x292, 2},
    };

    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u)});
    Append(commands, 0x76, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)});
    Append(commands, 0x76, {0x8b, 1u << 1u});
    Append(commands, 0x76, {0x8c, 0});
    const auto pixel = reinterpret_cast<std::uintptr_t>(BufferPixelCode.data());
    for (const auto slice : slices) {
        if (filled.has_value() && slice == *filled) {
            const auto source = reinterpret_cast<std::uintptr_t>(BufferMemory.data() + FillSource);
            const auto destination = reinterpret_cast<std::uintptr_t>(SliceAt(slice));
            Append(commands, 0x50, {0u, static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u), 16u});
        }
        std::array<std::uint32_t, 16> words{};
        const auto image = ImageDescriptor(SampledTexture.data());
        std::copy(image.begin(), image.end(), words.begin());
        const auto descriptor = BufferDescriptor(SliceAt(slice), SliceBytes(slice));
        std::copy(descriptor.begin(), descriptor.end(), words.begin() + 12);
        Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u)});
        Append(commands, 0x76, {0x009, static_cast<std::uint32_t>(pixel >> 40u)});
        Append(commands, 0x76, {0x00b, 16u << 1u});
        for (std::uint32_t index = 0; index < 16u; ++index) Append(commands, 0x76, {0x00cu + index, words[index]});
        Append(commands, 0x2d, {3, 2});
    }
    return commands;
}

unsigned DrawnAlpha() {
    const auto color = MergedTarget(0);
    for (std::size_t offset = 0; offset < color.size(); offset += 4) {
        if (std::all_of(color.begin() + static_cast<std::ptrdiff_t>(offset), color.begin() + static_cast<std::ptrdiff_t>(offset + 4), [](std::byte value) { return value == Kept; })) continue;
        return static_cast<unsigned>(color[offset + 3]);
    }
    throw std::runtime_error("the draws covered no pixel");
}

unsigned Submit(std::span<const std::size_t> slices, std::optional<std::size_t> filled = std::nullopt) {
    std::ranges::fill(MergedTarget(0), Kept);
    auto commands = Frame(slices, filled);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    return DrawnAlpha();
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(BufferMemory.data(), sizeof(BufferMemory), true, true, true);
            mutation.Add(OtherMemory.data(), sizeof(OtherMemory), true, true, true);
        }
        for (std::size_t offset = 0; offset < SampledTexture.size(); offset += Texel.size()) std::memcpy(SampledTexture.data() + offset, Texel.data(), Texel.size());
        BufferMemory.fill(0.0f);
        constexpr std::size_t First = 1, Moved = 9;
        BufferMemory[First] = BufferAlpha;
        BufferMemory[Moved] = SecondAlpha;
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, BufferPixelCode.data(), sizeof(BufferPixelCode), 1);
        const std::array<std::size_t, 2> slices{First, Moved};
        const auto keepsBefore = AgcDriver::Graphics::PushedUserDataKeeps();
        for (int round = 0; round < 2; ++round) Require(Submit(slices) == SecondAlphaByte, "the draw of the moved buffer did not read it");
        const auto keeps = AgcDriver::Graphics::PushedUserDataKeeps() - keepsBefore;
        if (!AgcDriver::Graphics::DescriptorHeapEnabled()) Require(keeps == 0u, "draws without the GPU heap kept a pushed user-data block " + std::to_string(keeps) + " times");
        else Require(keeps >= 2u, "heap draws whose only change was the moved buffer's pushed user words kept their user-data block " + std::to_string(keeps) + " times, expected at least 2");

        const auto hitsBefore = AgcDriver::VulkanDevice::DrawRecipeHits();
        const auto pathsBefore = AgcDriver::Graphics::HeapBufferPaths();
        Require(Submit(slices) == SecondAlphaByte, "the repeated draw of the moved buffer did not read it");
        const auto hits = AgcDriver::VulkanDevice::DrawRecipeHits() - hitsBefore;
        const auto paths = AgcDriver::Graphics::HeapBufferPaths();
        if (AgcDriver::Graphics::DrawRecipes() && std::getenv("APS5_VERIFY_DRAW_ENTRIES") == nullptr) {
            Require(hits == slices.size(), "a repeated draw whose read-only buffer moved from its template's did not replay a recipe (" + std::to_string(hits) + " recipe hits for " + std::to_string(slices.size()) + " draws)");
            if (AgcDriver::Graphics::DescriptorHeapEnabled()) {
                Require(paths.pushedStages - pathsBefore.pushedStages >= hits, "recipe hits over a moved buffer pushed the buffer addresses of " + std::to_string(paths.pushedStages - pathsBefore.pushedStages) + " stages for " + std::to_string(hits) + " hits");
                if (AgcDriver::Graphics::PushedBufferRecordCap() == 0u) Require(paths.overflowStages - pathsBefore.overflowStages >= hits, "recipe hits whose buffers all overflow the push data wrote the overflow block of " + std::to_string(paths.overflowStages - pathsBefore.overflowStages) + " stages for " + std::to_string(hits) + " hits");
            }
        }
        BufferMemory[Moved] = BufferAlpha;
        Require(Submit(slices) == BufferAlphaByte, "a recipe over a moved buffer drew the buffer's old contents after they changed");
        BufferMemory[Moved] = SecondAlpha;
        const std::array<std::size_t, 1> unmoved{First};
        for (int round = 0; round < 3; ++round) Require(Submit(unmoved) == BufferAlphaByte, "the draw of the template's own buffer did not read it");
        const std::array<std::size_t, 1> movedOnly{Moved};
        for (int round = 0; round < 3; ++round) Require(Submit(movedOnly) == SecondAlphaByte, "a draw whose buffer moved after a recipe without moves was recorded did not read the moved buffer");
        std::fill(BufferMemory.begin() + FillSource, BufferMemory.begin() + FillSource + 4, FilledAlpha);
        const auto behindBefore = AgcDriver::Graphics::ShaderResources::MovedBehindGpuWrites();
        constexpr std::size_t Aligned = First + 64;
        const std::array<std::size_t, 2> alignedSlices{First, Aligned};
        BufferMemory[Aligned] = SecondAlpha;
        const auto cpuAlpha = Submit(alignedSlices);
        Require(cpuAlpha == SecondAlphaByte, "a draw whose buffer moved by a whole number of 256-byte blocks drew another buffer's contents (alpha " + std::to_string(cpuAlpha) + ")");
        BufferMemory[Aligned] = 0.0f;
        const auto alignedAlpha = Submit(alignedSlices, Aligned);
        Require(alignedAlpha == FilledAlphaByte, "a draw whose buffer moved onto a range a GPU copy wrote earlier in its batch did not read the copied value (alpha " + std::to_string(alignedAlpha) + ")");
        if (AgcDriver::Graphics::DescriptorHeapEnabled())
            Require(AgcDriver::Graphics::ShaderResources::MovedBehindGpuWrites() > behindBefore, "a heap draw whose buffer moved onto a range with a pending GPU write did not reuse its template by reading the buffer in place");
        std::fill(BufferMemory.begin() + Aligned, BufferMemory.begin() + Aligned + 4, 0.0f);
        constexpr std::size_t Elsewhere = OtherSlices + First;
        const std::array<std::size_t, 2> elsewhereSlices{First, Elsewhere};
        const auto elsewhereBefore = AgcDriver::Graphics::ShaderResources::MovedBehindGpuWrites();
        const auto elsewhereAlpha = Submit(elsewhereSlices, Elsewhere);
        Require(elsewhereAlpha == FilledAlphaByte, "a draw whose buffer moved into other guest memory a GPU copy wrote earlier in its batch did not read the copied value (alpha " + std::to_string(elsewhereAlpha) + ")");
        if (AgcDriver::Graphics::DescriptorHeapEnabled())
            Require(AgcDriver::Graphics::ShaderResources::MovedBehindGpuWrites() > elsewhereBefore, "a heap draw whose buffer moved into other guest memory with a pending GPU write did not reuse its template by reading the buffer in place");
        OtherMemory.fill(0.0f);
        constexpr std::size_t Shifted = Elsewhere + 1;
        const std::array<std::size_t, 2> shiftedSlices{First, Shifted};
        const auto shiftedBefore = AgcDriver::Graphics::ShaderResources::MovedBehindGpuWrites();
        const auto shiftedAlpha = Submit(shiftedSlices, Shifted);
        Require(shiftedAlpha == FilledAlphaByte, "a draw whose buffer moved to another view alignment behind a GPU copy did not read the copied value (alpha " + std::to_string(shiftedAlpha) + ")");
        Require(AgcDriver::Graphics::ShaderResources::MovedBehindGpuWrites() == shiftedBefore, "a draw whose buffer moved to another view alignment was read in place through its template's view");
        OtherMemory.fill(0.0f);
        std::puts("draws whose read-only buffer moved from their template's replayed recipes that read the buffer's current contents");
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
