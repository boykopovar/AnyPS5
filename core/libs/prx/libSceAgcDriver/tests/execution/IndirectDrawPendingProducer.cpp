#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
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
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using Path = AgcDriver::Graphics::IndirectDrawPath;

constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t DistanceVector = 0x00400000u;
constexpr std::uint32_t SetContextReg = 0x69u;
constexpr std::uint32_t SetShReg = 0x76u;
constexpr std::uint32_t SetUconfigReg = 0x79u;

constexpr std::size_t Block = 65536;
constexpr std::size_t PoolBytes = Block;
constexpr std::size_t ColorBytes = Width * Height * 4;
constexpr std::size_t VertexOffset = 0x2000;
constexpr std::size_t RecordOffset = 0x3000;
constexpr std::size_t CountOffset = 0x3100;
constexpr std::size_t SinkOffset = 0x3800;
constexpr std::array<std::uint32_t, 3> Indices{0, 1, 2};

std::byte* ColorMemory = nullptr;
std::byte* Pool = nullptr;

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80020005, 0xe0382010, 0x80020405, 0xbf8c3f70,
    0xf80000cf, 0x03020100, 0xf80008df, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0x7e0002f2, 0x7e0202f2, 0x7e0402f2, 0x7e0602f2, 0xf800080f, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> AddressWriterCode{0x7e000202u, 0x7e020203u, 0x7e040204u, 0x7e060280u, 0xdc7c8000u, 0x00000003u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 7> AddressReaderCode{0x7e060280u, 0xdc308000u, 0x00000003u, 0xbf8c3f70u, 0xe0700000u, 0x80010000u, 0xbf810000u};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 4> distances;
};

std::array<Vertex, 3> Vertices{};

constexpr std::array<float, 3> PositionX{-3.0f, 1.0f, 1.0f};
constexpr std::array<float, 3> PositionY{-1.0f, -1.0f, 3.0f};

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

enum class Shape { Direct, Indexed, IndexedCount };

enum class Producer { Writer, Reader };

struct Case {
    const char* name;
    Shape shape;
    Path path;
    Producer producer;
};

constexpr Case GpuPath{"a draw whose records the GPU reads", Shape::Direct, Path::Gpu, Producer::Writer};
constexpr Case CpuRecords{"an indexed draw with no INDEX_BUFFER_SIZE, its records read by the driver", Shape::Indexed, Path::IndexLimitUnknown, Producer::Writer};
constexpr Case CpuCount{"an indexed multi-draw with no INDEX_BUFFER_SIZE, its count read by the driver before it decides the draw is not empty", Shape::IndexedCount, Path::IndexLimitUnknown, Producer::Writer};
constexpr Case CpuRecordsReadOnly{"an indexed draw with no INDEX_BUFFER_SIZE behind a dispatch that only reads through addresses", Shape::Indexed, Path::IndexLimitUnknown, Producer::Reader};
constexpr Case DisabledRecords{"a draw whose records are read on the CPU with APS5_NO_GPU_INDIRECT_DRAW", Shape::Direct, Path::Disabled, Producer::Writer};

std::byte* AllocateWatched(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) throw std::runtime_error("indirect draw pending producer: cannot map the watched block");
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

std::uint32_t Low(std::uintptr_t address) {
    return static_cast<std::uint32_t>(address);
}

std::uint32_t High(std::uintptr_t address) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(address) >> 32u);
}

std::uintptr_t PoolAt(std::size_t offset) {
    return reinterpret_cast<std::uintptr_t>(Pool + offset);
}

void SetRegister(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    words.push_back(0xc0010000u | (opcode << 8u));
    words.push_back(offset);
    words.push_back(value);
}

