#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Half = Width / 2u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t SetContextReg = 0x69u;
constexpr std::uint32_t SetShReg = 0x76u;
constexpr std::uint32_t SetUconfigReg = 0x79u;
constexpr std::uint32_t DrawIndexAuto = 0x2du;
constexpr std::uint32_t Float11_11_10Info = 0x4171cu;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t PremultipliedOver = 0x40000501u;
constexpr std::uint32_t LateKill = 0x40u;
constexpr std::uint32_t DepthTest = 0x2u;
constexpr std::uint32_t DepthWrite = 0x4u;
constexpr std::uint32_t Less = 1u << 4u;
constexpr std::uint32_t Greater = 4u << 4u;
constexpr std::uint32_t GreaterEqual = 6u << 4u;
constexpr std::uint32_t Always = 7u << 4u;
constexpr std::array<std::uint32_t, 5> CheckerboardKill{0x7e040f00u, 0x7e060f01u, 0x3a040702u, 0x36040481u, 0x7da40480u};

std::byte* ColorMemory = nullptr;
alignas(256) std::array<std::byte, 65536> DepthMemory{};

std::byte* AllocateWatched(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) throw std::runtime_error("cannot map the watched block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + BlockBytes - 1) & ~static_cast<std::uintptr_t>(BlockBytes - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    GuestAllocations::Mutation mutation;
    mutation.Add(block, bytes, true, true, true);
    return static_cast<std::byte*>(block);
}

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80020005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> CountingVertexCode{
    0x7e0c0281, 0xe0c80000, 0x80030600, 0xe0382000, 0x80020005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(16) std::array<std::uint32_t, 4> Counter{};

using Vertex = std::array<float, 4>;

constexpr std::array<Vertex, 3> Screen(float z) {
    return {{{-1.0f, -1.0f, z, 1.0f}, {3.0f, -1.0f, z, 1.0f}, {-1.0f, 3.0f, z, 1.0f}}};
}

constexpr std::array<Vertex, 6> LeftHalf(float z) {
    return {{{-1.0f, -1.0f, z, 1.0f}, {0.0f, -1.0f, z, 1.0f}, {-1.0f, 1.0f, z, 1.0f}, {0.0f, -1.0f, z, 1.0f}, {0.0f, 1.0f, z, 1.0f}, {-1.0f, 1.0f, z, 1.0f}}};
}

alignas(256) constexpr auto BackgroundLess = Screen(0.9375f);
alignas(256) constexpr auto BackgroundGreater = Screen(0.0625f);
alignas(256) constexpr auto OccluderLess = LeftHalf(0.25f);
alignas(256) constexpr auto OccluderGreater = LeftHalf(0.75f);
alignas(256) constexpr auto Middle = Screen(0.5f);
alignas(256) constexpr auto ProbeLess = Screen(0.75f);
alignas(256) constexpr auto ProbeGreater = Screen(0.25f);

alignas(256) std::array<std::array<std::uint32_t, 64>, 6> Programs{};

struct RegisteredShader {
    Shader shader;
    ShaderUserData userData;
};
RegisteredShader VertexShader{};
RegisteredShader CountingVertexShader{};
std::array<RegisteredShader, 6> PixelShaders{};

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

const std::uint32_t* PixelProgram(std::size_t index, std::array<float, 4> rgba, bool kill, bool counting = false) {
    auto& code = Programs[index];
    std::size_t at = 0;
    if (kill) {
        for (const auto word : CheckerboardKill) code[at++] = word;
    }
    if (counting) {
        for (const auto word : {0x7e100281u, 0xe0c80000u, 0x80000800u}) code[at++] = word;
    }
    for (std::uint32_t vgpr = 0; vgpr < 4u; ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(rgba[vgpr]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    Register(PixelShaders[index], code.data(), static_cast<std::uint32_t>(at * sizeof(std::uint32_t)), 1);
    return code.data();
}

std::uint32_t SmallFloat(float value, std::uint32_t mantissaBits) {
    if (!(value > 0.0f)) return 0;
    const double scale = std::ldexp(1.0, static_cast<int>(mantissaBits));
    if (value < std::ldexp(1.0f, -14)) return static_cast<std::uint32_t>(std::floor(static_cast<double>(value) * std::ldexp(1.0, 14) * scale));
    int exponent = 0;
    const double fraction = std::frexp(static_cast<double>(value), &exponent);
    Require(exponent - 1 <= 15, "the test's values stay finite in 11_11_10");
    return (static_cast<std::uint32_t>(exponent - 1 + 15) << mantissaBits) | static_cast<std::uint32_t>(std::floor((fraction * 2.0 - 1.0) * scale));
}

float Unpacked(std::uint32_t raw, std::uint32_t mantissaBits) {
    const auto exponent = raw >> mantissaBits;
    const auto mantissa = raw & ((1u << mantissaBits) - 1u);
    Require(exponent != 0x1fu, "the test's stored words hold finite values");
    if (exponent == 0u) return static_cast<float>(std::ldexp(static_cast<double>(mantissa), -14 - static_cast<int>(mantissaBits)));
    return static_cast<float>(std::ldexp(1.0 + std::ldexp(static_cast<double>(mantissa), -static_cast<int>(mantissaBits)), static_cast<int>(exponent) - 15));
}

constexpr std::array<std::uint32_t, 3> FieldOffset{0u, 10u, 21u};
constexpr std::array<std::uint32_t, 3> FieldMantissa{5u, 6u, 6u};

std::uint32_t Written(std::array<float, 4> rgba, std::uint32_t stored, bool premultipliedOver) {
    const std::array<float, 3> source{rgba[2], rgba[1], rgba[0]};
    std::uint32_t word = 0;
    for (std::uint32_t field = 0; field < 3; ++field) {
        const auto destination = Unpacked((stored >> FieldOffset[field]) & ((1u << (FieldMantissa[field] + 5u)) - 1u), FieldMantissa[field]);
        const auto value = premultipliedOver ? source[field] + destination * (1.0f - rgba[3]) : source[field];
        word |= SmallFloat(value, FieldMantissa[field]) << FieldOffset[field];
    }
    return word;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 4> CounterDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Counter.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), sizeof(Counter), 0x31016facu};
}

void SetRegister(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    words.push_back(0xc0010000u | (opcode << 8u));
    words.push_back(offset);
    words.push_back(value);
}

struct Pass {
    std::span<const Vertex> vertices;
    const std::uint32_t* pixel;
    std::uint32_t depthControl;
    std::uint32_t shaderControl;
    std::uint32_t targetMask;
    std::uint32_t blendControl;
    const std::uint32_t* vertex = VertexCode.data();
    bool counter = false;
};

void Submit(const Pass& pass, const char* what) {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory);
    const auto depth = reinterpret_cast<std::uintptr_t>(DepthMemory.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(pass.vertex);
    const auto pixel = reinterpret_cast<std::uintptr_t>(pass.pixel);
    const auto extent = (Height << 16u) | Width;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b3, 0x300}, {0x1b4, 0x300}, {0x1b6, 0}, {0x207, 0}, {0x200, pass.depthControl}, {0x203, pass.shaderControl},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, pass.targetMask}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x31b, 0}, {0x31c, Float11_11_10Info}, {0x31d, 0}, {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)}, {0x3b8, TiledAttrib3},
        {0x1e0, pass.blendControl},
        {0xc, 0}, {0xd, extent}, {0x81, 0x80000000}, {0x82, extent}, {0x90, 0x80000000}, {0x91, extent}, {0x94, 0x80000000}, {0x95, extent},
        {0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>(Width / 2.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-(Height / 2.0f))}, {0x112, std::bit_cast<std::uint32_t>(Height / 2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0}, {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
        {0x000, 0}, {0x002, 0}, {0x007, ((Height - 1u) << 16u) | (Width - 1u)},
        {0x008, 0}, {0x009, std::bit_cast<std::uint32_t>(1.0f)}, {0x00a, 0}, {0x00b, 0},
        {0x010, 3}, {0x011, 0},
        {0x012, static_cast<std::uint32_t>(depth >> 8u)}, {0x014, static_cast<std::uint32_t>(depth >> 8u)},
        {0x01a, static_cast<std::uint32_t>(depth >> 40u)}, {0x01c, static_cast<std::uint32_t>(depth >> 40u)},
    };
    std::vector<std::uint32_t> words;
    SetRegister(words, SetUconfigReg, 0x242, 4u);
    for (const auto& [offset, value] : context) SetRegister(words, SetContextReg, offset, value);
    SetRegister(words, SetShReg, 0x008, static_cast<std::uint32_t>(pixel >> 8u));
    SetRegister(words, SetShReg, 0x009, static_cast<std::uint32_t>(pixel >> 40u));
    SetRegister(words, SetShReg, 0x00b, pass.counter ? 4u << 1u : 0u);
    SetRegister(words, SetShReg, 0x0c8, static_cast<std::uint32_t>(vertex >> 8u));
    SetRegister(words, SetShReg, 0x0c9, static_cast<std::uint32_t>(vertex >> 40u));
    SetRegister(words, SetShReg, 0x08b, (pass.counter ? 8u : 4u) << 1u);
    const auto descriptor = BufferDescriptor(pass.vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(pass.vertices.size()));
    for (std::uint32_t i = 0; i < descriptor.size(); ++i) SetRegister(words, SetShReg, 0x08c + i, descriptor[i]);
    if (pass.counter) {
        const auto counter = CounterDescriptor();
        for (std::uint32_t i = 0; i < counter.size(); ++i) {
            SetRegister(words, SetShReg, 0x00c + i, counter[i]);
            SetRegister(words, SetShReg, 0x090 + i, counter[i]);
        }
    }
    words.insert(words.end(), {0xc0000000u | (1u << 16u) | (DrawIndexAuto << 8u), static_cast<std::uint32_t>(pass.vertices.size()), 2u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, std::string("DCB submission failed for ") + what);
    AgcDriverWaitIdle_nid_postfix();
}

std::vector<std::uint32_t> ReadBack(std::size_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ColorMemory));
    AgcDriver::Graphics::StorageTexture::FlushPending(address, bytes, nullptr, "test read-back");
    AgcDriverWaitIdle_nid_postfix();
    std::vector<std::uint32_t> stored(bytes / 4u);
    AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::uint32_t Counted() {
    std::uint32_t counted = 0;
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(Counter.data()), std::as_writable_bytes(std::span(&counted, 1)), 1);
    return counted;
}

