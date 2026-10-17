#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::DriverDetail::DrawPipeline;

constexpr std::uint32_t Width = 160;
constexpr std::uint32_t Height = 96;
constexpr std::uint32_t Texels = Width * Height;
constexpr std::uint32_t Threads = 1024;
constexpr std::uint32_t Format8Uint = 5;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t SetContextReg = 0x69u;
constexpr std::uint32_t SetShReg = 0x76u;
constexpr std::uint32_t SetUconfigReg = 0x79u;
constexpr std::uint32_t DrawIndexAuto = 0x2du;
constexpr std::uint32_t DispatchDirect = 0x15u;
constexpr std::size_t Block = 65536;
constexpr std::uint32_t CompressDisable = 0x60u;
constexpr std::uint32_t ZInfo = 0x20000183u;
constexpr std::uint32_t Z16Info = 0x20000181u;
constexpr std::uint32_t Format16Unorm = 7;
constexpr std::uint32_t StencilInfo = 0x20000181u;
constexpr std::uint32_t NoStencilInfo = 0x20000180u;
constexpr std::uint32_t ColorDisabled = 0x00cc0000u;
constexpr std::uint32_t DepthWriteAlways = 0x76u;
constexpr std::uint32_t StencilReplaceAlways = DepthWriteAlways | 1u | (7u << 8u);
constexpr std::uint32_t StencilPassReplace = 3u << 4u;
constexpr std::uint32_t StencilValue = 0x5au;
constexpr float XSlope = 2.0f * Width / 65536.0f;
constexpr float YSlope = 512.0f * Height / 65536.0f;
constexpr float SecondOffset = 0.5f;
constexpr float FlatDepth = 0.25f;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000u, 0x80020005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 10> LoadCode{
    0x34020084u, 0xe0381000u, 0x80000201u, 0xbf8c3f70u, 0xf0001f08u, 0x00020602u, 0xbf8c3f70u, 0xe0781000u, 0x80010601u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 8> BufferLoadCode{
    0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x34020084u, 0xe0701000u, 0x80010201u, 0xbf810000u,
};

using Triangle = std::array<std::array<float, 4>, 3>;

alignas(256) Triangle Gradient{{
    {-1.0f, -1.0f, 0.0f, 1.0f}, {3.0f, -1.0f, XSlope, 1.0f}, {-1.0f, 3.0f, YSlope, 1.0f},
}};

alignas(256) Triangle SecondGradient{{
    {-1.0f, -1.0f, SecondOffset, 1.0f}, {3.0f, -1.0f, SecondOffset + XSlope, 1.0f}, {-1.0f, 3.0f, SecondOffset + YSlope, 1.0f},
}};

alignas(256) Triangle Corner{{
    {-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 1.0f, 0.0f, 1.0f},
}};

alignas(256) Triangle Flat{{
    {-1.0f, -1.0f, FlatDepth, 1.0f}, {3.0f, -1.0f, FlatDepth, 1.0f}, {-1.0f, 3.0f, FlatDepth, 1.0f},
}};

struct RegisteredShader {
    Shader shader;
    ShaderUserData userData;
};
RegisteredShader VertexShader{};
RegisteredShader PixelShader{};

std::uint8_t* NewBlock(std::size_t bytes) {
#ifdef _WIN32
    auto* block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint8_t*>(std::aligned_alloc(Block, bytes));
#endif
    Require(block != nullptr, "depth expand review: cannot allocate a guest block");
    std::memset(block, 0, bytes);
    GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    return block;
}

std::uint64_t Address(const void* data) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }

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

void SetRegister(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    words.push_back(0xc0010000u | (opcode << 8u));
    words.push_back(offset);
    words.push_back(value);
}

void SetBase(std::vector<std::uint32_t>& words, std::uint32_t low, std::uint32_t high, std::uint64_t address) {
    SetRegister(words, SetContextReg, low, static_cast<std::uint32_t>(address >> 8u));
    SetRegister(words, SetContextReg, high, static_cast<std::uint32_t>(address >> 40u));
}

void Submit(std::vector<std::uint32_t> words, const std::string& what) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, what + ": the DCB submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

struct Blocker {
    std::atomic<bool> release{false};
    void Enqueue() {
        DrawPipeline::Queue0().Enqueue([this] {
            while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
        }, {});
    }
};

void SubmitBehind(Blocker& blocker, std::vector<std::uint32_t>& words) {
    std::unique_lock gpuLock(AgcDriver::GuestMemory::GpuMutex());
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    blocker.Enqueue();
}

