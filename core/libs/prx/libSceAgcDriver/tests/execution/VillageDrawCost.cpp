#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
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
#ifdef __linux__
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t MaxSamples = 4;
constexpr std::uint32_t MaxDraws = 2048;
constexpr std::uint32_t MaxObjectSets = 8;
constexpr std::uint32_t MaxTableEntries = 4096;
constexpr std::uint32_t QuadPositions = 64;
constexpr std::uint32_t QuadPixels = 48;
constexpr std::uint32_t VerticesPerDraw = 6;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t RenderTargetTileMode = 0x1b;
constexpr std::size_t SamplerOffset = 0x200;
constexpr std::size_t FrameConstantsOffset = 0x210;
constexpr std::size_t TableDescriptorOffset = 0x220;
constexpr std::size_t FrameDataOffset = 0x400;
constexpr std::size_t VertexTableOffset = 0x1000;
constexpr std::size_t VertexDataOffset = 0x2000;
constexpr std::size_t VertexDataBytes = 128;
constexpr std::size_t TableOffset = 0x10000;
constexpr std::size_t ObjectOffset = TableOffset + MaxTableEntries * 32u;
constexpr std::size_t ObjectBytes = 256;
constexpr std::size_t DataBytes = (ObjectOffset + MaxObjectSets * MaxDraws * ObjectBytes + 65535) & ~static_cast<std::size_t>(65535);
constexpr std::size_t TextureStride = 4096;
constexpr std::size_t TextureBytes = (MaxTableEntries * TextureStride + 65535) & ~static_cast<std::size_t>(65535);
constexpr std::size_t TargetBytes = 16u << 20u;

std::uint32_t Setting(const char* name, std::uint32_t fallback, std::uint32_t low, std::uint32_t high) {
    const char* text = std::getenv(name);
    const auto parsed = text != nullptr ? std::strtoul(text, nullptr, 10) : static_cast<unsigned long>(fallback);
    return static_cast<std::uint32_t>(std::clamp<unsigned long>(parsed, low, high));
}

std::uint32_t Draws() { return Setting("VILLAGE_DRAWS", 850, 1, MaxDraws); }
std::uint32_t TableEntries() { return Setting("VILLAGE_TABLE", 2000, MaxSamples, MaxTableEntries); }
std::uint32_t FullPercent() { return Setting("VILLAGE_FULL_PERCENT", 10, 0, 100); }
std::uint32_t ObjectSets() { return Setting("VILLAGE_SETS", 8, 1, MaxObjectSets); }
std::uint32_t ProgramRun() { return Setting("VILLAGE_PROGRAM_RUN", 8, 1, MaxDraws); }
std::uint32_t Width() { return Setting("VILLAGE_WIDTH", 1920, 64, 1920); }
std::uint32_t Height() { return Setting("VILLAGE_HEIGHT", 1080, 64, 1080); }
std::uint32_t WritePercent() { return Setting("VILLAGE_WRITE_PERCENT", 0, 0, 100); }

enum class IndexMode { Table, Constant, Immediate };

IndexMode Indexing() {
    static const IndexMode mode = [] {
        const char* text = std::getenv("VILLAGE_INDEX");
        const std::string name = text != nullptr ? text : "table";
        return name == "constant" ? IndexMode::Constant : name == "immediate" ? IndexMode::Immediate : IndexMode::Table;
    }();
    return mode;
}

const char* IndexName() {
    return Indexing() == IndexMode::Constant ? "constant index" : Indexing() == IndexMode::Immediate ? "immediate T#, no table" : "table index through the T# table V# at root+0x220, plus a zero read at run time";
}

std::uint32_t TableIndex(std::uint32_t draw, std::uint32_t slot) {
    return (draw * 131u + slot * 517u + 7u) % TableEntries();
}

std::uint32_t Samples(std::uint32_t draw) {
    return 1u + (draw / ProgramRun()) % MaxSamples;
}

std::uint32_t TexelByte(std::uint32_t entry) {
    return 1u + entry % 8u;
}

bool FullTarget(std::uint32_t draw) {
    return draw < Draws() * FullPercent() / 100u;
}

alignas(256) constexpr std::array<std::uint32_t, 9> VertexCode{
    0xf4080304u, 0xfa000000u, 0xbf8cc07fu, 0xe00c2000u, 0x80030105u, 0xbf8c3f70u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};

constexpr std::size_t PixelWords = 13 + 5 + MaxSamples * 24 + 5 + 8 + 3;

