#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_SEGMENTLOOP_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_SEGMENTLOOP_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/Submission.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace AgcDriver::DriverDetail {

struct LeanPacket {
    static constexpr std::uint32_t NoSkip = 0xffffffffu;
    static constexpr std::uint32_t Predicated = 1u << 0u;
    static constexpr std::uint32_t Draw = 1u << 1u;
    static constexpr std::uint32_t IndirectDraw = 1u << 2u;
    static constexpr std::uint32_t FlipHeader = 1u << 3u;
    static constexpr std::uint32_t RenderingWait = 1u << 4u;
    static constexpr std::uint32_t Flip = 1u << 5u;
    static constexpr std::uint32_t KeepsFinisher = 1u << 6u;
    static constexpr std::uint32_t Seal = 1u << 7u;
    static constexpr std::uint32_t Held = 1u << 8u;
    static constexpr std::uint32_t LabelIssue = 1u << 9u;
    static constexpr std::uint32_t Enqueues = 1u << 10u;
    static constexpr std::uint32_t Memory = 1u << 11u;
    static constexpr std::uint32_t Traced = 1u << 12u;
    static constexpr std::uint32_t Plain = 1u << 13u;
    static constexpr std::uint32_t Wait = 1u << 14u;
    static constexpr std::uint32_t IndirectRegisters = 1u << 15u;

    enum class Pipeline : std::uint8_t { Drains, Keeps, Asks, AsksLists };

    std::uint32_t cursor = 0;
    std::uint32_t words = 0;
    std::uint32_t header = 0;
    std::uint32_t skip = NoSkip;
    std::uint32_t flags = 0;
    std::uint8_t opcode = 0;
    Pipeline pipeline = Pipeline::Drains;

    bool Has(std::uint32_t flag) const { return (flags & flag) != 0; }
};

struct PacketTable {
    std::vector<LeanPacket> packets;
    std::vector<std::uint32_t> outputs;
    std::uint32_t flipHeaders = 0;
    bool built = false;

    void Clear() {
        packets.clear();
        outputs.clear();
        flipHeaders = 0;
        built = false;
    }
    std::size_t IndexAt(std::size_t cursor) const;
};

struct SegmentSlot {
    Submission own{};
    PacketTable table;
    std::uint64_t mark = 0;
    std::uint64_t generation = 0;
    bool busy = false;
    bool prepared = false;
};

class SegmentSlots {
public:
    SegmentSlot& Take();
    std::size_t Size() const { return slots.size(); }

private:
    std::vector<std::unique_ptr<SegmentSlot>> slots;
};

struct FlushGate {
    bool everyPacket = false;
    std::uint64_t cap = 0;
    std::uint64_t capGeneration = 0;
    bool labelArmed = false;
    std::chrono::steady_clock::time_point label{};
    std::int64_t labelDue = 0;
    bool reapArmed = false;
    std::chrono::steady_clock::time_point reap{};
    std::int64_t reapDue = 0;
    std::uint64_t skipped = 0;
    std::uint64_t calls = 0;
};

struct FlushInputs {
    bool given = false;
    std::optional<std::chrono::steady_clock::time_point> pending;
    bool completions = false;
    std::uint64_t work = 0;
    std::chrono::steady_clock::time_point reap{};
    std::chrono::steady_clock::time_point now{};
};

namespace FlushDecision {

inline constexpr std::uint32_t Noted = 1u << 0u;
inline constexpr std::uint32_t Record = 1u << 1u;
inline constexpr std::uint32_t Submit = 1u << 2u;
inline constexpr std::uint32_t Deadline = 1u << 3u;
inline constexpr std::uint32_t Capped = 1u << 4u;
inline constexpr std::uint32_t Reap = 1u << 5u;
inline constexpr std::uint32_t RecordAtLock = 1u << 6u;
inline constexpr std::uint32_t Acts = Record | Submit | Capped | Reap | RecordAtLock;

}

struct LeanRun;

std::int64_t FlushTicks();
std::int64_t FlushTicksAt(std::chrono::steady_clock::time_point at);

}

#endif
