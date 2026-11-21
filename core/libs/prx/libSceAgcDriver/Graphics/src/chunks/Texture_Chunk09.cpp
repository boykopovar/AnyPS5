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
std::uint64_t StorageTexture::writeBackWindows(std::span<const ImportSegment> segments, std::span<const std::pair<std::uint64_t, std::uint64_t>> keep, std::uint64_t firstStored, std::uint64_t lastStored, std::vector<ShadowedRange>& shadowed, std::vector<std::pair<std::uint64_t, std::uint64_t>>& imported, Buffer* save) {
    const auto elementBytes = BytesPerElement(descriptor.format);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    std::uint64_t storedBytes = 0;
    for (const auto& [from, to] : keep) {
        runs.emplace_back(from - descriptor.baseAddress, to - descriptor.baseAddress);
        storedBytes += to - from;
    }
    if (runs.empty()) return 0;
    auto windows = sliceWindows(runs);
    if (windows.empty()) return 0;
    std::vector<std::uint64_t> linearPositions, scratchPositions;
    std::uint64_t linearTotal = 0, scratchTotal = 0;
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        linearPositions.push_back(linearTotal);
        linearTotal += (window.linearBytes + 255) & ~std::uint64_t{255};
        std::size_t shared = 0;
        while (shared < i && (windows[shared].tiledBegin != window.tiledBegin || windows[shared].tiledEnd != window.tiledEnd)) ++shared;
        if (shared < i) {
            scratchPositions.push_back(scratchPositions[shared]);
            continue;
        }
        scratchPositions.push_back(scratchTotal);
        scratchTotal += (window.tiledEnd - window.tiledBegin + 255) & ~std::uint64_t{255};
    }
    struct SlabPieces {
        std::shared_ptr<ShadowSlab> slab;
        std::shared_ptr<ShadowSlabPin> pin;
        std::vector<VkBufferCopy> copies;
    };
    struct SeedUnit {
        std::uint64_t begin;
        std::uint64_t end;
        std::shared_ptr<ShadowSlab> slab;
        const HostImport* import;
    };
    std::vector<SlabPieces> slabPieces;
    std::vector<std::vector<VkBufferCopy>> importCopies(segments.size());
    std::vector<SeedUnit> seeds;
    const bool spanWatched = GuestMemory::Watched(firstStored, static_cast<std::size_t>(lastStored - firstStored));
    struct AddedPiece {
        std::size_t pieces;
        std::size_t copy;
        std::uint64_t begin;
        std::uint64_t end;
        bool slab;
        bool seeded;
    };
    std::vector<AddedPiece> added;
    const auto addPiece = [&](std::uint64_t scratchOffset, std::uint64_t guestBegin, std::uint64_t guestEnd) {
        while (guestBegin < guestEnd) {
            std::size_t segment = 0;
            while (segment + 1 < segments.size() && segments[segment + 1].begin <= guestBegin) ++segment;
            Require(segments[segment].begin <= guestBegin && guestBegin < segments[segment].end, "storage image write-back piece lies outside its import segments");
            const auto& import = *segments[segment].import;
            auto pieceEnd = std::min(guestEnd, segments[segment].end);
            if (UnitShadowEnabled()) pieceEnd = std::min(pieceEnd, SlabBoundary(import, guestBegin));
            const auto destination = spanWatched && guestBytes > SmallStoreBytes ? ShadowDestinationFor(context, import, guestBegin, pieceEnd) : std::nullopt;
            if (destination.has_value()) {
                if (slabPieces.empty() || slabPieces.back().slab != destination->slab) slabPieces.push_back({destination->slab, destination->pin, {}});
                slabPieces.back().copies.push_back({scratchOffset, destination->offset, pieceEnd - guestBegin});
                for (const auto& [seedBegin, seedEnd] : destination->seedUnits) {
                    if (std::none_of(seeds.begin(), seeds.end(), [&](const SeedUnit& seed) { return seed.begin == seedBegin; })) seeds.push_back({seedBegin, seedEnd, destination->slab, &import});
                }
                added.push_back({slabPieces.size() - 1, slabPieces.back().copies.size() - 1, guestBegin - descriptor.baseAddress, pieceEnd - descriptor.baseAddress, true, !destination->seedUnits.empty()});
                if (!shadowed.empty() && shadowed.back().end == guestBegin && shadowed.back().slab == destination->slab) shadowed.back().end = pieceEnd;
                else shadowed.push_back({guestBegin, pieceEnd, destination->slab});
            } else {
                added.push_back({0, 0, guestBegin - descriptor.baseAddress, pieceEnd - descriptor.baseAddress, false, false});
                importCopies[segment].push_back({scratchOffset, HostImportOffset(import, guestBegin), pieceEnd - guestBegin});
                if (!imported.empty() && imported.back().second == guestBegin) imported.back().second = pieceEnd;
                else imported.emplace_back(guestBegin, pieceEnd);
            }
            scratchOffset += pieceEnd - guestBegin;
            guestBegin = pieceEnd;
        }
    };
    struct SlabRetile {
        std::size_t window;
        std::size_t first;
        std::size_t last;
    };
    std::vector<SlabRetile> slabRetiled;
    const auto blockBytes = ThinBlockLayout(descriptor.tileMode, elementBytes)[0];
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        if (i != 0 && scratchPositions[i] == scratchPositions[i - 1]) continue;
        const auto first = added.size();
        std::size_t kept = 0;
        bool whole = false;
        for (const auto& [from, to] : runs) {
            const auto begin = std::max(from, window.tiledBegin);
            const auto end = std::min(to, window.tiledEnd);
            if (begin >= end) continue;
            ++kept;
            addPiece(scratchPositions[i] + (begin - window.tiledBegin), descriptor.baseAddress + begin, descriptor.baseAddress + end);
            whole = begin == window.tiledBegin && end == window.tiledEnd;
        }
        if (save != nullptr || geometry.thick || mips[window.level].tail || std::count(scratchPositions.begin(), scratchPositions.end(), scratchPositions[i]) != 1 || kept != 1 || !whole) continue;
        const auto last = added.size();
        const auto parts = std::span<const AddedPiece>(added).subspan(first, last - first);
        const auto sliceBegin = window.tiledBegin - window.window.rangeBegin;
        const auto intoSlabs = std::all_of(parts.begin(), parts.end(), [&](const AddedPiece& part) {
            if (!part.slab || part.seeded) return false;
            const auto& copy = slabPieces[part.pieces].copies[part.copy];
            return copy.dstOffset % 4 == 0 && copy.size % 4 == 0 && (part.begin - sliceBegin) % blockBytes == 0 && (part.end - sliceBegin) % blockBytes == 0;
        });
        if (intoSlabs) slabRetiled.push_back({i, first, last});
    }
    std::sort(seeds.begin(), seeds.end(), [](const SeedUnit& a, const SeedUnit& b) { return a.begin < b.begin; });
    std::vector<CopiedBytes> copied;
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        if (i != 0 && scratchPositions[i] == scratchPositions[i - 1]) continue;
        for (const auto& [from, to] : runs) {
            const auto begin = std::max(from, window.tiledBegin);
            const auto end = std::min(to, window.tiledEnd);
            if (begin < end) copied.push_back({begin, end, scratchPositions[i] + (begin - window.tiledBegin)});
        }
    }
    std::vector<VkBufferCopy> saves;
    if (save != nullptr) {
        for (const auto& piece : copied) saves.push_back({piece.scratch, piece.begin, piece.end - piece.begin});
    }
    auto padding = paddingSeeds(segments, copied);
    struct DirectPiece {
        VkBuffer slab;
        VkDeviceSize offset;
        DetileWindow window;
        std::uint64_t linearOffset;
    };
    std::vector<DirectPiece> directPieces;
    std::vector<std::pair<std::size_t, std::size_t>> directRanges(windows.size(), {0, 0});
    struct HoleCopies {
        VkBuffer source;
        VkBuffer slab;
        std::vector<VkBufferCopy> copies;
    };
    std::vector<HoleCopies> holeCopies;
    if (!slabRetiled.empty()) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        for (const auto& retiled : slabRetiled) ranges.emplace_back(windows[retiled.window].tiledBegin, windows[retiled.window].tiledEnd);
        const auto holes = uncoveredBytes(ranges);
        for (const auto& retiled : slabRetiled) {
            const auto& window = windows[retiled.window];
            const auto parts = std::span<const AddedPiece>(added).subspan(retiled.first, retiled.last - retiled.first);
            const auto position = scratchPositions[retiled.window];
            const auto length = window.tiledEnd - window.tiledBegin;
            const auto inside = [&](const VkBufferCopy& hole) { return hole.size != 0 && hole.dstOffset >= position && hole.dstOffset + hole.size <= position + length; };
            const auto partCopy = [&](const AddedPiece& part) -> VkBufferCopy& { return slabPieces[part.pieces].copies[part.copy]; };
            const auto partSlab = [&](const AddedPiece& part) { return slabPieces[part.pieces].slab->buffer; };
            std::uint64_t holeBytes = 0, sourcedBytes = 0;
            for (const auto& [first, last] : holes) {
                const auto begin = std::max(first, window.tiledBegin);
                const auto end = std::min(last, window.tiledEnd);
                if (begin < end) holeBytes += end - begin;
            }
            bool sameSlabMoved = false;
            for (const auto& [source, copies] : padding.copies) {
                for (const auto& hole : copies) {
                    if (!inside(hole)) continue;
                    sourcedBytes += hole.size;
                    const auto holeBegin = window.tiledBegin + (hole.dstOffset - position);
                    for (const auto& part : parts) {
                        const auto from = std::max(holeBegin, part.begin);
                        const auto to = std::min(holeBegin + hole.size, part.end);
                        if (from < to && source == partSlab(part) && hole.srcOffset + (from - holeBegin) != partCopy(part).dstOffset + (from - part.begin)) sameSlabMoved = true;
                    }
                }
            }
            if (sourcedBytes != holeBytes || sameSlabMoved) continue;
            for (auto& [source, copies] : padding.copies) {
                for (auto& hole : copies) {
                    if (!inside(hole)) continue;
                    const auto holeBegin = window.tiledBegin + (hole.dstOffset - position);
                    for (const auto& part : parts) {
                        const auto from = std::max(holeBegin, part.begin);
                        const auto to = std::min(holeBegin + hole.size, part.end);
                        if (from >= to || source == partSlab(part)) continue;
                        const auto slab = partSlab(part);
                        auto entry = std::find_if(holeCopies.begin(), holeCopies.end(), [&](const HoleCopies& entry) { return entry.source == source && entry.slab == slab; });
                        if (entry == holeCopies.end()) entry = holeCopies.insert(holeCopies.end(), {source, slab, {}});
                        entry->copies.push_back({hole.srcOffset + (from - holeBegin), partCopy(part).dstOffset + (from - part.begin), to - from});
                    }
                    hole.size = 0;
                }
            }
            directRanges[retiled.window] = {directPieces.size(), directPieces.size() + parts.size()};
            for (const auto& part : parts) {
                auto& copy = partCopy(part);
                if (parts.size() == 1) {
                    directPieces.push_back({partSlab(part), copy.dstOffset, window.window, 0});
                } else {
                    const std::pair<std::uint64_t, std::uint64_t> run{part.begin, part.end};
                    const auto narrowed = sliceWindows(std::span(&run, 1));
                    Require(narrowed.size() == 1, "a storage write-back slab piece is not one window");
                    directPieces.push_back({partSlab(part), copy.dstOffset, narrowed.front().window, narrowed.front().window.linearBase - window.window.linearBase});
                }
                copy.size = 0;
            }
            slabRetiles.fetch_add(1, std::memory_order_relaxed);
        }
        for (auto& pieces : slabPieces) std::erase_if(pieces.copies, [](const VkBufferCopy& copy) { return copy.size == 0; });
        for (auto& entry : padding.copies) std::erase_if(entry.second, [](const VkBufferCopy& copy) { return copy.size == 0; });
        std::erase_if(padding.copies, [](const auto& entry) { return entry.second.empty(); });
    }
    auto flushBegin = firstStored;
    auto flushEnd = lastStored;
    for (const auto& seed : seeds) {
        flushBegin = std::min(flushBegin, seed.begin);
        flushEnd = std::max(flushEnd, seed.end);
    }
    const bool direct = singlePass();
    auto linear = direct ? nullptr : std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearTotal), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const bool scratchUsed = std::any_of(directRanges.begin(), directRanges.end(), [](const auto& range) { return range.first == range.second; }) || std::any_of(slabPieces.begin(), slabPieces.end(), [](const SlabPieces& pieces) { return !pieces.copies.empty(); }) || std::any_of(importCopies.begin(), importCopies.end(), [](const auto& copies) { return !copies.empty(); }) || !saves.empty() || !padding.copies.empty() || !seeds.empty();
    auto tiledScratch = scratchUsed ? std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(scratchTotal), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT) : nullptr;
    detiler.BeginBatch();
    auto* recorder = recorderFor(context);
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkAccessFlags covered = 0;
    auto timing = Recorder::NoTiming;
    if (recorder != nullptr) {
        recorder->FlushKeyStoresOverlapping(flushBegin, static_cast<std::size_t>(flushEnd - flushBegin));
        recorder->FlushStoresOverlapping(flushBegin, static_cast<std::size_t>(flushEnd - flushBegin));
        commands = recorder->Commands(&covered);
        timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageWriteBack);
        if (linear) recorder->Keep(linear, linear->Size());
        if (tiledScratch) recorder->Keep(tiledScratch, tiledScratch->Size());
        for (const auto& pieces : slabPieces) recorder->Keep(pieces.slab);
        for (const auto& slab : padding.slabs) recorder->Keep(slab);
        if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
        Recorder::CountBarriers(Recorder::CommandClass::StorageWriteBack, (direct ? 3 : 4) - (tiledScratch ? 0 : 1));
        if (Recorder::BarrierValidate()) {
            const std::pair<VkImage, bool> read{image, false};
            std::vector<std::pair<std::uint64_t, std::uint64_t>> seedReads;
            for (const auto& seed : seeds) seedReads.emplace_back(seed.begin, seed.end);
            seedReads.insert(seedReads.end(), padding.importReads.begin(), padding.importReads.end());
            recorder->NoteAccess(Recorder::CommandClass::StorageWriteBack, Recorder::Access{seedReads, imported, std::span(&read, 1), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT});
        }
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSource.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.image = image;
    toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
    if (!direct) context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
    if (!seeds.empty() || !padding.copies.empty() || !holeCopies.empty()) {
        constexpr VkAccessFlags transferAccess = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        if (recorder != nullptr && (covered & transferAccess) == transferAccess && Recorder::MergeBarriers()) {
            Recorder::CountMerged(Recorder::CommandClass::StorageWriteBack);
        } else {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, transferAccess);
            Recorder::CountBarriers(Recorder::CommandClass::StorageWriteBack);
        }
        for (const auto& [begin, end, slab, import] : seeds) {
            const VkBufferCopy seed{HostImportOffset(*import, begin), SlabOffset(*import, *slab, begin), end - begin};
            context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, import->buffer, slab->buffer, 1, &seed);
            NoteShadowSeed(begin, end);
        }
        for (const auto& [source, copies] : padding.copies) {
            context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, source, tiledScratch->Handle(), static_cast<std::uint32_t>(copies.size()), copies.data());
            paddingSeedCopies.fetch_add(copies.size(), std::memory_order_relaxed);
        }
        for (const auto& [source, slab, copies] : holeCopies) context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, source, slab, static_cast<std::uint32_t>(copies.size()), copies.data());
    }
    const VkMemoryBarrier importReady{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT};
    if (direct) {
        VkImageMemoryBarrier toRead = toSource;
        toRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toRead.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        const BarrierMask ready{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, importReady.srcAccessMask | toRead.srcAccessMask, importReady.dstAccessMask | toRead.dstAccessMask, toRead.oldLayout != toRead.newLayout, toRead.srcQueueFamilyIndex != toRead.dstQueueFamilyIndex};
        const bool ordered = recorder != nullptr && seeds.empty() && padding.copies.empty() && holeCopies.empty() && Recorder::ElideCovered(Recorder::CommandClass::StorageWriteBack, ready, covered);
        if (!ordered) context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &importReady, 0, nullptr, 1, &toRead);
        for (std::size_t i = 0; i < windows.size(); ++i) {
            const auto& window = windows[i];
            auto retile = window.window;
            retile.pipeBankXor = descriptor.pipeBankXor;
            for (auto k = directRanges[i].first; k < directRanges[i].second; ++k) {
                auto piece = directPieces[k].window;
                piece.pipeBankXor = descriptor.pipeBankXor;
                detiler.DispatchImage(commands, descriptor.tileMode, elementBytes, directPieces[k].slab, directPieces[k].offset, elementLayerView(window.level, window.layer), mips[window.level], true, window.layer, piece);
            }
            if (directRanges[i].first == directRanges[i].second) detiler.DispatchImage(commands, descriptor.tileMode, elementBytes, tiledScratch->Handle(), scratchPositions[i], elementLayerView(window.level, window.layer), mips[window.level], true, window.layer, retile);
        }
        singlePassMoves.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::vector<VkBufferImageCopy> regions;
        for (std::size_t i = 0; i < windows.size(); ++i) {
            for (auto region : windows[i].regions) {
                region.bufferOffset += linearPositions[i];
                regions.push_back(region);
            }
        }
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear->Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
        const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &importReady, 1, &linearRead, 0, nullptr);
        for (std::size_t i = 0; i < windows.size(); ++i) {
            const auto& window = windows[i];
            auto retile = window.window;
            retile.pipeBankXor = descriptor.pipeBankXor;
            for (auto k = directRanges[i].first; k < directRanges[i].second; ++k) {
                auto piece = directPieces[k].window;
                piece.pipeBankXor = descriptor.pipeBankXor;
                detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear->Handle(), linearPositions[i] + directPieces[k].linearOffset, directPieces[k].slab, directPieces[k].offset, mips[window.level], true, window.layer, false, piece);
            }
            if (directRanges[i].first == directRanges[i].second) detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear->Handle(), linearPositions[i], tiledScratch->Handle(), scratchPositions[i], mips[window.level], true, window.layer, false, retile);
        }
    }
    if (tiledScratch) {
        const VkMemoryBarrier scratchDone{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &scratchDone, 0, nullptr, 0, nullptr);
        const auto copyBuffer = context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
        for (const auto& pieces : slabPieces) {
            if (!pieces.copies.empty()) copyBuffer(commands, tiledScratch->Handle(), pieces.slab->buffer, static_cast<std::uint32_t>(pieces.copies.size()), pieces.copies.data());
        }
        for (std::size_t segment = 0; segment < segments.size(); ++segment) {
            if (!importCopies[segment].empty()) copyBuffer(commands, tiledScratch->Handle(), segments[segment].import->buffer, static_cast<std::uint32_t>(importCopies[segment].size()), importCopies[segment].data());
        }
        if (!saves.empty()) copyBuffer(commands, tiledScratch->Handle(), save->Handle(), static_cast<std::uint32_t>(saves.size()), saves.data());
    }
    VkImageMemoryBarrier backToGeneral = toSource;
    backToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    backToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    backToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    backToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    constexpr VkAccessFlags storedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
    const VkMemoryBarrier stored{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, storedAccess};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &stored, 0, nullptr, direct ? 0u : 1u, &backToGeneral);
    if (batch) {
        batch->SubmitAndWait();
    } else {
        recorder->EndGpuTiming(timing, storedBytes);
        recorder->MarkCovered(storedAccess);
        if (!imported.empty()) recorder->NoteStampedWrites(imported);
    }
    partialWriteBacks.fetch_add(1, std::memory_order_relaxed);
    partialWriteBackBytes.fetch_add(storedBytes, std::memory_order_relaxed);
    return storedBytes;
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