std::array<std::uint32_t, PixelWords> MakePixelCode(std::uint32_t samples, IndexMode mode, const std::array<std::array<std::uint32_t, 8>, MaxSamples>& immediates) {
    std::array<std::uint32_t, PixelWords> code{};
    code.fill(0xbf810000u);
    std::size_t n = 0;
    for (const auto word : {0xf4080200u, 0xfa000200u, 0xf4080300u, 0xfa000210u, 0x7e0002f0u, 0x7e0202f0u, 0x7e100280u, 0x7e120280u, 0x7e140280u, 0x7e160280u, 0xf4280802u, 0xfa000010u, 0xbf8cc07fu}) code[n++] = word;
    if (mode == IndexMode::Table) {
        for (const auto word : {0xf4080a00u, 0xfa000220u, 0xe0300004u, 0x80030d00u, 0xbf8c0070u}) code[n++] = word;
    }
    for (std::uint32_t slot = 0; slot < samples; ++slot) {
        if (mode == IndexMode::Immediate) {
            for (std::uint32_t word = 0; word < 8; ++word) {
                code[n++] = 0xbe9003ffu + (word << 16u);
                code[n++] = immediates[slot][word];
            }
        } else if (mode == IndexMode::Constant) {
            for (const auto word : {0xbf800000u, 0xf40c0401u, 0xfa000000u | (TableIndex(0, slot) * 32u)}) code[n++] = word;
        } else {
            for (const auto word : {0x4a181a20u + slot, 0x7e48050cu, 0x8f248524u, 0xf42c0414u, 0x48000000u}) code[n++] = word;
        }
        for (const auto word : {0xbf8cc07fu, 0xf09c0f08u, 0x00440400u, 0xbf8c3f70u, 0x06101104u, 0x06121305u, 0x06141506u, 0x06161707u}) code[n++] = word;
    }
    for (const auto word : {0xf4200606u, 0xfa000000u, 0xf4200642u, 0xfa000000u, 0xbf8cc07fu}) code[n++] = word;
    for (const auto word : {0x10101018u, 0x10121218u, 0x10141418u, 0x10161618u, 0x10101019u, 0x10121219u, 0x10141419u, 0x10161619u}) code[n++] = word;
    for (const auto word : {0xf800180fu, 0x0b0a0908u, 0xbf810000u}) code[n++] = word;
    return code;
}

struct alignas(256) PixelProgram {
    std::array<std::uint32_t, PixelWords> code{};
};

std::array<PixelProgram, MaxSamples>& PixelPrograms() {
    static std::array<PixelProgram, MaxSamples> programs{};
    return programs;
}

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) std::array<Header, MaxSamples> PixelHeaders{};

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

class GuestBlock {
public:
    explicit GuestBlock(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
        constexpr std::uintptr_t alignment = 65536;
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
        Require(block != nullptr, "village draw cost: cannot allocate a guest block");
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }
    std::size_t Size() const { return bytes; }

private:
    std::size_t bytes;
    std::uint8_t* block = nullptr;
};

struct Layout {
    const GuestBlock& data;
    const GuestBlock& textures;
    const GuestBlock& target;
};

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::uint32_t stride, std::uint32_t records, std::uint32_t word3) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, word3};
}

std::array<float, 4> QuadBounds(std::uint32_t position) {
    if (position == QuadPositions) return {-1.0f, -1.0f, 1.0f, 1.0f};
    const auto width = static_cast<float>(Width());
    const auto height = static_cast<float>(Height());
    const auto x = 16.0f + static_cast<float>(position % 8u) * (width - 64.0f) / 8.0f;
    const auto y = 16.0f + static_cast<float>(position / 8u) * (height - 64.0f) / 8.0f;
    const auto size = static_cast<float>(QuadPixels);
    return {x / width * 2.0f - 1.0f, y / height * 2.0f - 1.0f, (x + size) / width * 2.0f - 1.0f, (y + size) / height * 2.0f - 1.0f};
}

std::array<std::uint32_t, 8> TextureDescriptor(const GuestBlock& textures, std::uint32_t entry) {
    const auto address = textures.At(entry * TextureStride);
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 7u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
}

void BuildPixelPrograms(const GuestBlock& textures) {
    std::array<std::array<std::uint32_t, 8>, MaxSamples> immediates{};
    for (std::uint32_t slot = 0; slot < MaxSamples; ++slot) immediates[slot] = TextureDescriptor(textures, TableIndex(0, slot));
    for (std::uint32_t samples = 1; samples <= MaxSamples; ++samples) PixelPrograms()[samples - 1].code = MakePixelCode(samples, Indexing(), immediates);
}

