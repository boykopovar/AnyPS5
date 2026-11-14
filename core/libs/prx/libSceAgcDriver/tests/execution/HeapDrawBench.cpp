#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
alignas(4096) std::array<float, 1024> BufferMemory{};
alignas(256) std::array<std::byte, 1024> ColorMemory{};

std::span<std::byte> MergedTarget(std::size_t index) {
    constexpr std::size_t Bytes = 65536;
    static std::array<std::byte*, 2> data{};
    if (data.at(index) == nullptr) data[index] = [] {
        std::byte* block = nullptr;
        const bool watched = AgcDriver::GuestMemory::WriteWatched();
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
        Require(!watched || AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uintptr_t>(block), Bytes), "the merged color target is not write-watched");
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


constexpr std::size_t Textures = 16;
constexpr std::size_t Buffers = 32;
alignas(256) std::array<std::array<std::uint8_t, 1024>, Textures> BenchTextures{};

std::vector<std::uint32_t> Frame(std::size_t draws, std::size_t salt) {
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
    for (std::size_t draw = 0; draw < draws; ++draw) {
        const bool buffer = draw % 2u == 1u;
        const auto pixel = reinterpret_cast<std::uintptr_t>(buffer ? BufferPixelCode.data() : PixelCode.data());
        std::array<std::uint32_t, 16> words{};
        const auto image = ImageDescriptor(BenchTextures[(draw + salt) % Textures].data());
        std::copy(image.begin(), image.end(), words.begin());
        const auto slice = 1u + (draw * 7u + salt) % Buffers;
        const auto descriptor = BufferDescriptor(BufferMemory.data() + slice, sizeof(BufferMemory) - slice * sizeof(float));
        std::copy(descriptor.begin(), descriptor.end(), words.begin() + 12);
        const std::uint32_t userWords = buffer ? 16u : 12u;
        Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u)});
        Append(commands, 0x76, {0x009, static_cast<std::uint32_t>(pixel >> 40u)});
        Append(commands, 0x76, {0x00b, userWords << 1u});
        for (std::uint32_t index = 0; index < userWords; ++index) Append(commands, 0x76, {0x00cu + index, words[index]});
        Append(commands, 0x2d, {3, 2});
    }
    return commands;
}

}

int main(int argc, char** argv) {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        const bool heap = std::getenv("APS5_GPU_HEAP") != nullptr;
        const std::size_t draws = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 4000u;
        const std::size_t rounds = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 12u;
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(BufferMemory.data(), sizeof(BufferMemory), true, true, true);
        }
        for (auto& texture : BenchTextures)
            for (std::size_t offset = 0; offset < texture.size(); offset += Texel.size()) std::memcpy(texture.data() + offset, Texel.data(), Texel.size());
        BufferMemory.fill(BufferAlpha);
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        static Header TexturePixel{}, BufferPixel{};
        Registered(TexturePixel, PixelCode.data(), sizeof(PixelCode), 1);
        Registered(BufferPixel, BufferPixelCode.data(), sizeof(BufferPixelCode), 1);
        const auto takenBefore = AgcDriver::DriverDetail::HeapStagesTaken();
        const auto hitsBefore = AgcDriver::VulkanDevice::DrawRecipeHits();
        std::vector<double> perDraw;
        for (std::size_t round = 0; round < rounds; ++round) {
            auto commands = Frame(draws, round);
            const auto start = std::chrono::steady_clock::now();
            Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
            Require(sceAgcDriverSubmitDcb(&packet) == 0, "the bench submission failed");
            AgcDriverWaitIdle_nid_postfix();
            const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
            if (round >= 2) perDraw.push_back(us / static_cast<double>(draws));
        }
        std::sort(perDraw.begin(), perDraw.end());
        const bool tookHeap = AgcDriver::DriverDetail::HeapStagesTaken() > takenBefore;
        Require(tookHeap == heap, heap ? "the bench draws did not take the GPU heap" : "the bench draws took the GPU heap with it off");
        const auto hits = AgcDriver::VulkanDevice::DrawRecipeHits() - hitsBefore;
        if (rounds > 2 && AgcDriver::Graphics::DrawRecipes() && std::getenv("APS5_VERIFY_DRAW_ENTRIES") == nullptr)
            Require(hits >= (rounds - 2) * draws, "the bench's repeated draws replayed " + std::to_string(hits) + " recipes, fewer than the " + std::to_string((rounds - 2) * draws) + " draws of its repeated rounds");
        std::printf("bench %s: %zu draws x %zu rounds: min %.3f us/draw, median %.3f us/draw, heap stages taken %s\n", heap ? "heap" : "per-draw", draws,
                    perDraw.size(), perDraw.front(), perDraw[perDraw.size() / 2], tookHeap ? "yes" : "no");
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