void ReleaseWhenQueued(Blocker& blocker, std::size_t items, const std::string& what) {
    const auto start = std::chrono::steady_clock::now();
    bool queued = false;
    while (!(queued = DrawPipeline::Queue0().Outstanding() >= items) && std::chrono::steady_clock::now() - start < std::chrono::seconds(20)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (queued) std::this_thread::sleep_for(std::chrono::seconds(2));
    blocker.release = true;
    Require(queued, "depth expand review: " + what + ": the depth draw was not queued behind the held draw pipeline");
}

void FinishQueued(const std::string& what) {
    auto idle = std::async(std::launch::async, [] { AgcDriverWaitIdle_nid_postfix(); });
    if (idle.wait_for(std::chrono::seconds(20)) != std::future_status::ready) {
        std::fprintf(stderr, "depth expand review: %s: the queue worker and the draw pipeline deadlocked\n", what.c_str());
        std::fflush(stderr);
        std::_Exit(1);
    }
    idle.get();
}

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint64_t address, std::uint32_t format) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | ((ZInfo >> 4u) & 0x1fu) << 20u | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

struct Surfaces {
    std::uint64_t depth;
    std::uint64_t stencil;
    std::uint64_t htile;
};

Surfaces NewSurfaces() {
    return {
        Address(NewBlock(static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 4u)))),
        Address(NewBlock(static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 1u)))),
        Address(NewBlock(Block)),
    };
}

std::uint64_t NewPlane() {
    return Address(NewBlock(static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 4u))));
}

struct DrawState {
    std::uint32_t renderControl = 0;
    std::uint32_t depthControl = DepthWriteAlways;
    std::uint32_t zInfo = ZInfo;
    std::uint32_t view = 0;
    std::uint32_t stencilInfo = StencilInfo;
    std::uint32_t stencilControl = 0;
    std::uint32_t stencilReference = 0;
    std::uint64_t writeBase = 0;
    std::uint64_t stencilWriteBase = 0;
    const Triangle* vertices = &Gradient;
    bool pixelShader = false;
};

DrawState DepthDrawn(const Surfaces& surfaces, const Triangle& vertices, std::uint32_t stencilInfo = StencilInfo) {
    DrawState state;
    state.stencilInfo = stencilInfo;
    state.writeBase = surfaces.depth;
    state.vertices = &vertices;
    return state;
}

DrawState Expand(std::uint64_t writeBase, std::uint32_t stencilInfo = StencilInfo) {
    DrawState state;
    state.renderControl = CompressDisable;
    state.depthControl = 0;
    state.stencilInfo = stencilInfo;
    state.writeBase = writeBase;
    state.pixelShader = true;
    return state;
}

std::vector<std::uint32_t> Draw(const Surfaces& surfaces, const DrawState& state) {
    const auto extent = (Height << 16u) | Width;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000}, {0x1b6, 0}, {0x203, 0}, {0x2dc, 0xaa00}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa}, {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240}, {0x8f, 0}, {0x202, ColorDisabled},
        {0x1c4, 0}, {0x1c5, 0}, {0x1c3, 4}, {0x207, 0}, {0xc, 0}, {0x81, 0x80000000}, {0x90, 0x80000000}, {0x94, 0x80000000},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0}, {0x113, std::bit_cast<std::uint32_t>(1.0f)},
        {0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>(Width / 2.0f)},
        {0x111, std::bit_cast<std::uint32_t>(Height / 2.0f)}, {0x112, std::bit_cast<std::uint32_t>(Height / 2.0f)},
        {0xd, extent}, {0x82, extent}, {0x91, extent}, {0x95, extent},
        {0x002, state.view}, {0x007, ((Height - 1u) << 16u) | (Width - 1u)}, {0x00a, 0}, {0x00b, std::bit_cast<std::uint32_t>(1.0f)},
        {0x010, state.zInfo}, {0x011, state.stencilInfo}, {0x201, 0}, {0x10b, state.stencilControl}, {0x10c, state.stencilReference}, {0x10d, 0},
        {0x000, state.renderControl}, {0x200, state.depthControl},
        {0x8e, state.pixelShader ? 0xffffffffu : 0u}, {0x2f8, 0u}, {0x292, 2u},
    };
    std::vector<std::uint32_t> words;
    for (const auto& [offset, value] : context) SetRegister(words, SetContextReg, offset, value);
    for (const auto offset : {0x2feu, 0x302u, 0x306u, 0x30au}) SetRegister(words, SetContextReg, offset, 0u);
    SetBase(words, 0x005, 0x01e, surfaces.htile);
    SetBase(words, 0x012, 0x01a, surfaces.depth);
    SetBase(words, 0x014, 0x01c, state.writeBase);
    SetBase(words, 0x013, 0x01b, surfaces.stencil);
    SetBase(words, 0x015, 0x01d, state.stencilWriteBase != 0 ? state.stencilWriteBase : surfaces.stencil);
    SetRegister(words, SetUconfigReg, 0x242, 4u);
    const auto pixel = state.pixelShader ? Address(PixelCode.data()) : 0u;
    const auto vertex = Address(VertexCode.data());
    SetRegister(words, SetShReg, 0x008, static_cast<std::uint32_t>(pixel >> 8u));
    SetRegister(words, SetShReg, 0x009, static_cast<std::uint32_t>(pixel >> 40u));
    SetRegister(words, SetShReg, 0x00b, 0u);
    SetRegister(words, SetShReg, 0x0c8, static_cast<std::uint32_t>(vertex >> 8u));
    SetRegister(words, SetShReg, 0x0c9, static_cast<std::uint32_t>(vertex >> 40u));
    SetRegister(words, SetShReg, 0x08b, 4u << 1u);
    const auto address = Address(state.vertices->data());
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(state.vertices->size()), 0x01016facu};
    for (std::uint32_t i = 0; i < descriptor.size(); ++i) SetRegister(words, SetShReg, 0x08c + i, descriptor[i]);
    words.insert(words.end(), {0xc0000000u | (1u << 16u) | (DrawIndexAuto << 8u), static_cast<std::uint32_t>(state.vertices->size()), 2u});
    return words;
}