void Prepare(const Layout& layout) {
    const auto& block = layout.data;
    for (std::uint32_t entry = 0; entry < MaxTableEntries; ++entry) {
        std::memset(layout.textures.Bytes(entry * TextureStride), static_cast<int>(TexelByte(entry)), TextureStride);
        const auto words = TextureDescriptor(layout.textures, entry);
        std::memcpy(block.Bytes(TableOffset + entry * 32u), words.data(), sizeof(words));
    }
    const std::array<std::uint32_t, 4> sampler{ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, 0u, 0u};
    std::memcpy(block.Bytes(SamplerOffset), sampler.data(), sizeof(sampler));
    const auto frame = BufferDescriptor(block.At(FrameDataOffset), 0, 256, 0x31016facu);
    std::memcpy(block.Bytes(FrameConstantsOffset), frame.data(), sizeof(frame));
    const auto table = BufferDescriptor(block.At(TableOffset), 0, TableEntries() * 32u, 0x31016facu);
    std::memcpy(block.Bytes(TableDescriptorOffset), table.data(), sizeof(table));
    const float one = 1.0f;
    std::memcpy(block.Bytes(FrameDataOffset), &one, sizeof(one));
    for (std::uint32_t position = 0; position <= QuadPositions; ++position) {
        const auto [x0, y0, x1, y1] = QuadBounds(position);
        const std::array<float, VerticesPerDraw * 4> vertices{x0, y0, 0.5f, 1.0f, x1, y0, 0.5f, 1.0f, x0, y1, 0.5f, 1.0f, x1, y0, 0.5f, 1.0f, x1, y1, 0.5f, 1.0f, x0, y1, 0.5f, 1.0f};
        std::memcpy(block.Bytes(VertexDataOffset + position * VertexDataBytes), vertices.data(), sizeof(vertices));
        const auto descriptor = BufferDescriptor(block.At(VertexDataOffset + position * VertexDataBytes), 16, VerticesPerDraw, 0x0104dfacu);
        std::memcpy(block.Bytes(VertexTableOffset + position * 16u), descriptor.data(), sizeof(descriptor));
    }
    for (std::uint32_t set = 0; set < MaxObjectSets; ++set) {
        for (std::uint32_t draw = 0; draw < MaxDraws; ++draw) {
            auto* object = block.Bytes(ObjectOffset + (static_cast<std::size_t>(set) * MaxDraws + draw) * ObjectBytes);
            std::memcpy(object, &one, sizeof(one));
            for (std::uint32_t slot = 0; slot < MaxSamples; ++slot) {
                const auto index = TableIndex(draw, slot);
                std::memcpy(object + 16u + slot * 4u, &index, sizeof(index));
            }
        }
    }
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::uint32_t Position(std::uint32_t draw) {
    return FullTarget(draw) ? QuadPositions : draw % QuadPositions;
}

std::vector<std::uint32_t> Commands(const Layout& layout, const std::vector<std::uint32_t>& draws, std::uint32_t set) {
    const auto& block = layout.data;
    const auto color = layout.target.At(0);
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto root = block.At(0);
    const auto table = block.At(TableOffset);
    const auto width = Width();
    const auto height = Height();
    const auto halfWidth = static_cast<float>(width) / 2.0f;
    const auto halfHeight = static_cast<float>(height) / 2.0f;
    const auto scissor = (height << 16u) | width;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((width - 1u) << 14u) | (height - 1u)},
        {0x3b8, 0x9000000u | (RenderTargetTileMode << 14u)}, {0x1e0, 0},
        {0xc, 0}, {0xd, scissor},
        {0x81, 0x80000000}, {0x82, scissor},
        {0x90, 0x80000000}, {0x91, scissor},
        {0x94, 0x80000000}, {0x95, scissor},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(halfWidth)}, {0x110, std::bit_cast<std::uint32_t>(halfWidth)},
        {0x111, std::bit_cast<std::uint32_t>(-halfHeight)}, {0x112, std::bit_cast<std::uint32_t>(halfHeight)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u), static_cast<std::uint32_t>(vertex >> 40u)});
    Append(commands, 0x76, {0x8b, 2u << 1u});
    Append(commands, 0x76, {0x00b, 8u << 1u});
    Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u), static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u)});
    std::uint32_t boundSamples = 0;
    std::uint32_t boundPosition = ~0u;
    for (const auto draw : draws) {
        const auto samples = Samples(draw);
        if (samples != boundSamples) {
            const auto pixel = reinterpret_cast<std::uintptr_t>(PixelPrograms()[samples - 1].code.data());
            Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u), static_cast<std::uint32_t>(pixel >> 40u)});
            boundSamples = samples;
        }
        const auto position = Position(draw);
        if (position != boundPosition) {
            const auto vertices = block.At(VertexTableOffset + position * 16u);
            Append(commands, 0x76, {0x08c, static_cast<std::uint32_t>(vertices), static_cast<std::uint32_t>(vertices >> 32u)});
            boundPosition = position;
        }
        const auto object = BufferDescriptor(block.At(ObjectOffset + (static_cast<std::size_t>(set) * MaxDraws + draw) * ObjectBytes), 0, ObjectBytes, 0x31016facu);
        Append(commands, 0x76, {0x010, object[0], object[1], object[2], object[3]});
        Append(commands, 0x2d, {VerticesPerDraw, 2});
    }
    return commands;
}

