#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/Submission.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadPriority.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>

namespace AgcDriver::DriverDetail {

namespace {

void noteIndirectSegment(Submission& submission, std::size_t start, const std::uint32_t* target, std::size_t words) {
    if (submission.commands.size() - start != words || !std::equal(submission.commands.begin() + static_cast<std::ptrdiff_t>(start), submission.commands.end(), target)) return;
    submission.indirectSegments.push_back({start, target, words});
}

void collectLabelDwords(std::span<const std::uint32_t> packet, std::set<std::uint64_t>& dwords) {
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    if ((packet[0] >> 30u) != 3u || (opcode != 0x49 && opcode != 0x37)) return;
    const auto label = Pm4::DecodeLabelWrite(packet);
    if (!label.has_value()) return;
    const auto bytes = label->Bytes();
    if (label->address % 4 != 0 || bytes.size() > 64) return;
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) dwords.insert(label->address + offset);
}

bool awaitsAny(std::span<const std::uint32_t> commands, std::size_t cursor, const std::set<std::uint64_t>& dwords) {
    if (cursor >= commands.size()) return false;
    const auto packet = commands.subspan(cursor, std::min<std::size_t>(Pm4::PacketWords(commands[cursor]), commands.size() - cursor));
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    if ((opcode != 0x3c && opcode != 0x93) || packet.size() < 7) return false;
    const auto wait = Pm4::DecodeWait(packet);
    for (std::size_t offset = 0; offset < wait.bytes; offset += 4) {
        if (dwords.contains(wait.address + offset)) return true;
    }
    return false;
}

template<typename Map>
void shiftKeys(Map& map, std::size_t start, std::size_t end, std::ptrdiff_t delta) {
    Map shifted;
    for (auto& [key, value] : map) {
        if (key >= start && key < end) continue;
        shifted.emplace(key >= end ? static_cast<std::size_t>(static_cast<std::ptrdiff_t>(key) + delta) : key, std::move(value));
    }
    map = std::move(shifted);
}

bool& submitsQueue0() {
    struct SubmitsQueue0Storage {};
    return HostThreadLocal<bool, SubmitsQueue0Storage>();
}

std::uint64_t& flipSubmissionsOfThread() {
    struct FlipSubmissionsStorage {};
    return HostThreadLocal<std::uint64_t, FlipSubmissionsStorage>();
}

bool progressEligible(const Submission& submission) {
    if (submission.queue != 0 || !submission.flips.empty()
        || !submission.renderingWaits.empty() || !submission.conditionalEnds.empty()) return false;
    bool terminalRelease = false;
    std::optional<Pm4::LabelWrite> knownWrite;
    for (std::size_t at = 0; at < submission.commands.size();) {
        const auto packet = std::span(submission.commands).subspan(at);
        const auto count = Pm4::PacketWords(packet[0]);
        if (count == 0 || count > packet.size()) return false;
        const auto opcode = (packet[0] >> 8u) & 0xffu;
        const bool type3 = (packet[0] >> 30u) == 3;
        if (type3 && (opcode == 0x3c || opcode == 0x93)) {
            if (!knownWrite || Pm4::Predicated(packet[0]) || ((packet[1] >> 4u) & 3u) != 1u) return false;
            const auto wait = Pm4::DecodeWait(packet.first(count));
            const auto bytes = knownWrite->Bytes();
            if (wait.address < knownWrite->address) return false;
            const auto offset = wait.address - knownWrite->address;
            if (offset > bytes.size() || wait.bytes > bytes.size() - offset) return false;
            std::uint64_t value = 0;
            std::memcpy(&value, bytes.data() + offset, wait.bytes);
            if (!Pm4::WaitComparesValue(packet.first(count), value)) return false;
            terminalRelease = false;
            at += count;
            continue;
        }
        if (type3 && opcode == 0x59 && submission.rewindTail != nullptr && at + count == submission.commands.size()) {
            at += count;
            continue;
        }
        if (!Pm4::FillerPacket(packet[0]) && !(type3 && opcode == 0x10) && !Pm4::IsTagMarker(packet.first(count))) {
            if (type3 && (opcode == 0x49 || opcode == 0x37)) {
                const auto write = Pm4::DecodeLabelWrite(packet.first(count));
                terminalRelease = opcode == 0x49 && write.has_value();
                if (write && !Pm4::Predicated(packet[0]) && (opcode == 0x37 || (packet[2] >> 29u) < 3)) knownWrite = *write;
                else if (opcode != 0x49 || (packet[2] >> 29u) != 0) knownWrite.reset();
            } else {
                terminalRelease = false;
                if (!type3 || (opcode != 0x42 && opcode != 0x58)) knownWrite.reset();
            }
        }
        at += count;
    }
    return terminalRelease;
}

std::uint64_t captureTime() {
    return static_cast<std::uint64_t>(std::llround(TraceMs() * 1000.0));
}

void writeCaptured(Driver& driver, SubmissionCapture& capture, const CapturedSubmission& captured) {
    try {
        capture.Write(captured);
    } catch (...) {
        driver.ReportFailure(std::current_exception());
        throw;
    }
}

bool EopRelease(std::span<const std::uint32_t> words) {
    return words.size() >= 8 && (words[0] >> 30u) == 3u && Pm4::PacketWords(words[0]) == 8u && ((words[0] >> 8u) & 0xffu) == 0x49u && ((words[2] >> 24u) & 7u) == 2u;
}

}

