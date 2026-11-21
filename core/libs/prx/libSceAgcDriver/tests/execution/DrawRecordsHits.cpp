#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DirectDrawReport.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRecords.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "RuntimeAbi.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
namespace DrawRecords = AgcDriver::DriverDetail::DrawRecords;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t ColorBytes = 65536;
constexpr std::uint32_t RenderTargetTileMode = 0x1b;
constexpr std::size_t TableOffset = 0x300;
constexpr std::size_t NearOffset = 0x800;
constexpr std::size_t ShiftOffset = 0x900;
constexpr std::size_t StillOffset = 0xa00;
constexpr std::size_t FarOffset = 0xb00;
constexpr std::size_t ConstantsOffset = 0xc00;
constexpr std::size_t FetchBufferTableOffset = 0x000;
constexpr std::size_t FetchAttribTableOffset = 0x100;
constexpr std::size_t ShiftedSlotsOffset = 0x200;
constexpr std::size_t CurrentSlotsOffset = 0x400;
constexpr std::size_t InstanceSlotsOffset = 0x600;
constexpr std::uint32_t SlotRecords = 32;
constexpr std::uint32_t SlotShift = 16;
constexpr std::uint32_t OutputSlots = 32;
constexpr std::uint32_t Unwritten = 0xdeadbeefu;
constexpr std::uint32_t FetchIndexOffset = 5;
constexpr std::uint32_t FetchVertexOffset = 3;
constexpr std::uint32_t DrawsPerSubmission = 6;
constexpr std::size_t RingSlotsOffset = 0x1000;
constexpr std::size_t RingSlotBytes = 0x100;
constexpr std::size_t InnerOffset = 0x4000;
constexpr std::size_t InnerBytes = 0x100;
constexpr std::size_t RingSlots = 16;
constexpr std::size_t InnerBuffers = 8;
constexpr std::uint32_t ConstantRecords = 64;
constexpr std::uint32_t InnerRecords = 16;
constexpr std::array<std::uint32_t, 4> RawFormats{0x31016facu, 0x31014facu, 0x31015facu, 0x21016facu};
constexpr std::array<std::uint32_t, 2> SlotFormats{0x01014facu, 0x01015facu};
constexpr std::size_t TextureBlockBytes = 0x40000;
constexpr std::array<std::size_t, 2> TextureOffsets{0x0, 0x20000};
constexpr std::size_t TextureBytes = 1024;
constexpr std::uint32_t TextureUserWords = 12;
constexpr std::array<std::array<std::uint8_t, 4>, 2> Texels{{{0x10u, 0x80u, 0xc0u, 0xffu}, {0x33u, 0x66u, 0x99u, 0xffu}}};
constexpr std::array<std::uint32_t, 4> TextureVertexCounts{3u, 6u, 9u, 12u};
constexpr std::uint32_t TextureDraws = 3 * DrawsPerSubmission;

alignas(256) constexpr std::array<std::uint32_t, 16> PlainVertex{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 10> ConstantPixel{0xf4000100u, 0xfa000000u, 0xbf8cc07fu, 0x7e000204u, 0x06000002u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 40> TableVertex{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u,
    0xf4080104u, 0xfa000000u, 0xf4080204u, 0xfa000010u, 0xf4080304u, 0xfa000020u,
    0xbf8cc07fu,
    0xe00c2000u, 0x80010a05u, 0xe00c2000u, 0x80020e05u, 0xe00c2000u, 0x80031205u,
    0xbf8c3f70u,
    0x0602030au, 0x0604050bu, 0x0606070cu, 0x0608090du,
    0x0602030eu, 0x0604050fu, 0x06060710u, 0x06080911u,
    0x06020312u, 0x06040513u, 0x06060714u, 0x06080915u,
    0xf80008cfu, 0x04030201u,
    0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 11> ReadPixel{0x7e1a0280u, 0xdc308000u, 0x0c02000du, 0xbf8c3f70u, 0x7e10030cu, 0x7e120280u, 0x7e140280u, 0x7e160280u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 30> FetchVertex{
    0x7e020305u, 0x7e040308u,
    0xd70f6a05u, 0x00020a10u,
    0xd70f6a08u, 0x00021011u,
    0xf4080506u, 0xfa000000u,
    0xf4080606u, 0xfa000010u,
    0x02060b08u,
    0xbf8cc07fu,
    0xe0002000u, 0x80050a03u,
    0xe0002000u, 0x80060b03u,
    0xbf8c3f70u,
    0x4a08170au, 0x34080884u,
    0x7e180301u, 0x7e1a0302u, 0x7e1c0305u, 0x7e1e0308u,
    0xbf800000u, 0xbf800000u,
    0x7e200280u, 0x7e2202f2u,
    0xf80008cfu, 0x1110100au,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 16> RingPixel{
    0xf4280100u, 0xfa000010u,
    0xf4200200u, 0xfa000000u,
    0xbf8cc07fu,
    0xe0300000u, 0x80010400u,
    0xbf8c3f70u,
    0x7e000208u, 0x7e020304u, 0x7e040280u, 0x7e0602f2u,
    0xf800180fu, 0x03020100u,
    0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 12> SamplePixel{0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0xf800180fu, 0x03020100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header PlainVertexHeader{};
alignas(8) Header ConstantPixelHeader{};
alignas(8) Header TableVertexHeader{};
alignas(8) Header ReadPixelHeader{};
alignas(8) Header RingPixelHeader{};
alignas(8) Header SamplePixelHeader{};

alignas(256) constexpr std::array<std::uint32_t, 17> DataVertex{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u,
    0xf4000284u, 0xfa000000u, 0xbf8cc07fu, 0x0602020au,
    0xf80008cfu, 0x04030201u, 0xbf810000u,
};

alignas(8) Header DataVertexHeader{};

void Register(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

struct FetchHeader {
    Shader shader{};
    ShaderUserData userData{};
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> direct{};
    std::array<ShaderSemantic, 2> semantics{};
};

alignas(8) FetchHeader FetchVertexHeader{};

void RegisterFetchVertex() {
    auto& header = FetchVertexHeader;
    header.direct.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    header.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)] = 4;
    header.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)] = 6;
    header.userData.direct_resource_offset = header.direct.data();
    header.userData.direct_resource_count = static_cast<std::uint16_t>(header.direct.size());
    header.semantics[0].semantic = 0;
    header.semantics[0].hardware_mapping = 10;
    header.semantics[0].size_in_elements = 1;
    header.semantics[1].semantic = 1;
    header.semantics[1].hardware_mapping = 11;
    header.semantics[1].size_in_elements = 1;
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(FetchHeader);
    header.shader.shader_size = static_cast<std::uint32_t>(sizeof(FetchVertex));
    header.shader.code = FetchVertex.data();
    header.shader.type = 2;
    header.shader.user_data = &header.userData;
    header.shader.input_semantics = header.semantics.data();
    header.shader.num_input_semantics = static_cast<std::uint32_t>(header.semantics.size());
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "draw record hits: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }

private:
    std::uint8_t* block = nullptr;
};

class TextureBlock {
public:
    TextureBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(TextureBlockBytes, 0x200000));
        Require(block != nullptr, "draw record hits: cannot reserve the texture block");
        section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, static_cast<DWORD>(TextureBlockBytes), nullptr);
        Require(section != nullptr, "draw record hits: cannot create the texture block's backing");
        GuestArena::GuestArenaMapDirect_nid_postfix(block, TextureBlockBytes, section, 0, PAGE_READWRITE);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, TextureBlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + TextureBlockBytes != begin + TextureBlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + TextureBlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, TextureBlockBytes);
        }
        Require(block != nullptr, "draw record hits: cannot allocate the texture block");
#endif
        std::memset(block, 0, TextureBlockBytes);
        GuestAllocations::Mutation().Add(block, TextureBlockBytes, true, true, true);
    }

    TextureBlock(const TextureBlock&) = delete;
    TextureBlock& operator=(const TextureBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }

    bool Armed(std::size_t offset) const {
#ifdef _WIN32
        MEMORY_BASIC_INFORMATION info{};
        return VirtualQuery(block + offset, &info, sizeof(info)) == sizeof(info) && info.Protect == PAGE_READONLY;
#else
        static_cast<void>(offset);
        return false;
#endif
    }

    bool Arm(std::size_t offset) const {
        AgcDriver::GuestMemory::CollectWritesUncached(At(0), TextureBlockBytes);
        AgcDriver::GuestMemory::CollectWritesUncached(At(0), TextureBlockBytes);
        return Armed(offset);
    }

private:
    std::uint8_t* block = nullptr;
#ifdef _WIN32
    HANDLE section = nullptr;
#endif
};

std::array<std::uint32_t, 8> TextureDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
}

using RegisterWrites = std::vector<std::pair<std::uint32_t, std::uint32_t>>;

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

using DrawStep = std::function<std::uint32_t(std::vector<std::uint32_t>&, std::uint32_t)>;

