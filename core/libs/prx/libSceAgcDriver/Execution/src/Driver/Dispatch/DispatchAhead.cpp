#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

std::map<std::size_t, Driver::ResolvedDispatch>& Driver::resolvedAhead() {
    static thread_local std::map<std::size_t, ResolvedDispatch> resolved;
    return resolved;
}

Driver::GroupCaptureStats& Driver::groupCaptureStats() {
    static thread_local GroupCaptureStats stats;
    return stats;
}

std::size_t& Driver::currentPacketOffset() {
    static thread_local std::size_t offset = 0;
    return offset;
}

bool& Driver::resolvingAhead() {
    static thread_local bool resolving = false;
    return resolving;
}

bool& Driver::gateOpened() {
    static thread_local bool opened = false;
    return opened;
}

void Driver::clearResolvedAhead() {
    auto& resolved = resolvedAhead();
    if (resolved.empty()) return;
    groupCaptureStats().stale += resolved.size();
    resolved.clear();
}

namespace {

struct RunLabel {
    std::uint64_t address;
    std::size_t size;
    std::uint64_t value;
};

bool InCode(const ShaderRecompiler::MemoryRegion& region, const std::shared_ptr<const ShaderSnapshot>& code) {
    if (code == nullptr) return false;
    if (region.guestAddress >= code->codeAddress && region.guestAddress < code->codeAddress + code->code.size() * sizeof(std::uint32_t)) return true;
    return !code->header.empty() && region.guestAddress >= code->headerAddress && region.guestAddress < code->headerAddress + code->header.size();
}

bool OverlapsRange(std::span<const ShaderRecompiler::MemoryRegion> regions, const std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges, const std::shared_ptr<const ShaderSnapshot>& code) {
    for (const auto& region : regions) {
        if (InCode(region, code)) continue;
        for (const auto& [begin, end] : ranges) {
            if (begin < region.guestAddress + region.bytes.size() && region.guestAddress < end) return true;
        }
    }
    return false;
}

bool WrittenByRun(std::span<const std::uint32_t> wait, const std::vector<RunLabel>& labels) {
    const std::uint64_t awaited = wait[2] | (static_cast<std::uint64_t>(wait[3]) << 32u);
    const auto bytes = Pm4::WaitAwaitedBytes(wait);
    for (auto it = labels.rbegin(); it != labels.rend(); ++it) {
        if (it->address != awaited || it->size < bytes) continue;
        return Pm4::WaitComparesValue(wait, bytes == 8 ? it->value : (it->value & 0xffffffffu));
    }
    return false;
}

void NoteLabel(std::vector<RunLabel>& labels, std::uint64_t address, std::span<const std::byte> bytes) {
    if (bytes.size() != 4 && bytes.size() != 8) return;
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data(), bytes.size());
    labels.push_back({address, bytes.size(), value});
}

bool OverlapsLabel(std::span<const ShaderRecompiler::MemoryRegion> regions, const std::vector<RunLabel>& labels) {
    for (const auto& label : labels) {
        for (const auto& region : regions) {
            if (label.address < region.guestAddress + region.bytes.size() && region.guestAddress < label.address + label.size) return true;
        }
    }
    return false;
}

}