std::uint64_t Driver::FlipSubmissions() const {
    return flipSubmissionsOfThread();
}

std::uint64_t Driver::RewindTailFlips() const {
    return rewindTailFlips.load(std::memory_order_acquire);
}

std::deque<const std::uint32_t*>& Driver::releasedTails() {
    struct ReleasedTailsStorage {};
    return HostThreadLocal<std::deque<const std::uint32_t*>, ReleasedTailsStorage>();
}

void Driver::noteReleasedTails(const Submission& submission) {
    auto& released = releasedTails();
    released.clear();
    thread_local Submission* scratchSlot = nullptr;
    auto& scratch = ShaderRecompiler::ThreadOwned(scratchSlot);
    const std::uint32_t* tail = submission.rewindTail;
    std::size_t words = submission.rewindWords;
    for (std::size_t chunk = 0; chunk < 256 && tail != nullptr; ++chunk) {
        if ((std::atomic_ref<std::uint32_t>(*const_cast<std::uint32_t*>(tail - 1)).load(std::memory_order_acquire) & 0x80000000u) == 0) break;
        released.push_back(tail - 1);
        scratch.rewindTail = nullptr;
        scratch.rewindWords = 0;
        try {
            copyCommands(scratch, tail, words);
        } catch (const std::exception&) {
            break;
        }
        tail = scratch.rewindTail;
        words = scratch.rewindWords;
    }
}

void Driver::copyCommands(Submission& submission, const std::uint32_t* guest, std::size_t words) {
    submission.commands.clear();
    submission.conditionalEnds.clear();
    submission.indirectSegments.clear();
    std::size_t budget = std::size_t{1} << 26u;
    copySegment(submission, guest, words, budget, 0);
    std::ranges::sort(submission.indirectSegments, {}, &Submission::IndirectSegment::start);
}

bool Driver::copySegment(Submission& submission, const std::uint32_t* guest, std::size_t words, std::size_t& budget, std::size_t depth) {
    require(depth < MaximumSegmentDepth, "command buffer jumps nest too deeply (a jump loop?)");
    for (;;) {
        const std::uint32_t* chained = nullptr;
        std::size_t chainedWords = 0;
        if (copyLink(submission, guest, words, budget, depth, chained, chainedWords)) return true;
        if (chained == nullptr) return false;
        guest = chained;
        words = chainedWords;
    }
}

