#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
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
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>


#ifndef _WIN32
namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}
#endif

namespace {

using AgcDriver::Graphics::Require;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Bytes = Threads * 4u;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::size_t Piece = 65536;

alignas(256) constexpr std::array<std::uint32_t, 13> RowCode{
    0x7e3c0300u,
    0x7e3e0280u,
    0x34060082u,
    0xe0301000u, 0x80040b03u,
    0xbf8c3f70u,
    0x4a181681u,
    0xe0701000u, 0x80000c03u,
    0xf0201108u, 0x00010b1eu,
    0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 10> ProbeCode{
    0x7e3c0300u,
    0x7e3e020cu,
    0xf0001108u, 0x00010a1eu,
    0x34060082u,
    0xbf8c3f70u,
    0xe0701000u, 0x80000a03u,
    0xbf810000u, 0xbf810000u,
};

class GuestBlock {
public:
    GuestBlock(std::size_t pieces, bool inWatch) : bytes(pieces * Piece), watched(inWatch) {
        if (inWatch) {
#ifdef _WIN32
            block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, Piece));
            if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
            constexpr std::uintptr_t alignment = Piece;
            void* mapped = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapped != MAP_FAILED) {
                const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
                const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
                if (aligned != begin) munmap(mapped, aligned - begin);
                if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
                block = reinterpret_cast<std::uint8_t*>(aligned);
                GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
            }
#endif
        } else {
#ifdef _WIN32
            block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
            block = static_cast<std::uint8_t*>(std::aligned_alloc(Piece, bytes));
#endif
            if (block != nullptr) std::memset(block, 0, bytes);
        }
        Require(block != nullptr, "dispatch move depth: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
        if (watched) {
#ifdef _WIN32
            GuestArena::GuestArenaReset_nid_postfix(block, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#else
            munmap(block, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
#endif
        } else {
#ifdef _WIN32
            VirtualFree(block, 0, MEM_RELEASE);
#else
            std::free(block);
#endif
        }
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* PieceAt(std::size_t index) const { return block + index * Piece; }

private:
    std::size_t bytes = 0;
    bool watched = false;
    std::uint8_t* block = nullptr;
};

struct Buffers {
    explicit Buffers(const GuestBlock& block) : block(block) {}
    std::uint32_t* InputA() const { return reinterpret_cast<std::uint32_t*>(block.PieceAt(4)); }
    std::uint32_t* InputB() const { return reinterpret_cast<std::uint32_t*>(block.PieceAt(5) + 12); }
    std::uint32_t* InputC() const { return reinterpret_cast<std::uint32_t*>(block.PieceAt(6) + 32); }
    std::uint32_t* Output() const { return reinterpret_cast<std::uint32_t*>(block.PieceAt(8)); }

private:
    const GuestBlock& block;
};

struct Env {
    AgcDriver::VulkanDevice& device;
    const Shaders& shaders;
    const Buffers& buffers;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

ShaderRecompiler::RecompileResult RowProgram(AgcDriver::VulkanDevice& device, const void* plane, const std::uint32_t* input, const std::uint32_t* output) {
    std::vector<std::uint32_t> userData(20, 0u);
    const auto out = BufferDescriptor(output, Bytes);
    const auto texture = TextureDescriptor(plane);
    const auto in = BufferDescriptor(input, Bytes);
    std::copy(out.begin(), out.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(in.begin(), in.end(), userData.begin() + 16);
    return Compile(device, RowCode, userData);
}

void RunRow(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled, const std::uint32_t* input, const std::uint32_t* output) {
    auto result = compiled;
    std::size_t elements = 0;
    for (auto& binding : result.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
        Require(binding.guestDescriptor.size() == static_cast<std::size_t>(binding.count) * 4u, "dispatch move depth: the program's buffer bindings have another shape");
        for (std::uint32_t element = 0; element < binding.count; ++element, ++elements) {
            auto* current = binding.guestDescriptor.data() + static_cast<std::size_t>(element) * 4u;
            const auto built = static_cast<std::uint64_t>(current[0]) | (static_cast<std::uint64_t>(current[1] & 0xffffu) << 32u);
            const auto words = built == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output)) ? BufferDescriptor(output, Bytes) : BufferDescriptor(input, Bytes);
            std::copy(words.begin(), words.end(), current);
        }
    }
    Require(elements == 2, "dispatch move depth: the program does not bind an input and an output");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(RowCode.data()));
    device.WaitIdle();
}

std::array<std::uint32_t, Threads> ReadRow(const Env& env, const void* plane, std::uint32_t y) {
    std::vector<std::uint32_t> userData(16, 0u);
    auto* output = env.buffers.Output();
    const auto out = BufferDescriptor(output, Bytes);
    const auto texture = TextureDescriptor(plane);
    std::copy(out.begin(), out.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = y;
    const auto program = Compile(env.device, ProbeCode, userData);
    std::fill(output, output + Threads, Sentinel);
    env.device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(ProbeCode.data()));
    env.device.WaitIdle();
    std::array<std::uint32_t, Threads> lanes{};
    std::copy(output, output + Threads, lanes.begin());
    return lanes;
}

void FillInput(std::uint32_t* where, float first) {
    for (std::uint32_t lane = 0; lane < Threads; ++lane) where[lane] = std::bit_cast<std::uint32_t>(first + static_cast<float>(lane) / 1024.0f);
}

void FillPlane(std::uint8_t* plane, float value) {
    auto* words = reinterpret_cast<std::uint32_t*>(plane);
    std::fill(words, words + Piece / sizeof(std::uint32_t), std::bit_cast<std::uint32_t>(value));
}

void DrawDepth(const Env& env, const void* plane, float depth) {
    const Surface surface{reinterpret_cast<std::uintptr_t>(plane), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    const float low = depth <= 0.5f ? 0.0f : 2.0f * depth - 1.0f;
    const float high = depth <= 0.5f ? 2.0f * depth : 1.0f;
    Require(Draw(env.device, env.shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .minDepth = low, .maxDepth = high}) == Covered, "dispatch move depth: the depth producer did not cover the surface");
}

struct PlaneModel {
    std::array<std::uint32_t, Threads> row0{};
    std::uint32_t rest = 0;

    void Set(float value) {
        rest = std::bit_cast<std::uint32_t>(value);
        row0.fill(rest);
    }

    std::array<std::uint32_t, Threads> Dispatch(const std::uint32_t* input) {
        std::array<std::uint32_t, Threads> sums{};
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            sums[lane] = input[lane] + 1u;
            row0[lane] = input[lane];
        }
        return sums;
    }
};

void Totals(const std::string& what);

void Step(const Env& env, const ShaderRecompiler::RecompileResult& program, PlaneModel& model, const std::uint32_t* input, const std::string& what) {
    const auto expected = model.Dispatch(input);
    auto* output = env.buffers.Output();
    std::fill(output, output + Threads, Sentinel);
    RunRow(env.device, program, input, output);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Require(output[lane] == expected[lane], "dispatch move depth: " + what + ": lane " + std::to_string(lane) + " is " + Hex(output[lane]) + ", expected " + Hex(expected[lane]));
    }
    Totals(what);
}

