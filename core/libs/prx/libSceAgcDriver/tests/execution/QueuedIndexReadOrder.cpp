#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t DistanceVector = 0x00400000u;
constexpr std::uint32_t SetContextReg = 0x69u;
constexpr std::uint32_t SetShReg = 0x76u;
constexpr std::uint32_t SetUconfigReg = 0x79u;
constexpr std::uint32_t DrawIndexOffset2Header = 0xc0033500u;

constexpr std::size_t Block = 65536;
constexpr std::size_t PoolBytes = 2 * Block;
constexpr std::size_t ColorBytes = Width * Height * 4;
constexpr std::uint32_t FormatRgba16Float = 71;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Standard64KB = 0x09;
constexpr std::uint32_t ImageWidth = 128;
constexpr std::uint32_t ImageHeight = 120;
constexpr std::uint8_t Sentinel = 0xc7;
constexpr std::array<std::uint32_t, 3> Indices{0, 1, 2};
constexpr std::size_t VertexOffset = 0x2000;

std::byte* ColorMemory = nullptr;
std::byte* Pool = nullptr;
std::byte* Copied = nullptr;

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80020005, 0xe0382010, 0x80020405, 0xbf8c3f70,
    0xf80000cf, 0x03020100, 0xf80008df, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0x7e0002f2, 0x7e0202f2, 0x7e0402f2, 0x7e0602f2, 0xf800080f, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> ImageWriteCode{
    0xd7460004u, 0x0401060eu, 0xd7460005u, 0x0405060fu, 0x7e100d04u, 0x7e120d05u,
    0x7e140c0du, 0x7e1602f2u, 0xf0200f08u, 0x00000804u, 0xbf810000u,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 4> distances;
};

std::array<Vertex, 3> Vertices{};

constexpr std::array<float, 3> PositionX{-3.0f, 1.0f, 1.0f};
constexpr std::array<float, 3> PositionY{-1.0f, -1.0f, 3.0f};
constexpr std::array<float, 3> Negative{-1.0f, -1.0f, -1.0f};

struct RegisteredShader {
    Shader shader;
    ShaderUserData userData;
};
RegisteredShader VertexShader{};
RegisteredShader PixelShader{};

