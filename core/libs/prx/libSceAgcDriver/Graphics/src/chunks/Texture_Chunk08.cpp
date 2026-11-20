#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
// For the declaration of PendingStorageOverlaps, defined below.
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ShaderCacheDirectory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeptUnits.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include <algorithm>
#include <filesystem>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>
#include <limits>
#include <exception>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#if defined(__APPLE__)
#include <unistd.h>
#endif

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
std::vector<std::pair<std::uint64_t, std::uint64_t>> StorageTexture::unitRuns(const std::vector<bool>& units) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (!units[unit]) continue;
        const auto begin = static_cast<std::uint64_t>(unit) * trackedLayerBytes;
        const auto end = begin + layerBytes(unit);
        if (!runs.empty() && runs.back().second == begin) runs.back().second = end;
        else runs.emplace_back(begin, end);
    }
    return runs;
}

std::vector<StorageTexture::SliceWindow> StorageTexture::sliceWindows(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs) const {
    std::vector<SliceWindow> windows;
    const auto elementBytes = BytesPerElement(descriptor.format);
    for (const auto& [runBegin, runEnd] : runs) {
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
                if (!geometry.HasLayer(level, layer)) continue;
                const auto& mip = mips[level];
                const auto block = ThinBlockLayout(MipTileMode(descriptor.tileMode, mip), elementBytes);
                const auto sliceBegin = geometry.GuestOffset(layer, mip);
                const auto sliceEnd = sliceBegin + mip.tiledSize;
                if (runEnd <= sliceBegin || sliceEnd <= runBegin) continue;
                SliceWindow window{layer, level, 0, 0, {}, 0, {}};
                const auto pitch = static_cast<std::uint32_t>(mip.pitchBytes / elementBytes * BlockWidth(descriptor.format));
                if (mip.tail) {
                    // The tail block holds every tail level: each is moved whole.
                    window.tiledBegin = sliceBegin;
                    window.tiledEnd = sliceEnd;
                    window.window = {0, static_cast<std::uint32_t>(mip.tiledSize), 0, 0, mip.linearSize};
                    window.linearBytes = mip.linearSize;
                    VkBufferImageCopy region{};
                    region.bufferRowLength = pitch;
                    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
                    region.imageOffset = {0, 0, geometry.CopyDepth(layer)};
                    region.imageExtent = {mip.width, mip.height, 1u};
                    window.regions.push_back(region);
                    windows.push_back(std::move(window));
                    continue;
                }
                const auto low = std::max(runBegin, sliceBegin) - sliceBegin;
                const auto high = std::min(runEnd, sliceEnd) - sliceBegin;
                Require(low % block[0] == 0 && high % block[0] == 0, "storage image window is not tile-block aligned");
                const auto firstBlock = low / block[0];
                const auto lastBlock = (high - 1) / block[0];
                const auto firstRow = static_cast<std::uint32_t>(firstBlock / mip.blocksPerRow);
                const auto lastRow = static_cast<std::uint32_t>(lastBlock / mip.blocksPerRow);
                const auto rowBegin = firstRow * block[2];
                const auto rowEnd = std::min(mip.height, (lastRow + 1u) * block[2]);
                window.tiledBegin = sliceBegin + low;
                window.tiledEnd = sliceBegin + high;
                window.linearBytes = static_cast<std::uint64_t>(rowEnd - rowBegin) * mip.pitchBytes;
                window.window = {static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(high), static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(rowBegin) * mip.pitchBytes, window.linearBytes};
                // The dispatched rectangle: the block rows, and inside one block row the block span.
                window.window.rowBegin = rowBegin;
                window.window.rowEnd = rowEnd;
                if (firstRow == lastRow) {
                    window.window.columnBegin = static_cast<std::uint32_t>(firstBlock - static_cast<std::uint64_t>(firstRow) * mip.blocksPerRow) * block[1];
                    window.window.columnEnd = std::min(mip.width, static_cast<std::uint32_t>(lastBlock - static_cast<std::uint64_t>(firstRow) * mip.blocksPerRow + 1u) * block[1]);
                }
                // One region per block row; whole rows merge (a run's middle rows are whole).
                for (auto row = firstRow; row <= lastRow; ++row) {
                    const auto rowFirst = std::max<std::uint64_t>(firstBlock, static_cast<std::uint64_t>(row) * mip.blocksPerRow);
                    const auto rowLast = std::min<std::uint64_t>(lastBlock, static_cast<std::uint64_t>(row + 1u) * mip.blocksPerRow - 1u);
                    const auto x0 = static_cast<std::uint32_t>(rowFirst - static_cast<std::uint64_t>(row) * mip.blocksPerRow) * block[1];
                    const auto x1 = std::min(mip.width, static_cast<std::uint32_t>(rowLast - static_cast<std::uint64_t>(row) * mip.blocksPerRow + 1u) * block[1]);
                    const auto y0 = row * block[2];
                    const auto y1 = std::min(mip.height, (row + 1u) * block[2]);
                    if (x0 >= x1 || y0 >= y1) continue;
                    if (!window.regions.empty()) {
                        auto& previous = window.regions.back();
                        const bool wholeRows = previous.imageOffset.x == 0 && previous.imageExtent.width == mip.width && x0 == 0 && x1 == mip.width;
                        if (wholeRows && static_cast<std::uint32_t>(previous.imageOffset.y) + previous.imageExtent.height == y0) {
                            previous.imageExtent.height += y1 - y0;
                            continue;
                        }
                    }
                    VkBufferImageCopy region{};
                    region.bufferOffset = static_cast<std::uint64_t>(y0 - rowBegin) * mip.pitchBytes + static_cast<std::uint64_t>(x0) * elementBytes;
                    region.bufferRowLength = pitch;
                    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
                    region.imageOffset = {static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0), geometry.CopyDepth(layer)};
                    region.imageExtent = {x1 - x0, y1 - y0, 1u};
                    window.regions.push_back(region);
                }
                windows.push_back(std::move(window));
            }
        }
    }
    return windows;
}