void Verify(const Env& env, const void* plane, const PlaneModel& model, const std::string& what) {
    for (const std::uint32_t y : {0u, 1u, Height - 1u}) {
        const auto lanes = ReadRow(env, plane, y);
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto expected = y == 0 ? model.row0[lane] : model.rest;
            Require(lanes[lane] == expected, "dispatch move depth: " + what + ": row " + std::to_string(y) + ", texel " + std::to_string(lane) + " of the plane is " + Hex(lanes[lane]) + ", expected " + Hex(expected));
        }
    }
}

struct MoveSteps {
    std::uint64_t checked, exact, moved, refused;
};

MoveSteps Since(const AgcDriver::VulkanDevice::DispatchMoveCount& before) {
    const auto now = AgcDriver::VulkanDevice::DispatchMoveCounts();
    return {now.checked - before.checked, now.exact - before.exact, now.moved - before.moved, now.refused - before.refused};
}

void Totals(const std::string& what) {
    const auto now = AgcDriver::VulkanDevice::DispatchMoveCounts();
    std::printf("dispatch move depth: %s: in total checked %llu, exact %llu, moved %llu, refused %llu, dropped by stage A %llu\n", what.c_str(), static_cast<unsigned long long>(now.checked), static_cast<unsigned long long>(now.exact), static_cast<unsigned long long>(now.moved), static_cast<unsigned long long>(now.refused), static_cast<unsigned long long>(now.filtered));
}