class TexelReader {
public:
    TexelReader() : kernel(NewBlock(Block)), bufferKernel(NewBlock(Block)), coordinates(NewBlock(Block)), output(NewBlock(Block)) {
        std::memcpy(kernel, LoadCode.data(), sizeof(LoadCode));
        std::memcpy(bufferKernel, BufferLoadCode.data(), sizeof(BufferLoadCode));
    }

    std::vector<std::uint32_t> BufferLoad(std::uint64_t address, std::uint32_t bytes) {
        std::array<std::uint32_t, 16> userData{};
        const auto input = BufferDescriptor(address, bytes);
        const auto stored = BufferDescriptor(Address(output), Threads * 16u);
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(stored.begin(), stored.end(), userData.begin() + 4);
        std::vector<std::uint32_t> packet;
        const auto code = Address(bufferKernel);
        SetRegister(packet, SetShReg, 0x20c, static_cast<std::uint32_t>(code >> 8u));
        SetRegister(packet, SetShReg, 0x20d, static_cast<std::uint32_t>(code >> 40u) & 0xffu);
        SetRegister(packet, SetShReg, 0x207, Threads);
        SetRegister(packet, SetShReg, 0x208, 1u);
        SetRegister(packet, SetShReg, 0x209, 1u);
        SetRegister(packet, SetShReg, 0x213, 16u << 1u);
        for (std::uint32_t i = 0; i < userData.size(); ++i) SetRegister(packet, SetShReg, 0x240 + i, userData[i]);
        packet.insert(packet.end(), {0xc0000000u | (3u << 16u) | (DispatchDirect << 8u), 1u, 1u, 1u, 0x41u});
        return packet;
    }