void SubmitDraws(std::uint64_t vertex, std::uint64_t pixel, const RegisterWrites& userConfig, const RegisterWrites& shaderRegisters, const GuestBlock& target, std::uint32_t draws, std::uint32_t vertices = 3, std::uint32_t blend = 0, const DrawStep& step = {}) {
    const auto color = target.At(0);
    const float scale = 32.0f;
    const RegisterWrites context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000u | (RenderTargetTileMode << 14u)}, {0x1e0, blend},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(scale)}, {0x110, std::bit_cast<std::uint32_t>(scale)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    std::vector<std::uint32_t> commands;
    for (const auto& [offset, value] : userConfig) Append(commands, 0x79, {offset, value});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u)});
    Append(commands, 0x76, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)});
    Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u), static_cast<std::uint32_t>(pixel >> 40u)});
    for (const auto& [offset, value] : shaderRegisters) Append(commands, 0x76, {offset, value});
    for (std::uint32_t draw = 0; draw < draws; ++draw) {
        const auto count = step ? step(commands, draw) : vertices;
        Append(commands, 0x2d, {count, 2});
    }
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "draw record hits: a draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

std::vector<std::uint8_t> Image(const GuestBlock& target) {
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(target.At(0)), ColorBytes);
    return {target.Bytes(0), target.Bytes(0) + ColorBytes};
}

std::array<std::uint32_t, 4> VertexDescriptor(std::uint64_t address, std::uint32_t stride = 16u) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), 3u, 0x0104dfacu};
}

std::array<std::uint32_t, 4> SlotDescriptor(std::uint64_t address, std::uint32_t format = SlotFormats[0]) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), SlotRecords, format};
}

std::array<std::uint32_t, 4> OutputDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), OutputSlots * 16u, 0x31016facu};
}

std::array<std::uint32_t, 4> RawDescriptor(std::uint64_t address, std::uint32_t records, std::uint32_t format = RawFormats[0]) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), records, format};
}

void WriteWords(const GuestBlock& block, std::size_t offset, std::array<std::uint32_t, 4> words) {
    std::memcpy(block.Bytes(offset), words.data(), sizeof(words));
}

void WriteVertices(const GuestBlock& block, std::size_t offset, float x) {
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
        const std::array<float, 4> value{x, 0.0f, 0.0f, 0.0f};
        std::memcpy(block.Bytes(offset + vertex * 16u), value.data(), sizeof(value));
    }
}

struct Blocks {
    GuestBlock vertices;
    GuestBlock data;
    GuestBlock target;
    GuestBlock fetch;
    GuestBlock output;
    GuestBlock ring;
    GuestBlock other;
    std::unique_ptr<TextureBlock> texture;
};

Blocks* Scene = nullptr;

enum class Kind { Constants, Table, Fetch, Ring, Inner, RingInner, Sized, Format, FetchFormat, Texture };

struct State {
    Kind kind = Kind::Table;
    std::uint32_t variant = 0;
    std::uint32_t stride = 16;
    std::uint32_t vertices = 3;
    std::uint32_t texture = 0;
};

bool RingKind(Kind kind) {
    return kind == Kind::Ring || kind == Kind::Inner || kind == Kind::RingInner || kind == Kind::Sized || kind == Kind::Format;
}

std::size_t OuterSlot(const State& state) {
    switch (state.kind) {
    case Kind::Ring: return state.variant % 6;
    case Kind::RingInner: return 6 + state.variant % 6;
    case Kind::Inner: return 12;
    case Kind::Sized: return 13;
    default: return 14;
    }
}

std::size_t InnerAt(const State& state) {
    switch (state.kind) {
    case Kind::Inner:
    case Kind::RingInner: return InnerOffset + (state.variant % 6) * InnerBytes;
    case Kind::Sized: return InnerOffset + 6 * InnerBytes;
    default: return InnerOffset + 7 * InnerBytes;
    }
}

void Apply(const State& state) {
    auto& blocks = *Scene;
    if (RingKind(state.kind)) {
        const auto records = state.kind == Kind::Sized ? InnerRecords * (1 + state.variant % 6) : InnerRecords;
        const auto format = state.kind == Kind::Format ? RawFormats[state.variant % RawFormats.size()] : RawFormats[0];
        WriteWords(blocks.ring, RingSlotsOffset + OuterSlot(state) * RingSlotBytes + 16u, RawDescriptor(blocks.ring.At(InnerAt(state)), records, format));
        return;
    }
    switch (state.kind) {
    case Kind::Constants: {
        const float value = state.variant == 0 ? 0.25f : 0.5f;
        std::memcpy(blocks.data.Bytes(ConstantsOffset + state.variant * 0x40u), &value, sizeof(value));
        break;
    }
    case Kind::Table:
        WriteWords(blocks.vertices, TableOffset, VertexDescriptor(blocks.vertices.At(state.variant == 0 ? NearOffset : FarOffset)));
        WriteWords(blocks.vertices, TableOffset + 16u, VertexDescriptor(blocks.vertices.At(ShiftOffset), state.stride));
        break;
    case Kind::Fetch:
        WriteWords(blocks.fetch, FetchBufferTableOffset, SlotDescriptor(blocks.fetch.At(state.variant == 0 ? CurrentSlotsOffset : ShiftedSlotsOffset)));
        break;
    case Kind::FetchFormat:
        WriteWords(blocks.fetch, FetchBufferTableOffset, SlotDescriptor(blocks.fetch.At(CurrentSlotsOffset), SlotFormats[state.variant % SlotFormats.size()]));
        break;
    case Kind::Texture: {
        std::array<std::uint8_t, TextureBytes> wanted{};
        for (std::size_t at = 0; at < wanted.size(); at += Texels[0].size()) std::memcpy(wanted.data() + at, Texels[state.variant % Texels.size()].data(), Texels[0].size());
        auto* texels = blocks.texture->Bytes(TextureOffsets[state.texture % TextureOffsets.size()]);
        if (std::memcmp(texels, wanted.data(), wanted.size()) != 0) std::memcpy(texels, wanted.data(), wanted.size());
        break;
    }
    default:
        break;
    }
}

void Submit(const State& state, std::uint32_t draws) {
    auto& blocks = *Scene;
    switch (state.kind) {
    case Kind::Constants: {
        const auto pointer = blocks.data.At(ConstantsOffset + state.variant * 0x40u);
        SubmitDraws(reinterpret_cast<std::uintptr_t>(PlainVertex.data()), reinterpret_cast<std::uintptr_t>(ConstantPixel.data()), {{0x242, 4}}, {{0x8b, 0}, {0x8c, 0}, {0x00b, 3u << 1u}, {0x00c, static_cast<std::uint32_t>(pointer)}, {0x00d, static_cast<std::uint32_t>(pointer >> 32u)}, {0x00e, std::bit_cast<std::uint32_t>(0.25f)}}, blocks.target, draws, state.vertices);
        break;
    }
    case Kind::Table: {
        const auto table = blocks.vertices.At(TableOffset);
        const auto pointer = blocks.data.At(0);
        SubmitDraws(reinterpret_cast<std::uintptr_t>(TableVertex.data()), reinterpret_cast<std::uintptr_t>(ReadPixel.data()), {{0x242, 4}}, {{0x8b, 2u << 1u}, {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)}, {0x00b, 8u << 1u}, {0x00c, 0}, {0x00d, 0}, {0x00e, static_cast<std::uint32_t>(pointer)}, {0x00f, static_cast<std::uint32_t>(pointer >> 32u)}}, blocks.target, draws);
        break;
    }
    case Kind::Fetch:
    case Kind::FetchFormat: {
        const auto stored = OutputDescriptor(blocks.output.At(0));
        const auto buffers = blocks.fetch.At(FetchBufferTableOffset);
        const auto attributes = blocks.fetch.At(FetchAttribTableOffset);
        const std::array<std::uint32_t, 10> user{stored[0], stored[1], stored[2], stored[3], static_cast<std::uint32_t>(buffers), static_cast<std::uint32_t>(buffers >> 32u), static_cast<std::uint32_t>(attributes), static_cast<std::uint32_t>(attributes >> 32u), FetchVertexOffset, 0u};
        const auto pointer = blocks.data.At(0);
        RegisterWrites registers{{0x8b, static_cast<std::uint32_t>(user.size()) << 1u}};
        for (std::uint32_t word = 0; word < user.size(); ++word) registers.emplace_back(0x8cu + word, user[word]);
        for (const auto& entry : RegisterWrites{{0x00b, 8u << 1u}, {0x00c, 0}, {0x00d, 0}, {0x00e, static_cast<std::uint32_t>(pointer)}, {0x00f, static_cast<std::uint32_t>(pointer >> 32u)}}) registers.push_back(entry);
        SubmitDraws(reinterpret_cast<std::uintptr_t>(FetchVertex.data()), reinterpret_cast<std::uintptr_t>(ReadPixel.data()), {{0x242, 4}, {0x24a, FetchIndexOffset}}, registers, blocks.target, draws, state.vertices);
        break;
    }
    case Kind::Texture: {
        const auto image = TextureDescriptor(blocks.texture->At(TextureOffsets[state.texture % TextureOffsets.size()]));
        RegisterWrites registers{{0x8b, 0}, {0x8c, 0}, {0x00b, TextureUserWords << 1u}};
        for (std::uint32_t word = 0; word < TextureUserWords; ++word) registers.emplace_back(0x00cu + word, word < image.size() ? image[word] : 0u);
        SubmitDraws(reinterpret_cast<std::uintptr_t>(PlainVertex.data()), reinterpret_cast<std::uintptr_t>(SamplePixel.data()), {{0x242, 4}, {0x24a, 0}}, registers, blocks.target, draws, state.vertices);
        break;
    }
    default: {
        const auto constants = RawDescriptor(blocks.ring.At(RingSlotsOffset + OuterSlot(state) * RingSlotBytes), ConstantRecords);
        SubmitDraws(reinterpret_cast<std::uintptr_t>(PlainVertex.data()), reinterpret_cast<std::uintptr_t>(RingPixel.data()), {{0x242, 4}}, {{0x8b, 0}, {0x8c, 0}, {0x00b, 4u << 1u}, {0x00c, constants[0]}, {0x00d, constants[1]}, {0x00e, constants[2]}, {0x00f, constants[3]}}, blocks.target, draws, state.vertices);
        break;
    }
    }
}