void DescribeBindings(const ShaderRecompiler::RecompileResult& program) {
    for (const auto& binding : program.bindings) {
        std::string written;
        for (const bool bit : binding.bufferWritten) written += bit ? '1' : '0';
        std::printf("dispatch move depth: binding role %d kind %d count %u, %zu descriptor words, buffers written [%s], images written %zu\n", static_cast<int>(binding.role), static_cast<int>(binding.kind), binding.count, binding.guestDescriptor.size(), written.c_str(), binding.imageWritten.size());
    }
}

void Print(const char* what, const MoveSteps& step) {
    std::printf("dispatch move depth: %s: checked %llu, exact %llu, moved %llu, refused %llu\n", what, static_cast<unsigned long long>(step.checked), static_cast<unsigned long long>(step.exact), static_cast<unsigned long long>(step.moved), static_cast<unsigned long long>(step.refused));
}

void Warm(const Env& env, std::uint8_t* plane) {
    FillPlane(plane, 0.5f);
    PlaneModel model;
    model.Set(0.5f);
    const auto program = RowProgram(env.device, plane, env.buffers.InputA(), env.buffers.Output());
    Step(env, program, model, env.buffers.InputA(), "warm-up: the first input");
    Step(env, program, model, env.buffers.InputB(), "warm-up: the second input");
    Step(env, program, model, env.buffers.InputC(), "warm-up: the third input");
}

bool Control(const Env& env, std::uint8_t* plane) {
    FillPlane(plane, 0.5f);
    PlaneModel model;
    model.Set(0.5f);
    const auto refusals = AgcDriver::Graphics::ShaderResources::DepthGuardRefusals();
    const auto program = RowProgram(env.device, plane, env.buffers.InputA(), env.buffers.Output());
    DescribeBindings(program);
    Step(env, program, model, env.buffers.InputA(), "control: the first dispatch");
    Totals("control, after the first dispatch");
    const auto start = AgcDriver::VulkanDevice::DispatchMoveCounts();
    Step(env, program, model, env.buffers.InputB(), "control: an input moved");
    Totals("control, after an input moved");
    Step(env, program, model, env.buffers.InputC(), "control: the input moved again");
    Totals("control, after the input moved again");
    Step(env, program, model, env.buffers.InputC(), "control: an exact repeat");
    Totals("control, after an exact repeat");
    const auto steps = Since(start);
    Print("control, a plane without a depth surface", steps);
    Verify(env, plane, model, "control");
    Require(AgcDriver::Graphics::ShaderResources::DepthGuardRefusals() == refusals, "dispatch move depth: the depth guard refused a proof with no depth surface in sight");
    Require(steps.refused == 0, "dispatch move depth: the control's repeats were refused");
    if (steps.checked == 0) return false;
    Require(steps.checked == 3 && steps.moved == 3, "dispatch move depth: the control's repeats were not all served by the template of the first dispatch");
    return true;
}

void Mirror(const Env& env, std::uint8_t* plane, const char* where) {
    const std::string at = std::string(where) + ", the mirror: ";
    FillPlane(plane, 0.5f);
    PlaneModel model;
    DrawDepth(env, plane, 0.25f);
    model.Set(0.25f);
    const auto program = RowProgram(env.device, plane, env.buffers.InputA(), env.buffers.Output());
    Step(env, program, model, env.buffers.InputA(), at + "the first dispatch");
    Verify(env, plane, model, at + "after the first dispatch");
    DrawDepth(env, plane, 0.75f);
    model.Set(0.75f);
    const auto start = AgcDriver::VulkanDevice::DispatchMoveCounts();
    Step(env, program, model, env.buffers.InputB(), at + "an input moved after the depth was drawn again");
    Verify(env, plane, model, at + "after the moved dispatch");
    Step(env, program, model, env.buffers.InputB(), at + "an exact repeat");
    DrawDepth(env, plane, 0.25f);
    model.Set(0.25f);
    Step(env, program, model, env.buffers.InputC(), at + "the input moved again after another redraw");
    Verify(env, plane, model, at + "after the last dispatch");
    Print((at + "repeats and redraws").c_str(), Since(start));
}

