#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_PM4ANALYSIS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_PM4ANALYSIS_HPP

#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionCapture.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace AgcDriver::Pm4Analysis {

struct MemoryStore {
    Pm4::PacketStore store;
    bool conditional = false;
    bool label = false;
};

struct MemoryWait {
    enum class Kind : std::uint8_t { Memory, Rewind };
    Kind kind = Kind::Memory;
    Pm4::WaitFields fields{};
    bool heldAtSubmit = false;
    bool conditional = false;
    bool released = false;
    std::vector<std::uint32_t> packet;
};

struct DecodedPacket {
    std::size_t offset = 0;
    std::size_t words = 0;
    std::uint32_t header = 0;
    std::string name;
    std::string operands;
    bool conditional = false;
    std::vector<MemoryStore> stores;
    std::optional<MemoryWait> wait;
};

std::vector<DecodedPacket> Decode(const CapturedSubmission& submission);

struct PacketLocation {
    std::uint64_t ordinal = 0;
    std::uint32_t queue = 0;
    std::uint64_t serial = 0;
    std::size_t offset = 0;
    std::string name;
};

struct WriterMatch {
    enum class Order : std::uint8_t { EarlierOnQueue, LaterOnQueue, OtherQueue };
    enum class Effect : std::uint8_t { CurrentValue, LabelHistory, Evicted, Overwritten, Undecided, OtherQueue, Rejected, After };
    PacketLocation at;
    Order order = Order::OtherQueue;
    Effect effect = Effect::OtherQueue;
    std::optional<std::uint64_t> stored;
    bool accepted = false;
};

struct WaitReport {
    enum class Verdict : std::uint8_t { Released, Unknown, Unreleased };
    enum class Route : std::uint8_t { None, Always, HeldAtSubmit, CurrentValue, LabelHistory, RewindTail };
    PacketLocation at;
    MemoryWait wait;
    Verdict verdict = Verdict::Unreleased;
    Route route = Route::None;
    bool crossQueue = false;
    std::string reason;
    std::size_t writerCount = 0;
    std::vector<WriterMatch> writers;
};

struct Analysis {
    static constexpr std::size_t ListedWriters = 8;
    std::vector<CapturedSubmission> submissions;
    std::size_t packets = 0;
    std::size_t stores = 0;
    std::vector<WaitReport> waits;
    std::size_t Count(WaitReport::Verdict verdict) const;
    std::size_t CrossQueue() const;
};

Analysis Analyze(const std::filesystem::path& directory);
int Report(const std::filesystem::path& directory, bool full, std::FILE* output);

}

#endif