bool Driver::copyLink(Submission& submission, const std::uint32_t* guest, std::size_t words, std::size_t& budget, std::size_t depth, const std::uint32_t*& chained, std::size_t& chainedWords) {
    require(words <= budget, "command buffer jumps exceed the copy limit (a jump loop?)");
    budget -= words;
    std::vector<std::pair<std::size_t, std::size_t>> guarded;
    const auto reach = [&](std::size_t cursor, std::size_t next) {
        std::erase_if(guarded, [&](const auto& range) {
            if (range.first != cursor) return false;
            submission.conditionalEnds.emplace(range.second, submission.commands.size());
            return true;
        });
        for (const auto& range : guarded) require(range.first >= next, "conditional execution range ends inside a packet");
    };
    for (std::size_t cursor = 0; cursor < words;) {
        const auto header = guest[cursor];
        if (Pm4::FillerPacket(header)) {
            reach(cursor, cursor + 1);
            submission.commands.push_back(header);
            ++cursor;
            continue;
        }
        const auto count = (header & 0xc0000000u) == 0xc0000000u ? Pm4::PacketWords(header) : words - cursor;
        if ((header & 0xc0000000u) != 0xc0000000u || count > words - cursor) {
            submission.commands.insert(submission.commands.end(), guest + cursor, guest + words);
            return false;
        }
        reach(cursor, cursor + count);
        const auto opcode = (header >> 8u) & 0xffu;
        if (opcode == 0x3fu && count == 14) {
            const auto mode = guest[cursor + 1] & 3u;
            require(mode == 1u || mode == 2u, "invalid COND_INDIRECT_BUFFER mode");
            require(((guest[cursor + 1] >> 8u) & 7u) == 0u, "a COND_INDIRECT_BUFFER with a comparison is not implemented");
            require(!Pm4::Predicated(header), "a predicated COND_INDIRECT_BUFFER is not implemented");
            const auto* target = reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(guest[cursor + 8] & ~3u) | (static_cast<std::uintptr_t>(guest[cursor + 9] & 0xffffu) << 32u));
            const std::size_t targetWords = guest[cursor + 10] & 0xfffffu;
            if (targetWords != 0) {
                GuestMemory::CheckRange(target, targetWords * sizeof(std::uint32_t), alignof(std::uint32_t));
                const auto start = submission.commands.size();
                if (copySegment(submission, target, targetWords, budget, depth + 1)) {
                    require(guarded.empty(), "a REWIND inside a conditional execution range is not implemented");
                    return true;
                }
                noteIndirectSegment(submission, start, target, targetWords);
                if (guarded.empty()) return false;
            }
            cursor += count;
            continue;
        }
        if (opcode == 0x3fu) {
            require(count == 4, "invalid INDIRECT_BUFFER size");
            const auto* target = reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(guest[cursor + 1] & ~3u) | (static_cast<std::uintptr_t>(guest[cursor + 2] & 0xffffu) << 32u));
            const std::size_t targetWords = guest[cursor + 3] & 0xfffffu;
            const bool chain = (guest[cursor + 3] & (1u << 20u)) != 0;
            require(!chain || guarded.empty(), "a chained INDIRECT_BUFFER inside a conditional execution range is not implemented");
            GuestMemory::CheckRange(target, targetWords * sizeof(std::uint32_t), alignof(std::uint32_t));
            if (Pm4::Predicated(header)) {
                require(!chain, "predicated command buffer chains are not implemented");
                const auto start = submission.commands.size();
                submission.commands.insert(submission.commands.end(), guest + cursor, guest + cursor + count);
                require(!copySegment(submission, target, targetWords, budget, depth + 1), "a REWIND inside a predicated command buffer is not implemented");
                for (auto inner = start + count; inner < submission.commands.size(); inner += Pm4::PacketWords(submission.commands[inner])) {
                    const auto innerHeader = submission.commands[inner];
                    require(innerHeader != FlipPacketHeader && innerHeader != RenderingWaitPacketHeader, "flips and rendering waits in predicated command buffers are not implemented");
                }
                submission.conditionalEnds.emplace(start, submission.commands.size());
                cursor += count;
                continue;
            }
            if (chain) {
                chained = target;
                chainedWords = targetWords;
                return false;
            }
            const auto start = submission.commands.size();
            if (copySegment(submission, target, targetWords, budget, depth + 1)) {
                require(guarded.empty(), "a REWIND inside a conditional execution range is not implemented");
                return true;
            }
            noteIndirectSegment(submission, start, target, targetWords);
            cursor += count;
            continue;
        }
        if (opcode == 0x59u) {
            require(count == 2, "a REWIND packet must be two dwords");
            require(guarded.empty(), "a REWIND inside a conditional execution range is not implemented");
            submission.commands.push_back(header);
            submission.commands.push_back(std::atomic_ref<std::uint32_t>(*const_cast<std::uint32_t*>(guest + cursor + 1)).load(std::memory_order_acquire));
            cursor += count;
            submission.rewindTail = guest + cursor;
            submission.rewindWords = words - cursor;
            return true;
        }
        if (opcode == 0x22u && count == 5) guarded.emplace_back(cursor + count + Pm4::ConditionalWords(std::span(guest + cursor, count)), submission.commands.size());
        submission.commands.insert(submission.commands.end(), guest + cursor, guest + cursor + count);
        cursor += count;
    }
    reach(words, words);
    require(guarded.empty(), "conditional execution range exceeds its command buffer");
    return false;
}