std::vector<std::uint8_t> Output() {
    auto& output = Scene->output;
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(output.At(0)), OutputSlots * 16u);
    return {output.Bytes(0), output.Bytes(0) + OutputSlots * 16u};
}

std::vector<std::uint8_t> Render(const State& state, std::uint32_t draws) {
    auto& blocks = *Scene;
    std::memset(blocks.target.Bytes(0), 0, ColorBytes);
    std::vector<std::uint32_t> unwritten(OutputSlots * 4u, Unwritten);
    std::memcpy(blocks.output.Bytes(0), unwritten.data(), unwritten.size() * sizeof(std::uint32_t));
    Apply(state);
    Submit(state, draws);
    auto image = Image(blocks.target);
    if (state.kind == Kind::Fetch || state.kind == Kind::FetchFormat) {
        const auto written = Output();
        image.insert(image.end(), written.begin(), written.end());
    }
    return image;
}

namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;
using AgcDriver::Graphics::RuntimeSwitch;

std::vector<State> Alternating(Kind kind, std::size_t count) {
    std::vector<State> states;
    for (std::size_t index = 0; index < count; ++index) states.push_back({kind, static_cast<std::uint32_t>(index & 1u), 16});
    return states;
}

void Prepare() {
    auto& blocks = *Scene;
    WriteVertices(blocks.vertices, NearOffset, -8.0f);
    WriteVertices(blocks.vertices, ShiftOffset, 8.0f);
    WriteVertices(blocks.vertices, StillOffset, 0.0f);
    WriteVertices(blocks.vertices, FarOffset, -4.0f);
    WriteWords(blocks.vertices, TableOffset + 32u, VertexDescriptor(blocks.vertices.At(StillOffset)));
    const float one = 1.0f;
    std::memcpy(blocks.data.Bytes(0), &one, sizeof(one));
    const auto slots = [&](std::size_t offset, std::uint32_t shift) {
        for (std::uint32_t record = 0; record < SlotRecords; ++record) {
            const std::uint32_t slot = record + shift;
            std::memcpy(blocks.fetch.Bytes(offset + record * sizeof(slot)), &slot, sizeof(slot));
        }
    };
    slots(ShiftedSlotsOffset, SlotShift);
    slots(CurrentSlotsOffset, 0);
    std::memset(blocks.fetch.Bytes(InstanceSlotsOffset), 0, SlotRecords * sizeof(std::uint32_t));
    const std::array<std::uint32_t, 2> attributes{0u, 1u | (1u << 26u)};
    std::memcpy(blocks.fetch.Bytes(FetchAttribTableOffset), attributes.data(), sizeof(attributes));
    WriteWords(blocks.fetch, FetchBufferTableOffset + 16u, SlotDescriptor(blocks.fetch.At(InstanceSlotsOffset)));
    for (std::size_t slot = 0; slot < RingSlots; ++slot) {
        const float red = 0.125f * static_cast<float>(1 + slot % 7);
        std::memcpy(blocks.ring.Bytes(RingSlotsOffset + slot * RingSlotBytes), &red, sizeof(red));
    }
    for (std::size_t inner = 0; inner < InnerBuffers; ++inner) {
        const float green = 0.1f + 0.1f * static_cast<float>(inner);
        std::memcpy(blocks.ring.Bytes(InnerOffset + inner * InnerBytes), &green, sizeof(green));
    }
}

std::vector<State> Moving(Kind kind, std::size_t count) {
    std::vector<State> states;
    for (std::size_t index = 0; index < count; ++index) states.push_back({kind, static_cast<std::uint32_t>(index), 16});
    return states;
}

std::vector<State> MovingIdentities(Kind kind, std::size_t count) {
    std::vector<State> states;
    for (std::size_t index = 0; index < count; ++index) states.push_back({kind, static_cast<std::uint32_t>(index), 16, (index & 1u) != 0 ? 6u : 3u});
    return states;
}

namespace DirectReport = AgcDriver::DriverDetail::DirectDraws;
using DirectCounter = DirectReport::Counter;

struct DirectTotals {
    std::array<std::uint64_t, static_cast<std::size_t>(DirectCounter::Count)> counters{};
    std::uint64_t differences = 0;
    std::uint64_t buildDifferences = 0;
    std::uint64_t commitChecks = 0;

    static DirectTotals Now() {
        DirectTotals totals;
        for (std::size_t index = 0; index < totals.counters.size(); ++index) totals.counters[index] = DirectReport::Total(static_cast<DirectCounter>(index));
        for (std::size_t index = 0; index < static_cast<std::size_t>(DrawRecords::Field::Count); ++index) {
            totals.differences += DirectReport::Differences(static_cast<DrawRecords::Field>(index));
            totals.buildDifferences += DirectReport::BuildDifferences(static_cast<DrawRecords::Field>(index));
        }
        totals.commitChecks = DrawRecords::Checked(DrawRecords::Item::Commit);
        return totals;
    }

    std::uint64_t Since(const DirectTotals& base, DirectCounter counter) const { return counters[static_cast<std::size_t>(counter)] - base.counters[static_cast<std::size_t>(counter)]; }
    std::uint64_t Since(const DirectTotals& base, DirectCounter first, DirectCounter last) const {
        std::uint64_t total = 0;
        for (auto index = static_cast<std::size_t>(first); index <= static_cast<std::size_t>(last); ++index) total += Since(base, static_cast<DirectCounter>(index));
        return total;
    }
};

std::string DescribeDirect(const DirectTotals& now, const DirectTotals& base) {
    std::string text;
    for (std::size_t index = 0; index < now.counters.size(); ++index) {
        const auto moved = now.Since(base, static_cast<DirectCounter>(index));
        if (moved != 0) text += std::string("; ") + DirectReport::CounterName(static_cast<DirectCounter>(index)) + " " + std::to_string(moved);
    }
    if (now.commitChecks != base.commitChecks) text += "; committer checks " + std::to_string(now.commitChecks - base.commitChecks);
    return text;
}