void Dispatch(std::vector<std::uint32_t>& words, const void* program, std::span<const std::uint32_t> userData) {
    const auto code = reinterpret_cast<std::uintptr_t>(program);
    SetRegister(words, SetShReg, 0x20c, static_cast<std::uint32_t>(code >> 8u));
    SetRegister(words, SetShReg, 0x20d, static_cast<std::uint32_t>(static_cast<std::uint64_t>(code) >> 40u) & 0xffu);
    for (const std::uint32_t threads : {0x207u, 0x208u, 0x209u}) SetRegister(words, SetShReg, threads, 1u);
    SetRegister(words, SetShReg, 0x212, 0u);
    SetRegister(words, SetShReg, 0x213, static_cast<std::uint32_t>(userData.size()) << 1u);
    for (std::uint32_t i = 0; i < userData.size(); ++i) SetRegister(words, SetShReg, 0x240 + i, userData[i]);
    words.insert(words.end(), {0xc0031500u, 1u, 1u, 1u, 0x41u});
}

void Produce(std::vector<std::uint32_t>& words, std::size_t offset, std::array<std::uint32_t, 3> values) {
    const auto target = PoolAt(offset);
    const std::array<std::uint32_t, 5> userData{Low(target), High(target), values[0], values[1], values[2]};
    Dispatch(words, AddressWriterCode.data(), userData);
}

void ReadRecord(std::vector<std::uint32_t>& words) {
    const auto source = PoolAt(RecordOffset);
    const auto sink = PoolAt(SinkOffset);
    const std::array<std::uint32_t, 8> userData{Low(source), High(source), 0u, 0u, Low(sink), High(sink) & 0xffffu, 16u, 0x31016facu};
    Dispatch(words, AddressReaderCode.data(), userData);
}

std::vector<std::uint32_t> Commands(Shape shape, bool produce) {
    std::vector<std::uint32_t> words;
    if (produce && shape == Shape::IndexedCount) Produce(words, CountOffset, {1u, 0u, 0u});
    else if (produce) Produce(words, RecordOffset, {static_cast<std::uint32_t>(Indices.size()), 1u, 0u});
    const auto colorAddress = reinterpret_cast<std::uintptr_t>(ColorMemory);
    const auto vertexAddress = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixelAddress = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    for (const auto& [offset, value] : ContextBaseline) SetRegister(words, SetContextReg, offset, value);
    SetRegister(words, SetContextReg, 0x207, DistanceVector);
    SetRegister(words, SetContextReg, 0x318, static_cast<std::uint32_t>(colorAddress >> 8u));
    SetRegister(words, SetContextReg, 0x390, static_cast<std::uint32_t>(static_cast<std::uint64_t>(colorAddress) >> 40u));
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
    SetRegister(words, SetShReg, 0x009, static_cast<std::uint32_t>(static_cast<std::uint64_t>(pixelAddress) >> 40u));
    SetRegister(words, SetShReg, 0x00b, 0u);
    SetRegister(words, SetShReg, 0x0c8, static_cast<std::uint32_t>(vertexAddress >> 8u));
    SetRegister(words, SetShReg, 0x0c9, static_cast<std::uint32_t>(static_cast<std::uint64_t>(vertexAddress) >> 40u));
    SetRegister(words, SetShReg, 0x08b, 4u << 1u);
    const auto vertices = PoolAt(VertexOffset);
    const std::array<std::uint32_t, 4> descriptor{Low(vertices), (High(vertices) & 0xffffu) | (static_cast<std::uint32_t>(sizeof(Vertex)) << 16u), static_cast<std::uint32_t>(Vertices.size()), 0x01016facu};
    for (std::uint32_t i = 0; i < descriptor.size(); ++i) SetRegister(words, SetShReg, 0x08c + i, descriptor[i]);
    words.insert(words.end(), {0xc0021100u, 1u, Low(PoolAt(0)), High(PoolAt(0))});
    SetRegister(words, SetUconfigReg, 0x24a, 0u);
    if (shape == Shape::Direct) {
        words.insert(words.end(), {0xc0032400u, static_cast<std::uint32_t>(RecordOffset), 0u, 0u, 2u});
        return words;
    }
    words.insert(words.end(), {0xc0012600u, Low(PoolAt(0)), High(PoolAt(0))});
    SetRegister(words, SetUconfigReg, 0x243, 0x401u);
    if (shape == Shape::Indexed) {
        words.insert(words.end(), {0xc0032500u, static_cast<std::uint32_t>(RecordOffset), 0u, 0u, 0u});
    } else {
        const auto count = PoolAt(CountOffset);
        words.insert(words.end(), {0xc0083800u, static_cast<std::uint32_t>(RecordOffset), 0u, 0u, 1u << 30u, 1u, Low(count), High(count), 20u, 0u});
    }
    return words;
}

