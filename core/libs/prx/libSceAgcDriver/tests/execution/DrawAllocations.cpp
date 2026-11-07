#ifdef _WIN32
#include <cstdio>

int main() {
    std::puts("draw allocations: skipped, the driver allocates through its own operator new in a DLL that this census cannot interpose");
    return 77;
}
#else
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "VulkanTestDevice.hpp"
#include <dlfcn.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Probe = void (*)(std::size_t bytes, const void* caller) noexcept;

std::atomic<Probe> hostProbe{nullptr};

void* Counted(std::size_t bytes, const void* caller) noexcept {
    if (const auto observe = hostProbe.load(std::memory_order_relaxed)) observe(bytes, caller);
    return std::malloc(bytes != 0 ? bytes : 1);
}

}

void* operator new(std::size_t bytes) {
    if (void* block = Counted(bytes, __builtin_return_address(0))) return block;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) {
    if (void* block = Counted(bytes, __builtin_return_address(0))) return block;
    throw std::bad_alloc();
}
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept { return Counted(bytes, __builtin_return_address(0)); }
void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept { return Counted(bytes, __builtin_return_address(0)); }
void operator delete(void* block) noexcept { std::free(block); }
void operator delete[](void* block) noexcept { std::free(block); }
void operator delete(void* block, std::size_t) noexcept { std::free(block); }
void operator delete[](void* block, std::size_t) noexcept { std::free(block); }
void operator delete(void* block, const std::nothrow_t&) noexcept { std::free(block); }
void operator delete[](void* block, const std::nothrow_t&) noexcept { std::free(block); }

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t ReadWriteProtection = 0x04;

std::atomic<std::uint32_t> nextThread{0};

std::uint32_t CurrentThread() {
    thread_local const std::uint32_t id = nextThread.fetch_add(1, std::memory_order_relaxed) + 1;
    return id;
}

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TableOffset = 0x300;
constexpr std::size_t VertexOffset = 0x800;
constexpr std::uint32_t RenderTargetTileMode = 0x1b;

alignas(256) constexpr std::array<std::uint32_t, 40> VertexCode{
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

alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

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
    GuestBlock() {
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, ReadWriteProtection, BlockBytes);
        Require(block != nullptr, "draw allocations: cannot allocate a guest block");
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

void WriteVertices(const GuestBlock& block) {
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
        const std::array<float, 4> value{0.0f, 0.0f, 0.0f, 0.0f};
        std::memcpy(block.Bytes(VertexOffset + vertex * 16u), value.data(), sizeof(value));
    }
    const auto address = block.At(VertexOffset);
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), 3u, 0x0104dfacu};
    for (std::size_t slot = 0; slot < 3; ++slot) std::memcpy(block.Bytes(TableOffset + slot * 16u), descriptor.data(), sizeof(descriptor));
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

enum class DrawKind { Auto, Indexed };

std::vector<std::uint32_t> Draws(const GuestBlock& vertices, const GuestBlock& target, std::size_t draws, DrawKind kind = DrawKind::Auto, std::uint64_t indices = 0) {
    const auto color = target.At(0);
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto table = vertices.At(TableOffset);
    const float scale = 32.0f;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000u | (RenderTargetTileMode << 14u)}, {0x1e0, 0},
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
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : std::initializer_list<std::pair<std::uint32_t, std::uint32_t>>{{0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 2u << 1u}, {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)}}) Append(commands, 0x76, {offset, value});
    Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u), static_cast<std::uint32_t>(pixel >> 40u)});
    if (kind == DrawKind::Indexed) Append(commands, 0x2a, {0});
    for (std::size_t draw = 0; draw < draws; ++draw) {
        if (kind == DrawKind::Indexed) Append(commands, 0x27, {3, static_cast<std::uint32_t>(indices), static_cast<std::uint32_t>(indices >> 32u), 3, 0});
        else Append(commands, 0x2d, {3, 2});
    }
    return commands;
}

alignas(256) constexpr std::array<std::uint32_t, 11> DispatchCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x34020082u, 0xe0301000u, 0x80020201u, 0xbf8c3f70u, 0x4a060481u, 0xe0701000u, 0x80010301u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint16_t, 4> ImageIndices{0, 1, 2, 0};