    std::vector<std::byte> ReadBuffer(std::uint64_t address, std::size_t bytes, const std::string& what) {
        std::vector<std::byte> data(bytes);
        std::array<std::uint32_t, Threads * 4> words{};
        for (std::size_t first = 0; first < bytes; first += Threads * 4u) {
            const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(Threads * 4u, bytes - first));
            std::memset(output, 0xff, sizeof(words));
            Submit(BufferLoad(address + first, count), what);
            AgcDriver::GuestMemory::Read(Address(output), std::as_writable_bytes(std::span(words)));
            for (std::uint32_t thread = 0; thread * 4u < count; ++thread) std::memcpy(data.data() + first + thread * 4u, &words[thread * 4u], sizeof(std::uint32_t));
        }
        return data;
    }

    std::vector<std::uint32_t> Read(std::uint64_t surface, std::uint32_t format, const std::string& what) {
        std::vector<std::uint32_t> texels(Texels);
        std::array<std::uint32_t, Threads * 4> words{};
        for (std::uint32_t first = 0; first < Texels; first += Threads) {
            words.fill(0);
            for (std::uint32_t thread = 0; thread < Threads; ++thread) {
                const auto texel = std::min(first + thread, Texels - 1u);
                words[thread * 4u] = texel % Width;
                words[thread * 4u + 1u] = texel / Width;
            }
            std::memcpy(coordinates, words.data(), sizeof(words));
            std::memset(output, 0xff, sizeof(words));
            std::array<std::uint32_t, 16> userData{};
            const auto input = BufferDescriptor(Address(coordinates), sizeof(words));
            const auto stored = BufferDescriptor(Address(output), sizeof(words));
            const auto texture = TextureDescriptor(surface, format);
            std::copy(input.begin(), input.end(), userData.begin());
            std::copy(stored.begin(), stored.end(), userData.begin() + 4);
            std::copy(texture.begin(), texture.end(), userData.begin() + 8);
            std::vector<std::uint32_t> packet;
            const auto code = Address(kernel);
            SetRegister(packet, SetShReg, 0x20c, static_cast<std::uint32_t>(code >> 8u));
            SetRegister(packet, SetShReg, 0x20d, static_cast<std::uint32_t>(code >> 40u) & 0xffu);
            SetRegister(packet, SetShReg, 0x207, Threads);
            SetRegister(packet, SetShReg, 0x208, 1u);
            SetRegister(packet, SetShReg, 0x209, 1u);
            SetRegister(packet, SetShReg, 0x213, 16u << 1u);
            for (std::uint32_t i = 0; i < userData.size(); ++i) SetRegister(packet, SetShReg, 0x240 + i, userData[i]);
            packet.insert(packet.end(), {0xc0000000u | (3u << 16u) | (DispatchDirect << 8u), 1u, 1u, 1u, 0x41u});
            Submit(std::move(packet), what);
            AgcDriver::GuestMemory::Read(Address(output), std::as_writable_bytes(std::span(words)));
            for (std::uint32_t thread = 0; thread < Threads && first + thread < Texels; ++thread) texels[first + thread] = words[thread * 4u];
        }
        return texels;
    }

private:
    std::uint8_t* kernel;
    std::uint8_t* bufferKernel;
    std::uint8_t* coordinates;
    std::uint8_t* output;
};

std::uint64_t EquationOffset(const TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < 16; ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12) & (mask & 0xfff000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

std::vector<std::uint32_t> Detiled(std::uint64_t surface, std::span<const std::byte> raw, std::uint32_t format = Format32Float, std::uint32_t texelBytes = 4u) {
    const auto descriptor = DecodeTextureResource(TextureDescriptor(surface, format));
    const auto geometry = DescribeSurface(descriptor);
    Require(geometry.guestBytes == raw.size(), "depth expand review: the view of the plane is not the size of the plane");
    const auto* equation = FindTextureSwizzleEquation(XorSwizzleMode(descriptor.tileMode), texelBytes);
    Require(equation != nullptr, "depth expand review: the SW_64KB_Z_X equation for the plane's texels is missing");
    const auto& mip = geometry.mips.at(0);
    const auto layout = ThinBlockLayout(descriptor.tileMode, texelBytes);
    std::vector<std::uint32_t> texels(Texels);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            auto offset = mip.tiledOffset;
            if (mip.tail) offset += EquationOffset(*equation, x + mip.tailX, y + mip.tailY);
            else offset += (static_cast<std::uint64_t>(y / layout[2]) * mip.blocksPerRow + x / layout[1]) * layout[0] + EquationOffset(*equation, x, y);
            std::memcpy(&texels[y * Width + x], raw.data() + offset, texelBytes);
        }
    }
    return texels;
}

std::vector<std::uint32_t> StoredPlane(std::uint64_t surface) {
    std::vector<std::byte> raw(static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 4u)));
    AgcDriver::GuestMemory::Read(surface, raw);
    return Detiled(surface, raw);
}

std::vector<std::byte> GuestBytes(std::uint64_t address, std::size_t bytes) {
    std::vector<std::byte> data(bytes);
    AgcDriver::GuestMemory::Read(address, data);
    return data;
}

float Expected(std::uint32_t x, std::uint32_t y, float offset) {
    return offset + (static_cast<float>(x) + 0.5f + 256.0f * (static_cast<float>(y) + 0.5f)) / 65536.0f;
}

std::string Hex(std::uint32_t value) {
    char text[48];
    std::snprintf(text, sizeof(text), "0x%08x (%.9g)", value, static_cast<double>(std::bit_cast<float>(value)));
    return text;
}

