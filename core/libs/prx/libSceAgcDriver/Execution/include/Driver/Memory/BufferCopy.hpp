#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_BUFFERCOPY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_BUFFERCOPY_HPP

#include "Recompiler.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver::DriverDetail {

inline constexpr std::size_t KnownValueMaxBytes = 4096;

extern std::atomic<std::uint64_t> knownValueEntries, knownValueReads, knownValueVerified, knownValueMismatches;

enum CopyPath { CopyCpu = 0, CopyGpu = 1, CopyNotImported = 2, CopyAliased = 3, CopyRecordPending = 4, CopyShape = 5, CopyOverlapping = 6, CopyInaccessible = 7, CopyOverGpuMax = 8, CopyPaths = 9 };

extern std::array<std::atomic<std::uint64_t>, CopyPaths> copyPathCounts;

struct StreamCopy {
    std::uint64_t source = 0;
    std::uint64_t destination = 0;
    std::size_t bytes = 0;
};

bool MatchesStreamCopyKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
std::optional<StreamCopy> StreamCopyRange(const std::vector<std::uint32_t>& userData, std::uint32_t groups);
bool MatchesIndexedCopyKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
std::optional<StreamCopy> IndexedCopyRange(const std::vector<std::uint32_t>& userData, std::uint32_t groups, std::uint32_t count, std::uint32_t sourceOffset, std::uint32_t destinationOffset);

}

#endif
