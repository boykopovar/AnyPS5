#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
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

std::vector<std::uint32_t> Draw(const std::uint32_t* pixelCode, Mode mode, const std::uint32_t* vertexCode = nullptr) {
    const bool vertexSampling = mode == Mode::Vertex || mode == Mode::VertexTable;
    const auto vertex = reinterpret_cast<std::uintptr_t>(vertexCode != nullptr ? vertexCode : VertexProgram(mode));
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode);
    const auto color = reinterpret_cast<std::uintptr_t>(mode == Mode::MergedBlend ? MergedTarget(0).data() : mode == Mode::MergedBlendOther ? MergedTarget(1).data() : ColorMemory.data());
    std::array<std::uint32_t, 24> descriptors{};
    const auto textureImage = ImageDescriptor(TextureMemory.data());
    const auto bufferDescriptor = BufferDescriptor(BufferMemory.data() + 1, sizeof(BufferMemory) - sizeof(float));
    const auto storageImage = ImageDescriptor(StorageMemory.data());
    std::copy(textureImage.begin(), textureImage.end(), descriptors.begin());
    std::copy(bufferDescriptor.begin(), bufferDescriptor.end(), descriptors.begin() + 12);
    std::copy(storageImage.begin(), storageImage.end(), descriptors.begin() + 16);
    const auto userWords = PixelUserWords(mode, vertexSampling);
    const auto otherImage = ImageDescriptor(OtherTextureMemory.data());
    std::copy(otherImage.begin(), otherImage.end(), TableMemory.begin());
    std::copy(descriptors.begin(), descriptors.begin() + 8, TableMemory.begin() + 8);
    MaterialMemory = {1u, 0u, 0u, 0u};
    std::array<std::uint32_t, 12> tableWords{};
    const auto table = BufferDescriptor(TableMemory.data(), sizeof(TableMemory));
    const auto material = BufferDescriptor(MaterialMemory.data(), sizeof(MaterialMemory));
    std::copy(table.begin(), table.end(), tableWords.begin());
    std::copy(material.begin(), material.end(), tableWords.begin() + 8);
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
    std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)},
        {0xc9, static_cast<std::uint32_t>(vertex >> 40u)},
        {0x8b, 1u << 1u},
        {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)},
        {0x009, static_cast<std::uint32_t>(pixel >> 40u)},
        {0x00b, userWords << 1u},
    };
    const auto userWord = [&](std::uint32_t index) -> std::uint32_t {
        if (mode == Mode::Table) return tableWords[index];
        if (mode == Mode::Bda && index >= 12u)
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(BufferMemory.data() + 1) >> (index == 12u ? 0u : 32u));
        return descriptors[index];
    };
    for (std::uint32_t index = 0; index < userWords; ++index) shader.emplace_back(0x00cu + index, userWord(index));
    if (vertexSampling) {
        shader.emplace_back(0x8bu, 12u << 1u);
        for (std::uint32_t index = 0; index < 12u; ++index)
            shader.emplace_back(0x08cu + index, mode == Mode::VertexTable ? tableWords[index] : descriptors[index]);
        shader.emplace_back(0x00bu, 0u);
    }
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    if (mode == Mode::MergedBlend || mode == Mode::MergedBlendOther) {
        Append(commands, 0x69, {0x1e0, (1u << 30u) | (1u << 8u) | 1u});
        Append(commands, 0x69, {0x31c, 0x28028u & ~0x8000u});
    }
    if (vertexSampling) {
        Append(commands, 0x69, {0x1b6, 1});
        Append(commands, 0x69, {0x191, 1u << 10u});
        Append(commands, 0x69, {0x1b1, 0});
    }
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {3, 2});
    if (mode == Mode::Bda) {
        const auto second = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(BufferMemory.data() + 2));
        Append(commands, 0x76, {0x00cu + 12u, static_cast<std::uint32_t>(second)});
        Append(commands, 0x76, {0x00cu + 13u, static_cast<std::uint32_t>(second >> 32u)});
    }
    if (mode == Mode::MovedBuffer) {
        const auto moved = BufferDescriptor(BufferMemory.data() + 3, sizeof(BufferMemory) - 3 * sizeof(float));
        for (std::uint32_t index = 0; index < 4u; ++index) Append(commands, 0x76, {0x00cu + 12u + index, moved[index]});
    }
    if (mode == Mode::MovedTexture) {
        for (std::uint32_t index = 0; index < 8u; ++index) Append(commands, 0x76, {0x00cu + index, otherImage[index]});
    }
    Append(commands, 0x2d, {3, 2});
    return commands;
}

