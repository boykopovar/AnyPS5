#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace AgcDriver::Pm4 {

namespace {

std::atomic<bool> importableGdsRequested{false};
std::atomic<bool> gdsShared{false};
std::atomic<bool> gdsImportable{false};

}

void RequestImportableGds(bool requested) {
    importableGdsRequested.store(requested, std::memory_order_release);
}

bool ImportableGdsRequested() {
    return importableGdsRequested.load(std::memory_order_acquire);
}

bool GdsShared() {
    if (!ImportableGdsRequested()) return false;
    static_cast<void>(GdsAddress());
    return gdsShared.load(std::memory_order_acquire);
}

bool GdsImportable() {
    if (!ImportableGdsRequested()) return false;
    static_cast<void>(GdsAddress());
    return gdsImportable.load(std::memory_order_acquire);
}

void RejectGdsImport() {
    gdsImportable.store(false, std::memory_order_release);
}

std::uint64_t GdsAddress() {
    static const std::uint64_t address = [] {
        constexpr int ReadWrite = 0x3;
        constexpr int PrivateAnonymous = 0x1002;
        constexpr int Shared = 0x1;
        constexpr int OpenReadWrite = 0x2;
        const char* const anonymousSharedMemory = reinterpret_cast<const char*>(1);
        if (ImportableGdsRequested() && std::getenv("APS5_NO_SHARED_GDS") == nullptr) {
            if (const int descriptor = shm_open_nid_postfix(anonymousSharedMemory, OpenReadWrite, 0600); descriptor >= 0) {
                void* shared = ftruncate_nid_postfix(descriptor, GdsBytes) == 0 ? mmap_nid_postfix(nullptr, GdsBytes, ReadWrite, Shared, descriptor, 0) : nullptr;
                close_nid_postfix(descriptor);
                if (shared != nullptr && shared != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1))) {
                    std::memset(shared, 0, GdsBytes);
                    gdsShared.store(true, std::memory_order_release);
                    gdsImportable.store(true, std::memory_order_release);
                    return reinterpret_cast<std::uint64_t>(shared);
                }
            }
        }
        void* mapped = mmap_nid_postfix(nullptr, GdsBytes, ReadWrite, PrivateAnonymous, -1, 0);
        if (mapped == nullptr || mapped == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1))) throw std::runtime_error("cannot allocate the global data share");
        return reinterpret_cast<std::uint64_t>(mapped);
    }();
    return address;
}

namespace {

std::string ToHex(std::uint32_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%x", value);
    return buffer;
}

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

constexpr std::uint32_t GcnPrepareFlipMarker = 0x68750777u;
constexpr std::uint32_t GcnPrepareFlipLabelMarker = 0x68750778u;
constexpr std::size_t GcnFlipReservationWords = 64;
constexpr std::uint32_t GcnReleaseEventControl = 0xf3fu;
constexpr std::uint32_t GcnVolatileTextureCacheAction = 0x10000u;

std::uint64_t address(std::uint32_t low, std::uint32_t high) {
    return low | (static_cast<std::uint64_t>(high) << 32u);
}

using GcnSystemCounterTicks = std::chrono::duration<std::uint64_t, std::ratio<1, 800000000>>;

const auto GcnSystemCounterStart = std::chrono::steady_clock::now();

std::uint64_t ReleaseMemValue(std::span<const std::uint32_t> packet) {
    const auto dataSelect = packet[2] >> 29u;
    if (dataSelect == 3) return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 10);
    if (dataSelect == 4) return std::chrono::duration_cast<GcnSystemCounterTicks>(std::chrono::steady_clock::now() - GcnSystemCounterStart).count();
    return address(packet[5], packet[6]);
}

std::uint32_t registerOffset(std::uint32_t value) {
    require(value != 0xffffffffu, "indirect register sentinel semantics are not implemented");
    const auto offset = value & ~0x70000000u;
    if (offset > 0xffffu) {
        char what[80];
        std::snprintf(what, sizeof(what), "extended register semantics are not implemented (offset dword 0x%08x)", value);
        throw std::runtime_error(what);
    }
    return offset;
}

// Debug aid: APS5_TRACE_CONTEXT_STATE logs context-state operations and the context registers
// written while a context is pushed. Read once: getenv scans the environment under a lock.
bool TraceContextState() {
    static const bool value = std::getenv("APS5_TRACE_CONTEXT_STATE") != nullptr;
    return value;
}

// Kill switch: APS5_NO_INDIRECT_DRAW=1 rejects the indirect draw packets at submission validation as
// before they were implemented (the driver then dumps the submission as it did).
bool IndirectDrawsDisabled() {
    static const bool value = std::getenv("APS5_NO_INDIRECT_DRAW") != nullptr;
    return value;
}

