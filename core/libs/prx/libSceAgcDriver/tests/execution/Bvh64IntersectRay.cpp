#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
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
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Record = 16;
constexpr std::uint32_t Forms = 4;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint64_t NodeBytes = 64;
constexpr std::uint32_t LastNode = 6;
constexpr std::uint32_t Invalid = 0xffffffffu;
constexpr std::uint32_t BoxSort = 0x80000000u;
constexpr std::uint32_t TriangleId = 0x00abc123u;
constexpr std::array<std::uint32_t, 4> Box32Children{0x1000u, 0x1001u, 0x1002u, 0x1003u};
constexpr std::array<std::uint32_t, 4> Box16Children{0x2000u, 0x2001u, 0x2002u, 0x2003u};
constexpr std::array<std::uint32_t, 4> PastEndChildren{0x3000u, 0x3001u, 0x3002u, 0x3003u};
constexpr std::array<const char*, Forms> FormNames{"image_bvh64_intersect_ray", "image_bvh_intersect_ray", "image_bvh64_intersect_ray a16", "image_bvh_intersect_ray a16"};

alignas(256) std::array<std::uint32_t, Threads * Record> Input{};
alignas(256) std::array<std::uint32_t, Threads * Record> Output{};

constexpr std::array<std::uint32_t, 82> MakeCode() {
    std::array<std::uint32_t, 82> code{};
    std::size_t at = 0;
    code[at++] = 0x34020086u;
    for (std::uint32_t dword = 0; dword < Record; ++dword) {
        code[at++] = 0xe0301000u | (dword * 4u);
        code[at++] = 0x80000001u | ((4u + dword) << 8u);
    }
    code[at++] = 0xbf8c3f70u;
    code[at++] = 0xf19c9f01u;
    code[at++] = 0x00031405u;
    code[at++] = 0xf1989f07u;
    code[at++] = 0x00021804u;
    code[at++] = 0x0a090807u;
    code[at++] = 0x0e0d0c0bu;
    code[at++] = 0x0000100fu;
    code[at++] = 0xf19c9f05u;
    code[at++] = 0x40031c05u;
    code[at++] = 0x09080706u;
    code[at++] = 0x1312110au;
    code[at++] = 0xf1989f05u;
    code[at++] = 0x40022004u;
    code[at++] = 0x0a090807u;
    code[at++] = 0x00131211u;
    for (std::uint32_t dword = 0; dword < Record; ++dword) {
        code[at++] = 0xe0701000u | (dword * 4u);
        code[at++] = 0x80010001u | ((20u + dword) << 8u);
    }
    code[at++] = 0xbf810000u;
    return code;
}

alignas(256) constexpr std::array<std::uint32_t, 82> Code = MakeCode();

struct Ray {
    float extent;
    std::array<float, 3> origin;
    std::array<float, 3> direction;
    std::array<float, 3> inverse;
};

struct Case {
    std::uint32_t node;
    Ray ray;
    std::array<std::uint32_t, 4> expected;
};

constexpr float Infinity = std::numeric_limits<float>::infinity();
constexpr std::array<float, 3> Diagonal{0.25f, 0.5f, 1.0f};
constexpr std::array<float, 3> DiagonalInverse{4.0f, 2.0f, 1.0f};
constexpr Ray Long{100.0f, {0.0f, 0.0f, 0.0f}, Diagonal, DiagonalInverse};
constexpr Ray Short{2.0f, {0.0f, 0.0f, 0.0f}, Diagonal, DiagonalInverse};
constexpr Ray Forward{100.0f, {0.25f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f}, {Infinity, Infinity, 1.0f}};

constexpr std::uint32_t Pointer(std::uint32_t node, std::uint32_t type) {
    return (node << 3u) | type;
}

constexpr std::array<Case, 8> Cases{{
    {Pointer(2, 5), Long, {Box32Children[2], Box32Children[0], Invalid, Invalid}},
    {Pointer(4, 4), Long, {Box16Children[2], Box16Children[0], Invalid, Invalid}},
    {Pointer(1, 0), Forward, {0xc0a00000u, 0xbf800000u, TriangleId, 1u}},
    {Pointer(1, 1), Forward, {0x7f800000u, 0x3f800000u, TriangleId, 0u}},
    {Pointer(1, 7), Long, {Invalid, Invalid, Invalid, Invalid}},
    {Pointer(6, 5), Long, {Invalid, Invalid, Invalid, Invalid}},
    {Pointer(7, 0), Forward, {Invalid, Invalid, Invalid, Invalid}},
    {Pointer(2, 5), Short, {Box32Children[2], Invalid, Invalid, Invalid}},
}};

constexpr std::array<std::array<float, 6>, 4> Boxes{{
    {1.0f, 2.0f, 4.0f, 2.0f, 4.0f, 8.0f},
    {10.0f, 0.0f, 0.0f, 11.0f, 1.0f, 1.0f},
    {0.25f, 0.5f, 1.0f, 0.5f, 1.0f, 2.0f},
    {-2.0f, -4.0f, -8.0f, -1.0f, -2.0f, -4.0f},
}};

constexpr std::array<float, 12> TriangleVertices{0.0f, 0.0f, 5.0f, 1.0f, 0.0f, 5.0f, 0.0f, 1.0f, 5.0f, 1.0f, 1.0f, 5.0f};

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t Half(float value) {
    const auto bits = Bits(value);
    const auto sign = (bits >> 16u) & 0x8000u;
    const auto exponent = (bits >> 23u) & 0xffu;
    const auto mantissa = bits & 0x7fffffu;
    if (exponent == 0u && mantissa == 0u) return sign;
    if (exponent == 0xffu && mantissa == 0u) return sign | 0x7c00u;
    Require(exponent >= 113u && exponent <= 142u && (mantissa & 0x1fffu) == 0u, "bvh64 intersect ray: a test value is not a float16");
    return sign | ((exponent - 112u) << 10u) | (mantissa >> 13u);
}