bool Driver::refreshIndirectSegment(Submission& submission, std::size_t index) {
    const auto segment = submission.indirectSegments[index];
    const auto address = reinterpret_cast<std::uint64_t>(segment.target);
    const auto bytes = segment.words * sizeof(std::uint32_t);
    const bool pipelineWrites = DrawPipeline::Active() && DrawPipeline::Queue0().Overlaps(address, bytes);
    const bool finisherWrites = DrawFinisher::Busy() && DrawFinisher::Get().PendingOverlaps(address, address + bytes);
    const bool recordedWrites = [&] {
        std::lock_guard gpu(GuestMemory::GpuMutex());
        const auto* recorder = Graphics::Recorder::Active();
        return recorder != nullptr && recorder->PendingWriterSerial(address, bytes) != 0;
    }();
    if (!pipelineWrites && !finisherWrites && !recordedWrites) return false;
    if (pipelineWrites) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, 0x3f);
    if (finisherWrites) DrawFinisher::Get().Drain();
    std::vector<std::uint32_t> words(segment.words);
    GuestMemory::Read(address, std::as_writable_bytes(std::span(words)), alignof(std::uint32_t));
    const auto end = segment.start + segment.words;
    if (std::equal(words.begin(), words.end(), submission.commands.begin() + static_cast<std::ptrdiff_t>(segment.start))) return false;
    Submission expanded{};
    expanded.queue = submission.queue;
    std::size_t budget = std::size_t{1} << 26u;
    try {
        if (copySegment(expanded, words.data(), words.size(), budget, 0)) return false;
        validate(expanded, segment.target, false);
        readRegisterLists(expanded);
    } catch (const std::exception& error) {
        static std::atomic<int> reported{0};
        if (reported.fetch_add(1, std::memory_order_relaxed) < 8) std::fprintf(stderr, "[gpu] indirect buffer 0x%llx (%zu dwords) changed since its submission but runs as copied: %s\n", static_cast<unsigned long long>(address), segment.words, error.what());
        return false;
    }
    const auto inside = [&](std::size_t cursor) { return cursor >= segment.start && cursor < end; };
    if (std::ranges::any_of(submission.flips, [&](const auto& entry) { return inside(entry.first); })) return false;
    {
        std::lock_guard lock(mutex);
        for (std::size_t cursor = 0; cursor < expanded.commands.size(); cursor += Pm4::FillerPacket(expanded.commands[cursor]) ? 1u : Pm4::PacketWords(expanded.commands[cursor])) {
            const auto* packet = expanded.commands.data() + cursor;
            if (packet[0] == FlipPacketHeader) return false;
            if (EopRelease(std::span(expanded.commands).subspan(cursor)) && eopFlips.contains(packet[7])) return false;
            if (packet[0] != RenderingWaitPacketHeader) continue;
            const std::pair<std::uint32_t, std::uint32_t> target{packet[1], packet[2]};
            const auto prepared = segment.waits != nullptr ? segment.waits->find(target) : Submission::PreparedWaits::const_iterator{};
            if (segment.waits == nullptr || prepared == segment.waits->end()) {
                renderingWaitTargets.insert(target);
                return false;
            }
            expanded.renderingWaits.emplace(cursor, prepared->second);
        }
    }
    if (DrawPipeline::Active() && DrawPipeline::Queue0().Busy()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Packet, 0x3f);
    if (DrawFinisher::Busy()) DrawFinisher::Get().Drain();
    const auto delta = static_cast<std::ptrdiff_t>(expanded.commands.size()) - static_cast<std::ptrdiff_t>(segment.words);
    const auto moved = [&](std::size_t cursor) { return cursor >= end ? static_cast<std::size_t>(static_cast<std::ptrdiff_t>(cursor) + delta) : cursor; };
    auto& commands = submission.commands;
    commands.erase(commands.begin() + static_cast<std::ptrdiff_t>(segment.start), commands.begin() + static_cast<std::ptrdiff_t>(end));
    commands.insert(commands.begin() + static_cast<std::ptrdiff_t>(segment.start), expanded.commands.begin(), expanded.commands.end());
    shiftKeys(submission.flips, segment.start, end, delta);
    shiftKeys(submission.renderingWaits, segment.start, end, delta);
    for (auto& [cursor, wait] : expanded.renderingWaits) submission.renderingWaits.emplace(segment.start + cursor, std::move(wait));
    shiftKeys(submission.registerLists, segment.start, end, delta);
    for (auto& [cursor, list] : expanded.registerLists) submission.registerLists.emplace(segment.start + cursor, std::move(list));
    std::map<std::size_t, std::size_t> ends;
    for (const auto& [begin, rangeEnd] : submission.conditionalEnds) {
        if (!inside(begin)) ends.emplace(moved(begin), moved(rangeEnd));
    }
    for (const auto& [begin, rangeEnd] : expanded.conditionalEnds) ends.emplace(segment.start + begin, segment.start + rangeEnd);
    submission.conditionalEnds = std::move(ends);
    std::set<std::size_t> held;
    for (const auto cursor : submission.heldAtSubmit) {
        if (!inside(cursor)) held.insert(moved(cursor));
    }
    submission.heldAtSubmit = std::move(held);
    std::vector<Submission::IndirectSegment> segments(submission.indirectSegments.begin(), submission.indirectSegments.begin() + static_cast<std::ptrdiff_t>(index));
    for (const auto& nested : expanded.indirectSegments) segments.push_back({segment.start + nested.start, nested.target, nested.words, segment.waits});
    for (auto later = submission.indirectSegments.begin() + static_cast<std::ptrdiff_t>(index) + 1; later != submission.indirectSegments.end(); ++later) segments.push_back({moved(later->start), later->target, later->words, later->waits});
    std::ranges::sort(segments, {}, &Submission::IndirectSegment::start);
    submission.indirectSegments = std::move(segments);
    std::set<std::uint64_t> written;
    for (std::size_t cursor = 0; cursor < expanded.commands.size(); cursor += Pm4::FillerPacket(expanded.commands[cursor]) ? 1u : Pm4::PacketWords(expanded.commands[cursor])) {
        if (Pm4::FillerPacket(expanded.commands[cursor])) continue;
        collectLabelDwords(std::span<const std::uint32_t>(expanded.commands).subspan(cursor, std::min<std::size_t>(Pm4::PacketWords(expanded.commands[cursor]), expanded.commands.size() - cursor)), written);
    }
    if (!written.empty()) {
        std::lock_guard lock(mutex);
        auto& worker = workers[submission.queue];
        for (const auto dword : written) {
            submission.labelWrites.push_back(dword);
            ++worker.unfinishedWrites[dword];
        }
        const auto after = segment.start + expanded.commands.size();
        std::erase_if(submission.heldAtSubmit, [&](std::size_t cursor) { return cursor >= after && awaitsAny(submission.commands, cursor, written); });
        for (auto& later : worker.pending) std::erase_if(later.heldAtSubmit, [&](std::size_t cursor) { return awaitsAny(later.commands, cursor, written); });
    }
    clearResolvedAhead();
    return true;
}