std::vector<std::uint32_t> Frame() {
    std::vector<std::uint32_t> draws(Draws());
    for (std::uint32_t draw = 0; draw < draws.size(); ++draw) draws[draw] = draw;
    return draws;
}

double Submit(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    const auto start = std::chrono::steady_clock::now();
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "village draw cost: the frame submission failed");
    AgcDriverWaitIdle_nid_postfix();
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
}

unsigned BrightestByte(const GuestBlock& target) {
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(target.At(0)), target.Size());
    unsigned brightest = 0;
    for (std::size_t i = 0; i < target.Size(); ++i) brightest = std::max(brightest, static_cast<unsigned>(target.Bytes(0)[i]));
    return brightest;
}

void Check(const Layout& layout) {
    const auto full = std::max(1u, Draws() * FullPercent() / 100u);
    for (std::uint32_t samples = 1; samples <= MaxSamples; ++samples) {
        std::uint32_t chosen = ~0u;
        for (std::uint32_t draw = 0; draw < full && chosen == ~0u; ++draw) {
            if (Samples(draw) == samples) chosen = draw;
        }
        if (chosen == ~0u) continue;
        unsigned expected = 0;
        for (std::uint32_t slot = 0; slot < samples; ++slot) expected += TexelByte(Indexing() != IndexMode::Table ? TableIndex(0, slot) : TableIndex(chosen, slot));
        std::memset(layout.target.Bytes(0), 0, layout.target.Size());
        auto commands = Commands(layout, {chosen}, 0);
        Submit(commands);
        const auto byte = BrightestByte(layout.target);
        Require(byte == expected, "village draw cost: a full-target draw sampling " + std::to_string(samples) + " table textures wrote " + std::to_string(byte) + ", expected " + std::to_string(expected));
    }
}

#ifdef __linux__
std::vector<std::pair<pid_t, std::string>> DriverThreads() {
    std::vector<std::pair<pid_t, std::string>> threads;
    if (DIR* tasks = opendir("/proc/self/task")) {
        while (const dirent* entry = readdir(tasks)) {
            if (entry->d_name[0] == '.') continue;
            std::ifstream comm(std::string("/proc/self/task/") + entry->d_name + "/comm");
            std::string name;
            std::getline(comm, name);
            if (name.rfind("qworker 0x0", 0) == 0 || name.rfind("draw-commit", 0) == 0 || name.rfind("llvmpipe-", 0) == 0) threads.emplace_back(static_cast<pid_t>(std::stol(entry->d_name)), name + "/" + entry->d_name);
        }
        closedir(tasks);
    }
    std::sort(threads.begin(), threads.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    return threads;
}

std::uint64_t ThreadCpuNs(pid_t tid) {
    const auto clock = static_cast<clockid_t>((~static_cast<unsigned>(tid) << 3u) | 6u);
    timespec time{};
    if (clock_gettime(clock, &time) != 0) return 0;
    return static_cast<std::uint64_t>(time.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(time.tv_nsec);
}

std::string ResidentMemory() {
    std::ifstream status("/proc/self/status");
    std::string line;
    std::string memory;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0 || line.rfind("VmHWM:", 0) == 0) memory += " " + line.substr(0, 5) + " " + std::to_string(std::stoull(line.substr(6)) >> 10u) + " MiB";
    }
    return memory;
}
#endif

void WriteTextures(const Layout& layout, std::uint32_t frame) {
    const auto entries = TableEntries();
    const auto count = WritePercent() == 0 ? 0u : std::max(1u, entries * WritePercent() / 100u);
    for (std::uint32_t k = 0; k < count; ++k) {
        const auto entry = static_cast<std::uint32_t>((static_cast<std::uint64_t>(frame) * count + k) * 37u % entries);
        std::memset(layout.textures.Bytes(entry * TextureStride), static_cast<int>(TexelByte(entry)), 4);
    }
}

