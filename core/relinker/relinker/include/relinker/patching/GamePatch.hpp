#pragma once

#include <codegen/CodegenTypes.hpp>
#include <relinker/domain/RelinkResult.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Relinker::GamePatch {

inline constexpr char PatchFileName[] = "relinker-patches.toml";
inline constexpr char ManifestSuffix[] = ".patches.toml";

struct PatchInteger {
    bool Negative = false;
    std::uint64_t Magnitude = 0;
};

struct PatchEntry {
    std::string Target;
    std::uint64_t VirtualAddress = 0;
    std::vector<std::uint8_t> Expected;
    std::vector<std::uint8_t> Value;
    std::string Description;
    std::string Name;
    std::string Type;
    PatchInteger ExpectedNumber;
    PatchInteger ValueNumber;
    std::optional<PatchInteger> Min;
    std::optional<PatchInteger> Max;
    std::optional<PatchInteger> Align;
    std::size_t Line = 0;
};

struct PatchFile {
    std::vector<PatchEntry> Entries;
    std::optional<std::string> MainTarget;
    std::optional<std::uint64_t> LinuxBase;
};

struct PatchedRange {
    std::uint64_t Offset = 0;
    std::uint64_t Length = 0;
    std::uint64_t VirtualAddress = 0;
};

struct AppliedPatch {
    std::string Target;
    std::uint64_t VirtualAddress = 0;
    std::uint64_t Offset = 0;
    std::vector<std::uint8_t> Expected;
    std::vector<std::uint8_t> Value;
    std::string Description;
    std::string Name;
    std::string Type;
    PatchInteger ExpectedNumber;
    PatchInteger ValueNumber;
    std::optional<PatchInteger> Min;
    std::optional<PatchInteger> Max;
    std::optional<PatchInteger> Align;
};

struct PatchedTarget {
    std::vector<AppliedPatch> Applied;
    std::vector<PatchedRange> Ranges;
};

struct WriterRange {
    std::uint64_t Offset = 0;
    std::uint64_t Length = 0;
    std::string Writer;
};

struct WriterSites {
    std::span<const Codegen::Amd64OnlySubstitutionReport> Reports;
    std::span<const Codegen::TrampolineSite> Trampolines;
    std::span<const RelinkPatch> RelinkPatches;
    std::span<const WriterRange> Traps;
    std::span<const std::uint8_t> RelaData;
    std::span<const std::uint8_t> RelaPltData;
};

enum class VerifyStatus { Applicable, Applied, NotChecked, Mismatch };

struct VerifyTarget {
    std::string Name;
    std::vector<std::uint8_t> Bytes;
};

struct VerifyResult {
    std::string Name;
    std::string Target;
    std::uint64_t VirtualAddress = 0;
    std::uint64_t Offset = 0;
    VerifyStatus Status = VerifyStatus::Mismatch;
    std::string Detail;
};

PatchFile ParsePatchFile(std::string_view text);

std::vector<PatchedRange> ResolveRanges(const ElfReader& reader, const std::vector<PatchEntry>& entries);

std::vector<AppliedPatch> ApplyPatches(std::vector<std::uint8_t>& bytes, const std::string& target, const std::vector<PatchEntry>& entries, const std::vector<PatchedRange>& ranges);

PatchedTarget PatchTarget(std::vector<std::uint8_t>& bytes, const std::string& target, const std::vector<PatchEntry>& entries);

void CheckOverlaps(const std::string& target, const std::vector<PatchedRange>& patched, const WriterSites& writers);

void MatchTargets(const std::vector<PatchEntry>& entries, const std::vector<std::string>& targetNames);

std::vector<AppliedPatch> AggregateManifest(const std::vector<std::vector<AppliedPatch>>& records);

std::string WriteManifest(const std::vector<AppliedPatch>& patches, const std::string& mainTarget, const std::string& configPath, std::optional<std::uint64_t> linuxBase = std::nullopt);

std::vector<VerifyResult> VerifyPatches(const PatchFile& file, const std::vector<VerifyTarget>& executables, const std::vector<VerifyTarget>& modules, bool outputMode);

std::string_view StatusName(VerifyStatus status);

std::filesystem::path OutputExecutablePath(const std::filesystem::path& outputPath, const std::string& executableArgument);

std::string Hex(std::span<const std::uint8_t> bytes);

std::string FormatApplied(const AppliedPatch& patch);

std::string ModuleTargetName(const std::filesystem::path& modulePath, const std::filesystem::path& moduleRoot);

}