std::vector<VkRect2D> StorageTexture::UnitRects(const std::vector<bool>& units) const {
    std::vector<VkRect2D> rects;
    if (units.size() != trackedLayers) return rects;
    const auto runs = unitRuns(units);
    for (const auto& window : sliceWindows(runs)) {
        for (const auto& region : window.regions) rects.push_back({{region.imageOffset.x, region.imageOffset.y}, {region.imageExtent.width, region.imageExtent.height}});
    }
    return rects;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> StorageTexture::uncoveredBytes(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> covered;
    if (!geometry.thick) {
        const auto elementBytes = BytesPerElement(descriptor.format);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
                if (!geometry.HasLayer(level, layer)) continue;
                const auto sliceBegin = geometry.GuestOffset(layer, mips[level]);
                for (const auto& [begin, end] : CoveredMipBytes(descriptor.tileMode, elementBytes, mips[level])) covered.emplace_back(sliceBegin + begin, sliceBegin + end);
            }
        }
        std::sort(covered.begin(), covered.end());
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> sorted(runs.begin(), runs.end());
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> uncovered;
    const auto add = [&](std::uint64_t begin, std::uint64_t end) {
        if (begin >= end) return;
        if (!uncovered.empty() && uncovered.back().second >= begin) uncovered.back().second = std::max(uncovered.back().second, end);
        else uncovered.emplace_back(begin, end);
    };
    std::size_t next = 0;
    for (const auto& [runBegin, runEnd] : sorted) {
        while (next < covered.size() && covered[next].second <= runBegin) ++next;
        auto at = runBegin;
        for (auto i = next; i < covered.size() && covered[i].first < runEnd; ++i) {
            add(at, std::min(covered[i].first, runEnd));
            at = std::max(at, covered[i].second);
        }
        add(at, runEnd);
    }
    return uncovered;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> StorageTexture::coveredBytes(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs) const {
    const auto uncovered = uncoveredBytes(runs);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> sorted(runs.begin(), runs.end());
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> covered;
    std::size_t next = 0;
    for (const auto& [runBegin, runEnd] : sorted) {
        while (next < uncovered.size() && uncovered[next].second <= runBegin) ++next;
        auto at = runBegin;
        for (auto i = next; i < uncovered.size() && uncovered[i].first < runEnd; ++i) {
            if (uncovered[i].first > at) covered.emplace_back(at, uncovered[i].first);
            at = std::max(at, uncovered[i].second);
        }
        if (at < runEnd) covered.emplace_back(at, runEnd);
    }
    return covered;
}

StorageTexture::PaddingSeeds StorageTexture::paddingSeeds(const HostImport& import, std::vector<CopiedBytes> copied) const {
    PaddingSeeds seeds;
    std::sort(copied.begin(), copied.end(), [](const CopiedBytes& a, const CopiedBytes& b) { return a.begin < b.begin; });
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    for (const auto& range : copied) runs.emplace_back(range.begin, range.end);
    const auto uncovered = uncoveredBytes(runs);
    if (uncovered.empty()) return seeds;
    std::vector<CopiedBytes> pieces;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pieceRuns;
    std::size_t next = 0;
    for (const auto& [begin, end] : uncovered) {
        while (next < copied.size() && copied[next].end <= begin) ++next;
        for (auto i = next; i < copied.size() && copied[i].begin < end; ++i) {
            const auto from = std::max(begin, copied[i].begin);
            const auto to = std::min(end, copied[i].end);
            if (from >= to) continue;
            pieces.push_back({from, to, copied[i].scratch + (from - copied[i].begin)});
            pieceRuns.emplace_back(from, to);
        }
    }
    if (pieces.empty()) return seeds;
    const auto sources = ShadowSources(context, import, descriptor.baseAddress, pieceRuns, {}, false);
    std::size_t first = 0;
    for (const auto& source : sources) {
        while (first < pieces.size() && pieces[first].end <= source.begin) ++first;
        for (auto i = first; i < pieces.size() && pieces[i].begin < source.end; ++i) {
            const auto from = std::max(source.begin, pieces[i].begin);
            const auto to = std::min(source.end, pieces[i].end);
            if (from >= to) continue;
            auto target = std::find_if(seeds.copies.begin(), seeds.copies.end(), [&](const auto& entry) { return entry.first == source.buffer; });
            if (target == seeds.copies.end()) target = seeds.copies.insert(seeds.copies.end(), {source.buffer, {}});
            target->second.push_back({source.offset + (from - source.begin), pieces[i].scratch + (from - pieces[i].begin), to - from});
            if (!source.shadow) {
                const auto guestBegin = descriptor.baseAddress + from;
                if (!seeds.importReads.empty() && seeds.importReads.back().second == guestBegin) seeds.importReads.back().second = descriptor.baseAddress + to;
                else seeds.importReads.emplace_back(guestBegin, descriptor.baseAddress + to);
            }
        }
        if (source.shadow && std::find(seeds.slabs.begin(), seeds.slabs.end(), source.slab) == seeds.slabs.end()) seeds.slabs.push_back(source.slab);
    }
    return seeds;
}

StorageTexture::PaddingSeeds StorageTexture::paddingSeeds(std::span<const ImportSegment> segments, const std::vector<CopiedBytes>& copied) const {
    if (segments.size() == 1) return paddingSeeds(*segments.front().import, copied);
    PaddingSeeds merged;
    for (const auto& segment : segments) {
        const auto segmentBegin = segment.begin - descriptor.baseAddress;
        const auto segmentEnd = segment.end - descriptor.baseAddress;
        std::vector<CopiedBytes> inside;
        for (const auto& range : copied) {
            const auto begin = std::max(range.begin, segmentBegin);
            const auto end = std::min(range.end, segmentEnd);
            if (begin < end) inside.push_back({begin, end, range.scratch + (begin - range.begin)});
        }
        if (inside.empty()) continue;
        auto seeds = paddingSeeds(*segment.import, std::move(inside));
        for (auto& entry : seeds.copies) merged.copies.push_back(std::move(entry));
        for (auto& slab : seeds.slabs) merged.slabs.push_back(std::move(slab));
        merged.importReads.insert(merged.importReads.end(), seeds.importReads.begin(), seeds.importReads.end());
    }
    return merged;
}

std::vector<StorageTexture::ImportSegment> StorageTexture::importSegments() const {
    const auto begin = descriptor.baseAddress;
    const auto end = begin + guestBytes;
    const bool unitAligned = blockUnits && UnitShadowEnabled();
    const auto samePiece = [](const RegisteredPiece& a, const RegisteredPiece& b) { return a.address == b.address && a.bytes == b.bytes && a.writable == b.writable && a.gpu == b.gpu; };
    const bool memoized = !RuntimeSwitches::Set(RuntimeSwitch::NoImportSegmentMemo);
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (memoized) {
        std::lock_guard lock(segmentMemoMutex);
        if (segmentMemo.valid && segmentMemo.generation == generation) {
            std::vector<const HostImport*> imports;
            imports.reserve(segmentMemo.segments.size());
            for (const auto& segment : segmentMemo.segments) imports.push_back(segment.import);
            if (TouchHostImportsIfUnchanged(context, HostImportsProof{segmentMemo.device, segmentMemo.importEpoch, segmentMemo.importRefreshed, 0}, imports)) {
                return segmentMemo.segments;
            }
        }
    }
    const auto remember = [&](const HostImportsProof& proof, std::vector<ImportSegment> segments) {
        if (memoized && proof.device != VK_NULL_HANDLE && GuestAllocations::GuestAllocationsGeneration_nid_postfix() == generation) {
            std::lock_guard lock(segmentMemoMutex);
            segmentMemo = {true, generation, proof.device, proof.epoch, proof.refreshedGeneration, segments};
        }
        return segments;
    };
    for (std::uint32_t attempt = 0; attempt < 2; ++attempt) {
        std::vector<RegisteredPiece> pieces;
        const auto before = HostImportsIdentity(context);
        if (!RegisteredReadableCovers(begin, static_cast<std::size_t>(guestBytes), &pieces) || pieces.size() < 2) return remember(before, {});
        if (std::any_of(pieces.begin(), pieces.end(), [&](const RegisteredPiece& piece) { return piece.writable != pieces.front().writable || piece.gpu != pieces.front().gpu; })) return remember(before, {});
        std::vector<ImportSegment> segments;
        const auto proof = HostImportsIdentity(context);
        for (const auto& piece : pieces) {
            const auto from = std::max(piece.address, begin);
            const auto to = std::min(piece.address + piece.bytes, end);
            const auto* import = HostImportFor(context, from, static_cast<std::size_t>(to - from), ImportRequester::Segment);
            if (import == nullptr) return {};
            if (!segments.empty() && segments.back().import == import) segments.back().end = to;
            else if (!segments.empty() && unitAligned && from % 65536 != 0) return {};
            else segments.push_back({import, from, to});
        }
        std::vector<RegisteredPiece> still;
        if (HostImportsUnchanged(context, proof) && RegisteredReadableCovers(begin, static_cast<std::size_t>(guestBytes), &still) && std::equal(pieces.begin(), pieces.end(), still.begin(), still.end(), samePiece)) return remember(proof, std::move(segments));
    }
    return {};
}

void StorageTexture::recordDirectUploadBarrier(VkCommandBuffer commands, bool discard) {
    const VkMemoryBarrier sourceReady{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
    VkImageMemoryBarrier toWrite{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toWrite.srcAccessMask = discard ? 0u : VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toWrite.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toWrite.oldLayout = discard ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    toWrite.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toWrite.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toWrite.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toWrite.image = image;
    toWrite.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &sourceReady, 0, nullptr, 1, &toWrite);
}

void StorageTexture::recordDirectUploadDone(VkCommandBuffer commands) {
    VkImageMemoryBarrier written{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    written.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    written.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    written.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    written.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    written.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    written.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    written.image = image;
    written.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &written);
}

std::uint64_t StorageTexture::uploadWindows(std::span<const ImportSegment> segments, std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, bool discard) {
    const auto elementBytes = BytesPerElement(descriptor.format);
    // Each run's pieces by source (a unit shadow's slab while fresh, else the import), the tail
    // blocks moved whole by one window each; the windows are built per piece.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> tailBlocks;
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            const auto& mip = mips[level];
            if (!mip.tail) continue;
            const auto sliceBegin = geometry.GuestOffset(layer, mip);
            if (tailBlocks.empty() || tailBlocks.back().first != sliceBegin) tailBlocks.emplace_back(sliceBegin, sliceBegin + mip.tiledSize);
        }
    }
    const bool gather = segments.size() > 1;
    std::vector<ShadowRun> sources, gathered;
    if (!gather) {
        sources = ShadowSources(context, *segments.front().import, descriptor.baseAddress, runs, tailBlocks);
    } else {
        for (const auto& segment : segments) {
            std::vector<std::pair<std::uint64_t, std::uint64_t>> clipped;
            const auto from = segment.begin - descriptor.baseAddress;
            const auto to = segment.end - descriptor.baseAddress;
            for (const auto& [begin, end] : runs) {
                if (std::max(begin, from) < std::min(end, to)) clipped.emplace_back(std::max(begin, from), std::min(end, to));
            }
            const auto found = ShadowSources(context, *segment.import, descriptor.baseAddress, clipped, tailBlocks);
            gathered.insert(gathered.end(), found.begin(), found.end());
        }
    }
    const auto& read = gather ? gathered : sources;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pieces;
    bool allShadow = true;
    for (const auto& source : read) {
        pieces.emplace_back(source.begin, source.end);
        if (!source.shadow) allShadow = false;
    }
    if (allShadow && !read.empty() && (std::strcmp(uploadReason, "flushed") == 0 || std::strcmp(uploadReason, "store") == 0)) uploadReason = "shadow";
    auto windows = gather ? sliceWindows(runs) : sliceWindows(pieces);
    Require(!windows.empty(), "storage image upload selects no unit");
    std::shared_ptr<DeviceBuffer> gatherBuffer;
    std::vector<std::pair<VkBuffer, std::vector<VkBufferCopy>>> gatherCopies;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> gatherReads;
    if (gather) {
        std::uint64_t gatherBytes = 0;
        for (const auto& window : windows) {
            if (std::any_of(sources.begin(), sources.end(), [&](const ShadowRun& source) { return source.begin == window.tiledBegin && source.end == window.tiledEnd; })) continue;
            sources.push_back({window.tiledBegin, window.tiledEnd, VK_NULL_HANDLE, gatherBytes, true, nullptr});
            gatherBytes += (window.tiledEnd - window.tiledBegin + 255) & ~std::uint64_t{255};
        }
        gatherBuffer = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(gatherBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto copy = [&](VkBuffer buffer, VkBufferCopy region) {
            const auto group = std::find_if(gatherCopies.begin(), gatherCopies.end(), [&](const auto& entry) { return entry.first == buffer; });
            if (group != gatherCopies.end()) group->second.push_back(region);
            else gatherCopies.push_back({buffer, {region}});
        };
        for (auto& source : sources) {
            source.buffer = gatherBuffer->Handle();
            for (auto cursor = source.begin; cursor < source.end;) {
                const auto piece = std::find_if(gathered.begin(), gathered.end(), [&](const ShadowRun& run) { return run.begin <= cursor && cursor < run.end; });
                if (piece != gathered.end()) {
                    const auto end = std::min(source.end, piece->end);
                    copy(piece->buffer, {piece->offset + (cursor - piece->begin), source.offset + (cursor - source.begin), end - cursor});
                    cursor = end;
                    continue;
                }
                const auto address = descriptor.baseAddress + cursor;
                const auto segment = std::find_if(segments.begin(), segments.end(), [&](const ImportSegment& candidate) { return candidate.begin <= address && address < candidate.end; });
                Require(segment != segments.end(), "storage image upload window lies outside its import segments");
                auto end = std::min(source.end, segment->end - descriptor.baseAddress);
                for (const auto& run : gathered) {
                    if (run.begin > cursor) end = std::min(end, run.begin);
                }
                copy(segment->import->buffer, {HostImportOffset(*segment->import, address), source.offset + (cursor - source.begin), end - cursor});
                gatherReads.emplace_back(address, descriptor.baseAddress + end);
                cursor = end;
            }
        }
    }
    const auto sourceOf = [&](const SliceWindow& window) -> const ShadowRun& {
        for (const auto& source : sources) {
            if (source.begin <= window.tiledBegin && window.tiledBegin < source.end) return source;
        }
        throw std::runtime_error("AGC graphics: storage image window has no source");
    };
    // One linear buffer for every window, each at a 256-byte position (the detiler's descriptor
    // offsets are aligned by the dispatch).
    std::vector<std::uint64_t> positions;
    std::uint64_t linearTotal = 0, uploadedBytes = 0;
    for (const auto& window : windows) {
        positions.push_back(linearTotal);
        linearTotal += (window.linearBytes + 255) & ~std::uint64_t{255};
    }
    for (const auto& [begin, end] : runs) uploadedBytes += end - begin;
    const bool direct = singlePass();
    auto linear = direct ? nullptr : std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearTotal), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    detiler.BeginBatch();
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    auto timing = Recorder::NoTiming;
    if (recorder != nullptr) {
        // The import pieces are read in place when the batch runs; the slab pieces are the
        // device's own.
        std::vector<std::pair<std::uint64_t, std::uint64_t>> reads = gatherReads;
        for (const auto& source : read) {
            if (!source.shadow) reads.emplace_back(descriptor.baseAddress + source.begin, descriptor.baseAddress + source.end);
        }
        // A queued label store in a run lands before the detile reads it.
        for (const auto& [begin, end] : reads) recorder->FlushStoresOverlapping(begin, static_cast<std::size_t>(end - begin));
        commands = recorder->Commands();
        timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageUpload);
        if (linear) recorder->Keep(linear, linear->Size());
        if (gatherBuffer) recorder->Keep(gatherBuffer, gatherBuffer->Size());
        for (const auto& source : read) {
            if (source.slab != nullptr) recorder->Keep(source.slab);
        }
        if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
        if (!reads.empty()) recorder->NotePendingReads(reads, Recorder::ReadKind::StorageUpload);
        Recorder::CountBarriers(Recorder::CommandClass::StorageUpload, (direct ? 2 : 3) + (gather ? 2 : 0));
        if (Recorder::BarrierValidate()) {
            const std::pair<VkImage, bool> written{image, true};
            recorder->NoteAccess(Recorder::CommandClass::StorageUpload, Recorder::Access{reads, {}, std::span(&written, 1), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT});
        }
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    const auto& import = *segments.front().import;
    const auto importOffset = HostImportOffset(import, descriptor.baseAddress);
    if (gather) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        for (const auto& [buffer, copies] : gatherCopies) context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, buffer, gatherBuffer->Handle(), static_cast<std::uint32_t>(copies.size()), copies.data());
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    }
    if (direct) {
        recordDirectUploadBarrier(commands, discard);
        for (const auto& window : windows) {
            const auto& mip = mips[window.level];
            const auto& source = sourceOf(window);
            auto detile = window.window;
            detile.pipeBankXor = descriptor.pipeBankXor;
            const auto view = elementLayerView(window.level, window.layer);
            if (source.shadow) {
                detiler.DispatchImage(commands, descriptor.tileMode, elementBytes, source.buffer, source.offset + (window.tiledBegin - source.begin), view, mip, false, window.layer, detile);
            } else {
                detile.tiledBase = 0;
                detiler.DispatchImage(commands, descriptor.tileMode, elementBytes, import.buffer, importOffset + geometry.GuestOffset(window.layer, mip), view, mip, false, window.layer, detile);
            }
        }
        recordDirectUploadDone(commands);
        singlePassMoves.fetch_add(1, std::memory_order_relaxed);
        if (batch) batch->SubmitAndWait();
        else recorder->EndGpuTiming(timing, uploadedBytes);
        return uploadedBytes;
    }
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    std::vector<VkBufferImageCopy> regions;
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        const auto& mip = mips[window.level];
        const auto& source = sourceOf(window);
        auto detile = window.window;
        detile.pipeBankXor = descriptor.pipeBankXor;
        if (source.shadow) {
            // The slab holds the piece from the window's first tiled byte: the window's tiled base
            // is its range begin (sliceWindows builds it so), read from the piece's slab offset.
            detiler.Dispatch(commands, descriptor.tileMode, elementBytes, source.buffer, source.offset + (window.tiledBegin - source.begin), linear->Handle(), positions[i], mip, false, window.layer, false, detile);
        } else {
            // The import holds the whole mip: the window's tiled base is the mip's.
            detile.tiledBase = 0;
            detiler.Dispatch(commands, descriptor.tileMode, elementBytes, import.buffer, importOffset + geometry.GuestOffset(window.layer, mip), linear->Handle(), positions[i], mip, false, window.layer, false, detile);
        }
        for (auto region : window.regions) {
            region.bufferOffset += positions[i];
            regions.push_back(region);
        }
    }
    const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.srcAccessMask = discard ? 0u : VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    // The other blocks keep their content (a new image, every block of which is written, has none).
    toTransfer.oldLayout = discard ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image;
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearRead, 1, &toTransfer);
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
    VkImageMemoryBarrier toGeneral = toTransfer;
    toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    if (batch) batch->SubmitAndWait();
    else recorder->EndGpuTiming(timing, uploadedBytes);
    return uploadedBytes;
}

}
}
}
}
}
}
}
}
}