std::span<const std::uint32_t> PixelProgram(Mode mode, bool buffer, bool vertexSampling) {
    switch (mode) {
    case Mode::PixelLds:
        return LdsPixelCode;
    case Mode::Table:
        return TablePixelCode;
    case Mode::Gds:
        return GdsPixelCode;
    case Mode::Storage:
        return StoragePixelCode;
    case Mode::Bda:
        return BdaPixelCode;
    case Mode::ConstantSampler:
        return ConstantSamplerPixelCode;
    case Mode::BorderSampler:
        return BorderSamplerPixelCode;
    case Mode::WrapSampler:
        return WrapSamplerPixelCode;
    default:
        break;
    }
    if (buffer) return BufferPixelCode;
    if (vertexSampling) return InterpolantPixelCode;
    return PixelCode;
}

void DrawSamplesTheTexture(bool heap, Mode mode, bool repeated = false) {
    const bool buffer = mode == Mode::Buffer || mode == Mode::Bda || mode == Mode::MovedBuffer;
    const auto colorMemory = mode == Mode::MergedBlend ? MergedTarget(0) : mode == Mode::MergedBlendOther ? MergedTarget(1) : std::span<std::byte>(ColorMemory);
    std::ranges::fill(colorMemory, Kept);
    StorageMemory.fill(0u);
    BufferMemory.fill(0.0f);
    BufferMemory[1] = BufferAlpha;
    BufferMemory[2] = SecondAlpha;
    BufferMemory[3] = SecondAlpha;
    for (std::size_t offset = 0; offset < TextureMemory.size(); offset += Texel.size())
        std::memcpy(TextureMemory.data() + offset, Texel.data(), Texel.size());
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    for (std::size_t offset = 0; offset < OtherTextureMemory.size(); offset += 4)
        OtherTextureMemory[offset] = OtherTextureMemory[offset + 1] = OtherTextureMemory[offset + 2] = OtherTextureMemory[offset + 3] =
            0x33u;
    const bool vertexSampling = mode == Mode::Vertex || mode == Mode::VertexTable;
    const auto program = PixelProgram(mode, buffer, vertexSampling);
    const auto* code = program.data();
    const auto bytes = program.size_bytes();
    if (mode == Mode::Vertex) Registered(VertexHeader, SamplingVertexCode.data(), sizeof(SamplingVertexCode), 2);
    if (mode == Mode::VertexTable) Registered(VertexHeader, TableVertexCode.data(), sizeof(TableVertexCode), 2);
    Registered(PixelHeader, code, bytes, 1);
    const auto takenBefore = AgcDriver::DriverDetail::HeapStagesTaken();
    const auto tablesBefore = AgcDriver::Graphics::ImageTableBuilds();
    const auto pushesBefore = AgcDriver::Graphics::DataAddressPushes();
    const auto embeddedBefore = AgcDriver::Graphics::EmbeddedSamplerMappings();
    const auto templatesBefore = ShaderRecompiler::HeapTemplateCounts().entries;
    auto* gdsCounter = reinterpret_cast<volatile std::uint32_t*>(AgcDriver::Pm4::GdsAddress() + 0x10u);
    if (mode == Mode::Gds) *gdsCounter = 0u;
    auto commands = Draw(code, mode);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    std::size_t drawn = 0;
    for (std::size_t offset = 0; offset < colorMemory.size(); offset += 4) {
        if (std::all_of(colorMemory.begin() + static_cast<std::ptrdiff_t>(offset),
                        colorMemory.begin() + static_cast<std::ptrdiff_t>(offset + 4), [](std::byte value) { return value == Kept; }))
            continue;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const auto value = static_cast<unsigned>(colorMemory[offset + channel]);
            const unsigned expected = ExpectedChannel(mode, buffer, channel);
            Require(value == expected, "pixel " + std::to_string(offset / 4) + " channel " + std::to_string(channel) + " is " +
                                           std::to_string(value) + ", expected " + std::to_string(expected) + DrawnWhat(mode, buffer) +
                                           (heap ? " through the heap" : ""));
        }
        ++drawn;
    }
    Require(drawn != 0, "the draw covered no pixel");
    if ((mode == Mode::Table || mode == Mode::VertexTable) && !repeated)
        Require(AgcDriver::Graphics::ImageTableBuilds() > tablesBefore,
                std::string("the table draw's ") + (mode == Mode::Table ? "fragment" : "vertex") + " stage bound no image table");
    if (mode == Mode::Gds) Require(*gdsCounter != 0u, "the GDS append did not count the drawn lanes");
    if (mode == Mode::Storage) {
        for (std::size_t channel = 0; channel < 4; ++channel)
            Require(StorageMemory[channel] == Texel[channel], "storage texel (0, 0) channel " + std::to_string(channel) + " is " +
                                                                  std::to_string(StorageMemory[channel]) + ", expected the sampled " +
                                                                  std::to_string(Texel[channel]) + (heap ? " through the heap" : ""));
    }
    if (!heap || repeated) return;
    if (mode == Mode::Vertex || mode == Mode::VertexTable)
        Require(AgcDriver::DriverDetail::HeapStagesTaken() > takenBefore,
                "the heap did not take the sampling vertex stage with descriptor memory");
    else
        Require(AgcDriver::DriverDetail::HeapStagesTaken() > takenBefore,
                std::string("the heap did not take the fragment stage") + TakenWhat(mode, buffer));
    Require(AgcDriver::Graphics::DataAddressPushes() > pushesBefore,
            "the heap stages did not read their shader data through pushed addresses");
    if (mode == Mode::ConstantSampler || mode == Mode::BorderSampler || mode == Mode::WrapSampler)
        Require(AgcDriver::Graphics::EmbeddedSamplerMappings() > embeddedBefore, "the fragment stage's constant sampler was not embedded in its pipeline");
    if (mode == Mode::MergedBlendOther)
        Require(ShaderRecompiler::HeapTemplateCounts().entries == templatesBefore,
                "merging into a second color target built another heap template instead of reusing the first target's");
}