std::uint32_t PoolWord(std::size_t offset) {
    std::uint32_t value = 0;
    AgcDriver::GuestMemory::Read(static_cast<std::uint64_t>(PoolAt(offset)), std::as_writable_bytes(std::span(&value, 1)), 4);
    return value;
}

void ResetPool(bool record) {
    std::memset(Pool, 0, PoolBytes);
    std::memcpy(Pool, Indices.data(), Indices.size() * sizeof(std::uint32_t));
    std::memcpy(Pool + VertexOffset, Vertices.data(), sizeof(Vertex) * Vertices.size());
    if (record) {
        const std::array<std::uint32_t, 5> words{static_cast<std::uint32_t>(Indices.size()), 1u, 0u, 0u, 0u};
        std::memcpy(Pool + RecordOffset, words.data(), sizeof(words));
    }
}

std::size_t WrittenPixels() {
    std::vector<std::byte> colors(ColorBytes);
    AgcDriver::GuestMemory::Read(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ColorMemory)), colors);
    std::size_t written = 0;
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        const auto value = std::to_integer<std::uint8_t>(colors[pixel * 4u]);
        Require(value == 0u || value == 255u, "indirect draw pending producer: pixel " + std::to_string(pixel) + " stored " + std::to_string(value));
        written += value == 255u ? 1u : 0u;
    }
    return written;
}

void Submit(const Case& test, Shape shape, bool produce) {
    std::vector<std::uint32_t> words;
    if (produce && test.producer == Producer::Reader) ReadRecord(words);
    const auto commands = Commands(shape, produce && test.producer == Producer::Writer);
    words.insert(words.end(), commands.begin(), commands.end());
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, std::string(test.name) + ": the submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void PrimeCpuPath(const Case& test) {
    ResetPool(true);
    std::memset(ColorMemory, 0, ColorBytes);
    const auto paths = AgcDriver::Graphics::IndirectDrawCounts();
    Submit(test, Shape::Indexed, false);
    const auto taken = AgcDriver::Graphics::IndirectDrawCounts()[static_cast<std::size_t>(test.path)] - paths[static_cast<std::size_t>(test.path)];
    Require(taken == 1 && WrittenPixels() == Width * Height, std::string(test.name) + ": the indexed draw that marks its vertex program as read by the driver did not draw through the " + AgcDriver::Graphics::IndirectDrawPathName(test.path) + " path");
}