std::array<std::uint32_t, 4> StorageDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), 128u, 0x31016facu};
}

std::vector<std::uint32_t> Dispatches(const GuestBlock& table, const GuestBlock& output, std::size_t dispatches) {
    const auto code = reinterpret_cast<std::uintptr_t>(DispatchCode.data());
    const auto tableAddress = table.At(0);
    const auto stored = StorageDescriptor(output.At(0));
    std::vector<std::uint32_t> commands{
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u),
        0xc0037600u, 0x207u, 32u, 1u, 1u,
        0xc0017600u, 0x213u, 8u << 1u,
        0xc0087600u, 0x240u, static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), 0u, 0u, stored[0], stored[1], stored[2], stored[3],
    };
    for (std::size_t dispatch = 0; dispatch < dispatches; ++dispatch) commands.insert(commands.end(), {0xc0031500u, 1u, 1u, 1u, 0x8041u});
    return commands;
}

void Submit(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "draw allocations: the submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

constexpr std::size_t SiteSlots = 4096;
constexpr std::size_t ThreadSlots = 128;

struct Site {
    std::atomic<std::uintptr_t> caller{0};
    std::atomic<std::uint64_t> count{0};
    std::atomic<std::uint64_t> bytes{0};
};

struct ThreadCount {
    std::atomic<std::uint32_t> id{0};
    std::atomic<std::uint64_t> count{0};
    std::atomic<std::uint64_t> bytes{0};
};

std::array<Site, SiteSlots> sites;
std::array<ThreadCount, ThreadSlots> threads;
std::atomic<std::uint64_t> lostThreads{0}, lostSites{0};

void Observe(std::size_t bytes, const void* caller) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(caller);
    bool recorded = false;
    auto slot = static_cast<std::size_t>((address >> 4u) * 0x9e3779b97f4a7c15ull >> 52u) % SiteSlots;
    for (std::size_t probe = 0; probe < SiteSlots; ++probe, slot = (slot + 1) % SiteSlots) {
        auto& site = sites[slot];
        auto current = site.caller.load(std::memory_order_relaxed);
        if (current == 0 && site.caller.compare_exchange_strong(current, address)) current = address;
        if (current != address) continue;
        site.count.fetch_add(1, std::memory_order_relaxed);
        site.bytes.fetch_add(bytes, std::memory_order_relaxed);
        recorded = true;
        break;
    }
    if (!recorded) lostSites.fetch_add(1, std::memory_order_relaxed);
    const auto id = CurrentThread();
    for (auto& thread : threads) {
        auto current = thread.id.load(std::memory_order_relaxed);
        if (current == 0 && thread.id.compare_exchange_strong(current, id)) current = id;
        if (current != id) continue;
        thread.count.fetch_add(1, std::memory_order_relaxed);
        thread.bytes.fetch_add(bytes, std::memory_order_relaxed);
        return;
    }
    lostThreads.fetch_add(1, std::memory_order_relaxed);
}

struct SiteCount {
    std::uintptr_t caller = 0;
    std::uint64_t count = 0;
    std::uint64_t bytes = 0;
};

struct Window {
    std::vector<SiteCount> sites;
    std::uint64_t count = 0;
    std::uint64_t bytes = 0;
    std::uint64_t busiestCount = 0;
    std::uint32_t busiest = 0;
};

Window Measure(std::vector<std::uint32_t>& commands) {
    lostThreads.store(0);
    lostSites.store(0);
    for (auto& site : sites) {
        site.caller.store(0);
        site.count.store(0);
        site.bytes.store(0);
    }
    for (auto& thread : threads) {
        thread.id.store(0);
        thread.count.store(0);
        thread.bytes.store(0);
    }
    {
        hostProbe.store(&Observe, std::memory_order_release);
        struct StopProbe {
            ~StopProbe() { hostProbe.store(nullptr, std::memory_order_release); }
        } stop;
        Submit(commands);
    }
    Require(lostSites.load() == 0 && lostThreads.load() == 0, "draw allocations: the counter lost allocation sites or threads");
    Window window;
    for (const auto& site : sites) {
        if (site.caller.load() == 0) continue;
        window.sites.push_back({site.caller.load(), site.count.load(), site.bytes.load()});
        window.count += site.count.load();
        window.bytes += site.bytes.load();
    }
    for (const auto& thread : threads) {
        if (thread.count.load() > window.busiestCount) {
            window.busiestCount = thread.count.load();
            window.busiest = thread.id.load();
        }
    }
    return window;
}

