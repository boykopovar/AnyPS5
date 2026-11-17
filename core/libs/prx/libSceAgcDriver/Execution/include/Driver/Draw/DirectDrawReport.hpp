#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DIRECTDRAWREPORT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DIRECTDRAWREPORT_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRecordReport.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace AgcDriver::DriverDetail {

namespace DirectDraws {

enum class Counter : std::uint8_t {
    Draws,
    Direct,
    FallbackShape,
    FallbackHeap,
    FallbackInvocation,
    FallbackSampleRetry,
    FallbackWalk,
    FallbackTornReads,
    FallbackTables,
    FallbackPoisoned,
    FallbackNoPatch,
    FallbackAddressed,
    FallbackSide,
    FallbackPush,
    FallbackUnsettled,
    FallbackViews,
    FallbackMemoryChanged,
    FallbackLabels,
    FallbackOverflow,
    FallbackClassNew,
    FallbackClassRefused,
    FallbackVerify,
    ClassesBuilt,
    RefusedNotQueued,
    RefusedWrites,
    RefusedVerify,
    RefusedDiffers,
    ClassesEvicted,
    StagesBuilt,
    StagesEvicted,
    InvocationsRebuilt,
    WalksRetried,
    VerifyChecked,
    VerifyDiffering,
    CommitHits,
    CommitFirst,
    CommitMisses,
    CommitDemoted,
    HoldOwn,
    HoldShared,
    HoldFirst,
    HoldPass,
    HoldEpoch,
    HoldProof,
    HoldPending,
    HoldDepth,
    HoldTarget,
    BuffersPrepared,
    BuffersNoPlan,
    BuffersClassification,
    BuffersInaccessible,
    BuffersUploadLimit,
    BuffersBudget,
    BuffersUnsettled,
    BuffersBlobBytes,
    BuffersUsed,
    BuffersPlanDiffers,
    BuffersMovedHazard,
    BuffersUnmovedHazard,
    BuffersDepthGuards,
    BuffersDropped,
    BuffersVerifyChecked,
    BuffersVerifyDiffering,
    BuffersMemoryChanged,
    BuffersUnwatched,
    InputsPrepared,
    InputsNoRecipe,
    InputsShape,
    InputsNotInPlace,
    InputsInaccessible,
    InputsThrown,
    InputsUnsettled,
    InputsUsed,
    InputsRechecked,
    InputsImportChanged,
    InputsCommitShape,
    InputsRefused,
    InputsDropped,
    InputsVerifyChecked,
    InputsVerifyDiffering,
    SettleRangesBefore,
    SettleRangesAfter,
    MergedSettlesPassed,
    MergedSettlesRefused,
    MergedSettlesRecovered,
    VerifyMergedChecked,
    VerifyMergedDiffering,
    VerifyMergedMemoryChanged,
    VerifyMergedUnwatched,
    MergedSettlesRechecked,
    MergedSettlesRecheckRefused,
    Count,
};

enum class Step : std::uint8_t { Request, Invocation, Walk, Key, Side, Class, Checks, Buffers, Inputs, HandOff, Count };

constexpr std::uint64_t StepSampleEvery = 64;

std::uint64_t VerifyEvery();
std::uint64_t BufferVerifyEvery();
std::uint64_t InputVerifyEvery();

inline bool Due(LeanFront::Latch& latch) {
    if (latch.directVerifyEvery == 0) return false;
    return ++latch.directOrdinal % latch.directVerifyEvery == 0;
}

void Add(Counter counter, std::uint64_t count = 1);
void AddStep(Step step, std::uint64_t nanoseconds);
void NoteClasses(std::size_t known, std::size_t bytes);
void NoteStages(std::size_t known);
void NoteDifference(DrawRecords::Field field, std::string_view detail);
void NoteBuildDifference(DrawRecords::Field field, std::string_view detail);
void NoteBufferDifference(std::string_view what, std::string_view detail);
void NoteAllowed(DrawRecords::Allowed kind);
void Report(bool force);

const char* CounterName(Counter counter);
std::uint64_t Total(Counter counter);
std::uint64_t Differences(DrawRecords::Field field);
std::uint64_t BuildDifferences(DrawRecords::Field field);
std::uint64_t AllowedCount(DrawRecords::Allowed kind);

}

}

#endif