void RunCounting(bool vertexCounts) {
    const auto* killing = PixelProgram(0, {0.5f, 0.25f, 0.125f, 0.5f}, true);
    const auto* plain = PixelProgram(1, {0.0f, 0.0f, 0.0f, 1.0f}, false);
    const auto* countingPlain = PixelProgram(3, {0.0f, 0.0f, 0.0f, 1.0f}, false, true);
    const auto* countingKilling = PixelProgram(4, {0.5f, 0.25f, 0.125f, 0.5f}, true, true);
    const auto* vertex = vertexCounts ? CountingVertexCode.data() : VertexCode.data();
    const std::string what = std::string("a late-test merge split whose ") + (vertexCounts ? "vertex" : "pixel") + " shader adds to a counter";
    Submit({Middle, vertexCounts ? plain : countingPlain, DepthTest | DepthWrite | Always, 0u, 0xfu, 0u, vertex, true}, (what + ", the counted draw").c_str());
    const auto counted = Counted();
    Require(counted != 0u, what + ": the draw without a merge did not add to the counter");
    bool refused = false;
    try {
        Submit({Middle, vertexCounts ? killing : countingKilling, DepthTest | DepthWrite | Less, LateKill, 0xfu, PremultipliedOver, vertex, true}, (what + ", the split draw").c_str());
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        Require(message.find("kills pixels under late tests of a draw writing depth or stencil") != std::string::npos, what + " failed on another refusal: " + message);
        refused = true;
    }
    Require(refused, what + " was split and drawn twice");
    Require(Counted() == counted, what + " added to the counter before it was refused");
}