void RequireSame(const std::vector<std::uint32_t>& actual, const std::vector<std::uint32_t>& expected, const std::string& what) {
    std::uint32_t matching = 0;
    std::uint32_t first = Texels;
    for (std::uint32_t texel = 0; texel < Texels; ++texel) {
        if (actual[texel] == expected[texel]) ++matching;
        else if (first == Texels) first = texel;
    }
    std::printf("  %s: %u of %u texels match", what.c_str(), matching, Texels);
    if (first != Texels) std::printf("; first difference at (%u, %u): %s, expected %s", first % Width, first / Width, Hex(actual[first]).c_str(), Hex(expected[first]).c_str());
    std::printf("\n");
    Require(matching == Texels, "depth expand review: " + what);
}

std::vector<std::uint32_t> RequireDepth(TexelReader& reader, std::uint64_t surface, const Triangle& vertices, const std::string& what) {
    const auto depth = reader.Read(surface, Format32Float, what);
    std::uint32_t drawn = 0;
    for (std::uint32_t texel = 0; texel < Texels; ++texel) {
        const auto x = texel % Width;
        const auto y = texel / Width;
        const auto expected = &vertices == &Flat ? FlatDepth : Expected(x, y, &vertices == &SecondGradient ? SecondOffset : 0.0f);
        if (std::fabs(std::bit_cast<float>(depth[texel]) - expected) <= 0.5f / 65536.0f) ++drawn;
    }
    std::printf("  %s: %u of %u texels hold the drawn depth; texel (0, 0) %s\n", what.c_str(), drawn, Texels, Hex(depth.front()).c_str());
    Require(drawn == Texels, "depth expand review: " + what + " does not hold the drawn depth");
    return depth;
}

void RequireExpanded(TexelReader& reader, std::uint64_t written, const std::vector<std::uint32_t>& depth, const std::string& what) {
    RequireSame(reader.Read(written, Format32Float, what + ": texture read of the write base"), depth, what + ": texture read of the write base (R32F)");
    RequireSame(StoredPlane(written), depth, what + ": raw read of the write base after a flush");
}

void DepthOnlySurface(TexelReader& reader) {
    std::puts("an expand of a surface without a stencil plane");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    Submit(Draw(surfaces, DepthDrawn(surfaces, Gradient, NoStencilInfo)), "depth-only surface: depth draw");
    Submit(Draw(surfaces, Expand(written, NoStencilInfo)), "depth-only surface: DB expand");
    const auto depth = RequireDepth(reader, surfaces.depth, Gradient, "depth-only surface: read base");
    RequireExpanded(reader, written, depth, "depth-only surface");
}

void StencilKept(TexelReader& reader) {
    std::puts("an expand of a surface with a drawn stencil plane");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    auto draw = DepthDrawn(surfaces, Gradient);
    draw.depthControl = StencilReplaceAlways;
    draw.stencilControl = StencilPassReplace;
    draw.stencilReference = StencilValue | (0xffu << 8u) | (0xffu << 16u);
    Submit(Draw(surfaces, draw), "stencil surface: depth and stencil draw");
    const auto stencilBefore = reader.Read(surfaces.stencil, Format8Uint, "stencil surface: stencil before the expand");
    RequireSame(stencilBefore, std::vector<std::uint32_t>(Texels, StencilValue), "stencil surface: stencil drawn before the expand");
    const auto guestDepth = GuestBytes(surfaces.depth, static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 4u)));
    const auto guestStencil = GuestBytes(surfaces.stencil, static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 1u)));
    const auto guestHtile = GuestBytes(surfaces.htile, Block);
    Submit(Draw(surfaces, Expand(written)), "stencil surface: DB expand");
    Require(GuestBytes(surfaces.depth, guestDepth.size()) == guestDepth, "depth expand review: the expand changed the read base's guest bytes");
    Require(GuestBytes(surfaces.stencil, guestStencil.size()) == guestStencil, "depth expand review: the expand changed the stencil plane's guest bytes");
    Require(GuestBytes(surfaces.htile, guestHtile.size()) == guestHtile, "depth expand review: the expand changed the HTILE guest bytes");
    std::puts("  the read base, stencil and HTILE guest bytes are unchanged");
    RequireSame(reader.Read(surfaces.stencil, Format8Uint, "stencil surface: stencil after the expand"), stencilBefore, "stencil surface: stencil after the expand");
    const auto depth = RequireDepth(reader, surfaces.depth, Gradient, "stencil surface: read base after the expand");
    RequireExpanded(reader, written, depth, "stencil surface");
}