void Direct(bool ring) {
    const bool exact = std::getenv("APS5_DIRECT_DRAWS_EXACT_KEY") != nullptr;
    const bool buffers = RuntimeSwitches::Set(RuntimeSwitch::DirectBuffers);
    const bool inputs = RuntimeSwitches::Set(RuntimeSwitch::DirectInputs);
    std::vector<State> cycled;
    for (std::uint32_t format = 0; format < RawFormats.size(); ++format) cycled.push_back({Kind::Format, format, 16, 3});
    const std::vector<std::pair<std::string, std::vector<State>>> sets{
        {"constants behind a moving user data pointer", Alternating(Kind::Constants, 4)},
        {"the same constants in draws of other vertex counts", MovingIdentities(Kind::Constants, 4)},
        {"a vertex buffer descriptor moving inside a memory table", Alternating(Kind::Table, 4)},
        {"a fetched vertex buffer moving", Alternating(Kind::Fetch, 4)},
        {"an inline constant buffer moving to a new ring slot", Moving(Kind::Ring, 4)},
        {"a constant buffer and the table in it moving", Moving(Kind::RingInner, 4)},
        {"a buffer format word cycling", cycled},
    };
    const auto base = DirectTotals::Now();
    for (const auto& [name, states] : sets) {
        RuntimeSwitches::Assign(RuntimeSwitch::LeanFront, true);
        RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, false);
        std::vector<std::vector<std::uint8_t>> today;
        for (const auto& state : states) today.push_back(Render(state, DrawsPerSubmission));
        RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, true);
        const auto before = DirectTotals::Now();
        if (buffers) {
            RuntimeSwitches::Assign(RuntimeSwitch::DirectBuffers, false);
            for (std::size_t index = 0; index < states.size(); ++index) Require(Render(states[index], DrawsPerSubmission) == today[index], "direct draws without prepared buffers, " + name + ": submission " + std::to_string(index) + " rendered differently from today's path" + DescribeDirect(DirectTotals::Now(), before));
            RuntimeSwitches::Assign(RuntimeSwitch::DirectBuffers, true);
        }
        if (inputs) {
            RuntimeSwitches::Assign(RuntimeSwitch::DirectInputs, false);
            for (std::size_t index = 0; index < states.size(); ++index) Require(Render(states[index], DrawsPerSubmission) == today[index], "direct draws without prepared inputs, " + name + ": submission " + std::to_string(index) + " rendered differently from today's path" + DescribeDirect(DirectTotals::Now(), before));
            RuntimeSwitches::Assign(RuntimeSwitch::DirectInputs, true);
        }
        for (std::size_t index = 0; index < states.size(); ++index) Require(Render(states[index], DrawsPerSubmission) == today[index], "direct draws, " + name + ": submission " + std::to_string(index) + " rendered differently from today's path" + DescribeDirect(DirectTotals::Now(), before));
        RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, false);
        const auto after = DirectTotals::Now();
        std::cout << "direct draws, " << name << ":" << DescribeDirect(after, before) << '\n';
        if (!ring || name != sets[1].first) continue;
        if (exact) Require(after.Since(before, DirectCounter::ClassesBuilt) != 0, "direct draws, " + name + ": with exact class keys, draws of another vertex count built no class of their own" + DescribeDirect(after, before));
        else Require(after.Since(before, DirectCounter::ClassesBuilt) == 0 && after.Since(before, DirectCounter::FallbackClassNew) == 0, "direct draws, " + name + ": draws of another vertex count did not share the class of the first" + DescribeDirect(after, before));
    }
    const auto now = DirectTotals::Now();
    const auto draws = now.Since(base, DirectCounter::Draws);
    const auto direct = now.Since(base, DirectCounter::Direct);
    const auto fallbacks = now.Since(base, DirectCounter::FallbackShape, DirectCounter::FallbackVerify);
    std::cout << "direct draws: " << draws << " draws, " << direct << " direct, " << fallbacks << " fallbacks, " << now.Since(base, DirectCounter::ClassesBuilt) << " classes built, " << now.Since(base, DirectCounter::StagesBuilt) << " stages built\n";
    if (!ring) return;
    Require(draws != 0 && direct + fallbacks == draws, "direct draws: the direct and fallback draws do not add up to the draws the finisher took" + DescribeDirect(now, base));
    Require(now.differences == base.differences && now.Since(base, DirectCounter::VerifyDiffering) == 0, "direct draws: the verify found differences" + DescribeDirect(now, base));
    Require(now.buildDifferences == base.buildDifferences && now.Since(base, DirectCounter::RefusedDiffers) == 0, "direct draws: a class build found differences" + DescribeDirect(now, base));
    Require(now.Since(base, DirectCounter::ClassesBuilt) != 0, "direct draws: no class was built" + DescribeDirect(now, base));
    const auto verified = now.Since(base, DirectCounter::VerifyChecked);
    if (DirectReport::VerifyEvery() == 1) Require(direct == 0 && verified != 0, "direct draws: with every direct job due for a verify, a class hit committed directly or none was verified" + DescribeDirect(now, base));
    else Require(direct != 0, "direct draws: no draw committed directly" + DescribeDirect(now, base));
    if (DirectReport::VerifyEvery() > 1) Require(verified != 0, "direct draws: the verify checked no class hit" + DescribeDirect(now, base));
    if (DirectReport::VerifyEvery() != 1 && DrawRecords::VerifyEvery() > 1) Require(now.commitChecks > base.commitChecks, "direct draws: the committer verify checked no direct hit" + DescribeDirect(now, base));
    if (inputs && DirectReport::VerifyEvery() != 1 && DrawRecords::VerifyEvery() != 1) {
        const auto prepared = now.Since(base, DirectCounter::InputsPrepared);
        const auto used = now.Since(base, DirectCounter::InputsUsed);
        const auto checked = now.Since(base, DirectCounter::InputsVerifyChecked);
        const bool inPlace = prepared != 0 || now.Since(base, DirectCounter::InputsNotInPlace) == 0;
        std::cout << "direct draws: " << prepared << " hits prepared their inputs, " << used << " took them, " << checked << " were verified" << (inPlace ? "" : " (the device binds no draw input in place)") << '\n';
        Require(now.Since(base, DirectCounter::InputsVerifyDiffering) == 0, "direct draws: the prepared inputs verify found differences" + DescribeDirect(now, base));
        Require(prepared == used + now.Since(base, DirectCounter::InputsImportChanged, DirectCounter::InputsDropped) + checked, "direct draws: the prepared input hits do not add up to the hits that took them, did not take them and were verified" + DescribeDirect(now, base));
        if (DirectReport::InputVerifyEvery() == 1) Require(used == 0 && (checked != 0 || !inPlace), "direct draws: with every prepared input hit due for a verify, a commit took prepared inputs or none was verified" + DescribeDirect(now, base));
        else if (inPlace) Require(used != 0, "direct draws: no commit took prepared inputs" + DescribeDirect(now, base));
        if (DirectReport::InputVerifyEvery() > 1 && inPlace) Require(checked != 0, "direct draws: the prepared inputs verify checked no hit" + DescribeDirect(now, base));
    }
    if (!buffers || DirectReport::VerifyEvery() == 1 || DrawRecords::VerifyEvery() == 1) return;
    const auto used = now.Since(base, DirectCounter::BuffersUsed);
    const auto checked = now.Since(base, DirectCounter::BuffersVerifyChecked);
    std::cout << "direct draws: " << now.Since(base, DirectCounter::BuffersPrepared) << " hits prepared their buffers, " << used << " bound them, " << checked << " were verified\n";
    Require(now.Since(base, DirectCounter::BuffersVerifyDiffering) == 0 && now.Since(base, DirectCounter::BuffersUnwatched) == 0, "direct draws: the prepared buffers verify found differences" + DescribeDirect(now, base));
    Require(now.Since(base, DirectCounter::BuffersPrepared) == used + now.Since(base, DirectCounter::BuffersPlanDiffers, DirectCounter::BuffersDropped) + checked, "direct draws: the prepared hits do not add up to the hits that bound, did not bind and were verified" + DescribeDirect(now, base));
    if (DirectReport::BufferVerifyEvery() == 1) Require(used == 0 && checked != 0, "direct draws: with every prepared hit due for a verify, a commit bound prepared buffers or none was verified" + DescribeDirect(now, base));
    else Require(used != 0, "direct draws: no commit bound prepared buffers" + DescribeDirect(now, base));
    if (DirectReport::BufferVerifyEvery() > 1) Require(checked != 0, "direct draws: the prepared buffers verify checked no hit" + DescribeDirect(now, base));
}

void PassTargets(bool ring) {
    const bool exact = std::getenv("APS5_DIRECT_DRAWS_EXACT_KEY") != nullptr;
    const bool passTargets = RuntimeSwitches::Set(RuntimeSwitch::DirectPassTargets);
    auto& blocks = *Scene;
    const State state{Kind::Constants, 0, 16, 3};
    const auto render = [&] {
        std::memset(blocks.target.Bytes(0), 0, ColorBytes);
        Apply(state);
        const auto pointer = blocks.data.At(ConstantsOffset);
        SubmitDraws(reinterpret_cast<std::uintptr_t>(PlainVertex.data()), reinterpret_cast<std::uintptr_t>(ConstantPixel.data()), {{0x242, 4}}, {{0x8b, 0}, {0x8c, 0}, {0x00b, 3u << 1u}, {0x00c, static_cast<std::uint32_t>(pointer)}, {0x00d, static_cast<std::uint32_t>(pointer >> 32u)}, {0x00e, std::bit_cast<std::uint32_t>(0.25f)}}, blocks.target, DrawsPerSubmission, 3, 0, [](std::vector<std::uint32_t>&, std::uint32_t draw) { return (draw & 1u) != 0 ? 6u : 3u; });
        return Image(blocks.target);
    };
    constexpr std::size_t Rounds = 4;
    RuntimeSwitches::Assign(RuntimeSwitch::LeanFront, true);
    RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, false);
    RuntimeSwitches::Assign(RuntimeSwitch::DirectPassTargets, false);
    const auto today = render();
    RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, true);
    const auto base = DirectTotals::Now();
    for (std::size_t round = 0; round < Rounds; ++round) Require(render() == today, "pass targets: round " + std::to_string(round) + " without the hold rendered differently from today's path" + DescribeDirect(DirectTotals::Now(), base));
    RuntimeSwitches::Assign(RuntimeSwitch::DirectPassTargets, true);
    const auto before = DirectTotals::Now();
    for (std::size_t round = 0; round < Rounds; ++round) Require(render() == today, "pass targets: round " + std::to_string(round) + " with the hold rendered differently from today's path" + DescribeDirect(DirectTotals::Now(), before));
    RuntimeSwitches::Assign(RuntimeSwitch::DirectPassTargets, passTargets);
    RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, false);
    const auto now = DirectTotals::Now();
    std::cout << "pass targets, two records alternating in one pass:" << DescribeDirect(now, before) << '\n';
    Require(before.Since(base, DirectCounter::HoldShared) == 0, "pass targets: a commit held targets by another record's pass with the switch off" + DescribeDirect(before, base));
    if (!ring || !exact || DirectReport::VerifyEvery() == 1 || DrawRecords::VerifyEvery() == 1) return;
    Require(now.Since(before, DirectCounter::HoldShared) != 0, "pass targets: no commit held the targets another record refreshed in the open pass" + DescribeDirect(now, before));
}

constexpr std::uint32_t PlannedBufferBytes = 256;
constexpr std::uint32_t LargeBufferBytes = 8192;
constexpr std::size_t KeptBufferOffset = 0x1000;
constexpr std::size_t InPlaceBufferOffset = 0x2000;
constexpr std::size_t MovedBufferOffset = 0x3000;
constexpr std::size_t LargeBufferOffset = 0x4000;
constexpr std::uint64_t UnmappedAddress = 0x1000;

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