struct Case {
    const char* name;
    std::uint32_t compare;
    std::span<const Vertex> background;
    std::span<const Vertex> occluder;
    std::span<const Vertex> probe;
};

void Run(const ColorTargetLayout& layout, const Case& run, const std::uint32_t* killing, const std::uint32_t* plain, const std::uint32_t* probe) {
    const std::array<float, 4> splitColor{0.5f, 0.25f, 0.125f, 0.5f};
    const std::array<float, 4> probeColor{1.0f, 0.5f, 0.25f, 1.0f};
    std::vector<std::uint32_t> before(layout.Bytes() / 4u);
    for (std::size_t index = 0; index < before.size(); ++index) {
        const auto hash = static_cast<std::uint32_t>((index + 7u) * 0x9e3779b9u);
        const auto field = [&](std::uint32_t shift, std::uint32_t mantissaBits) { return ((11u + ((hash >> shift) % 8u)) << mantissaBits) | ((hash >> (shift + 3u)) & ((1u << mantissaBits) - 1u)); };
        before[index] = field(0u, 5u) | (field(8u, 6u) << 10u) | (field(17u, 6u) << 21u);
    }
    std::memcpy(ColorMemory, before.data(), layout.Bytes());
    const std::string what = std::string("a late-test merge split under ") + run.name;
    Submit({run.background, plain, DepthTest | DepthWrite | Always, 0u, 0u, 0u}, (what + ", the background depth").c_str());
    Submit({run.occluder, plain, DepthTest | DepthWrite | Always, 0u, 0u, 0u}, (what + ", the left occluder").c_str());
    Submit({Middle, killing, DepthTest | DepthWrite | run.compare, LateKill, 0xfu, PremultipliedOver}, (what + ", the split draw").c_str());
    Submit({run.probe, probe, DepthTest | run.compare, 0u, 0xfu, 0u}, (what + ", the depth probe").c_str());
    const auto stored = ReadBack(layout.Bytes());
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y) / 4u;
            const bool kept = ((x ^ y) & 1u) == 0u;
            std::uint32_t expected = before[index];
            if (x >= Half && kept) expected = Written(splitColor, expected, true);
            if (x >= Half && !kept) expected = Written(probeColor, expected, false);
            if (stored[index] == expected) continue;
            char detail[224];
            std::snprintf(detail, sizeof(detail), ": pixel (%u, %u) %s holds 0x%08x, expected 0x%08x (it held 0x%08x)", x, y, x < Half ? "behind the occluder" : kept ? "kept by the kill" : "killed", stored[index], expected, before[index]);
            throw std::runtime_error(what + detail);
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (!TargetHasCapability(device->Target(), spv::CapabilityFragmentShaderPixelInterlockEXT)) {
                std::puts("skipped, the device lacks fragmentShaderPixelInterlock");
                return VulkanTestSkipped;
            }
        }
        const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 4);
        Require(layout.Bytes() <= BlockBytes, "the 11_11_10 target fits one watched block");
        ColorMemory = AllocateWatched(BlockBytes);
        Register(VertexShader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(CountingVertexShader, CountingVertexCode.data(), sizeof(CountingVertexCode), 2);
        if (argc == 2 && (std::string(argv[1]) == "--counting-pixel" || std::string(argv[1]) == "--counting-vertex")) {
            const bool vertexCounts = std::string(argv[1]) == "--counting-vertex";
            RunCounting(vertexCounts);
            try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
            std::printf("a late-test merge split whose %s shader adds to a counter was refused before either pass\n", vertexCounts ? "vertex" : "pixel");
            return 0;
        }
        const auto* killing = PixelProgram(0, {0.5f, 0.25f, 0.125f, 0.5f}, true);
        const auto* plain = PixelProgram(1, {0.0f, 0.0f, 0.0f, 1.0f}, false);
        const auto* probe = PixelProgram(2, {1.0f, 0.5f, 0.25f, 1.0f}, false);
        for (int round = 0; round < 2; ++round) {
            Run(layout, {"LESS", Less, BackgroundLess, OccluderLess, ProbeLess}, killing, plain, probe);
            Run(layout, {"GREATER", Greater, BackgroundGreater, OccluderGreater, ProbeGreater}, killing, plain, probe);
            Run(layout, {"GEQUAL", GreaterEqual, BackgroundGreater, OccluderGreater, ProbeGreater}, killing, plain, probe);
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("late-test merge splits through the driver passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