void InPlaceUnchanged(TexelReader& reader) {
    std::puts("an in-place expand (write base equal to read base)");
    const auto surfaces = NewSurfaces();
    Submit(Draw(surfaces, DepthDrawn(surfaces, Gradient)), "in-place expand: depth draw");
    const auto before = RequireDepth(reader, surfaces.depth, Gradient, "in-place expand: read base before");
    Submit(Draw(surfaces, Expand(surfaces.depth)), "in-place expand: DB expand onto the read base");
    RequireSame(reader.Read(surfaces.depth, Format32Float, "in-place expand: read base after"), before, "in-place expand: read base after the expand");
}

void AlwaysPassEqualBases(TexelReader& reader) {
    std::puts("an always-pass depth draw with equal bases and compress-disable bits");
    const auto surfaces = NewSurfaces();
    Submit(Draw(surfaces, DepthDrawn(surfaces, Gradient)), "always-pass draw: first depth draw");
    auto draw = DepthDrawn(surfaces, Flat);
    draw.renderControl = CompressDisable;
    Submit(Draw(surfaces, draw), "always-pass draw: compress-disable depth draw");
    static_cast<void>(RequireDepth(reader, surfaces.depth, Flat, "always-pass draw: read base"));
}

void ExpandedAgain(TexelReader& reader) {
    std::puts("the same expand after the read surface changed");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    Submit(Draw(surfaces, DepthDrawn(surfaces, Gradient)), "second expand: first depth draw");
    Submit(Draw(surfaces, Expand(written)), "second expand: first DB expand");
    RequireExpanded(reader, written, RequireDepth(reader, surfaces.depth, Gradient, "second expand: first read base"), "second expand, first");
    Submit(Draw(surfaces, DepthDrawn(surfaces, SecondGradient)), "second expand: second depth draw");
    Submit(Draw(surfaces, Expand(written)), "second expand: second DB expand");
    const auto depth = RequireDepth(reader, surfaces.depth, SecondGradient, "second expand: second read base");
    RequireExpanded(reader, written, depth, "second expand, second");
}

void BufferReadAfterExpand(TexelReader& reader) {
    std::puts("a GPU buffer read of the write base right after the expand");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    Submit(Draw(surfaces, DepthDrawn(surfaces, Gradient)), "buffer read: depth draw");
    Submit(Draw(surfaces, Expand(written)), "buffer read: DB expand");
    const auto raw = reader.ReadBuffer(written, static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 4u)), "buffer read: raw buffer loads of the write base");
    const auto depth = RequireDepth(reader, surfaces.depth, Gradient, "buffer read: read base");
    RequireSame(Detiled(written, raw), depth, "buffer read: buffer loads of the write base, detiled with the SW_64KB_Z_X equation");
}

void TwoDestinations(TexelReader& reader) {
    std::puts("one submission expanding one surface to two write bases");
    const auto surfaces = NewSurfaces();
    const auto first = NewPlane();
    const auto second = NewPlane();
    Submit(Draw(surfaces, DepthDrawn(surfaces, Gradient)), "two write bases: depth draw");
    auto words = Draw(surfaces, Expand(first));
    const auto more = Draw(surfaces, Expand(second));
    words.insert(words.end(), more.begin(), more.end());
    Submit(std::move(words), "two write bases: two DB expands");
    const auto depth = RequireDepth(reader, surfaces.depth, Gradient, "two write bases: read base");
    RequireExpanded(reader, first, depth, "two write bases, first");
    RequireExpanded(reader, second, depth, "two write bases, second");
}

void SubmitQueued(TexelReader& reader, const std::vector<std::uint32_t>& draw, const std::vector<std::uint32_t>& expand, const std::string& what) {
    auto words = reader.BufferLoad(Address(NewBlock(Block)), 64u);
    words.insert(words.end(), draw.begin(), draw.end());
    words.insert(words.end(), expand.begin(), expand.end());
    Blocker blocker;
    SubmitBehind(blocker, words);
    ReleaseWhenQueued(blocker, 2, what);
    FinishQueued(what);
}

void ExpandBehindQueuedDraw(TexelReader& reader) {
    std::puts("an expand right after a depth draw still queued for the draw pipeline's committer");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    Submit(Draw(surfaces, DepthDrawn(surfaces, Gradient)), "queued draw: first depth draw");
    SubmitQueued(reader, Draw(surfaces, DepthDrawn(surfaces, SecondGradient)), Draw(surfaces, Expand(written)), "queued draw");
    RequireExpanded(reader, written, RequireDepth(reader, surfaces.depth, SecondGradient, "queued draw: read base"), "queued draw");
}