void UnpreparedPairTakesTheHeap() {
    using AgcDriver::Graphics::HeapExcluded;
    using AgcDriver::Graphics::HeapExcludedCount;
    using AgcDriver::Graphics::HeapStageReason;
    using AgcDriver::Graphics::HeapStageReasons;
    FreshVertexCode = SamplingVertexCode;
    FreshPixelCode = InterpolantPixelCode;
    for (std::size_t offset = 0; offset < TextureMemory.size(); offset += Texel.size())
        std::memcpy(TextureMemory.data() + offset, Texel.data(), Texel.size());
    Registered(VertexHeader, FreshVertexCode.data(), sizeof(FreshVertexCode), 2);
    Registered(PixelHeader, FreshPixelCode.data(), sizeof(FreshPixelCode), 1);
    const auto heapBefore = HeapStageReasons(HeapStageReason::Heap);
    const auto takenBefore = AgcDriver::DriverDetail::HeapStagesTaken();
    const auto offHeapBefore = HeapStageReasons(HeapStageReason::Disabled) + HeapStageReasons(HeapStageReason::Excluded);
    const auto excludedBefore = HeapExcludedCount(HeapExcluded::Draw);
    ColorMemory.fill(Kept);
    auto commands = Draw(FreshPixelCode.data(), Mode::Vertex, FreshVertexCode.data());
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the unprepared pair's draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    std::size_t drawn = 0;
    for (std::size_t offset = 0; offset < ColorMemory.size(); offset += 4) {
        if (std::all_of(ColorMemory.begin() + static_cast<std::ptrdiff_t>(offset),
                        ColorMemory.begin() + static_cast<std::ptrdiff_t>(offset + 4), [](std::byte value) { return value == Kept; }))
            continue;
        for (std::size_t channel = 0; channel < 4; ++channel)
            Require(static_cast<unsigned>(ColorMemory[offset + channel]) == Texel[channel],
                    "the unprepared pair: pixel " + std::to_string(offset / 4) + " channel " + std::to_string(channel) + " is " +
                        std::to_string(static_cast<unsigned>(ColorMemory[offset + channel])) + ", expected " + std::to_string(Texel[channel]));
        ++drawn;
    }
    Require(drawn != 0, "the unprepared pair's draw covered no pixel");
    Require(HeapStageReasons(HeapStageReason::Heap) - heapBefore >= 2u && AgcDriver::DriverDetail::HeapStagesTaken() > takenBefore,
            "the first draw of an unprepared vertex and pixel pair did not put both stages on the heap");
    Require(HeapStageReasons(HeapStageReason::Disabled) + HeapStageReasons(HeapStageReason::Excluded) == offHeapBefore,
            "a stage of the unprepared pair's first draw was decided off the heap");
    Require(HeapExcludedCount(HeapExcluded::Draw) == excludedBefore, "the unprepared pair's draw took the per-draw path");
}

