#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWRECORDREPORT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWRECORDREPORT_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace AgcDriver::DriverDetail {

namespace DrawRecords {

enum class Item : std::uint8_t { Answer, Commit, Count };

enum class Field : std::uint8_t {
    ContentKeys,
    VertexLayouts,
    WriteTemplates,
    FragmentOutputs,
    Variants,
    PushConstants,
    Bindings,
    VertexAttributes,
    ImageViews,
    Parameters,
    ReadRanges,
    Templates,
    Pipelines,
    Framebuffers,
    TargetViews,
    Targets,
    PassKeys,
    MovedBuffers,
    CommitPushConstants,
    VertexInputs,
    ShaderStages,
    CommitFragmentOutputs,
    Count,
};

enum class Allowed : std::uint8_t { FragmentOutputsOneSide, TodayNotQueued, CommitRecordMissed, CommitTodayUnrecorded, CommitTemplateRebuilt, Count };

enum class Counter : std::uint8_t {
    CommitHits,
    CommitPassHeld,
    CommitMovedTrusted,
    CommitBuilt,
    CommitVerified,
    CommitFirst,
    CommitNotRecordable,
    CommitTargets,
    CommitTemplate,
    CommitObjects,
    CommitProof,
    CommitMoved,
    CommitUnbuilt,
    CommitDemoted,
    Count,
};

constexpr std::size_t ItemCount = static_cast<std::size_t>(Item::Count);

std::uint64_t VerifyEvery();
void CorruptCommits(bool corrupt);
bool CorruptingCommits();

void NoteChecked(Item item);
void NoteDifference(Field field, std::string_view detail);
void NoteAllowed(Allowed kind);
void Add(Counter counter, std::uint64_t count = 1);
void AddOnCommitter(Counter counter, std::uint64_t count = 1);
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