double Median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

void Cost(const Layout& layout, std::uint32_t frames) {
    const auto draws = Draws();
    std::printf("village shape: %u draws per frame on a %ux%u target, %u full-target, the rest %ux%u quads over %u positions; programs sample 1-%u textures in runs of %u draws; a %u-entry T# table, indices from the object V# in s[4:7] (%s); %u object V# sets, one per frame; %u%% of the table's textures rewritten with their own bytes before each frame\n", draws, Width(), Height(), draws * FullPercent() / 100u, QuadPixels, QuadPixels, QuadPositions, MaxSamples, ProgramRun(), TableEntries(), IndexName(), ObjectSets(), WritePercent());
    std::vector<std::vector<std::uint32_t>> commandSets;
    for (std::uint32_t set = 0; set < ObjectSets(); ++set) commandSets.push_back(Commands(layout, Frame(), set));
    std::vector<double> warmMs;
    for (auto& commands : commandSets) warmMs.push_back(Submit(commands) / 1e3);
    std::vector<double> perDraw;
#ifdef __linux__
    const auto threads = DriverThreads();
    const auto rasterizer = [](const std::string& name) { return name.rfind("llvmpipe-", 0) == 0; };
    std::vector<std::vector<double>> cpu(threads.size());
    std::vector<double> totalCpu;
    std::vector<double> rasterCpu;
#endif
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
#ifdef __linux__
        std::vector<std::uint64_t> cpuBefore;
        for (const auto& thread : threads) cpuBefore.push_back(ThreadCpuNs(thread.first));
#endif
        WriteTextures(layout, frame);
        perDraw.push_back(Submit(commandSets[frame % commandSets.size()]) / draws);
#ifdef __linux__
        double sum = 0.0;
        double raster = 0.0;
        for (std::size_t t = 0; t < threads.size(); ++t) {
            const auto value = static_cast<double>(ThreadCpuNs(threads[t].first) - cpuBefore[t]) / 1e3 / draws;
            cpu[t].push_back(value);
            (rasterizer(threads[t].second) ? raster : sum) += value;
        }
        totalCpu.push_back(sum);
        rasterCpu.push_back(raster);
#endif
    }
    std::string frameList;
    char text[128];
    for (const auto ms : warmMs) {
        std::snprintf(text, sizeof(text), " %.0f", ms);
        frameList += text;
    }
    frameList += " | measured ms:";
    for (const auto us : perDraw) {
        std::snprintf(text, sizeof(text), " %.0f", us * draws / 1e3);
        frameList += text;
    }
    std::printf("village frames: warm-up ms:%s\n", frameList.c_str());
    std::printf("village cost: %u frames of %u draws: wall median %.2f us per draw (min %.2f, max %.2f), %.2f ms per frame\n", frames, draws, Median(perDraw), *std::min_element(perDraw.begin(), perDraw.end()), *std::max_element(perDraw.begin(), perDraw.end()), Median(perDraw) * draws / 1e3);
#ifdef __linux__
    std::string split;
    std::size_t rasterThreads = 0;
    for (std::size_t t = 0; t < threads.size(); ++t) {
        if (rasterizer(threads[t].second)) {
            ++rasterThreads;
            continue;
        }
        std::snprintf(text, sizeof(text), " %s cpu %.2f", threads[t].second.c_str(), Median(cpu[t]));
        split += text;
    }
    std::snprintf(text, sizeof(text), " driver cpu %.2f llvmpipe cpu %.2f (%zu threads)", Median(totalCpu), Median(rasterCpu), rasterThreads);
    split += text;
    std::printf("village split: us per draw, median of %u frames:%s\n", frames, split.c_str());
    std::printf("village memory:%s\n", ResidentMemory().c_str());
#endif
}

}

int main(int argc, char** argv) {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GuestBlock data(DataBytes);
        GuestBlock textures(TextureBytes);
        GuestBlock target(TargetBytes);
        const Layout layout{data, textures, target};
        Prepare(layout);
        BuildPixelPrograms(textures);
        Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        for (std::uint32_t samples = 1; samples <= MaxSamples; ++samples) Register(PixelHeaders[samples - 1], PixelPrograms()[samples - 1].code.data(), sizeof(PixelProgram::code), 1);
        Check(layout);
        if (argc > 2 && std::string(argv[1]) == "--cost") Cost(layout, static_cast<std::uint32_t>(std::stoul(argv[2])));
        AgcDriverShutdown_nid_postfix();
        std::puts("village draw cost: full-target draws sampled 1 to 4 textures through the T# table");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