ShaderRecompiler::DescriptorBinding PlannedBuffers(const GuestBlock& block, std::uint64_t first, std::uint32_t firstBytes) {
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 2;
    const auto moving = RawDescriptor(first, firstBytes);
    const auto still = RawDescriptor(block.At(InPlaceBufferOffset), PlannedBufferBytes);
    binding.guestDescriptor.assign(moving.begin(), moving.end());
    binding.guestDescriptor.insert(binding.guestDescriptor.end(), still.begin(), still.end());
    binding.bufferWritten = {false, false};
    return binding;
}

ShaderRecompiler::DescriptorBinding PlannedData(std::uint32_t value) {
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::ShaderData;
    binding.descriptorSet = 0;
    binding.binding = 5;
    binding.count = 1;
    binding.guestDescriptor.assign(ShaderRecompiler::RuntimeAbi::ShaderDataDwords, 0u);
    binding.guestDescriptor[0] = ShaderRecompiler::RuntimeAbi::Version;
    binding.guestDescriptor[ShaderRecompiler::RuntimeAbi::UserDataDword] = value;
    return binding;
}

int PreparedBuffersMatch() {
    using AgcDriver::Graphics::CompiledShader;
    using AgcDriver::Graphics::ShaderResources;
    using Refusal = ShaderResources::PreparedBuffers::Refusal;
    using ShaderRecompiler::ShaderStage;
    const auto device = OpenVulkanTestDevice();
    if (!device) return VulkanTestSkipped;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    Require(context.recorder != nullptr && context.descriptorCache != nullptr, "prepared buffers: the device has no recorder or descriptor cache");
    Require(ShaderResources::DrawUploadLimit() < LargeBufferBytes, "prepared buffers: the draw upload limit does not leave the large buffer over it");
    GuestBlock block;
    std::memset(block.Bytes(KeptBufferOffset), 0x11, PlannedBufferBytes);
    std::memset(block.Bytes(InPlaceBufferOffset), 0x22, PlannedBufferBytes);
    std::memset(block.Bytes(MovedBufferOffset), 0x33, PlannedBufferBytes);
    std::memset(block.Bytes(LargeBufferOffset), 0x44, LargeBufferBytes);
    AgcDriver::GuestMemory::CollectWritesUncached(block.At(0), BlockBytes);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto& recorder = *context.recorder;
    ShaderRecompiler::RecompileResult vertex;
    vertex.bindings = {PlannedBuffers(block, block.At(KeptBufferOffset), PlannedBufferBytes), PlannedData(7)};
    vertex.shaderDataDwords = ShaderRecompiler::RuntimeAbi::ShaderDataDwords;
    const ShaderRecompiler::RecompileResult fragment{};
    const AgcDriver::Graphics::ColorTarget target{};
    const auto stagesOf = [&](const ShaderRecompiler::RecompileResult& program) { return std::array<CompiledShader, 2>{{{ShaderStage::Vertex, &program, 0}, {ShaderStage::Fragment, &fragment, 0}}}; };
    const auto expectRefused = [&](const ShaderResources& owner, const ShaderResources::BufferPlan& plan, const ShaderRecompiler::RecompileResult& program, Refusal reason, bool todayMoves, const std::string& what) {
        const auto stages = stagesOf(program);
        const auto moved = owner.MovedReadOnlyBuffers(stages, recorder, &target);
        Require(moved.has_value() == todayMoves, "prepared buffers: today's check " + std::string(todayMoves ? "refused " : "took ") + what);
        ShaderResources::PreparedBuffers prepared;
        Require(!ShaderResources::PrepareDirectBuffers(plan, stages, &target, prepared) && prepared.refusal == reason, "prepared buffers: " + what + " was prepared, or refused for another reason (" + std::to_string(static_cast<int>(prepared.refusal)) + ")");
    };
    {
        ShaderResources resources(context, vertex, fragment, target, 0, 0);
        const auto reads = resources.InPlaceReads();
        const bool inPlace = std::ranges::any_of(reads, [&](const auto& range) { return range.first <= block.At(InPlaceBufferOffset) && block.At(InPlaceBufferOffset) + PlannedBufferBytes <= range.second; });
        if (!resources.Reusable() || !inPlace) {
            std::cout << "skipped, the template reads the guest block through no host import, so no buffer is read in place\n";
            return VulkanTestSkipped;
        }
        const auto plan = resources.DirectBufferPlan(stagesOf(vertex));
        Require(plan != nullptr, "prepared buffers: a reusable template with guest buffers and shader data has no buffer plan");
        Require(resources.DirectBufferPlan(stagesOf(vertex)) == plan, "prepared buffers: the buffer plan was built again");
        const auto expectSame = [&](const ShaderRecompiler::RecompileResult& program, std::size_t movedCount, std::size_t selectedCount, const std::string& what) {
            const auto stages = stagesOf(program);
            const auto today = resources.MovedReadOnlyBuffers(stages, recorder, &target);
            Require(today.has_value() && today->size() == movedCount, "prepared buffers, " + what + ": today's check did not move " + std::to_string(movedCount) + " buffers");
            std::vector<std::size_t> selected;
            const auto bindings = resources.PrepareDrawBindings(recorder, *today, false, nullptr, &selected);
            Require(bindings != nullptr && bindings->snapshots.size() == selected.size() && selected.size() == selectedCount, "prepared buffers, " + what + ": today's selection did not snapshot " + std::to_string(selectedCount) + " allocations");
            ShaderResources::PreparedBuffers prepared;
            Require(ShaderResources::PrepareDirectBuffers(*plan, stages, &target, prepared) && prepared.refusal == Refusal::None, "prepared buffers, " + what + ": the draw's buffers were not prepared (" + std::to_string(static_cast<int>(prepared.refusal)) + ")");
            Require(prepared.moved.size() == today->size(), "prepared buffers, " + what + ": the moved lists differ in length");
            for (std::size_t index = 0; index < today->size(); ++index) {
                const auto& mine = prepared.moved[index];
                const auto& theirs = (*today)[index];
                Require(mine.allocation == theirs.allocation && mine.address == theirs.address && mine.size == theirs.size && std::ranges::equal(mine.words, theirs.words), "prepared buffers, " + what + ": moved entry " + std::to_string(index) + " differs");
            }
            Require(prepared.selected.size() == selected.size(), "prepared buffers, " + what + ": the selections differ in length");
            std::array<std::size_t, 3> kinds{};
            for (std::size_t index = 0; index < selected.size(); ++index) {
                const auto& mine = prepared.selected[index];
                const auto& snapshot = bindings->snapshots[index];
                Require(mine.allocation == selected[index] && mine.begin == snapshot.address && mine.bytes == snapshot.bytes, "prepared buffers, " + what + ": selected allocation " + std::to_string(index) + " has another index or guest range");
                Require(mine.offset % plan->alignment == 0 && mine.offset + mine.bytes <= prepared.blob.size(), "prepared buffers, " + what + ": selected allocation " + std::to_string(index) + " has an unaligned or outlying slice");
                const auto bytes = snapshot.Bytes();
                Require(bytes.size() == mine.bytes && std::memcmp(bytes.data(), prepared.blob.data() + mine.offset, mine.bytes) == 0, "prepared buffers, " + what + ": selected allocation " + std::to_string(index) + " holds other bytes");
                ++kinds[!mine.moved ? 0 : mine.begin == 0 ? 1 : 2];
            }
            return kinds;
        };
        auto drawn = vertex;
        drawn.bindings[0] = PlannedBuffers(block, block.At(MovedBufferOffset), PlannedBufferBytes);
        drawn.bindings[1] = PlannedData(9);
        const auto kinds = expectSame(drawn, 2, 3, "a moved buffer, an in-place buffer and changed shader data");
        Require(kinds == std::array<std::size_t, 3>{1, 1, 1}, "prepared buffers: the draw did not select one in-place buffer, one moved data block and one moved guest buffer");
        const auto still = expectSame(vertex, 0, 2, "the template's own stages");
        Require(still == std::array<std::size_t, 3>{2, 0, 0}, "prepared buffers: the template's own draw did not select its two in-place buffers");
        const auto expectCommitted = [&](const ShaderRecompiler::RecompileResult& program, std::size_t movedRanges, const std::string& what) {
            const auto stages = stagesOf(program);
            ShaderResources::PreparedBuffers prepared;
            Require(ShaderResources::PrepareDirectBuffers(*plan, stages, &target, prepared), "prepared buffers, " + what + ": the draw's buffers were not prepared");
            prepared.plan = plan;
            std::vector<std::pair<std::uint64_t, std::uint64_t>> todayRanges;
            std::vector<std::pair<std::uint64_t, std::uint64_t>> preparedRanges;
            ShaderResources::MoveTicket todayTicket;
            ShaderResources::MoveTicket preparedTicket;
            todayTicket.ranges = &todayRanges;
            preparedTicket.ranges = &preparedRanges;
            std::optional<std::vector<ShaderResources::MovedBuffer>> today;
            for (std::size_t round = 0; round < 2; ++round) {
                todayTicket.checked = todayTicket.trusted = preparedTicket.checked = preparedTicket.trusted = 0;
                today = resources.MovedReadOnlyBuffers(stages, recorder, &target, nullptr, &todayTicket);
                Require(today.has_value() && resources.PreparedMovesClear(prepared, recorder, &preparedTicket), "prepared buffers, " + what + ": a moved range was refused");
                const bool same = todayTicket.valid == preparedTicket.valid && todayTicket.recorder == preparedTicket.recorder && todayTicket.note == preparedTicket.note && todayTicket.pending == preparedTicket.pending && todayTicket.allocations == preparedTicket.allocations && todayTicket.epoch == preparedTicket.epoch && todayTicket.checked == preparedTicket.checked && todayTicket.trusted == preparedTicket.trusted && todayRanges == preparedRanges;
                Require(same && preparedTicket.checked == movedRanges && preparedTicket.trusted == (round == 0 ? 0 : movedRanges), "prepared buffers, " + what + ": round " + std::to_string(round) + " of the prepared moved-range check left the ticket otherwise than today's check");
                Require(ShaderResources::ComparePreparedTickets(&preparedTicket, todayTicket).what == nullptr, "prepared buffers, " + what + ": round " + std::to_string(round) + ": the verify found the tickets different");
            }
            auto otherTicket = preparedTicket;
            ++otherTicket.checked;
            Require(ShaderResources::ComparePreparedTickets(&otherTicket, todayTicket).what != nullptr && ShaderResources::ComparePreparedTickets(nullptr, todayTicket).what != nullptr, "prepared buffers, " + what + ": the verify did not find a ticket of another count, or a refused prepared check");
            Require(ShaderResources::ComparePreparedMoves(prepared, *today).what == nullptr, "prepared buffers, " + what + ": the verify found the moved lists different");
            std::vector<std::size_t> selected;
            const auto bindings = resources.PrepareDrawBindings(recorder, *today, false, nullptr, &selected);
            Require(resources.PreparedSelectionHolds(recorder, prepared), "prepared buffers, " + what + ": the committer would not take the prepared selection");
            Require(ShaderResources::ComparePreparedSelection(prepared, bindings.get(), selected).what == nullptr, "prepared buffers, " + what + ": the verify found the selections different");
            auto corrupted = prepared;
            corrupted.blob[corrupted.selected.back().offset] ^= std::byte{0xff};
            const auto changed = ShaderResources::ComparePreparedSelection(corrupted, bindings.get(), selected);
            Require(changed.what != nullptr && std::string_view(changed.what) == "bytes" && !changed.memoryChanged, "prepared buffers, " + what + ": the verify did not find a changed byte");
            if (movedRanges != 0) DirectReport::NoteBufferDifference(changed.what, changed.detail);
            if (!prepared.moved.empty()) {
                auto resized = prepared;
                resized.moved.front().size += 4;
                Require(ShaderResources::ComparePreparedMoves(resized, *today).what != nullptr, "prepared buffers, " + what + ": the verify did not find a moved buffer of another size");
            }
            Require(resources.ComparePreparedBinding(recorder, prepared, bindings.get(), selected).what == nullptr, "prepared buffers, " + what + ": the verify found the bound selection different from today's");
            if (!selected.empty()) {
                const std::vector<std::size_t> fewer(selected.begin(), selected.end() - 1);
                const auto shorter = resources.ComparePreparedBinding(recorder, prepared, bindings.get(), fewer);
                Require(shorter.what != nullptr && std::string_view(shorter.what) == "bound selection", "prepared buffers, " + what + ": the verify did not find a bound selection of another length");
            }
            std::vector<std::size_t> taken;
            auto used = ShaderResources::PreparedBuffers::Selection::Hazard;
            const auto bound = resources.PrepareDrawBindings(recorder, prepared.moved, false, nullptr, &taken, &prepared, &used);
            Require(used == ShaderResources::PreparedBuffers::Selection::Taken && bound != nullptr && taken == selected && bound->snapshots.size() == bindings->snapshots.size(), "prepared buffers, " + what + ": the prepared selection was not bound, or bound other allocations");
            const auto alignment = std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 1);
            for (std::size_t index = 0; index < taken.size(); ++index) {
                const auto& mine = bound->snapshots[index];
                const auto& theirs = bindings->snapshots[index];
                Require(mine.buffer == bound->snapshots.front().buffer && mine.offset % alignment == 0, "prepared buffers, " + what + ": snapshot " + std::to_string(index) + " is not an aligned part of one upload slice");
                Require(mine.address == theirs.address && mine.bytes == theirs.bytes && std::memcmp(mine.Bytes().data(), theirs.Bytes().data(), mine.bytes) == 0, "prepared buffers, " + what + ": snapshot " + std::to_string(index) + " binds another range or other bytes than today's");
            }
        };
        expectCommitted(drawn, 1, "a moved buffer, an in-place buffer and changed shader data, on the committer");
        expectCommitted(vertex, 0, "the template's own stages, on the committer");
        auto longer = vertex;
        longer.bindings[1].guestDescriptor.push_back(0u);
        expectRefused(resources, *plan, longer, Refusal::Classification, false, "shader data of another size");
        auto unmapped = vertex;
        unmapped.bindings[0] = PlannedBuffers(block, UnmappedAddress, PlannedBufferBytes);
        expectRefused(resources, *plan, unmapped, Refusal::Inaccessible, false, "a buffer moved to unmapped memory");
        auto large = vertex;
        large.bindings[0] = PlannedBuffers(block, block.At(LargeBufferOffset), LargeBufferBytes);
        expectRefused(resources, *plan, large, Refusal::UploadLimit, true, "a buffer moved to a range over the upload limit");
    }
    SetEnvironment("APS5_DRAW_SNAPSHOT_MAX_MIB", "0");
    {
        ShaderResources capped(context, vertex, fragment, target, 0, 0);
        SetEnvironment("APS5_DRAW_SNAPSHOT_MAX_MIB", "64");
        const auto plan = capped.DirectBufferPlan(stagesOf(vertex));
        Require(plan != nullptr, "prepared buffers: a template with no snapshot budget has no buffer plan");
        auto moving = vertex;
        moving.bindings[0] = PlannedBuffers(block, block.At(MovedBufferOffset), PlannedBufferBytes);
        expectRefused(capped, *plan, moving, Refusal::Budget, false, "a moved buffer over the snapshot budget");
    }
    {
        ShaderResources resources(context, vertex, fragment, target, 0, 0);
        const auto plan = resources.DirectBufferPlan(stagesOf(vertex));
        auto drawn = vertex;
        drawn.bindings[0] = PlannedBuffers(block, block.At(MovedBufferOffset), PlannedBufferBytes);
        const auto stages = stagesOf(drawn);
        ShaderResources::PreparedBuffers prepared;
        Require(plan != nullptr && ShaderResources::PrepareDirectBuffers(*plan, stages, &target, prepared), "prepared buffers, hazards: the draw's buffers were not prepared");
        prepared.plan = plan;
        const auto inPlace = std::ranges::find_if(prepared.selected, [](const ShaderResources::PreparedBuffers::Selected& item) { return !item.moved; });
        Require(inPlace != prepared.selected.end(), "prepared buffers, hazards: no in-place buffer was selected");
        GuestBlock outside;
        std::memset(outside.Bytes(0), 0x66, PlannedBufferBytes);
        auto away = vertex;
        away.bindings[0] = PlannedBuffers(block, outside.At(0), PlannedBufferBytes);
        const auto awayStages = stagesOf(away);
        ShaderResources::PreparedBuffers early;
        Require(ShaderResources::PrepareDirectBuffers(*plan, awayStages, &target, early), "prepared buffers, hazards: a buffer moved to another block was not prepared");
        early.plan = plan;
        const bool watched = AgcDriver::GuestMemory::Watched(outside.At(0), PlannedBufferBytes);
        std::cout << (watched ? "a guest write after the finisher's read is checked to count as memory changed\n" : "the moved buffer's block is not write-watched, so a guest write after the finisher's read is not checked to count as memory changed\n");
        if (watched) {
            early.generation = AgcDriver::GuestMemory::TrackerGeneration();
            std::memset(outside.Bytes(0), 0x77, PlannedBufferBytes);
            AgcDriver::GuestMemory::CollectWritesUncached(outside.At(0), BlockBytes);
            const auto today = resources.MovedReadOnlyBuffers(awayStages, recorder, &target);
            Require(today.has_value(), "prepared buffers, hazards: today's check refused a buffer moved to another block");
            std::vector<std::size_t> selected;
            const auto bindings = resources.PrepareDrawBindings(recorder, *today, false, nullptr, &selected);
            const auto difference = ShaderResources::ComparePreparedSelection(early, bindings.get(), selected);
            Require(difference.what != nullptr && difference.memoryChanged, "prepared buffers, hazards: a guest write between the finisher's read and today's was not told apart as memory changed (" + std::string(difference.what != nullptr ? difference.what : "no difference") + ")");
        }
        auto gone = prepared;
        const auto movedEntry = std::ranges::find_if(gone.moved, [](const ShaderResources::MovedBuffer& entry) { return entry.address != 0; });
        Require(movedEntry != gone.moved.end(), "prepared buffers, hazards: no moved guest buffer was prepared");
        movedEntry->address = UnmappedAddress;
        gone.forget = AgcDriver::GuestMemory::ForgetSerial();
        Require(resources.PreparedMovesClear(gone, recorder, nullptr), "prepared buffers, hazards: a moved range the finisher found accessible was refused while no mapping changed");
        gone.forget += 2;
        Require(!resources.PreparedMovesClear(gone, recorder, nullptr), "prepared buffers, hazards: an unmapped moved range was taken after a mapping changed");
        using Selection = ShaderResources::PreparedBuffers::Selection;
        auto guarded = prepared;
        ++guarded.depthGuards;
        std::vector<std::size_t> selected;
        auto used = Selection::Taken;
        static_cast<void>(resources.PrepareDrawBindings(recorder, guarded.moved, false, nullptr, &selected, &guarded, &used));
        Require(used == Selection::DepthGuards, "prepared buffers, hazards: a depth guard change since the finisher's read did not send the selection to today's way");
        recorder.NotePendingWrite(block.At(InPlaceBufferOffset), PlannedBufferBytes);
        Require(resources.PreparedMovesClear(prepared, recorder, nullptr), "prepared buffers, hazards: a pending write beside the moved range refused it");
        used = Selection::Taken;
        static_cast<void>(resources.PrepareDrawBindings(recorder, prepared.moved, false, nullptr, &selected, &prepared, &used));
        Require(used == Selection::Hazard && std::ranges::find(selected, inPlace->allocation) == selected.end(), "prepared buffers, hazards: a pending write over the in-place range did not send the selection to today's way, which leaves the range out");
        Require(!resources.PreparedSelectionHolds(recorder, prepared), "prepared buffers, hazards: the verify would compare a selection the committer does not take");
        recorder.NotePendingWrite(block.At(MovedBufferOffset), PlannedBufferBytes);
        Require(!resources.PreparedMovesClear(prepared, recorder, nullptr), "prepared buffers, hazards: a pending write over the moved range did not refuse it");
        recorder.Sync();
    }
    std::cout << "prepared buffers match today's moved buffers and snapshot selection for a moved buffer, an in-place buffer and changed shader data, and are refused for shader data of another size, unmapped memory, a range over the upload limit and the snapshot budget; on the committer they leave the ticket as today's check does, bind today's bytes from one aligned upload slice, and give way to today's check and selection under pending writes, a depth guard change since the read, and an unmapped moved range after a mapping change; the verify tells a changed byte, a resized moved buffer, another ticket, another bound selection and a guest write after the read apart\n";
    return 0;
}

