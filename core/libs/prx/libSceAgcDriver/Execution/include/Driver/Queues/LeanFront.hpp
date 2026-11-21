#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_LEANFRONT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_LEANFRONT_HPP

#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace AgcDriver::DriverDetail {

enum class FrontMode : std::uint8_t { Unlatched, Today, Lean };

namespace LeanFront {

enum class Item : std::uint8_t { Segment, RegisterList, Packet, Draw, SwitchRead, Count };

enum class Field : std::uint8_t {
    SegmentWords,
    ConditionalEnds,
    RewindTail,
    ReleasedTails,
    Flips,
    RenderingWaits,
    LabelWrites,
    HeldWaits,
    PacketTable,
    RegisterLists,
    ListCache,
    PipelineVerdicts,
    FlushRecords,
    FlushSubmits,
    FlushCaps,
    FlushReaps,
    DrawVerdicts,
    DrawKeys,
    DrawIdentity,
    Decodes,
    UserWords,
    AliasKeys,
    Patches,
    WriteTemplates,
    Jobs,
    SlotReuse,
    SwitchValues,
    Count,
};

enum class Allowed : std::uint8_t { CapMoved, EarlyFlushChecks, SwitchesMoved, ListsRewrittenInEpoch, HeldTargetsOnly, Count };

enum class Counter : std::uint8_t {
    LeanSegments,
    TodaySegments,
    ModeChanges,
    SwitchCopies,
    SegmentsLooped,
    PreparedCopiesUsed,
    PreparedCopiesMissed,
    ListCacheHits,
    ListCacheMisses,
    ListCacheInvalidations,
    FlushChecksSkipped,
    KickWakes,
    KickTimeouts,
    SettlesWaited,
    PreparedCopiesRefused,
    LateTailHoists,
    LeanLists,
    ListCacheAdmits,
    ListCacheBypassed,
    ListCacheEvictions,
    RewindWaits,
    ReleasesWithoutKick,
    KicksWithoutWaiter,
    IdleKickWakes,
    FlushCalls,
    PacketTables,
    SlotsCreated,
    SlotsHeld,
    LeanDraws,
    DrawsOnTodaysPath,
    IndirectDrawsOnTodaysPath,
    ShapeRecordsBuilt,
    ShapeRecordsRestamped,
    ShapeRecordsEvicted,
    ShapesRefused,
    RingRoomWaits,
    StoreSwitches,
    PrecheckedLeanDraws,
    DirectListTails,
    Count,
};

constexpr std::size_t ItemCount = static_cast<std::size_t>(Item::Count);

struct Latch {
    FrontMode mode = FrontMode::Today;
    FrontMode taken = FrontMode::Unlatched;
    bool checkSwitches = false;
    bool directDraws = false;
    bool directBuffers = false;
    bool directInputs = false;
    std::uint32_t depth = 0;
    std::uint64_t generation = 0;
    std::uint64_t verifyEvery = 0;
    std::uint64_t directVerifyEvery = 0;
    std::uint64_t directOrdinal = 0;
    std::uint64_t latches = 0;
    Graphics::SwitchSnapshot switches;
    std::array<std::uint64_t, ItemCount> ordinals{};

    bool Lean() const { return mode == FrontMode::Lean; }

    bool Renew(std::uint64_t& stamp) const {
        if (stamp == generation) return false;
        stamp = generation;
        return true;
    }
};

std::uint64_t VerifyEvery();
FrontMode Take(Latch& latch, bool eligible);

inline bool Due(Latch& latch, Item item) {
    if (latch.verifyEvery == 0) return false;
    return ++latch.ordinals[static_cast<std::size_t>(item)] % latch.verifyEvery == 0;
}

bool CheckedSwitch(const Latch& latch, Graphics::RuntimeSwitch id);

inline bool SwitchSet(const Latch& latch, Graphics::RuntimeSwitch id) {
    return latch.checkSwitches ? CheckedSwitch(latch, id) : latch.switches.Set(id);
}

class SegmentNesting {
public:
    explicit SegmentNesting(Latch* owner);
    ~SegmentNesting() {
        if (latch != nullptr) --latch->depth;
    }
    SegmentNesting(const SegmentNesting&) = delete;
    SegmentNesting& operator=(const SegmentNesting&) = delete;

private:
    Latch* latch;
};

void NoteChecked(Item item, bool cacheHit = false);
void NoteDifference(Field field, std::string_view detail);
void NoteAllowed(Allowed kind);
void Add(Counter counter, std::uint64_t count = 1);
void AddOnWorker(Counter counter, std::uint64_t count = 1);
void Report(bool force);

const char* ItemName(Item item);
const char* FieldName(Field field);
std::uint64_t Checked(Item item);
std::uint64_t Differences();
std::uint64_t Differences(Field field);
std::uint64_t AllowedCount(Allowed kind);
std::uint64_t Total(Counter counter);

}

}

#endif