ReferencedProgramScan ReferencedPrograms(std::span<const std::uint32_t> commands, const std::map<std::size_t, std::vector<std::uint32_t>>& registerLists) {
    constexpr std::array<std::uint32_t, 7> programRegisters{0x008u, 0x048u, 0x088u, 0x0c8u, 0x108u, 0x148u, 0x20cu};
    std::array<std::uint32_t, programRegisters.size() * 2> values{};
    std::array<bool, programRegisters.size() * 2> written{};
    std::array<bool, programRegisters.size()> bound{};
    ReferencedProgramScan scan;
    const auto write = [&](std::uint32_t offset, std::uint32_t value, bool conditional) {
        const auto base = std::ranges::find_if(programRegisters, [&](std::uint32_t program) { return offset - program < 2u; });
        if (base == programRegisters.end()) return;
        if (conditional) {
            scan.complete = false;
            return;
        }
        const auto slot = static_cast<std::size_t>(base - programRegisters.begin()) * 2u + (offset - *base);
        values[slot] = value;
        written[slot] = true;
        const auto low = slot & ~std::size_t{1};
        if (written[low] && written[low + 1]) {
            bound[low / 2] = true;
            scan.addresses.push_back((static_cast<std::uint64_t>(values[low]) << 8u) | (static_cast<std::uint64_t>(values[low + 1] & 0xffu) << 40u));
        }
    };
    std::size_t guardedEnd = 0;
    for (std::size_t cursor = 0; cursor < commands.size(); cursor += Pm4::PacketWords(commands[cursor])) {
        const auto header = commands[cursor];
        if ((header >> 30u) != 3u) continue;
        const auto opcode = (header >> 8u) & 0xffu;
        const auto words = std::min<std::size_t>(Pm4::PacketWords(header), commands.size() - cursor);
        const bool conditional = (header & 1u) != 0 || cursor < guardedEnd;
        if (opcode == 0x22u && words == 5) guardedEnd = std::max(guardedEnd, cursor + words + Pm4::ConditionalWords(commands.subspan(cursor, words)));
        if (Pm4::DrawOpcode(opcode) && !std::any_of(bound.begin(), bound.end() - 1, [](bool pair) { return pair; })) scan.complete = false;
        if ((opcode == 0x15u || opcode == 0x16u) && !bound.back()) scan.complete = false;
        if (opcode == 0x76u && words >= 3) {
            const auto packet = commands.subspan(cursor, words);
            const auto offset = packet[1] & ~0x70000000u;
            for (std::size_t i = 2; i < words; ++i) write(offset + static_cast<std::uint32_t>(i - 2), packet[i], conditional);
        } else if (opcode == 0x63u) {
            const auto list = registerLists.find(cursor);
            if (list == registerLists.end()) continue;
            if (!Pm4::IndirectRegistersValid(list->second)) {
                scan.complete = false;
                continue;
            }
            for (std::size_t i = 0; i + 1 < list->second.size(); i += 2) write(list->second[i] & ~0x70000000u, list->second[i + 1], conditional);
        }
    }
    for (std::size_t slot = 0; slot < written.size(); slot += 2) {
        if (written[slot] != written[slot + 1]) scan.complete = false;
    }
    std::ranges::sort(scan.addresses);
    scan.addresses.erase(std::unique(scan.addresses.begin(), scan.addresses.end()), scan.addresses.end());
    return scan;
}

}