constexpr std::size_t InputIndexOffset = 0x100;
constexpr std::size_t InputVertexOffset = 0x400;
constexpr std::uint32_t InputVertices = 4;

ShaderRecompiler::VertexAttribute InputAttribute(std::uint64_t address, std::uint32_t stride, std::uint32_t records) {
    return ShaderRecompiler::VertexAttribute{0, 4, {{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (stride << 16u), records, 77u << 12u}}, 0};
}

int PreparedInputsMatch() {
    using AgcDriver::DrawRecordInputs;
    using AgcDriver::Graphics::CompiledShader;
    using AgcDriver::Graphics::PreparedInputs;
    using Refusal = PreparedInputs::Refusal;
    using ShaderRecompiler::ShaderStage;
    namespace Graphics = AgcDriver::Graphics;
    const auto device = OpenVulkanTestDevice();
    if (!device) return VulkanTestSkipped;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    Require(context.recorder != nullptr, "prepared inputs: the device has no recorder");
    auto& recorder = *context.recorder;
    GuestBlock block;
    const std::array<std::uint16_t, 6> indices{0, 1, 2, 2, 1, 3};
    std::memcpy(block.Bytes(InputIndexOffset), indices.data(), sizeof(indices));
    for (std::uint32_t at = 0; at < InputVertices; ++at) {
        const std::array<float, 4> value{static_cast<float>(at), 0.0f, 0.0f, 1.0f};
        std::memcpy(block.Bytes(InputVertexOffset + at * 16u), value.data(), sizeof(value));
    }
    AgcDriver::GuestMemory::CollectWritesUncached(block.At(0), BlockBytes);
    ShaderRecompiler::RecompileResult vertex;
    vertex.vertexAttributes = {InputAttribute(block.At(InputVertexOffset), 16, InputVertices)};
    const ShaderRecompiler::RecompileResult fragment{};
    const std::array<CompiledShader, 2> stages{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Fragment, &fragment, 0}}};
    Graphics::State state{};
    state.stages.path = Graphics::ShaderPath::Vertex;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, 64, 64, -64, 0, 1};
    state.scissor = {{0, 0}, {64, 64}};
    state.renderExtent = {64, 64};
    auto recipe = std::make_shared<AgcDriver::DrawRecipe>();
    recipe->device = context.device;
    recipe->vertexInput = Graphics::BuildVertexInputLayout(context, vertex.vertexAttributes);
    recipe->fragmentOutputs = 1;
    recipe->shaderStages = Graphics::PipelineStages(stages);
    const AgcDriver::Pm4::DrawParameters indexed{block.At(InputIndexOffset), static_cast<std::uint32_t>(indices.size()), 2, 1, 0, true, 0, 0};
    const AgcDriver::Pm4::DrawParameters automatic{0, InputVertices, 0, 1, 0, false, 0, 0};
    const auto prepare = [&](const AgcDriver::Pm4::DrawParameters& draw, PreparedInputs& prepared, const std::string& what) {
        Require(Graphics::PrepareDrawInputs(context, state, draw, stages, recipe, prepared), "prepared inputs, " + what + ": the draw's inputs were not prepared (" + std::to_string(static_cast<int>(prepared.refusal)) + ")");
    };
    const auto verify = [&](const Graphics::State& at, const AgcDriver::Pm4::DrawParameters& draw, PreparedInputs& prepared, DrawRecordInputs wanted, const std::string& what, DrawRecordInputs took = DrawRecordInputs::None) {
        std::string difference;
        auto taken = DrawRecordInputs::None;
        const auto use = Graphics::VerifyPreparedInputsForTest(context, at, draw, stages, *recipe, prepared, difference, &taken);
        Require(use == wanted, "prepared inputs, " + what + ": the committer answered " + std::to_string(static_cast<int>(use)) + " instead of " + std::to_string(static_cast<int>(wanted)) + (difference.empty() ? std::string() : " (" + difference + ")"));
        Require(took == DrawRecordInputs::None || taken == took, "prepared inputs, " + what + ": the committer took the inputs as " + std::to_string(static_cast<int>(taken)) + " instead of " + std::to_string(static_cast<int>(took)));
        return difference;
    };
    const auto refused = [&](const Graphics::State& at, const AgcDriver::Pm4::DrawParameters& draw, std::shared_ptr<const AgcDriver::DrawRecipe> with, Refusal reason, const std::string& what) {
        PreparedInputs prepared;
        Require(!Graphics::PrepareDrawInputs(context, at, draw, stages, std::move(with), prepared) && prepared.refusal == reason, "prepared inputs: " + what + " was prepared, or refused for another reason (" + std::to_string(static_cast<int>(prepared.refusal)) + ")");
    };
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        if (!context.drawInputsInPlace || Graphics::HostImportFor(context, block.At(0), BlockBytes) == nullptr) {
            std::cout << "skipped, the device binds no draw input in place from a host import\n";
            return VulkanTestSkipped;
        }
        PreparedInputs prepared;
        prepare(indexed, prepared, "an indexed draw");
        Require(prepared.ranges.size() == 2 && prepared.maxIndex == 3 && prepared.inPlaceReads.size() == 2 && prepared.vertexHandles.size() == 1 && prepared.vertexOffsets.size() == 1, "prepared inputs: the indexed draw did not prepare one index range and one vertex range up to vertex 3");
        Require(prepared.inPlaceReads.front() == std::pair<std::uint64_t, std::uint64_t>{block.At(InputIndexOffset), block.At(InputIndexOffset) + sizeof(indices)}, "prepared inputs: the index range is not the draw's first in-place read");
        verify(state, indexed, prepared, DrawRecordInputs::Checked, "an indexed in-place draw", DrawRecordInputs::Used);
        PreparedInputs drawn;
        prepare(automatic, drawn, "an auto draw");
        Require(drawn.ranges.size() == 1 && drawn.maxIndex == InputVertices - 1u, "prepared inputs: the auto draw did not prepare one vertex range up to its last vertex");
        verify(state, automatic, drawn, DrawRecordInputs::Checked, "an auto draw", DrawRecordInputs::Used);
        PreparedInputs elsewhere;
        prepare(indexed, elsewhere, "an indexed draw for another state");
        auto copy = state;
        verify(copy, indexed, elsewhere, DrawRecordInputs::Shape, "the inputs prepared for another state object");
        PreparedInputs corrupted;
        prepare(indexed, corrupted, "an indexed draw to corrupt");
        ++corrupted.maxIndex;
        const auto difference = verify(state, indexed, corrupted, DrawRecordInputs::Differs, "a corrupted max index");
        Require(difference.find("max index") != std::string::npos, "prepared inputs: the verify did not name the max index: " + difference);
        PreparedInputs offset;
        prepare(indexed, offset, "an indexed draw to move");
        offset.vertexOffsets.front() += 16;
        Require(verify(state, indexed, offset, DrawRecordInputs::Differs, "a moved vertex offset").find("vertex binding 0") != std::string::npos, "prepared inputs: the verify did not name the vertex binding");
        PreparedInputs guarded;
        prepare(indexed, guarded, "an indexed draw before a depth guard change");
        ++guarded.depthGuards;
        verify(state, indexed, guarded, DrawRecordInputs::Refused, "an indexed draw after a depth guard change");
        auto quads = state;
        quads.quadList = true;
        refused(quads, automatic, recipe, Refusal::Shape, "a quad list");
        refused(state, indexed, nullptr, Refusal::NoRecipe, "a draw without a published recipe");
        auto foreign = std::make_shared<AgcDriver::DrawRecipe>(*recipe);
        foreign->device = VK_NULL_HANDLE;
        refused(state, indexed, foreign, Refusal::NoRecipe, "a draw with a recipe of another device");
        if (!context.primitiveListRestart) {
            auto restart = state;
            restart.primitiveRestart = true;
            refused(restart, indexed, recipe, Refusal::Shape, "an indexed list with primitive restart compacted on the CPU");
        }
        alignas(16) static std::array<std::uint16_t, 6> loose{0, 1, 2, 2, 1, 3};
        const AgcDriver::Pm4::DrawParameters unimported{reinterpret_cast<std::uintptr_t>(loose.data()), static_cast<std::uint32_t>(loose.size()), 2, 1, 0, true, 0, 0};
        refused(state, unimported, recipe, Refusal::NotInPlace, "an index buffer in memory without a host import");
        const auto kept = vertex.vertexAttributes;
        vertex.vertexAttributes = {InputAttribute(block.At(InputVertexOffset), 0, 0)};
        refused(state, automatic, recipe, Refusal::Shape, "a vertex fetch that reads zeros");
        vertex.vertexAttributes = kept;
        PreparedInputs rechecked;
        prepare(indexed, rechecked, "an indexed draw before another import");
        const auto* before = Graphics::HostImportFor(context, block.At(0), BlockBytes);
        Require(before != nullptr, "prepared inputs: the first block has no import");
        const auto beforeBuffer = before->buffer;
        const auto beforeOffset = Graphics::HostImportOffset(*before, block.At(0));
        GuestBlock other;
        Require(Graphics::HostImportFor(context, other.At(0), BlockBytes) != nullptr, "prepared inputs: the second block was not imported");
        Require(Graphics::HostImportSetGeneration() != rechecked.imports, "prepared inputs: a new import did not move the import set generation");
        const auto* after = Graphics::HostImportFor(context, block.At(0), BlockBytes);
        const bool replaced = after == nullptr || after->buffer != beforeBuffer || Graphics::HostImportOffset(*after, block.At(0)) != beforeOffset;
        if (replaced) std::cout << "the second block's import replaced the first block's, so the committer drops the prepared inputs\n";
        verify(state, indexed, rechecked, replaced ? DrawRecordInputs::ImportChanged : DrawRecordInputs::Checked, "an indexed draw after another block's import", replaced ? DrawRecordInputs::ImportChanged : DrawRecordInputs::Rechecked);
    }
    PreparedInputs moved;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        prepare(indexed, moved, "an indexed draw before its block is registered again");
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block.Bytes(0));
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block.Bytes(0), BlockBytes, true, true, true);
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto* import = Graphics::HostImportFor(context, block.At(0), BlockBytes);
        Require(import != nullptr, "prepared inputs: the block registered again was not imported");
        Require(Graphics::HostImportSetGeneration() != moved.imports, "prepared inputs: registering the block again did not move the import set generation");
        const bool reused = import->buffer == moved.ranges.front().handle && Graphics::HostImportOffset(*import, block.At(InputIndexOffset)) == moved.ranges.front().offset;
        if (reused) std::cout << "the import made after the block was registered again has the old import's buffer handle and offset, so the committer takes the prepared inputs\n";
        verify(state, indexed, moved, reused ? DrawRecordInputs::Checked : DrawRecordInputs::ImportChanged, "an indexed draw after its block was registered again");
        recorder.NotePendingWrite(block.At(InputVertexOffset), 16);
        PreparedInputs behind;
        prepare(indexed, behind, "an indexed draw behind a pending vertex write");
        verify(state, indexed, behind, DrawRecordInputs::Checked, "an indexed draw behind a pending vertex write");
        PreparedInputs stored;
        prepare(indexed, stored, "an indexed draw under a queued store");
        const std::array<std::byte, 4> label{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
        recorder.RecordStore(import->buffer, Graphics::HostImportOffset(*import, block.At(InputIndexOffset)), label, block.At(InputIndexOffset));
        Require(recorder.QueuedStoreOverlaps(block.At(InputIndexOffset), label.size()), "prepared inputs: the label store over the index range was not queued");
        verify(state, indexed, stored, DrawRecordInputs::Refused, "an indexed draw under a queued store");
        recorder.Sync();
    }
    std::cout << "prepared inputs match today's for an indexed and an auto in-place draw, also after another block's import moved the import set; they are refused for a quad list, a missing or foreign recipe, CPU-compacted restart lists, an index buffer without a host import and a zero-reading vertex fetch; on the committer they give way to today's inputs for another state object, a depth guard change since the indices were read, an import replaced by a registry change and a queued store over the index range, and keep the behind-a-write answer of today's check; the verify names a changed max index and vertex offset\n";
    return 0;
}