const std::vector<std::pair<std::uint32_t, std::uint32_t>> ContextBaseline{
    {0x2d5, 0x2000},
    {0x1b6, 0}, {0x200, 0}, {0x203, 0x800},
    {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
    {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
    {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
    {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
    {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
    {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
    {0x31b, 0}, {0x31d, 0}, {0x3b8, 0x9000000}, {0x1e0, 0},
    {0xc, 0},
    {0x81, 0x80000000}, {0x90, 0x80000000}, {0x94, 0x80000000},
    {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0}, {0x113, std::bit_cast<std::uint32_t>(1.0f)},
};

std::byte* AllocateWatched(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) throw std::runtime_error("queued index read: cannot map the watched block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + Block - 1) & ~static_cast<std::uintptr_t>(Block - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    return static_cast<std::byte*>(block);
}

void Register(RegisteredShader& registered, const void* code, std::uint32_t bytes, std::uint8_t type) {
    registered.shader.file_header = 0x34333231u;
    registered.shader.version = 0x18u;
    registered.shader.user_data = &registered.userData;
    registered.shader.code = code;
    registered.shader.header_size = sizeof(RegisteredShader);
    registered.shader.shader_size = bytes;
    registered.shader.type = type;
    AgcDriverRegisterShader_nid_postfix(&registered.shader);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void SetRegister(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    words.push_back(0xc0010000u | (opcode << 8u));
    words.push_back(offset);
    words.push_back(value);
}

std::array<std::uint32_t, 8> ImageDescriptor(const void* texels) {
    const auto address = reinterpret_cast<std::uintptr_t>(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (FormatRgba16Float << 20u) | (((ImageWidth - 1u) & 3u) << 30u),
        ((ImageWidth - 1u) >> 2u) | ((ImageHeight - 1u) << 14u),
        0xfacu | (Standard64KB << 20u) | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::vector<std::uint32_t> ImageWrite() {
    const auto descriptor = ImageDescriptor(Pool);
    std::vector<std::uint32_t> data(descriptor.begin(), descriptor.end());
    data.resize(14, 0u);
    data[13] = 7u;
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{8, 8, 1};
    registers(0x207, shape);
    const auto code = reinterpret_cast<std::uintptr_t>(ImageWriteCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (7u << 7u) | (1u << 11u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, shape[0], shape[1], shape[2], 0x41u});
    return words;
}

std::vector<std::uint32_t> Draw(const void* vertices, std::size_t count) {
    const auto colorAddress = reinterpret_cast<std::uintptr_t>(ColorMemory);
    const auto poolAddress = reinterpret_cast<std::uintptr_t>(Pool);
    const auto vertexAddress = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixelAddress = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    std::vector<std::uint32_t> words;
    for (const auto& [offset, value] : ContextBaseline) SetRegister(words, SetContextReg, offset, value);
    SetRegister(words, SetContextReg, 0x207, DistanceVector);
    SetRegister(words, SetContextReg, 0x318, static_cast<std::uint32_t>(colorAddress >> 8u));
    SetRegister(words, SetContextReg, 0x390, static_cast<std::uint32_t>(colorAddress >> 40u));
    SetRegister(words, SetContextReg, 0x3b0, ((Width - 1u) << 14u) | (Height - 1u));
    SetRegister(words, SetContextReg, 0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f));
    SetRegister(words, SetContextReg, 0x110, std::bit_cast<std::uint32_t>(Width / 2.0f));
    SetRegister(words, SetContextReg, 0x111, std::bit_cast<std::uint32_t>(-(Height / 2.0f)));
    SetRegister(words, SetContextReg, 0x112, std::bit_cast<std::uint32_t>(Height / 2.0f));
    const auto extent = (Height << 16u) | Width;
    SetRegister(words, SetContextReg, 0xd, extent);
    SetRegister(words, SetContextReg, 0x82, extent);
    SetRegister(words, SetContextReg, 0x91, extent);
    SetRegister(words, SetContextReg, 0x95, extent);
    SetRegister(words, SetUconfigReg, 0x242, 4u);
    SetRegister(words, SetShReg, 0x008, static_cast<std::uint32_t>(pixelAddress >> 8u));
    SetRegister(words, SetShReg, 0x009, static_cast<std::uint32_t>(pixelAddress >> 40u));
    SetRegister(words, SetShReg, 0x00b, 0u);
    SetRegister(words, SetShReg, 0x0c8, static_cast<std::uint32_t>(vertexAddress >> 8u));
    SetRegister(words, SetShReg, 0x0c9, static_cast<std::uint32_t>(vertexAddress >> 40u));
    SetRegister(words, SetShReg, 0x08b, 4u << 1u);
    const auto descriptor = BufferDescriptor(vertices, sizeof(Vertex), static_cast<std::uint32_t>(count));
    for (std::uint32_t i = 0; i < descriptor.size(); ++i) SetRegister(words, SetShReg, 0x08c + i, descriptor[i]);
    words.insert(words.end(), {0xc0012600u, static_cast<std::uint32_t>(poolAddress), static_cast<std::uint32_t>(poolAddress >> 32u)});
    words.insert(words.end(), {0xc0001300u, static_cast<std::uint32_t>(Indices.size())});
    SetRegister(words, SetUconfigReg, 0x243, 0x401u);
    words.insert(words.end(), {DrawIndexOffset2Header, static_cast<std::uint32_t>(Indices.size()), 0u, static_cast<std::uint32_t>(Indices.size()), 0u});
    return words;
}

void Submit(const std::vector<std::uint32_t>& words, const char* what) {
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, std::string("queued index read: the submission failed for ") + what);
}

std::vector<std::uint32_t> CopyPool(std::size_t bytes) {
    const auto source = reinterpret_cast<std::uintptr_t>(Pool);
    const auto destination = reinterpret_cast<std::uintptr_t>(Copied);
    return {0xc0055000u, 0u, static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u), static_cast<std::uint32_t>(bytes)};
}

void RequireCopiedImage(std::size_t bytes, const char* phase) {
    std::size_t differing = 0;
    for (std::size_t i = 0; i < bytes; ++i) differing += Copied[i] != Pool[i] ? 1u : 0u;
    Require(differing == 0, std::string("queued index read: ") + phase + ": " + std::to_string(differing) + " of " + std::to_string(bytes) + " copied bytes miss the image's results over the copy source");
}

std::vector<std::uint32_t> WaitRegMem(std::uint64_t address, std::uint32_t reference) {
    return {0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), reference, 0xffffffffu, 0u};
}

bool IndicesIntact() {
    return std::equal(Indices.begin(), Indices.end(), reinterpret_cast<const std::uint32_t*>(Pool));
}

bool VerticesIntact() {
    return std::memcmp(Pool + VertexOffset, Vertices.data(), sizeof(Vertex) * Vertices.size()) == 0;
}

std::uint32_t PoolWord(std::size_t offset) {
    return *reinterpret_cast<const std::uint32_t*>(Pool + offset);
}

void ResetPool() {
    std::memset(Pool, Sentinel, PoolBytes);
    std::memcpy(Pool, Indices.data(), Indices.size() * sizeof(std::uint32_t));
    std::memcpy(Pool + VertexOffset, Vertices.data(), sizeof(Vertex) * Vertices.size());
}

std::size_t WrittenPixels() {
    std::vector<std::byte> colors(ColorBytes);
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ColorMemory)), colors);
    std::size_t written = 0;
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        const auto value = std::to_integer<std::uint8_t>(colors[pixel * 4u]);
        Require(value == 0u || value == 255u, "queued index read: pixel " + std::to_string(pixel) + " stored " + std::to_string(value));
        written += value == 255u ? 1u : 0u;
    }
    return written;
}

void RequireTriangle(const char* phase) {
    const auto written = WrittenPixels();
    Require(written == Width * Height, std::string("queued index read: ") + phase + ": the draw left " + std::to_string(Width * Height - written) + " pixels cleared, expected the triangle its indices and vertices name");
}

void LandPool() {
    const auto landBy = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Pool)), PoolBytes);
        AgcDriverWaitIdle_nid_postfix();
    } while (IndicesIntact() && std::chrono::steady_clock::now() < landBy);
    Require(!IndicesIntact(), "queued index read: the image's results never reached the memory they cover, so the run above proves nothing");
}