void Driver::resolveGroupAhead(const Submission& submission, const QueueState& live, std::size_t from) {
    static const bool allQueues = std::getenv("APS5_GROUP_CAPTURE_ALL_QUEUES") != nullptr;
    if (!allQueues && submission.queue == 0) return;
    auto& resolved = resolvedAhead();
    if (resolved.contains(from) || device.Load() == nullptr) return;
    auto& stats = groupCaptureStats();
    const auto begin = std::chrono::steady_clock::now();
    ++stats.runs;
    QueueState scratch = live;
    std::vector<RunLabel> labels;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    for (const auto& label : deferredLabels().labels) NoteLabel(labels, label.address, std::span<const std::byte>(label.bytes).first(label.size));
    struct Resolving {
        std::size_t offset;
        Resolving() : offset(currentPacketOffset()) { resolvingAhead() = true; }
        ~Resolving() {
            resolvingAhead() = false;
            currentPacketOffset() = offset;
        }
    } resolving;
    const auto& commands = submission.commands;
    for (std::size_t cursor = from; cursor < commands.size();) {
        const auto header = commands[cursor];
        if (Pm4::FillerPacket(header)) {
            ++cursor;
            continue;
        }
        const auto count = Pm4::PacketWords(header);
        if (cursor + count > commands.size()) break;
        const auto packet = std::span(commands).subspan(cursor, count);
        const auto opcode = (header >> 8u) & 0xffu;
        const auto next = cursor + count;
        if (header == FlipPacketHeader || header == RenderingWaitPacketHeader || opcode == 0x22 || (Pm4::Predicated(header) && scratch.predication.operation != 0)) break;
        if (opcode == 0x3c || opcode == 0x93) {
            if (!Pm4::WaitSatisfiedUnchecked(packet) && !WrittenByRun(packet, labels)) break;
        } else if (opcode == 0x15 || opcode == 0x16) {
            if (resolved.contains(cursor)) break;
            currentPacketOffset() = cursor;
            try {
                if (opcode == 0x15) dispatch(scratch, packet, submission);
                else dispatchIndirect(scratch, packet, submission);
            } catch (const std::exception&) {
                ++stats.failures;
                break;
            }
            if (const auto found = resolved.find(cursor); found != resolved.end()) {
                if (OverlapsLabel(found->second.captured, labels)) {
                    resolved.erase(found);
                    ++stats.labelOverlaps;
                    break;
                }
                if (OverlapsRange(found->second.captured, writes, found->second.registeredShader)) {
                    resolved.erase(found);
                    ++stats.writtenBefore;
                    break;
                }
                if (found->second.compiledResult != nullptr) appendWrittenRanges(*found->second.compiledResult, writes);
            }
        } else if (opcode == 0x37 || opcode == 0x49) {
            const auto label = Pm4::DecodeLabelWrite(packet);
            if (!label) break;
            NoteLabel(labels, label->address, label->Bytes());
        } else if (opcode != 0x3f && opcode != 0x42 && opcode != 0x46 && opcode != 0x58) {
            if (Pm4::AccessesMemory(header)) break;
            try {
                Pm4::Execute(packet, scratch);
            } catch (const std::exception&) {
                break;
            }
        }
        cursor = next;
    }
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    stats.resolveMs += ms;
    stats.resolveMaxMs = std::max(stats.resolveMaxMs, ms);
    reportGroupCapture(submission.queue);
}

void Driver::reportGroupCapture(std::uint32_t queue) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = groupCaptureStats();
    const auto now = std::chrono::steady_clock::now();
    if (now - stats.reported < std::chrono::seconds(10)) return;
    AgcDriver::ProfilePrint_nid_no_patch("[groupcap] queue 0x%x (10 s): %llu runs resolved %llu dispatches (%llu cache hits) in %.1f ms (max %.2f ms); %llu adopted %.2f ms after the resolve on average, max %.2f ms; mismatches: address %llu key %llu; %llu resolve failures, %llu stale, %llu label overlaps, %llu read an earlier dispatch's output, %llu rejected by the device\n",
                                         queue, static_cast<unsigned long long>(stats.runs), static_cast<unsigned long long>(stats.resolved), static_cast<unsigned long long>(stats.resolvedHits), stats.resolveMs, stats.resolveMaxMs,
                                         static_cast<unsigned long long>(stats.adopted), stats.adopted != 0 ? stats.ageMs / static_cast<double>(stats.adopted) : 0.0, stats.ageMaxMs, static_cast<unsigned long long>(stats.addressMismatches), static_cast<unsigned long long>(stats.keyMismatches),
                                         static_cast<unsigned long long>(stats.failures), static_cast<unsigned long long>(stats.stale), static_cast<unsigned long long>(stats.labelOverlaps), static_cast<unsigned long long>(stats.writtenBefore), static_cast<unsigned long long>(stats.deviceRejects));
    stats = GroupCaptureStats{};
    stats.reported = now;
}

}