void InjectedCommit(bool ring) {
    RuntimeSwitches::Assign(RuntimeSwitch::LeanFront, true);
    RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, false);
    const auto states = Alternating(Kind::Constants, 6);
    std::vector<std::vector<std::uint8_t>> today;
    for (const auto& state : states) today.push_back(Render(state, DrawsPerSubmission));
    const auto base = DirectTotals::Now();
    const auto differences = DrawRecords::Differences();
    DrawRecords::CorruptCommits(true);
    RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, true);
    for (std::size_t index = 0; index < states.size(); ++index) Require(Render(states[index], DrawsPerSubmission) == today[index], "direct draws with a corrupted committer answer: submission " + std::to_string(index) + " rendered differently from today's path" + DescribeDirect(DirectTotals::Now(), base));
    RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, false);
    DrawRecords::CorruptCommits(false);
    const auto now = DirectTotals::Now();
    std::cout << "direct draws with a corrupted committer answer:" << DescribeDirect(now, base) << '\n';
    if (!ring || DrawRecords::VerifyEvery() == 0) return;
    Require(DrawRecords::Differences() > differences && now.Since(base, DirectCounter::CommitDemoted) != 0, "direct draws: the committer verify did not catch a corrupted committer answer" + DescribeDirect(now, base));
}

}

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--direct-buffers") return PreparedBuffersMatch();
        if (argc > 1 && std::string_view(argv[1]) == "--direct-inputs") return PreparedInputsMatch();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        const bool injectCommit = argc > 1 && std::string_view(argv[1]) == "--inject-commit";
        using AgcDriver::DriverDetail::DrawFinisher;
        using AgcDriver::DriverDetail::DrawPipeline;
        const bool leanEnvironment = RuntimeSwitches::Set(RuntimeSwitch::LeanFront);
        const bool directEnvironment = RuntimeSwitches::Set(RuntimeSwitch::DirectDraws);
        const bool passEnvironment = RuntimeSwitches::Set(RuntimeSwitch::DirectPassTargets);
        RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, false);
        const bool ring = DrawFinisher::Enabled() && DrawFinisher::Overlap() && DrawPipeline::Depth() != 0;
        Register(PlainVertexHeader, PlainVertex.data(), sizeof(PlainVertex), 2);
        Register(ConstantPixelHeader, ConstantPixel.data(), sizeof(ConstantPixel), 1);
        Register(TableVertexHeader, TableVertex.data(), sizeof(TableVertex), 2);
        Register(ReadPixelHeader, ReadPixel.data(), sizeof(ReadPixel), 1);
        Register(RingPixelHeader, RingPixel.data(), sizeof(RingPixel), 1);
        Register(SamplePixelHeader, SamplePixel.data(), sizeof(SamplePixel), 1);
        Register(DataVertexHeader, DataVertex.data(), sizeof(DataVertex), 2);
        RegisterFetchVertex();
        Blocks blocks;
        Scene = &blocks;
        Prepare();
        if (injectCommit) InjectedCommit(ring);
        else if (directEnvironment) Direct(ring);
        if (!injectCommit && directEnvironment && passEnvironment) PassTargets(ring);
        DrawRecords::Report(true);
        DirectReport::Report(true);
        RuntimeSwitches::Assign(RuntimeSwitch::LeanFront, leanEnvironment);
        RuntimeSwitches::Assign(RuntimeSwitch::DirectDraws, directEnvironment);
        AgcDriverShutdown_nid_postfix();
        std::cout << (injectCommit ? "the committer verify caught a corrupted committer answer and today's path drew" : directEnvironment ? "direct draws render as today's path for moved constants, descriptors and vertex buffers and commit through their records' committer halves" : "direct draws are off") << (ring ? "" : " (no finisher ring: direct draws never ran)") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