void RepeatedExpandBehindQueuedDraw(TexelReader& reader) {
    std::puts("a repeated expand right after a queued depth draw changed its read surface");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    Submit(Draw(surfaces, DepthDrawn(surfaces, Gradient)), "queued repeat: first depth draw");
    Submit(Draw(surfaces, Expand(written)), "queued repeat: first DB expand");
    SubmitQueued(reader, Draw(surfaces, DepthDrawn(surfaces, SecondGradient)), Draw(surfaces, Expand(written)), "queued repeat");
    RequireExpanded(reader, written, RequireDepth(reader, surfaces.depth, SecondGradient, "queued repeat: read base"), "queued repeat");
}

void FirstDrawQueuedBeforeExpand(TexelReader& reader) {
    std::puts("an expand right after the queued depth draw that creates its read surface");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    SubmitQueued(reader, Draw(surfaces, DepthDrawn(surfaces, Gradient)), Draw(surfaces, Expand(written)), "queued first draw");
    RequireExpanded(reader, written, RequireDepth(reader, surfaces.depth, Gradient, "queued first draw: read base"), "queued first draw");
}

void QueuedDraws(TexelReader& reader) {
    if (DrawPipeline::Depth() < 2) {
        std::puts("expands after queued depth draws: skipped, pipelined draws are off");
        return;
    }
    const std::array<void (*)(TexelReader&), 3> cases{ExpandBehindQueuedDraw, RepeatedExpandBehindQueuedDraw, FirstDrawQueuedBeforeExpand};
    std::size_t failed = 0;
    for (const auto run : cases) {
        try {
            run(reader);
        } catch (const std::exception& error) {
            std::printf("  failed: %s\n", error.what());
            ++failed;
        }
    }
    Require(failed == 0, "depth expand review: " + std::to_string(failed) + " of " + std::to_string(cases.size()) + " expands after queued depth draws failed");
}

void SixteenBitPlane(TexelReader& reader) {
    std::puts("an expand of a Z_16 surface");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    auto draw = DepthDrawn(surfaces, Gradient, NoStencilInfo);
    draw.zInfo = Z16Info;
    Submit(Draw(surfaces, draw), "Z_16 surface: depth draw");
    auto expand = Expand(written, NoStencilInfo);
    expand.zInfo = Z16Info;
    Submit(Draw(surfaces, expand), "Z_16 surface: DB expand");
    const auto depth = reader.Read(surfaces.depth, Format16Unorm, "Z_16 surface: read base");
    std::uint32_t drawn = 0;
    for (std::uint32_t texel = 0; texel < Texels; ++texel) {
        if (std::fabs(std::bit_cast<float>(depth[texel]) - Expected(texel % Width, texel / Width, 0.0f)) <= 1.0f / 65535.0f) ++drawn;
    }
    std::printf("  Z_16 surface: read base: %u of %u texels hold the drawn depth within one 16-bit step; texel (0, 0) %s\n", drawn, Texels, Hex(depth.front()).c_str());
    Require(drawn == Texels, "depth expand review: Z_16 surface: the read base does not hold the drawn depth");
    RequireSame(reader.Read(written, Format16Unorm, "Z_16 surface: texture read of the write base"), depth, "Z_16 surface: texture read of the write base (R16 UNORM)");
    std::vector<std::byte> raw(static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 2u)));
    AgcDriver::GuestMemory::Read(written, raw);
    std::vector<std::uint32_t> quantized(Texels);
    for (std::uint32_t texel = 0; texel < Texels; ++texel) quantized[texel] = static_cast<std::uint32_t>(std::lround(std::bit_cast<float>(depth[texel]) * 65535.0f));
    RequireSame(Detiled(written, raw, Format16Unorm, 2u), quantized, "Z_16 surface: raw read of the write base after a flush, detiled with the 16-bit SW_64KB_Z_X equation");
}