// An SH register an indirect draw packet may name for the CP's patch: the sentinel 0x280 (write
// nowhere) or a user-data register of the vertex-side stage banks (0x8c.. for the vertex / geometry
// programs, 0x10c.. for the local / hull programs).
bool drawLocation(std::uint32_t value) {
    return value == 0 || value == 0x280u || value - 0x8cu < 32u || value - 0x10cu < 32u;
}

Registers& registersFor(QueueState& queue, std::uint32_t opcode) {
    if (opcode == 0x69 || opcode == 0x9f) return queue.context;
    if (opcode == 0x76 || opcode == 0x63) return queue.shader;
    return queue.userConfig;
}

void writeRegister(QueueState& queue, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    if ((opcode == 0x69 || opcode == 0x9f) && (offset == 0x8e || offset == 0x8f || offset == 0x318 || offset == 0x31b || offset == 0x31c || offset == 0x31d || offset == 0x390 || offset == 0x3b0 || offset == 0x3b8))
        APS5_LOG_OUT_DEBUG("CONTEXT WRITE opcode=0x%x offset=0x%x value=0x%x", opcode, offset, value);
    registersFor(queue, opcode).insert_or_assign(offset, value);
    if (TraceContextState() && (opcode == 0x69 || opcode == 0x9f) && ((offset >= 0x318 && offset < 0x318 + 8 * 0xf && (offset - 0x318) % 0xf == 0) || offset == 0x8e)) std::fprintf(stderr, "[context]   write %x = %08x (0x%x)\n", offset, value, opcode);
    if ((opcode == 0x64 || opcode == 0x79 || opcode == 0x7a) && offset == 0x243) queue.indexType = value & 3u;
}

bool memorySelector(std::uint32_t selector) {
    return selector == 0 || selector == 3;
}

std::uint32_t dmaSource(std::span<const std::uint32_t> packet) {
    const auto selector = ((packet[1] >> 29u) & 3u) | ((packet[6] >> 24u) & 4u) | ((packet[6] >> 25u) & 8u);
    return selector == (DmaSelectGds | 12u) ? DmaSelectGds : selector;
}

std::uint32_t dmaDestination(std::span<const std::uint32_t> packet) {
    const auto selector = ((packet[1] >> 20u) & 3u) | ((packet[6] >> 25u) & 4u) | ((packet[6] >> 26u) & 8u);
    return selector == (DmaSelectGds | 12u) ? DmaSelectGds : selector;
}

// The bytes a COPY_DATA or DMA_DATA stores: an immediate is a repeating 32-bit pattern, a memory
// source is read through the checked path (it waits for recorded GPU work that writes it).
std::vector<std::byte> copySource(std::uint64_t source, std::size_t bytes, bool immediate) {
    std::vector<std::byte> data(bytes);
    if (immediate) {
        const auto value = static_cast<std::uint32_t>(source);
        for (std::size_t i = 0; i < bytes; ++i) data[i] = static_cast<std::byte>(value >> ((i % 4) * 8));
    } else {
        GuestMemory::Read(source, data);
    }
    return data;
}

bool gdsRange(std::uint64_t offset, std::size_t bytes) {
    return offset <= GdsBytes && bytes <= GdsBytes - offset;
}

std::vector<std::byte> dmaSourceBytes(std::span<const std::uint32_t> packet) {
    const std::size_t bytes = DmaBytes(packet);
    if (dmaSource(packet) != DmaSelectGds) return copySource(address(packet[2], packet[3]), bytes, dmaSource(packet) == DmaSourceData);
    return copySource(GdsAddress() + packet[2], bytes, false);
}

void copyMemory(std::uint64_t source, std::uint64_t destination, std::size_t bytes, bool immediate) {
    if (bytes == 0) return;
    GuestMemory::CheckRange(reinterpret_cast<void*>(destination), bytes, 1, true);
    GuestMemory::Write(destination, copySource(source, bytes, immediate));
}

constexpr std::uint32_t CopyDataRegisterSource = 0;
constexpr std::uint32_t CopyDataCachePolicy = (3u << 13u) | (3u << 25u);
constexpr std::uint32_t ShaderRegisterBase = 0x2c00;
constexpr std::uint32_t ContextRegisterBase = 0xa000;
constexpr std::uint32_t UserConfigRegisterBase = 0xc000;

std::uint32_t copyDataCount(std::span<const std::uint32_t> packet) {
    return (packet[1] & 0x10000u) != 0 ? 2u : 1u;
}

std::uint64_t gpuClockCount() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 10);
}

