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

constexpr std::array<std::uint8_t, 4> Texel{0x10u, 0x80u, 0xc0u, 0xffu};

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 12> PixelCode{0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u,
                                                               0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u,
                                                               0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> BufferPixelCode{
    0x7e0002f0u, 0x7e0202f0u, 0xf4200506u, 0xfa000000u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0x7e060214u,
    0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 24> SamplingVertexCode{
    0x7e1402f0u, 0x7e1602f0u, 0xf09c0f08u, 0x00820c0au, 0xbf8c0000u, 0xf800020fu, 0x0f0e0d0cu, 0x7e000d05u,
    0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u, 0x7e060280u,
    0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 28> TableVertexCode{
    0xf4200508u, 0xfa000000u, 0xbf8cc07fu, 0x8f148514u, 0xf42c0604u, 0x28000000u, 0xbf8cc07fu, 0x7e1402f0u, 0x7e1602f0u, 0xf09c0f08u,
    0x00660c0au, 0xbf8c0000u, 0xf800020fu, 0x0f0e0d0cu, 0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u,
    0x10040500u, 0x060404f3u, 0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 32> SrtPixelCode{
    0xf40c0506u, 0xfa000000u, 0xf40c0706u, 0xfa000020u, 0xbf8cc07fu, 0xf420090au, 0xfa000000u, 0xf420094cu,
    0xfa000000u, 0xf420098eu, 0xfa000000u, 0xf42009d0u, 0xfa000000u, 0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u,
    0x00400000u, 0xbf8c0000u, 0x06060624u, 0x06060625u, 0x06060626u, 0x06060627u, 0xf800180fu, 0x03020100u,
    0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 32> SrtVertexCode{
    0xf40c0504u, 0xfa000000u, 0xf40c0704u, 0xfa000020u, 0xbf8cc07fu, 0xf420090au, 0xfa000000u, 0xf420094cu,
    0xfa000000u, 0xf420098eu, 0xfa000000u, 0xf42009d0u, 0xfa000000u, 0xbf8cc07fu, 0x7e000d05u, 0x080200f4u,
    0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u, 0x7e060280u, 0x7e0802f2u,
    0x06060624u, 0x06060625u, 0x06060626u, 0x06060627u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u};
constexpr float BufferAlpha = 0.2f;
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

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};

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
constexpr std::size_t SrtTables = 256;
alignas(256) std::array<std::array<std::uint8_t, 1024>, Textures> BenchTextures{};
alignas(4096) std::array<std::array<std::uint32_t, 16>, SrtTables> SrtTableMemory{};

void FillSrtTables(std::size_t salt = 0) {
    for (std::size_t table = 0; table < SrtTables; ++table)
        for (std::size_t entry = 0; entry < 4u; ++entry) {
            const auto slice = 1u + (table * 5u + entry * 11u + salt) % Buffers;
            const auto descriptor = BufferDescriptor(BufferMemory.data() + slice, sizeof(BufferMemory) - slice * sizeof(float));
            std::copy(descriptor.begin(), descriptor.end(), SrtTableMemory[table].begin() + static_cast<std::ptrdiff_t>(entry * 4u));
        }
}

std::vector<std::uint32_t> Frame(std::size_t draws, std::size_t salt, bool srt) {
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
    const auto vertex = reinterpret_cast<std::uintptr_t>(srt ? SrtVertexCode.data() : VertexCode.data());
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u)});
    Append(commands, 0x76, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)});
    Append(commands, 0x76, {0x8b, (srt ? 2u : 1u) << 1u});
    Append(commands, 0x76, {0x8c, 0});
    for (std::size_t draw = 0; draw < draws; ++draw) {
        const bool buffer = !srt && draw % 2u == 1u;
        const auto pixel = reinterpret_cast<std::uintptr_t>(srt ? SrtPixelCode.data() : buffer ? BufferPixelCode.data() : PixelCode.data());
        std::array<std::uint32_t, 16> words{};
        const auto image = ImageDescriptor(BenchTextures[(draw + salt) % Textures].data());
        std::copy(image.begin(), image.end(), words.begin());
        const auto slice = 1u + (draw * 7u + salt) % Buffers;
        const auto descriptor = BufferDescriptor(BufferMemory.data() + slice, sizeof(BufferMemory) - slice * sizeof(float));
        std::copy(descriptor.begin(), descriptor.end(), words.begin() + 12);
        if (srt) {
            const auto table = reinterpret_cast<std::uintptr_t>(SrtTableMemory[(draw + salt) % SrtTables].data());
            words[12] = static_cast<std::uint32_t>(table);
            words[13] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(table) >> 32u);
            const auto vertexTable = reinterpret_cast<std::uintptr_t>(SrtTableMemory[((draw + salt) * 3u + 1u) % SrtTables].data());
            Append(commands, 0x76, {0x8c, static_cast<std::uint32_t>(vertexTable)});
            Append(commands, 0x76, {0x8d, static_cast<std::uint32_t>(static_cast<std::uint64_t>(vertexTable) >> 32u)});
        }
        const std::uint32_t userWords = srt ? 14u : buffer ? 16u : 12u;
        Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u)});
        Append(commands, 0x76, {0x009, static_cast<std::uint32_t>(pixel >> 40u)});
        Append(commands, 0x76, {0x00b, userWords << 1u});
        for (std::uint32_t index = 0; index < userWords; ++index) Append(commands, 0x76, {0x00cu + index, words[index]});
        Append(commands, 0x2d, {3, 2});
    }
    return commands;
}

}