void SecondSlice(TexelReader& reader) {
    std::puts("an expand of the second slice of a depth array");
    const auto depthSlice = static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 4u));
    const Surfaces surfaces{Address(NewBlock(2u * depthSlice)), Address(NewBlock(2u * static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 1u)))), Address(NewBlock(2u * Block))};
    const auto written = Address(NewBlock(2u * depthSlice));
    const std::vector<std::uint32_t> marker(depthSlice / sizeof(std::uint32_t), 0x3f000000u);
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(written)), marker.data(), depthSlice);
    constexpr std::uint32_t SliceOne = 1u | (1u << 13u);
    auto draw = DepthDrawn(surfaces, Gradient);
    draw.view = SliceOne;
    Submit(Draw(surfaces, draw), "second slice: depth draw into slice 1");
    auto expand = Expand(written);
    expand.view = SliceOne;
    Submit(Draw(surfaces, expand), "second slice: DB expand of slice 1");
    const auto depth = RequireDepth(reader, surfaces.depth + depthSlice, Gradient, "second slice: slice 1 of the read base");
    RequireExpanded(reader, written + depthSlice, depth, "second slice: slice 1 of the write base");
    const auto first = GuestBytes(written, depthSlice);
    Require(std::memcmp(first.data(), marker.data(), depthSlice) == 0, "depth expand review: second slice: the expand of slice 1 changed slice 0 of the write base");
    std::puts("  slice 0 of the write base still holds what the CPU wrote");
}

void StencilMoved(TexelReader& reader, bool depthMoves) {
    const auto name = std::string(depthMoves ? "depth and stencil expand" : "stencil-only expand");
    std::puts(depthMoves ? "an out-of-place expand of the depth and stencil planes" : "an out-of-place expand of the stencil plane alone, the depth plane in place");
    const auto surfaces = NewSurfaces();
    const auto written = NewPlane();
    const auto stencilBytes = static_cast<std::size_t>(DepthSliceBytes({Width, Height}, 1u));
    const auto stencilWritten = Address(NewBlock(stencilBytes));
    auto draw = DepthDrawn(surfaces, Gradient);
    draw.depthControl = StencilReplaceAlways;
    draw.stencilControl = StencilPassReplace;
    draw.stencilReference = StencilValue | (0xffu << 8u) | (0xffu << 16u);
    Submit(Draw(surfaces, draw), name + ": depth and stencil draw");
    auto corner = draw;
    corner.depthControl = StencilReplaceAlways & ~4u;
    corner.vertices = &Corner;
    corner.stencilReference = 0xa5u | (0xffu << 8u) | (0xffu << 16u);
    Submit(Draw(surfaces, corner), name + ": stencil draw over half the surface");
    const auto stencil = reader.Read(surfaces.stencil, Format8Uint, name + ": stencil of the read surface");
    const auto first = static_cast<std::uint32_t>(std::count(stencil.begin(), stencil.end(), StencilValue));
    const auto second = static_cast<std::uint32_t>(std::count(stencil.begin(), stencil.end(), 0xa5u));
    std::printf("  %s: read stencil: %u texels hold 0x%02x and %u hold 0xa5\n", name.c_str(), first, StencilValue, second);
    Require(first != 0 && second != 0 && first + second == Texels, "depth expand review: " + name + ": the stencil draws did not leave both values");
    auto expand = Expand(depthMoves ? written : surfaces.depth);
    expand.stencilWriteBase = stencilWritten;
    Submit(Draw(surfaces, expand), name + ": DB expand");
    RequireSame(reader.Read(surfaces.stencil, Format8Uint, name + ": stencil of the read surface after the expand"), stencil, name + ": stencil of the read surface after the expand");
    RequireSame(reader.Read(stencilWritten, Format8Uint, name + ": texture read of the stencil write base"), stencil, name + ": texture read of the stencil write base (R8 UINT)");
    std::vector<std::byte> raw(stencilBytes);
    AgcDriver::GuestMemory::Read(stencilWritten, raw);
    RequireSame(Detiled(stencilWritten, raw, Format8Uint, 1u), stencil, name + ": raw read of the stencil write base after a flush, detiled with the 8-bit SW_64KB_Z_X equation");
    const auto depth = RequireDepth(reader, surfaces.depth, Gradient, name + ": read base");
    if (depthMoves) RequireExpanded(reader, written, depth, name);
}

}

int main() {
    try {
        if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
        Register(VertexShader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelShader, PixelCode.data(), sizeof(PixelCode), 1);
        TexelReader reader;
        DepthOnlySurface(reader);
        StencilKept(reader);
        InPlaceUnchanged(reader);
        AlwaysPassEqualBases(reader);
        ExpandedAgain(reader);
        BufferReadAfterExpand(reader);
        TwoDestinations(reader);
        QueuedDraws(reader);
        SixteenBitPlane(reader);
        SecondSlice(reader);
        StencilMoved(reader, true);
        StencilMoved(reader, false);
        AgcDriverShutdown_nid_postfix();
        std::puts("depth expand review tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