void WaitForWord(std::uint64_t address, std::uint32_t value, const char* what) {
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(10);
    while (*reinterpret_cast<const volatile std::uint32_t*>(address) != value) {
        if (std::chrono::steady_clock::now() >= deadline) {
            char message[256];
            std::snprintf(message, sizeof(message), "queued index read: %s: 0x%08llx held 0x%08x after %.1f s, expected 0x%08x", what, static_cast<unsigned long long>(address), *reinterpret_cast<const volatile std::uint32_t*>(address), std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), value);
            Require(false, message);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::atomic<bool> GateOpen{true};

void GateUntilQueued() {
    AgcDriver::DriverDetail::DrawPipeline::Queue0().Enqueue([] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (AgcDriver::DriverDetail::DrawPipeline::QueuedItems() < 2 || !GateOpen.load()) {
            if (std::chrono::steady_clock::now() >= deadline) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }, {});
}

void WaitForCommits() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (AgcDriver::DriverDetail::DrawPipeline::QueuedItems() != 0) {
        Require(std::chrono::steady_clock::now() < deadline, "queued index read: the draw commit never ran");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void WaitForHeldRange(std::uint64_t address, std::size_t bytes, const char* what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!AgcDriver::DriverDetail::DrawPipeline::HeldReads(address, bytes) && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool held = AgcDriver::DriverDetail::DrawPipeline::HeldReads(address, bytes);
    GateOpen = true;
    Require(held, std::string("queued index read: the queued draw holds no read of its ") + what + " range");
}

void CopyPendingImage(std::size_t bytes, const char* phase) {
    const auto poolAddress = reinterpret_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Pool));
    ResetPool();
    std::memset(Copied, 0, PoolBytes);
    Submit(ImageWrite(), phase);
    AgcDriverWaitIdle_nid_postfix();
    Require(AgcDriver::Graphics::StorageTexture::AnyPending(), std::string("queued index read: ") + phase + ": the image write left no pending result to order against");
    GateOpen = false;
    GateUntilQueued();
    Submit(CopyPool(bytes), phase);
    WaitForHeldRange(poolAddress, bytes, "copy source");
    WaitForCommits();
    AgcDriverWaitIdle_nid_postfix();
    LandPool();
    RequireCopiedImage(bytes, phase);
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
    _putenv_s("APS5_PIPELINE_DMA", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
    setenv("APS5_PIPELINE_DMA", "1", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        ColorMemory = AllocateWatched(2 * Block);
        Pool = AllocateWatched(PoolBytes);
        Copied = AllocateWatched(PoolBytes);
        GuestAllocations::Mutation().Add(Pool, PoolBytes, true, true, true);
        GuestAllocations::Mutation().Add(Copied, PoolBytes, true, true, true);
        Register(VertexShader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelShader, PixelCode.data(), sizeof(PixelCode), 1);
        std::memset(Pool, Sentinel, PoolBytes);
        std::memcpy(Pool, Indices.data(), Indices.size() * sizeof(std::uint32_t));
        for (std::size_t i = 0; i < Vertices.size(); ++i) Vertices[i] = {{PositionX[i], PositionY[i], 0.5f, 1.0f}, {Negative[i], Negative[i], Negative[i], 0.0f}};
        std::memcpy(Pool + VertexOffset, Vertices.data(), sizeof(Vertex) * Vertices.size());

        const auto poolAddress = reinterpret_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Pool));
        const auto vertexAddress = poolAddress + VertexOffset;
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> poolRange{{
            {poolAddress, static_cast<std::uint64_t>(PoolBytes)}}};
        Submit(ImageWrite(), "the image write");
        AgcDriverWaitIdle_nid_postfix();
        Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "queued index read: the image write left no pending result to order against");
        Require(IndicesIntact(), "queued index read: the image's results reached guest memory before any consumer asked");
        GateUntilQueued();
        std::fill_n(ColorMemory, ColorBytes, std::byte{0});
        Submit(Draw(Vertices.data(), Vertices.size()), "the indexed draw");
        WaitForCommits();
        Require(IndicesIntact(), "queued index read: the queued draw's commit stored the image's results over its index buffer");
        AgcDriverWaitIdle_nid_postfix();
        RequireTriangle("the index read");
        LandPool();

        ResetPool();
        Submit(ImageWrite(), "the second image write");
        AgcDriverWaitIdle_nid_postfix();
        Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "queued index read: the second image write left no pending result to order against");
        GateOpen = false;
        GateUntilQueued();
        std::fill_n(ColorMemory, ColorBytes, std::byte{0});
        Submit(Draw(Pool + VertexOffset, Vertices.size()), "the draw whose vertices the image covers");
        WaitForHeldRange(vertexAddress, sizeof(Vertex) * Vertices.size(), "vertex");
        WaitForCommits();
        AgcDriverWaitIdle_nid_postfix();
        RequireTriangle("the vertex read");
        LandPool();

        std::array<std::uint32_t, 3> texels{};
        for (std::size_t i = 0; i < texels.size(); ++i) texels[i] = PoolWord(i * sizeof(std::uint32_t));
        const auto sentinelWord = static_cast<std::uint32_t>(Sentinel) * 0x01010101u;
        std::size_t awaited = texels.size();
        for (std::size_t i = 0; i < texels.size(); ++i) {
            if (texels[i] != Indices[i] && texels[i] != sentinelWord) {
                awaited = i;
                break;
            }
        }
        Require(awaited != texels.size(), "queued index read: the image's first texels repeat the indices and the sentinel, so a wait on them proves nothing");
        const auto awaitedAddress = poolAddress + awaited * sizeof(std::uint32_t);
        ResetPool();

        Submit(ImageWrite(), "the third image write");
        AgcDriverWaitIdle_nid_postfix();
        Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "queued index read: the third image write left no pending result to order against");
        std::fill_n(ColorMemory, ColorBytes, std::byte{0});
        Submit(Draw(Pool + VertexOffset, Vertices.size()), "the draw a wait behind it reads the held range of");
        Submit(WaitRegMem(awaitedAddress, texels[awaited]), "the wait over the held range");
        WaitForWord(awaitedAddress, texels[awaited], "the wait over the held range was not satisfied");
        AgcDriverWaitIdle_nid_postfix();
        RequireTriangle("the wait over the held range");
        Require(!VerticesIntact(), "queued index read: the image's results never reached the memory the wait covered");
        CopyPendingImage(4096, "the queued copy of the image");
        std::puts("queued index read vs. deferred storage write-back tests passed");
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