std::string Location(std::uintptr_t address) {
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(address), &info) == 0 || info.dli_fname == nullptr) return "?";
    std::string name(info.dli_fname);
    if (const auto slash = name.find_last_of('/'); slash != std::string::npos) name = name.substr(slash + 1);
    char offset[32];
    std::snprintf(offset, sizeof(offset), "+0x%llx", static_cast<unsigned long long>(address - reinterpret_cast<std::uintptr_t>(info.dli_fbase)));
    return name + offset;
}

double PerDraw(std::uint64_t many, std::uint64_t none, std::size_t draws) {
    return (static_cast<double>(many) - static_cast<double>(none)) / static_cast<double>(draws);
}

using Builder = std::vector<std::uint32_t> (*)(std::size_t);

struct Ceiling {
    double allocations;
    double bytes;
};

struct Shape {
    const char* name;
    Builder build;
    bool dispatch;
    Ceiling plain;
    Ceiling pipelined;
};

double ReportAllocations(const Shape& shape, std::size_t draws) {
    auto empty = shape.build(0);
    auto repeated = shape.build(draws);
    for (int warm = 0; warm < 3; ++warm) Submit(repeated);
    const auto base = Measure(empty);
    const auto many = Measure(repeated);
    Require(many.count > 0 && base.count > 0, std::string("draw allocations [") + shape.name + "]: the probe observed no allocations");
    const auto perDraw = PerDraw(many.count, base.count, draws);
    std::printf("draw allocations [%s]: %zu draws: %llu allocations %llu bytes, without them: %llu allocations %llu bytes, busiest thread %llu allocations\n", shape.name, draws, static_cast<unsigned long long>(many.count), static_cast<unsigned long long>(many.bytes), static_cast<unsigned long long>(base.count), static_cast<unsigned long long>(base.bytes), static_cast<unsigned long long>(many.busiestCount));
    std::printf("draw allocations [%s] per draw: %.3f (%.1f bytes)\n", shape.name, perDraw, PerDraw(many.bytes, base.bytes, draws));
    auto ordered = many.sites;
    std::sort(ordered.begin(), ordered.end(), [](const SiteCount& left, const SiteCount& right) { return left.count > right.count; });
    for (const auto& site : ordered) {
        std::uint64_t baseCount = 0;
        std::uint64_t baseBytes = 0;
        for (const auto& other : base.sites) {
            if (other.caller == site.caller) {
                baseCount = other.count;
                baseBytes = other.bytes;
            }
        }
        const auto sitePerDraw = PerDraw(site.count, baseCount, draws);
        if (sitePerDraw < 0.05) continue;
        std::printf("  site %s: %.3f per draw, %.1f bytes per draw\n", Location(site.caller).c_str(), sitePerDraw, PerDraw(site.bytes, baseBytes, draws));
    }
    const auto perDrawBytes = PerDraw(many.bytes, base.bytes, draws);
    const auto& ceiling = std::getenv("APS5_DRAW_PIPELINE") != nullptr ? shape.pipelined : shape.plain;
    if (std::getenv("APS5_DRAW_ALLOCATIONS_NO_LIMIT") == nullptr) {
        const auto maximum = std::getenv("APS5_DRAW_ALLOCATIONS_MAX");
        const auto limit = maximum != nullptr ? std::strtod(maximum, nullptr) : ceiling.allocations;
        Require(perDraw <= limit, std::string("draw allocations [") + shape.name + "]: " + std::to_string(perDraw) + " allocations per warmed draw, more than the limit " + std::to_string(limit));
    }
    const auto maximumBytes = std::getenv("APS5_DRAW_ALLOCATIONS_MAX_BYTES");
    if (maximumBytes != nullptr || std::getenv("APS5_DRAW_ALLOCATIONS_NO_LIMIT") == nullptr) {
        const auto limit = maximumBytes != nullptr ? std::strtod(maximumBytes, nullptr) : ceiling.bytes;
        Require(perDrawBytes <= limit, std::string("draw allocations [") + shape.name + "]: " + std::to_string(perDrawBytes) + " requested bytes per warmed draw, more than the limit " + std::to_string(limit));
    }
    return perDraw;
}