std::uint32_t registerBankBase(std::uint32_t offset) {
    if (offset - ShaderRegisterBase < 0x400u) return ShaderRegisterBase;
    if (offset - ContextRegisterBase < 0x400u) return ContextRegisterBase;
    if (offset - UserConfigRegisterBase < 0x4000u) return UserConfigRegisterBase;
    return 0;
}

std::vector<std::byte> copyDataSource(std::span<const std::uint32_t> packet, const QueueState& queue, std::size_t bytes) {
    const auto source = CopyDataSource(packet);
    if (source == CopyDataRegisterSource) {
        std::vector<std::byte> data(bytes);
        for (std::size_t word = 0; word < bytes / 4; ++word) {
            const auto offset = packet[2] + static_cast<std::uint32_t>(word);
            const auto base = registerBankBase(offset);
            const auto& bank = base == ShaderRegisterBase ? queue.shader : base == ContextRegisterBase ? queue.context : queue.userConfig;
            if (base == 0 || !bank.contains(offset - base)) throw std::runtime_error("COPY_DATA source register 0x" + ToHex(offset) + " has no known value");
            const auto value = bank.at(offset - base);
            for (std::size_t i = 0; i < 4; ++i) data[word * 4 + i] = static_cast<std::byte>(value >> (i * 8));
        }
        return data;
    }
    if (source == CopyDataSelectGds) return copySource(GdsAddress() + packet[2], bytes, false);
    if (source != CopyDataGpuClockSource) return copySource(address(packet[2], packet[3]), bytes, CopyDataImmediate(source));
    const auto value = gpuClockCount();
    std::vector<std::byte> data(bytes);
    for (std::size_t i = 0; i < bytes; ++i) data[i] = static_cast<std::byte>(value >> (i * 8));
    return data;
}

}

std::string_view KnownName(std::uint32_t header) {
    if (FillerPacket(header)) return "PM4_FILLER";
    const auto opcode = (header >> 8u) & 0xffu;
    if (opcode == 0x10 && (header & 0xfcu) != 0) {
        switch ((header >> 2u) & 0x3fu) {
            case 0x05: return "DRAW_RESET";
            case 0x06: return "WAIT_FLIP_DONE";
            case 0x09: return "DISPATCH_RESET";
            case 0x0b: return "PUSH_MARKER";
            case 0x0c: return "POP_MARKER";
            case 0x14: return "ACQUIRE_MEM_CUSTOM";
            case 0x15: return "WRITE_DATA_CUSTOM";
            case 0x17: return "FLIP";
            case 0x18: return "RELEASE_MEM_CUSTOM";
            case 0x19: return "DMA_DATA_CUSTOM";
            case 0x1a: return "CONTEXT_STATE";
            default: return "UNKNOWN_CUSTOM";
        }
    }
    for (const auto& entry : Opcodes) if (entry.value == opcode) return entry.name;
    return {};
}

std::string Name(std::uint32_t header) {
    if (const auto known = KnownName(header); !known.empty()) return std::string(known);
    char text[24]{};
    std::snprintf(text, sizeof(text), "UNKNOWN_0x%02x", (header >> 8u) & 0xffu);
    return text;
}

std::string_view UnsupportedReason(std::uint32_t header) {
    const auto opcode = (header >> 8u) & 0xffu;
    if (opcode == 0x10) {
        switch ((header >> 2u) & 0x3fu) {
            case 0: case 0x06: case 0x09: case 0x0b: case 0x0c: case 0x17: case 0x1a: return {};
            case 0x14: case 0x18: return "guest cache actions and GPU release events are not implemented";
            default: return "custom packet has no implemented contract in the reference dispatch table";
        }
    }
    switch (opcode) {
        case 0x11: case 0x12: case 0x13: case 0x15: case 0x16: case 0x20: case 0x22: case 0x26: case 0x27:
        case 0x2a: case 0x2d: case 0x2f: case 0x35: case 0x37: case 0x40: case 0x42: case 0x45: case 0x46: case 0x50:
        case 0x58: case 0x63: case 0x64: case 0x69: case 0x76: case 0x79: case 0x7a:
        case 0x51: case 0x81: case 0x83: case 0x91: case 0x9f: case 0x28: return {};
        case 0x24: case 0x25: case 0x2c: case 0x38:
            if (IndirectDrawsDisabled()) return "graphics draw, shader stages and guest render-target materialization are not implemented";
            return {};
        case 0x3a:
            return "DRAW_INDEX_MULTI_INST is not implemented: which fields hold the instance count, object ID address, index base and index count, and how object IDs reach the shaders, are not known";
        case 0x8d:
            return "graphics draw, shader stages and guest render-target materialization are not implemented";
        case 0x33: case 0x3f: return "nested command buffers, branching and nested flip reservation are not implemented";
        case 0x3c: case 0x93: return {};
        case 0x39: return {};
        case 0x59: return {};
        case 0x84: case 0x85: case 0x86: case 0x88:
            return "separate CE/DE execution and counter synchronization are not implemented";
        case 0x49: return {};
        case 0x43: case 0x47: case 0x48:
            return "guest cache actions, GPU events and interrupt delivery are not implemented";
        case 0x8e: return "GPU LOD statistics are not implemented; synthetic results are forbidden";
        case 0x41: case 0x68: case 0x78:
            return "opcode is named but has no handler in the reference dispatch table";
        default: return "opcode is not known in the reference";
    }
}