class GuestBlock {
public:
    GuestBlock() {
        constexpr std::uintptr_t hint = std::uintptr_t{1} << 40u;
#ifdef _WIN32
        for (std::uintptr_t attempt = 0; block == nullptr && attempt < 16u; ++attempt) {
            block = static_cast<std::uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(hint + (attempt << 32u)), BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
        if (block == nullptr) block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        void* mapped = mmap(reinterpret_cast<void*>(hint), BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        block = mapped == MAP_FAILED ? nullptr : static_cast<std::uint8_t*>(mapped);
#endif
        Require(block != nullptr, "bvh64 intersect ray: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, false);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        munmap(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

    void Write(std::uint32_t node, std::uint32_t dword, std::uint32_t value) {
        std::memcpy(block + node * NodeBytes + dword * 4u, &value, sizeof(value));
    }

private:
    std::uint8_t* block = nullptr;
};

void WriteBox32(GuestBlock& guest, std::uint32_t node, const std::array<std::uint32_t, 4>& children) {
    for (std::uint32_t child = 0; child < 4u; ++child) {
        guest.Write(node, child, children[child]);
        for (std::uint32_t index = 0; index < 6u; ++index) guest.Write(node, 4u + child * 6u + index, Bits(Boxes[child][index]));
    }
}

void WriteBox16(GuestBlock& guest, std::uint32_t node, const std::array<std::uint32_t, 4>& children) {
    for (std::uint32_t child = 0; child < 4u; ++child) {
        guest.Write(node, child, children[child]);
        for (std::uint32_t index = 0; index < 6u; index += 2u) {
            const auto position = child * 6u + index;
            guest.Write(node, 4u + position / 2u, Half(Boxes[child][index]) | (Half(Boxes[child][index + 1u]) << 16u));
        }
    }
}

void WriteTriangles(GuestBlock& guest, std::uint32_t node) {
    for (std::uint32_t index = 0; index < TriangleVertices.size(); ++index) guest.Write(node, index, Bits(TriangleVertices[index]));
    guest.Write(node, 15u, TriangleId);
}

void FillNodes(GuestBlock& guest) {
    WriteTriangles(guest, 1u);
    WriteBox32(guest, 2u, Box32Children);
    WriteBox16(guest, 4u, Box16Children);
    WriteBox32(guest, 6u, PastEndChildren);
    WriteTriangles(guest, 7u);
}

void FillInput(std::uint64_t nodeBias) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto& test = Cases[tid % Cases.size()];
        const auto& ray = test.ray;
        const std::uint64_t node64 = nodeBias + test.node;
        auto* words = &Input[tid * Record];
        words[0] = test.node;
        words[1] = static_cast<std::uint32_t>(node64);
        words[2] = static_cast<std::uint32_t>(node64 >> 32u);
        words[3] = Bits(ray.extent);
        for (std::uint32_t axis = 0; axis < 3u; ++axis) {
            words[4u + axis] = Bits(ray.origin[axis]);
            words[7u + axis] = Bits(ray.direction[axis]);
            words[10u + axis] = Bits(ray.inverse[axis]);
        }
        words[13] = Half(ray.direction[0]) | (Half(ray.direction[1]) << 16u);
        words[14] = Half(ray.direction[2]) | (Half(ray.inverse[0]) << 16u);
        words[15] = Half(ray.inverse[1]) | (Half(ray.inverse[2]) << 16u);
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> BvhDescriptor(std::uint64_t base, std::uint64_t lastNode) {
    return {static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | BoxSort, static_cast<std::uint32_t>(lastNode), static_cast<std::uint32_t>((lastNode >> 32u) & 0x3ffu)};
}

void Run(AgcDriver::VulkanDevice& device, const GuestBlock& guest, bool addressInPointer) {
    const auto address = guest.Address();
    FillInput(addressInPointer ? address >> 3u : 0u);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto narrow = BvhDescriptor(address, LastNode);
    const auto wide = addressInPointer ? BvhDescriptor(0u, address / NodeBytes + LastNode) : narrow;
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(narrow.begin(), narrow.end(), userData.begin() + 8);
    std::copy(wide.begin(), wide.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(bool addressInPointer) {
    const std::string mode = addressInPointer ? "address in the node pointer" : "base in the descriptor";
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto& expected = Cases[tid % Cases.size()].expected;
        for (std::uint32_t form = 0; form < Forms; ++form) {
            for (std::uint32_t dword = 0; dword < 4u; ++dword) {
                const auto actual = Output[tid * Record + form * 4u + dword];
                Require(actual == expected[dword], std::string(FormNames[form]) + ", " + mode + ": thread " + std::to_string(tid) + " dword " + std::to_string(dword) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[dword]));
            }
        }
    }
}

void RunMode(bool addressInPointer) {
    if (ShaderRecompiler::RayTracingStrict() || ShaderRecompiler::RayTracingMiss()) Testing::Skip("APS5_RAYTRACING replaces the node test");
    auto& device = SharedVulkanTestDevice();
    GuestBlock guest;
    Require((guest.Address() >> 35u) != 0u, "bvh64 intersect ray: the guest block is not above 32 GiB, the node pointer would fit 32 bits");
    FillNodes(guest);
    Run(device, guest, addressInPointer);
    Check(addressInPointer);
}

const Testing::Case nodePointerAddress{"Bvh64IntersectRay_AddressInNodePointer_MatchesReference", [] {
    RunMode(true);
}};

const Testing::Case descriptorBase{"Bvh64IntersectRay_BaseInDescriptor_MatchesReference", [] {
    RunMode(false);
}};

} // namespace
