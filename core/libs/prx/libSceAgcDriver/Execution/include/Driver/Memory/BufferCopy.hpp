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
bool MatchesTableCopyKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
std::optional<StreamCopy> TableCopyRange(const std::array<std::uint32_t, 8>& table, std::uint32_t groups);
inline constexpr std::array<std::uint32_t, 11> WideTableCopyKernel{0xd7460004u, 0x04010c02u, 0xf40c0100u, 0xfa000000u, 0xbf8cc07fu, 0xe00c2000u, 0x80010004u, 0xbf8c3f70u, 0xe01c2000u, 0x80020004u, 0xbf810000u};
inline constexpr std::uint32_t WideTableCopyFormat = 0x0004bfacu;
bool MatchesWideTableCopyKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
std::optional<StreamCopy> WideTableCopyRange(const std::array<std::uint32_t, 8>& table, std::uint32_t groups);
std::optional<StreamCopy> IndexedCopyRange(const std::vector<std::uint32_t>& userData, std::uint32_t groups, std::uint32_t count, std::uint32_t sourceOffset, std::uint32_t destinationOffset);

}

#endif