std::string Run(const Case& test) {
    if (test.shape == Shape::IndexedCount) PrimeCpuPath(test);
    const bool reader = test.producer == Producer::Reader;
    ResetPool(test.shape == Shape::IndexedCount || reader);
    std::memset(ColorMemory, 0, ColorBytes);
    const auto leases = AgcDriver::Graphics::LeaseCounters();
    const auto paths = AgcDriver::Graphics::IndirectDrawCounts();
    const auto empty = AgcDriver::Graphics::EmptyIndirectDrawCount();
    Submit(test, test.shape, true);
    const auto pathsAfter = AgcDriver::Graphics::IndirectDrawCounts();
    auto taken = pathsAfter[static_cast<std::size_t>(test.path)] - paths[static_cast<std::size_t>(test.path)];
    const auto notImported = pathsAfter[static_cast<std::size_t>(Path::NotImported)] - paths[static_cast<std::size_t>(Path::NotImported)];
    if (test.path == Path::Gpu && taken == 0 && notImported == 1) {
        std::cout << test.name << ": the device has no host import of the records, so it read them on the CPU\n";
        taken = 1;
    }
    const auto skipped = AgcDriver::Graphics::EmptyIndirectDrawCount() - empty;
    const auto written = WrittenPixels();
    const auto leasesAfter = AgcDriver::Graphics::LeaseCounters();
    if (leasesAfter.deferred == leases.deferred) return "the address-based dispatch did not defer its lease, so the draw had no pending producer to wait for";
    if (reader && leasesAfter.writing != leases.writing) return "a dispatch that only reads through addresses was counted as a lease that can write";
    if (!reader && leasesAfter.writing == leases.writing) return "the address-based store was not counted as a lease that can write, so the draw would not wait for it";
    if (reader && PoolWord(SinkOffset) != Indices.size()) return "the address-based read stored " + std::to_string(PoolWord(SinkOffset)) + " in its buffer, expected the record's " + std::to_string(Indices.size()) + " vertices";
    const bool count = test.shape == Shape::IndexedCount;
    const auto produced = count ? PoolWord(CountOffset) : PoolWord(RecordOffset);
    const std::uint32_t expected = count ? 1u : static_cast<std::uint32_t>(Indices.size());
    if (produced != expected) return "the producer's address-based store left " + std::to_string(produced) + " in the " + (count ? "count" : "record") + ", expected " + std::to_string(expected);
    if (written != Width * Height) return "the draw wrote " + std::to_string(written) + " of " + std::to_string(Width * Height) + " pixels (" + std::to_string(skipped) + " skipped as empty, " + std::to_string(taken) + " through the " + AgcDriver::Graphics::IndirectDrawPathName(test.path) + " path), expected the triangle its producer's " + (count ? "count" : "record") + " names";
    if (taken != 1 || skipped != 0) return "the draw went through the " + std::string(AgcDriver::Graphics::IndirectDrawPathName(test.path)) + " path " + std::to_string(taken) + " times and was skipped as empty " + std::to_string(skipped) + " times, expected once and never";
    return {};
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    _putenv_s("APS5_PROFILE_DRAW", "1");
#else
    setenv("APS5_PROFILE_DRAW", "1", 1);
#endif
    const bool disabled = argc > 1 && std::string(argv[1]) == "--gpu-indirect-disabled";
    try {
        if (disabled) Require(std::getenv("APS5_NO_GPU_INDIRECT_DRAW") != nullptr, "--gpu-indirect-disabled needs APS5_NO_GPU_INDIRECT_DRAW=1");
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        ColorMemory = AllocateWatched(2 * Block);
        Pool = AllocateWatched(PoolBytes);
        GuestAllocations::Mutation().Add(Pool, PoolBytes, true, true, true);
        Register(VertexShader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelShader, PixelCode.data(), sizeof(PixelCode), 1);
        for (std::size_t i = 0; i < Vertices.size(); ++i) Vertices[i] = {{PositionX[i], PositionY[i], 0.5f, 1.0f}, {-1.0f, -1.0f, -1.0f, 0.0f}};
        const std::vector<Case> cases = disabled ? std::vector<Case>{DisabledRecords} : std::vector<Case>{GpuPath, CpuRecords, CpuCount, CpuRecordsReadOnly};
        std::size_t failed = 0;
        for (const auto& test : cases) {
            const auto failure = Run(test);
            if (failure.empty()) {
                std::cout << test.name << ": passed\n";
            } else {
                std::cerr << test.name << ": " << failure << '\n';
                ++failed;
            }
        }
        AgcDriverShutdown_nid_postfix();
        if (failed != 0) return 1;
        std::cout << "indirect draw pending producer tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