void Appears(const Env& env, std::uint8_t* plane, const char* where, bool moved, bool templates) {
    const std::string at = std::string(where) + (moved ? ", a surface appears, input moved: " : ", a surface appears, exact repeat: ");
    FillPlane(plane, 0.125f);
    PlaneModel model;
    model.Set(0.125f);
    const auto program = RowProgram(env.device, plane, env.buffers.InputA(), env.buffers.Output());
    Step(env, program, model, env.buffers.InputA(), at + "the first dispatch, no depth surface yet");
    DrawDepth(env, plane, 0.25f);
    model.Set(0.25f);
    const auto start = AgcDriver::VulkanDevice::DispatchMoveCounts();
    const auto refusals = AgcDriver::Graphics::ShaderResources::DepthGuardRefusals();
    Step(env, program, model, moved ? env.buffers.InputB() : env.buffers.InputA(), at + "the dispatch after the surface appeared");
    std::printf("dispatch move depth: %sthe depth guard refused %llu proofs\n", at.c_str(), static_cast<unsigned long long>(AgcDriver::Graphics::ShaderResources::DepthGuardRefusals() - refusals));
    if (templates) Require(AgcDriver::Graphics::ShaderResources::DepthGuardRefusals() > refusals, "dispatch move depth: " + at + "the template over the plain storage image was proved current although a depth surface covers its memory");
    Print((at + "the dispatch after the surface appeared").c_str(), Since(start));
    Totals(at + "after that dispatch");
    Verify(env, plane, model, at + "after that dispatch");
    DrawDepth(env, plane, 0.75f);
    model.Set(0.75f);
    Step(env, program, model, env.buffers.InputC(), at + "the input moved after the next redraw");
    Verify(env, plane, model, at + "after the last dispatch");
}

bool RunTests(const Env& env, const GuestBlock& watched, const GuestBlock& unwatched, std::size_t pass) {
    const auto inWatch = pass * 10;
    const auto outside = pass * 3;
    FillInput(env.buffers.InputA(), 0.0625f);
    FillInput(env.buffers.InputB(), 0.109375f);
    FillInput(env.buffers.InputC(), 0.15625f);
    Warm(env, watched.PieceAt(9));
    const bool templates = Control(env, watched.PieceAt(inWatch));
    if (!templates) std::puts("dispatch move depth: the device kept no template (no host imports); the moved-buffer path is not exercised");
    Mirror(env, watched.PieceAt(inWatch + 1), "inside the write watch");
    Appears(env, watched.PieceAt(inWatch + 2), "inside the write watch", true, templates);
    Appears(env, watched.PieceAt(inWatch + 3), "inside the write watch", false, templates);
    Mirror(env, unwatched.PieceAt(outside), "outside the write watch");
    Appears(env, unwatched.PieceAt(outside + 1), "outside the write watch", true, templates);
    Appears(env, unwatched.PieceAt(outside + 2), "outside the write watch", false, templates);
    return templates;
}

}

int main() {
    try {
        const GuestBlock watched(20, true);
        const GuestBlock unwatched(6, false);
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = DepthFastClearHarness::Compile(*device);
        const Buffers buffers(watched);
        const Env env{*device, shaders, buffers};
        bool templates = true;
#ifndef _WIN32
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const auto decided = AgcDriver::Graphics::PrepareImportWatch(context);
        struct Restore {
            const AgcDriver::Graphics::Context& context;
            AgcDriver::Graphics::ImportWatch decided;
            ~Restore() { AgcDriver::Graphics::SetImportWatch(context, decided); }
        } restore{context, decided};
        std::size_t pass = 0;
        for (const auto mode : {AgcDriver::Graphics::ImportWatch::Watch, AgcDriver::Graphics::ImportWatch::Unwatch}) {
            AgcDriver::Graphics::SetImportWatch(context, mode);
            std::printf("dispatch move depth: pass %zu, imports %s\n", pass, mode == AgcDriver::Graphics::ImportWatch::Watch ? "under the write watch" : "left unwatched");
            templates = RunTests(env, watched, unwatched, pass++) && templates;
        }
#else
        templates = RunTests(env, watched, unwatched, 0);
#endif
        if (!templates) return VulkanTestSkipped;
        std::puts("dispatch move depth tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