constexpr std::uint32_t ConditionCachePolicy = 3u << 25u;
constexpr std::uint32_t ConditionalWordsMask = 0x3fffu;
constexpr std::uint32_t DmaSourceCachePolicy = 3u << 13u;
constexpr std::uint32_t DmaDestinationCachePolicy = 3u << 25u;
constexpr std::uint32_t WriteDataCachePolicy = 3u << 25u;

void Validate(std::span<const std::uint32_t> packet, std::uint32_t queue) {
    require(!packet.empty(), "truncated PM4 header");
    const auto header = packet[0];
    if (FillerPacket(header)) {
        require(packet.size() == 1, "invalid PM4 filler size");
        return;
    }
    require(packet.size() >= 2, "truncated PM4 header or payload");
    require((header & 0xc0000000u) == 0xc0000000u, "unsupported PM4 packet type");
    require(packet.size() == ((header >> 16u) & 0x3fffu) + 2u, "invalid PM4 packet size");
    const auto opcode = (header >> 8u) & 0xffu;
    const auto size = [&](std::size_t count) { require(packet.size() == count, "invalid packet size"); };
    const auto graphics = [&] { require(queue == 0, "graphics packet in compute queue"); };
    const auto reason = UnsupportedReason(header);
    if (!reason.empty() && !(opcode == 0x3f && Predicated(header))) throw std::runtime_error(std::string(reason));
    if (opcode == 0x10) {
        require((header & 3u) == 0, "unsupported NOP header flags");
        switch ((header >> 2u) & 0x3fu) {
            case 0:
                if ((packet[1] & 0xffff0000u) == 0x68750000u) {
                    require(ShaderRecompiler::GcnGuestShaders(), "typed user-data and legacy flip markers are not implemented");
                    require(packet.size() != GcnFlipReservationWords, "a GCN flip reservation in a command buffer is not implemented");
                    require(packet[1] != GcnPrepareFlipMarker && packet[1] != GcnPrepareFlipLabelMarker, "legacy flip markers are not implemented");
                }
                break;
            case 0x09: size(2); break;
            case 0x06: size(4); require(packet[3] == 0, "unsupported rendering wait mode"); break;
            case 0x0b: {
                const auto data = std::as_bytes(packet.subspan(1));
                require(std::find(data.begin(), data.end(), std::byte{}) != data.end(), "unterminated marker text");
                break;
            }
            case 0x0c: break;
            case 0x17: size(6); break;
            case 0x1a:
                graphics();
                require(packet.size() == 3 || packet.size() == 5, "invalid context-state packet size");
                require(packet[1] <= 3, "unknown context-state operation");
                require(std::all_of(packet.begin() + 2, packet.end(), [](auto value) { return value == 0; }), "context-state trailing fields are not implemented");
                break;
        }
        return;
    }
    // Header bit 1 selects the compute shader type and bit 2 (register writes) resets the filter CAM;
    // neither changes what the packet writes.
    const auto flags = header & 0xffu;
    const bool registerWrite = opcode == 0x69 || opcode == 0x76 || opcode == 0x79 || opcode == 0x7a || (opcode == 0x37 && ((packet[1] >> 8u) & 0xfu) == 0);
    auto allowedFlags = opcode == 0x11 || opcode == 0x15 || opcode == 0x16 ? 2u : registerWrite ? 6u : opcode == 0x3c || opcode == 0x93 ? 2u : 0u;
    if (opcode != 0x20 && opcode != 0x22) allowedFlags |= 1u;
    if (queue != 0 && ShaderRecompiler::GcnGuestShaders()) allowedFlags |= 2u;
    if ((flags & ~allowedFlags) != 0) throw std::runtime_error("PM4 header flags 0x" + ToHex(flags) + " are not implemented");
    switch (opcode) {
        case 0x11:
            size(4);
            require(packet[1] == 1 && (packet[2] & 3u) == 0 && packet[3] <= 0xffffu, "unsupported indirect base index, alignment or address bits");
            if ((header & 2u) == 0) graphics();
            break;
        case 0x51:
            graphics();
            size(4);
            require((packet[1] & 0xffff0000u) == 0, "CONTEXT_REG_RMW reserved offset bits are not implemented");
            break;
        case 0x12: graphics(); size(2); require((packet[1] & ~0xfu) == 0, "unsupported CLEAR_STATE payload bits"); break;
        case 0x20: {
            graphics();
            size(4);
            const auto operation = (packet[1] >> 16u) & 7u;
            require(operation != 2, "GPU primitive-count predication is not implemented");
            require(operation == 0 || operation == 1 || operation == 3 || operation == 4, "invalid predication operation");
            require((packet[1] & ~0x71100u) == 0, "unsupported SET_PREDICATION bits");
            if (operation != 0) require(address(packet[2], packet[3]) != 0 && (packet[2] & 0xfu) == 0, "unaligned predication address");
            break;
        }
        case 0x3f: size(4); break;
        case 0x13: case 0x2f: graphics(); size(2); break;
        case 0x26:
            graphics();
            size(3);
            require((packet[1] & 1u) == 0 && packet[2] <= 0xffffu, "unsupported index base alignment or address bits");
            break;
        case 0x27:
            graphics();
            size(6);
            require(packet[3] <= 0xffffu, "unsupported index address bits");
            require(packet[4] <= packet[1], "index count exceeds maximum index size");
            require((packet[5] & ~0x20u) == 0, "unsupported indexed draw flags");
            break;
        case 0x2a: graphics(); size(2); require(packet[1] <= 3, "unsupported index-type modifiers"); break;
        case 0x91:
            graphics();
            size(4);
            require((packet[1] & 1u) == 0, "INDEX_ATTRIBUTES_INDIRECT indirect mode (addr_offset) is not implemented");
            require((packet[1] & 0xeu) == 0, "INDEX_ATTRIBUTES_INDIRECT attribute base is not 16-byte aligned");
            require((packet[3] >> 16u) == 0, "INDEX_ATTRIBUTES_INDIRECT reserved attribute index bits are set");
            break;
        case 0x2d:
            graphics();
            size(3);
            require((packet[2] & ~0x20u) == 2u, "unsupported auto draw flags");
            break;
        case 0x35:
            graphics();
            size(5);
            require(packet[3] <= packet[1], "index count exceeds maximum index size");
            require((packet[4] & ~0x20u) == 0, "unsupported indexed draw flags");
            break;
        case 0x24: case 0x25:
            graphics();
            size(5);
            require((packet[1] & 3u) == 0, "misaligned indirect draw offset");
            require((packet[2] >> 16u) == 0 && (packet[3] >> 16u) == 0, "start-index location semantics are not implemented");
            require(drawLocation(packet[2]) && drawLocation(packet[3]), "invalid indirect draw register location");
            require((packet[4] & ~0x20u) == (opcode == 0x24 ? 2u : 0u), "unsupported indirect draw initiator");
            break;
        case 0x2c: case 0x38:
            graphics();
            size(10);
            require((packet[1] & 3u) == 0, "misaligned indirect draw offset");
            require((packet[2] >> 16u) == 0 && (packet[3] >> 16u) == 0, "start-index location semantics are not implemented");
            require(drawLocation(packet[2]) && drawLocation(packet[3]) && drawLocation(packet[4] & 0xffffu), "invalid indirect draw register location");
            require((packet[4] & ~(0xffffu | (1u << 30u) | (1u << 31u))) == 0, "indirect multi-draw control bits are not implemented");
            if ((packet[4] & (1u << 30u)) != 0) require((packet[6] & 3u) == 0 && address(packet[6], packet[7]) != 0 && packet[7] <= 0xffffu, "invalid indirect draw count address");
            else require(address(packet[6], packet[7]) == 0, "count address without an indirect draw count");
            require((packet[8] & 3u) == 0 && packet[8] >= (opcode == 0x2c ? 16u : 20u), "invalid indirect draw stride");
            require((packet[9] & ~0x20u) == (opcode == 0x2c ? 2u : 0u), "unsupported indirect draw initiator");
            break;
        case 0x15: size(5); if ((packet[4] & ~0xa06eu) != 0x1u || (packet[4] & 0x22u) == 0x22u) throw std::runtime_error("dispatch modifiers 0x" + ToHex(packet[4]) + " are not implemented"); break;
        case 0x16:
            require(packet.size() == 3 || packet.size() == 4, "invalid indirect dispatch size");
            require((packet.back() & ~0xa06cu) == 0x1u, "indirect dispatch modifiers are not implemented");
            break;
        case 0x22:
            size(5);
            require((packet[1] & 3u) == 0, "COND_EXEC reserved address bits are not implemented");
            require(packet[2] <= 0xffffu, "COND_EXEC address bits above 48 are not implemented");
            require((packet[3] & ~(queue == 0 ? 0u : ConditionCachePolicy)) == 0, "COND_EXEC reserved control fields are not implemented");
            require((packet[4] & ~ConditionalWordsMask) == 0, "COND_EXEC reserved count bits are not implemented");
            break;
        case 0x42: size(2); require(packet[1] == 0, "unsupported PFP_SYNC_ME payload"); break;
        case 0x28:
            graphics();
            size(3);
            require((packet[1] & 0x7fffffffu) == 0 && (packet[2] & 0x7fffffffu) == 0, "CONTEXT_CONTROL register loading and shadowing are not implemented");
            break;
        case 0x46: {
            const auto eventType = packet[1] & 0x3fu;
            const auto eventIndex = (packet[1] >> 8u) & 0xfu;
            const auto streamOutStatistics = (eventType >= 0x01u && eventType <= 0x03u) || eventType == 0x20u;
            require(!streamOutStatistics || (eventIndex != 3 && (eventIndex < 8 || eventIndex > 11)), "stream-out statistics sampling to memory is not implemented");
            require((packet[1] & ~0x73fu) == 0, "unsupported EVENT_WRITE flags or reserved bits");
            switch (eventType) {
                case 0x07: case 0x0f: case 0x10:
                    size(2);
                    require(eventIndex == 4, "invalid partial-flush event index");
                    if (eventType != 0x07) graphics();
                    break;
                case 0x16: case 0x31: case 0x2a: case 0x2c: case 0x2e:
                    graphics();
                    size(2);
                    require(eventIndex == 0 || eventIndex == 7, "invalid cache-flush event index");
                    break;
                case 0x19: case 0x1a: case 0x26:
                    size(2);
                    require(eventIndex == 0, "invalid pipeline statistics or SQ event index");
                    break;
                case 0x24:
                    graphics();
                    size(2);
                    require(eventIndex == 0, "invalid VGT_FLUSH event index");
                    break;
                case 0x01: case 0x02: case 0x03: case 0x20:
                    graphics();
                    size(2);
                    require(eventIndex == 0, "invalid stream-out statistics event index");
                    break;
                case 0x38:
                    graphics();
                    size(4);
                    require(eventIndex == 1, "invalid pixel pipe statistics control event index");
                    require((packet[2] & ~0x07fffff8u) == 0 && packet[3] == 0, "unsupported PIXEL_PIPE_STAT_CONTROL reserved bits");
                    require(((packet[2] >> 3u) & 0x3fu) == 0, "PIXEL_PIPE_STAT_CONTROL counters other than occlusion count 0 are not implemented");
                    require(((packet[2] >> 9u) & 3u) == 2, "PIXEL_PIPE_STAT_CONTROL dump strides other than 128 bits are not implemented");
                    break;
                case 0x39:
                    graphics();
                    size(4);
                    require(eventIndex == 1, "invalid occlusion counter dump event index");
                    require(address(packet[2], packet[3]) != 0 && (packet[2] & 7u) == 0, "null or misaligned occlusion counter dump address");
                    break;
                default: throw std::runtime_error("EVENT_WRITE event type " + std::to_string(eventType) + " is not implemented");
            }
            break;
        }
        case 0x58: {
            require(packet.size() == 7 || packet.size() == 8, "invalid ACQUIRE_MEM packet size");
            const auto controlMask = packet.size() == 8 ? 0x86287fc3u : 0xfeecfffbu | (ShaderRecompiler::GcnGuestShaders() ? GcnVolatileTextureCacheAction : 0u);
            require((packet[1] & ~controlMask) == 0, "unsupported ACQUIRE_MEM control flags");
            require(queue == 0 || (packet[1] & 0x06287fc3u) == 0, "graphics cache operation in compute queue");
            require(packet[3] <= 0xffu && packet[5] <= 0xffu, "invalid ACQUIRE_MEM range high bits");
            require(packet[6] <= 0xffffu, "invalid ACQUIRE_MEM poll interval");
            const auto base = ((static_cast<std::uint64_t>(packet[5]) << 32u) | packet[4]) << 8u;
            const auto bytes = ((static_cast<std::uint64_t>(packet[3]) << 32u) | packet[2]) << 8u;
            require(bytes <= (1ull << 48u) - base, "ACQUIRE_MEM range exceeds 48-bit address space");
            if (packet.size() == 8) {
                require((packet[7] & ~0x3ffffu) == 0, "unsupported ACQUIRE_MEM GCR flags");
                require((packet[7] & 0x2000u) == 0, "ACQUIRE_MEM cache discard is not implemented");
            }
            break;
        }
        case 0x63: case 0x64: case 0x9f:
            if (opcode != 0x63) graphics();
            size(5);
            require((packet[1] & 3u) == 0 && packet[4] <= 0x3fffu && (packet[3] == 0x80000000u || ((packet[3] & ~0xffffu) == 0 && packet[4] <= 0x10000u - packet[3])), "unsupported indirect-register address or control fields");
            break;
        case 0x69: case 0x76: case 0x79: case 0x7a: {
            if (IsTagMarker(packet)) break;
            if (opcode != 0x76) graphics();
            require(packet.size() >= 3, "register packet has no values");
            if (opcode == 0x7a) require((packet[1] & 0xf0000000u) == 0 || (packet.size() == 3 && (packet[1] == 0x20000243u || packet[1] == 0x10000242u)), "indexed register bank selection is not implemented");
            const auto offset = registerOffset(packet[1]);
            require(packet.size() - 2 <= 0x10000u - offset, "register range overflow");
            break;
        }
        case 0x59:
            size(2);
            require((packet[1] & ~(RewindValid | RewindOffloadEnable)) == 0, "unsupported REWIND payload bits");
            break;
        case 0x3c: case 0x93: {
            size(opcode == 0x3c ? 7 : 9);
            require((packet[1] & 0x10u) != 0, "register-space WAIT_REG_MEM is not implemented");
            require((packet[1] & 7u) <= 6u, "invalid WAIT_REG_MEM compare function");
            break;
        }
        case 0x39:
            size(4);
            require((packet[3] >> 29u) == 6u || (packet[3] >> 29u) == 7u, "invalid MEM_SEMAPHORE operation");
            require(address(packet[1], packet[2]) != 0 && (packet[1] & 7u) == 0, "null or misaligned MEM_SEMAPHORE address");
            require((packet[3] & 0x10000u) == 0, "MEM_SEMAPHORE mailbox waits are not implemented");
            break;
        case 0x45: {
            size(9);
            require((packet[1] & ~0x317u) == 0, "COND_WRITE reserved fields are not implemented");
            const bool memoryPoll = (packet[1] & 0x10u) != 0;
            require(memoryPoll || (packet[3] == 0 && registerBankBase(packet[2]) != 0), "register-space COND_WRITE poll outside modeled banks is not implemented");
            require((packet[1] & 7u) <= 6u, "invalid COND_WRITE compare function");
            const auto destination = (packet[1] >> 8u) & 3u;
            require(destination <= 1u, "scratch or reserved COND_WRITE destination is not implemented");
            require(destination != 0 || (packet[7] == 0 && registerBankBase(packet[6]) != 0), "COND_WRITE destination outside modeled banks is not implemented");
            if (destination == 0 && registerBankBase(packet[6]) != ShaderRegisterBase) graphics();
            require((!memoryPoll || (packet[2] & 3u) == 0) && (destination != 1 || (packet[6] & 3u) == 0), "misaligned COND_WRITE address");
            break;
        }
        case 0x49: {
            require(packet.size() == 8 || (packet.size() == 7 && ShaderRecompiler::GcnGuestShaders()), "invalid packet size");
            if (packet.size() == 7) {
                require((packet[1] & ~GcnReleaseEventControl) == 0, "GCN RELEASE_MEM event control bits beyond the event type and index are not implemented");
                const auto interruptSelect = (packet[2] >> 24u) & 7u;
                require(interruptSelect == 0 || interruptSelect == 3, "an interrupting GCN RELEASE_MEM has no interrupt context dword");
            }
            const auto dataSelect = packet[2] >> 29u;
            require(dataSelect != 4 || ShaderRecompiler::GcnGuestShaders(), "RELEASE_MEM of the 64-bit system counter (DATA_SEL 4) is not implemented");
            require(dataSelect <= 5, "reserved RELEASE_MEM DATA_SEL");
            require(dataSelect != 5, "GDS RELEASE_MEM data is not implemented");
            require(dataSelect == 0 || address(packet[3], packet[4]) == 0 || (packet[3] & (dataSelect == 1 ? 3u : 7u)) == 0, "misaligned RELEASE_MEM destination");
            break;
        }
        case 0x81:
            graphics();
            require(packet[1] <= 0xbffcu && (packet[1] & 3u) == 0 && packet.size() - 2 <= 0x3000u - packet[1] / 4u, "constant RAM write range overflow or misalignment");
            break;
        case 0x83:
            graphics(); size(5);
            require(packet[1] <= 0xbffcu && (packet[1] & 3u) == 0 && packet[2] <= 0x3000u - packet[1] / 4u, "constant RAM dump range overflow or misalignment");
            break;
        case 0x37: {
            require(packet.size() >= 5, "WRITE_DATA has no data");
            require((packet[1] & ~(0x40110f00u | WriteDataCachePolicy)) == 0, "WRITE_DATA engine or reserved fields are not implemented");
            const auto destination = (packet[1] >> 8u) & 0xfu;
            if (destination == 0) {
                const auto count = (packet[1] & 0x10000u) != 0 ? 1u : packet.size() - 4;
                require(packet[3] == 0 && packet[2] < 0x10000u && count <= 0x10000u - packet[2], "WRITE_DATA register destination outside the register space");
                require(packet[2] >= 0xc340u && packet[2] <= 0xc347u && count <= 0xc348u - packet[2], "WRITE_DATA register destination outside the thread-trace user-data registers is not implemented");
                break;
            }
            require(destination <= 5, "invalid WRITE_DATA destination");
            require(destination != 4, "reserved WRITE_DATA destination");
            require((packet[2] & 3u) == 0, "misaligned WRITE_DATA destination");
            break;
        }
        case 0x40: {
            size(6);
            require((packet[1] & ~(0x40110f0fu | CopyDataCachePolicy)) == 0, "COPY_DATA engine or reserved fields are not implemented");
            const auto source = CopyDataSource(packet);
            const auto destination = CopyDataDestination(packet);
            if (CopyDataToRegister(packet)) {
                require(source == 2 || source == 4, "COPY_DATA to a register from a register, GDS, immediate or clock source is not implemented");
                require((packet[2] & 3u) == 0, "misaligned COPY_DATA source");
                require(packet[5] == 0 && packet[4] < 0x10000u && copyDataCount(packet) <= 0x10000u - packet[4], "COPY_DATA register destination outside the register space");
                require(packet[4] >= ContextRegisterBase && packet[4] - ContextRegisterBase <= 0x400u - copyDataCount(packet), "COPY_DATA to a register outside the context registers is not implemented");
                break;
            }
            require(destination <= 10, "invalid COPY_DATA destination");
            require(destination == 2 || destination == 4 || destination == 10 || destination == CopyDataSelectGds, "COPY_DATA performance-counter destination is not implemented");
            require(source == 2 || source == 4 || source == 5 || source == 10 || source == 11 || source == CopyDataGpuClockSource || source == CopyDataRegisterSource || source == CopyDataSelectGds, "COPY_DATA reference-clock source is not implemented");
            require(source != CopyDataRegisterSource || (packet[3] == 0 && registerBankBase(packet[2]) != 0 && registerBankBase(packet[2]) == registerBankBase(packet[2] + copyDataCount(packet) - 1)), "COPY_DATA from a register outside the shader, context and user config registers is not implemented");
            require(source != CopyDataSelectGds || (packet[3] == 0 && gdsRange(packet[2], CopyDataBytes(packet))), "COPY_DATA GDS source range exceeds the GDS");
            require(destination != CopyDataSelectGds || (packet[5] == 0 && gdsRange(packet[4], CopyDataBytes(packet))), "COPY_DATA GDS destination range exceeds the GDS");
            require(source < 10 || source == CopyDataGpuClockSource || ((packet[1] & 0x10000u) == 0 && packet[3] == 0), "64-bit immediate COPY_DATA is not implemented");
            break;
        }
        case 0x50:
            size(7);
            require((packet[1] & ~(0xe0300001u | DmaSourceCachePolicy | DmaDestinationCachePolicy)) == 0, "DMA_DATA reserved fields are not implemented");
            require(memorySelector(dmaDestination(packet)) || dmaDestination(packet) == DmaSelectGds || dmaDestination(packet) == DmaDestinationNowhere, "DMA_DATA register destination is not implemented");
            require(dmaDestination(packet) != DmaDestinationNowhere || memorySelector(dmaSource(packet)), "DMA_DATA prefetch of a register, GDS or immediate source is not implemented");
            require(memorySelector(dmaSource(packet)) || dmaSource(packet) == DmaSourceData || dmaSource(packet) == DmaSelectGds, "DMA_DATA register source is not implemented");
            require(dmaSource(packet) != DmaSelectGds || (packet[3] == 0 && gdsRange(packet[2], DmaBytes(packet))), "DMA_DATA GDS source range exceeds the GDS");
            require(dmaDestination(packet) != DmaSelectGds || (packet[5] == 0 && gdsRange(packet[4], DmaBytes(packet))), "DMA_DATA GDS destination range exceeds the GDS");
            require(dmaSource(packet) != DmaSourceData || packet[3] == 0, "DMA_DATA immediate exceeds 32 bits");
            break;
        default: throw std::runtime_error("known packet has no validator");
    }
}

}