void UnmappableStageFails() {
    constexpr std::uint32_t Decorate = 4u << 16u | 71u;
    ShaderRecompiler::RecompileResult result;
    result.variantId = ~std::uint64_t{0} - 2u;
    result.spirv = {0x07230203u, 0x00010000u, 0u, 2u, 0u, Decorate, 1u, 34u, 0u, Decorate, 1u, 33u, 4000u};
    try {
        AgcDriver::DriverDetail::RequireHeapStageMappable(result);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("descriptor set 0 binding 4000") != std::string::npos,
                std::string("an unmappable heap stage failed for another reason: ") + error.what());
        Require(!AgcDriver::DriverDetail::HeapStageFits(result), "an unmappable heap stage was reported as fitting");
        return;
    }
    Require(false, "a stage whose module keeps a binding the heap does not map was mapped onto the heap");
}

void OversizedStageFails() {
    const auto capacity = AgcDriver::Graphics::BindlessHeap::ImageSlotCapacity();
    ShaderRecompiler::RecompileResult result;
    result.variantId = ~std::uint64_t{0} - 1u;
    result.bindings.push_back({ShaderRecompiler::DescriptorKind::SampledImage, ShaderRecompiler::DescriptorRole::GuestImages, 1u, 0u, capacity + 1u});
    try {
        AgcDriver::DriverDetail::RequireHeapStageMappable(result);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("image slots") != std::string::npos, std::string("an oversized heap stage failed for another reason: ") + error.what());
        Require(!AgcDriver::DriverDetail::HeapStageFits(result), "an oversized heap stage was reported as fitting");
        return;
    }
    Require(false, "a stage needing more image slots than the heap holds was mapped onto the heap");
}

}

int main() {
    SetTestEnvironment("APS5_EAGER_WRITEBACK", "1");
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        const bool excluded = std::getenv("APS5_GPU_HEAP_EXCLUDE") != nullptr;
        const bool heapMode = std::getenv("APS5_GPU_HEAP") != nullptr;
        const bool heap = heapMode && !excluded;
        if (heapMode && SkipWithoutDescriptorHeap()) return VulkanTestSkipped;
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(StorageMemory.data(), StorageMemory.size(), true, true, true);
            mutation.Add(BufferMemory.data(), sizeof(BufferMemory), true, true, true);
        }
        for (const auto mode : {Mode::Texture, Mode::Buffer, Mode::Storage, Mode::Gds, Mode::Vertex, Mode::Bda, Mode::Table, Mode::VertexTable,
                                Mode::MovedBuffer, Mode::MovedTexture, Mode::PixelLds, Mode::ConstantSampler, Mode::BorderSampler, Mode::WrapSampler, Mode::MergedBlend, Mode::MergedBlendOther, Mode::Texture})
            DrawSamplesTheTexture(heap, mode);
        for (const auto mode : {Mode::Table, Mode::VertexTable}) DrawSamplesTheTexture(heap, mode, true);
        const bool capped = heap && std::getenv("APS5_HEAP_SLOT_CAP") != nullptr;
        if (heap && !capped) UnpreparedPairTakesTheHeap();
        if (heap) {
            OversizedStageFails();
            UnmappableStageFails();
            const auto mark = AgcDriver::Graphics::PushDataMark();
            AgcDriver::Graphics::NoteCommandBufferBegun(VK_NULL_HANDLE);
            Require(AgcDriver::Graphics::PushDataMark() != mark, "beginning a command buffer kept the push data recorded before it");
        }
        if (excluded)
            Require(AgcDriver::Graphics::HeapExcludedCount(AgcDriver::Graphics::HeapExcluded::Draw) != 0u,
                    "no draw took the per-draw path for its excluded heap class");
        if (capped)
            Require(AgcDriver::Graphics::HeapSlotEvictions() != 0u,
                    "the capped heap evicted no image slot");
        AgcDriverShutdown_nid_postfix();
        if (heap && !capped)
            Require(AgcDriver::Graphics::EmptyResourceSets() != 0, "no heap draw bound without a per-draw resource set");
        std::cout << "A sampled texture reached the drawn pixels" << (heap ? " through the heap" : "") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