const GuestBlock* Vertices = nullptr;
const GuestBlock* Target = nullptr;
const GuestBlock* IndexBlock = nullptr;
const GuestBlock* Table = nullptr;
const GuestBlock* Output = nullptr;

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        Shader compute{};
        compute.file_header = 0x34333231u;
        compute.version = 0x18u;
        compute.header_size = sizeof(Shader);
        compute.shader_size = sizeof(DispatchCode);
        compute.code = DispatchCode.data();
        AgcDriverRegisterShader_nid_postfix(&compute);
        GuestBlock vertices;
        GuestBlock target;
        GuestBlock indices;
        GuestBlock table;
        GuestBlock input;
        GuestBlock output;
        WriteVertices(vertices);
        std::memcpy(indices.Bytes(0), ImageIndices.data(), sizeof(ImageIndices));
        for (std::uint32_t lane = 0; lane < 32; ++lane) std::memcpy(input.Bytes(lane * 4u), &lane, sizeof(lane));
        const auto source = StorageDescriptor(input.At(0));
        std::memcpy(table.Bytes(0), source.data(), sizeof(source));
        Vertices = &vertices;
        Target = &target;
        IndexBlock = &indices;
        Table = &table;
        Output = &output;
        const std::array<Shape, 4> shapes{{
            {"recipe auto draw", [](std::size_t count) { return Draws(*Vertices, *Target, count); }, false, {45.0, 8100.0}, {48.5, 8200.0}},
            {"indexed draw, guest indices", [](std::size_t count) { return Draws(*Vertices, *Target, count, DrawKind::Indexed, IndexBlock->At(0)); }, false, {47.5, 8300.0}, {50.5, 8450.0}},
            {"indexed draw, image indices", [](std::size_t count) { return Draws(*Vertices, *Target, count, DrawKind::Indexed, reinterpret_cast<std::uintptr_t>(ImageIndices.data())); }, false, {47.5, 10250.0}, {50.5, 10450.0}},
            {"compute dispatch", [](std::size_t count) { return Dispatches(*Table, *Output, count); }, true, {13.5, 1150.0}, {16.0, 1500.0}},
        }};
        const auto draws = static_cast<std::size_t>(std::max(1, std::atoi(std::getenv("APS5_DRAW_ALLOCATIONS_DRAWS") != nullptr ? std::getenv("APS5_DRAW_ALLOCATIONS_DRAWS") : "256")));
        const auto only = std::getenv("APS5_DRAW_ALLOCATIONS_SHAPE");
        bool drew = false;
        bool dispatched = false;
        for (const auto& shape : shapes) {
            if (only != nullptr && std::string(shape.name).find(only) == std::string::npos) continue;
            ReportAllocations(shape, draws);
            (shape.dispatch ? dispatched : drew) = true;
        }
        Require(drew || dispatched, std::string("draw allocations: no shape matches ") + (only != nullptr ? only : ""));
        if (drew) {
            AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(target.At(0)), BlockBytes);
            unsigned brightest = 0;
            for (std::size_t i = 0; i < BlockBytes; ++i) brightest = std::max(brightest, static_cast<unsigned>(target.Bytes(0)[i]));
            Require(brightest == 255u, "draw allocations: the repeated draws did not render, brightest byte " + std::to_string(brightest));
        }
        if (dispatched) {
            AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(output.At(0)), 128);
            for (std::uint32_t lane = 0; lane < 32; ++lane) {
                std::uint32_t stored = 0;
                std::memcpy(&stored, output.Bytes(lane * 4u), sizeof(stored));
                Require(stored == lane + 1u, "draw allocations: the repeated dispatches stored " + std::to_string(stored) + " in lane " + std::to_string(lane));
            }
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("draw allocations: the repeated draws and dispatches ran");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
#endif