int main() {
    SetTestEnvironment("APS5_NO_DRAW_RECIPE", "1");
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        const bool heap = std::getenv("APS5_GPU_HEAP") != nullptr;
        constexpr std::size_t Draws = 256, Rounds = 8;
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(BufferMemory.data(), sizeof(BufferMemory), true, true, true);
            mutation.Add(SrtTableMemory.data(), sizeof(SrtTableMemory), true, true, true);
        }
        FillSrtTables();
        for (auto& texture : BenchTextures)
            for (std::size_t offset = 0; offset < texture.size(); offset += Texel.size()) std::memcpy(texture.data() + offset, Texel.data(), Texel.size());
        BufferMemory.fill(BufferAlpha);
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        static Header TexturePixel{}, BufferPixel{};
        Registered(TexturePixel, PixelCode.data(), sizeof(PixelCode), 1);
        Registered(BufferPixel, BufferPixelCode.data(), sizeof(BufferPixelCode), 1);
        static Header SrtPixel{}, SrtVertex{};
        Registered(SrtPixel, SrtPixelCode.data(), sizeof(SrtPixelCode), 1);
        Registered(SrtVertex, SrtVertexCode.data(), sizeof(SrtVertexCode), 2);
        std::uint64_t keysBefore = 0, keysBuilt = 0, setsBefore = 0;
        for (std::size_t round = 0; round < Rounds; ++round) {
            if (round == 2) keysBefore = AgcDriver::Graphics::DrawContentKeysBuilt();
            if (round == 4) {
                keysBuilt = AgcDriver::Graphics::DrawContentKeysBuilt() - keysBefore;
                setsBefore = AgcDriver::Graphics::DescriptorCache::RewrittenSets();
            }
            auto commands = Frame(Draws, round, true);
            if (round >= 4) FillSrtTables(round);
            Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
            Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
            AgcDriverWaitIdle_nid_postfix();
        }
        const auto built = keysBuilt;
        const auto repeated = 2u * Draws;
        const auto sets = AgcDriver::Graphics::DescriptorCache::RewrittenSets() - setsBefore;
        const bool heapDraws = heap && AgcDriver::Graphics::DescriptorHeapEnabled();
        Require(heapDraws || sets != 0, "walked draws without the heap allocated no rewritten descriptor sets, so they did not relocate");
        if (heapDraws)
            Require(sets == 0, "walked heap draws allocated " + std::to_string(sets) + " rewritten descriptor sets that a descriptor heap pipeline never binds");
        std::printf("walked draws allocated %llu rewritten descriptor sets\n", static_cast<unsigned long long>(sets));
        if (heap && AgcDriver::Graphics::DescriptorHeapEnabled())
        {
            if (std::getenv("APS5_NO_MOVED_BUFFER_TEMPLATES") != nullptr)
                Require(built * 2u >= repeated, "with moved-buffer templates off, repeated heap draws built only " + std::to_string(built) + " content keys over " + std::to_string(repeated) + " draws, so a ring stage's key was shared across draws that carry different ShaderData words");
            else if (std::getenv("APS5_VERIFY_DRAW_ENTRIES") == nullptr)
                Require(built * 8u < repeated, "repeated heap draws built " + std::to_string(built) + " resource-cache content keys over " + std::to_string(repeated) + " draws instead of reusing their stages' keys");
        }
        std::printf("repeated draws built %llu content keys over %zu draws\n", static_cast<unsigned long long>(built), repeated);
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
