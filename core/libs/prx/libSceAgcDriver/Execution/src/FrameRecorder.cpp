#include "prx/libSceAgcDriver/State/include/Configuration.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/TraceRevision.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace AgcDriver {
namespace {

std::uint64_t readNumber(const char* text, std::uint64_t fallback) {
    if (text == nullptr || *text == '\0') return fallback;
    std::uint64_t value = 0;
    const std::string_view view(text);
    const auto [last, error] = std::from_chars(view.data(), view.data() + view.size(), value);
    if (error != std::errc{} || last != view.data() + view.size()) throw std::runtime_error("frame capture: '" + std::string(view) + "' is not a number");
    return value;
}

std::string jsonString(std::string_view text) {
    std::string result = "\"";
    for (const char character : text) {
        if (character == '"' || character == '\\') {
            result += '\\';
            result += character;
        } else if (static_cast<unsigned char>(character) < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(character)));
            result += escaped;
        } else {
            result += character;
        }
    }
    return result + "\"";
}

std::vector<std::pair<std::string, std::string>> captureEnvironment() {
#ifdef _WIN32
    char** entries = _environ;
#else
    char** entries = environ;
#endif
    std::vector<std::pair<std::string, std::string>> result;
    for (; entries != nullptr && *entries != nullptr; ++entries) {
        const std::string_view entry = *entries;
        const auto equals = entry.find('=');
        if (!entry.starts_with("APS5_") || equals == std::string_view::npos) continue;
        result.emplace_back(std::string(entry.substr(0, equals)), std::string(entry.substr(equals + 1)));
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::uint64_t threadId() {
    return static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

std::vector<ShaderRegister> copyRegisters(std::span<const ShaderRegister> registers) {
    return {registers.begin(), registers.end()};
}

std::uint64_t pointer(const void* value) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value));
}

std::uint64_t flipValue(std::int64_t value) {
    return static_cast<std::uint64_t>(value);
}

std::uint64_t contentHash(std::span<const std::byte> bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const auto value : bytes) hash = (hash ^ static_cast<std::uint64_t>(value)) * 0x100000001b3ull;
    return hash == 0 ? 1 : hash;
}

}

FrameRecorder* FrameRecorder::Active() {
    static FrameRecorder* const recorder = []() -> FrameRecorder* {
        const char* directory = std::getenv("APS5_FRAME_CAPTURE");
        if (directory == nullptr || *directory == '\0') return nullptr;
        const char* trigger = std::getenv("APS5_FRAME_CAPTURE_TRIGGER");
        const auto atBoundary = readNumber(std::getenv("APS5_FRAME_CAPTURE_AT_FLIP"), 0);
        const auto frames = readNumber(std::getenv("APS5_FRAME_CAPTURE_FRAMES"), DefaultFrames);
        if (frames == 0) throw std::runtime_error("frame capture: APS5_FRAME_CAPTURE_FRAMES must be at least 1");
        return new FrameRecorder(directory, trigger != nullptr ? trigger : "", atBoundary, frames);
    }();
    return recorder;
}

FrameRecorder::FrameRecorder(std::filesystem::path directory, std::filesystem::path trigger, std::uint64_t atBoundary, std::uint64_t frames)
    : directory(std::move(directory)), trigger(std::move(trigger)), atBoundary(atBoundary), frames(frames), origin(std::chrono::steady_clock::now()) {}

FrameCall FrameRecorder::make(FrameCallKind kind) {
    FrameCall call;
    call.kind = kind;
    call.timeNs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - origin).count());
    call.thread = threadId();
    return call;
}

void FrameRecorder::RegisterShader(const Shader* shader) {
    auto call = make(FrameCallKind::RegisterShader);
    call.address = pointer(shader);
    const std::array<const Shader*, 1> shaders{shader};
    record(std::move(call), true, shaders);
}

void FrameRecorder::ResolveShaderAbi(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    auto call = make(FrameCallKind::ResolveShaderAbi);
    call.address = pointer(shader);
    call.context = copyRegisters(context);
    call.primitive = copyRegisters(primitive);
    const std::array<const Shader*, 1> shaders{shader};
    record(std::move(call), true, shaders);
}

void FrameRecorder::ResolveGraphicsStagesAbi(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    auto call = make(FrameCallKind::ResolveGraphicsStagesAbi);
    for (const auto* stage : stages) call.values.push_back(pointer(stage));
    call.context = copyRegisters(context);
    call.primitive = copyRegisters(primitive);
    record(std::move(call), true, stages);
}

void FrameRecorder::ResolveGraphicsAbi(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    auto call = make(FrameCallKind::ResolveGraphicsAbi);
    call.values = {pointer(vertex), pointer(pixel), primitiveType};
    const std::array<const Shader*, 2> shaders{vertex, pixel};
    record(std::move(call), true, shaders);
}

void FrameRecorder::RegisterVideoOutput(std::uint32_t handle) {
    auto call = make(FrameCallKind::RegisterVideoOutput);
    call.values = {handle};
    record(std::move(call), true);
}

void FrameRecorder::UnregisterVideoOutput(std::uint32_t handle) {
    auto call = make(FrameCallKind::UnregisterVideoOutput);
    call.values = {handle};
    record(std::move(call), true);
}

void FrameRecorder::Submit(const Packet& packet, std::uint32_t queue, const std::function<void()>& accept) {
    auto call = make(FrameCallKind::Submit);
    call.queue = queue;
    call.address = pointer(packet.addr);
    call.size = packet.dw_num;
    call.flags = packet.flags;
    record(std::move(call), false, {}, accept);
}

void FrameRecorder::Flip(const FlipInfo& info, const std::function<void()>& accept) {
    auto call = make(FrameCallKind::Flip);
    call.cpuFlip = true;
    call.values = {info.handle, flipValue(info.index), info.mode, flipValue(info.argument)};
    record(std::move(call), false, {}, accept);
}

void FrameRecorder::ReserveEopFlip(const FlipInfo& info, std::uint32_t token) {
    auto call = make(FrameCallKind::ReserveEopFlip);
    call.values = {info.handle, flipValue(info.index), info.mode, flipValue(info.argument), token};
    std::lock_guard lock(mutex);
    if (state == State::Waiting) {
        call.ordinal = ++ordinal;
        reservations.push_back(std::move(call));
        if (reservations.size() > ReservationHistory) reservations.pop_front();
        return;
    }
    if (state != State::Recording) return;
    emitDelta();
    if (state != State::Recording) return;
    call.ordinal = ++ordinal;
    std::tie(call.accepted, call.completed) = DriverDetail::Driver::Get().SerialProgress();
    writer->WriteCall(call);
    ++recordedCalls;
}

void FrameRecorder::Suspend(const std::function<void()>& accept) {
    record(make(FrameCallKind::Suspend), false, {}, accept);
}

void FrameRecorder::PresentBuffer(const DisplayBuffer& buffer) {
    std::lock_guard lock(displayMutex);
    displayBuffers[buffer.address] = buffer;
}

void FrameRecorder::writeReferenceFrames() {
    std::map<std::uint64_t, DisplayBuffer> presented;
    {
        std::lock_guard lock(displayMutex);
        presented = displayBuffers;
    }
    if (presented.empty()) return;
    if (!DriverDetail::Driver::Get().WaitIdleFor(std::chrono::seconds(10))) {
        warnings.push_back("no reference frames: the driver did not go idle after the last frame");
        return;
    }
    std::uint64_t index = 0;
    for (const auto& [address, buffer] : presented) {
        try {
            const auto pixels = ReadDisplayBuffer(buffer);
            writer->WriteReference(index, address, pixels);
        } catch (const std::exception& failure) {
            warnings.push_back(std::string("no reference frame for a display buffer: ") + failure.what());
            continue;
        }
        auto call = make(FrameCallKind::DisplayBuffer);
        call.ordinal = ++ordinal;
        call.address = buffer.address;
        call.values = {index, buffer.pixelFormat, buffer.width, buffer.height, buffer.tilingMode, buffer.pitchInPixel, buffer.dccAddress, buffer.dccClearColor, buffer.alignment};
        writer->WriteCall(call);
        ++index;
    }
}

void FrameRecorder::record(FrameCall call, bool prologueCall, std::span<const Shader* const> shaders, const std::function<void()>& accept) {
    std::lock_guard lock(mutex);
    recordLocked(std::move(call), prologueCall, shaders);
    if (accept) accept();
}

void FrameRecorder::recordLocked(FrameCall call, bool prologueCall, std::span<const Shader* const> shaders) {
    if (state == State::Waiting) {
        if (!prologueCall) return;
        call.ordinal = ++ordinal;
        prologue.push_back(std::move(call));
        return;
    }
    if (state != State::Recording) return;
    if (prologueCall) emitShaderDelta(shaders);
    else emitDelta();
    if (state != State::Recording) return;
    call.ordinal = ++ordinal;
    std::tie(call.accepted, call.completed) = DriverDetail::Driver::Get().SerialProgress();
    writer->WriteCall(call);
    ++recordedCalls;
}

void FrameRecorder::FrameBoundary() {
    std::lock_guard lock(mutex);
    ++boundaries;
    if (state == State::Recording) {
        if (++recordedFrames >= frames) finish();
        return;
    }
    if (state == State::Waiting && triggered()) start();
}

bool FrameRecorder::triggered() {
    if (!trigger.empty()) {
        std::error_code error;
        if (!std::filesystem::exists(trigger, error)) return false;
        std::ifstream file(trigger);
        std::uint64_t requested = 0;
        if (file >> requested && requested != 0) frames = requested;
        return true;
    }
    return boundaries >= std::max<std::uint64_t>(atBoundary, 1);
}

void FrameRecorder::emitDelta() {
    std::vector<Span> spans;
    for (const auto& range : ranges) {
        if (range.kind == FrameRangeKind::Watched || range.kind == FrameRangeKind::Unwatched) spans.push_back({range.address, range.bytes, generation});
    }
    const auto mark = GuestMemory::WriteGeneration();
    const auto added = newRanges();
    writeDelta(spans, {}, added);
    generation = mark;
    shaderMarks.clear();
}

std::vector<FrameRecorder::Range> FrameRecorder::newRanges() {
    const auto current = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (current == allocationsGeneration) return {};
    allocationsGeneration = current;
    std::map<std::uint64_t, std::uint64_t> covered;
    for (const auto& range : ranges) {
        auto& end = covered[range.address];
        end = std::max(end, range.address + range.bytes);
    }
    std::vector<Range> added;
    for (const auto& range : committedRanges()) {
        const auto end = range.address + range.bytes;
        for (auto at = range.address; at < end;) {
            const auto next = covered.upper_bound(at);
            if (next != covered.begin() && std::prev(next)->second > at) {
                at = std::prev(next)->second;
                continue;
            }
            auto piece = range;
            piece.address = at;
            piece.bytes = (next == covered.end() ? end : std::min(end, next->first)) - at;
            added.push_back(piece);
            at += piece.bytes;
        }
    }
    return added;
}

void FrameRecorder::emitShaderDelta(std::span<const Shader* const> shaders) {
    std::set<std::uint64_t> blocks;
    std::set<std::uint64_t> hostPages;
    std::uintptr_t arenaBase = 0;
    std::size_t arenaBytes = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
    const auto note = [&](const void* address, std::uint64_t bytes = 0) {
        if (address == nullptr) return;
        const auto at = pointer(address);
        const bool arena = at >= arenaBase && at - arenaBase < arenaBytes;
        const auto blockBytes = arena ? FrameCaptureWriter::BlockBytes : HostPageBytes;
        const auto begin = at & ~std::uint64_t{blockBytes - 1};
        const auto count = bytes != 0 ? (at - begin + bytes + blockBytes - 1) / blockBytes : 2;
        auto& pages = arena ? blocks : hostPages;
        for (std::uint64_t index = 0; index < count; ++index) pages.insert(begin + index * blockBytes);
    };
    for (const auto* shader : shaders) {
        if (shader == nullptr || !GuestMemory::Accessible(shader, sizeof(Shader))) continue;
        note(shader);
        note(const_cast<const void*>(shader->code), shader->shader_size);
        note(shader->user_data);
        note(shader->cx_registers);
        note(shader->sh_registers);
        note(shader->specials);
        note(shader->input_semantics);
        note(shader->output_semantics);
        if (shader->user_data == nullptr || !GuestMemory::Accessible(shader->user_data, sizeof(ShaderUserData))) continue;
        note(shader->user_data->direct_resource_offset);
        for (const auto* sharps : shader->user_data->sharp_resource_offset) note(sharps);
    }
    if (blocks.empty() && hostPages.empty()) return;
    const auto mark = GuestMemory::WriteGeneration();
    std::vector<Span> spans;
    for (const auto block : blocks) {
        const auto found = shaderMarks.find(block);
        spans.push_back({block, FrameCaptureWriter::BlockBytes, found != shaderMarks.end() ? std::max(found->second, generation) : generation});
    }
    const std::vector<std::uint64_t> pages(hostPages.begin(), hostPages.end());
    writeDelta(spans, pages);
    for (const auto block : blocks) shaderMarks[block] = mark;
}

void FrameRecorder::writeDelta(std::span<const Span> spans, std::span<const std::uint64_t> hostPages, std::span<const Range> added) {
    if (abandonIfDiskLow()) return;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> blocks;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> written;
    for (const auto& span : spans) {
        GuestMemory::CollectWrittenBlocks(span.address, static_cast<std::size_t>(span.bytes), span.since, written);
        blocks.insert(blocks.end(), written.begin(), written.end());
    }
    auto& driver = DriverDetail::Driver::Get();
    auto [accepted, completed] = driver.SerialProgress();
    if (completed < accepted && (!added.empty() || !blocks.empty())) {
        if (!driver.WaitIdleFor(std::chrono::seconds(10))) {
            stop("stopped: the GPU work submitted before a memory delta did not finish within 10 s");
            return;
        }
        std::tie(accepted, completed) = driver.SerialProgress();
    }
    const auto group = deltaGroups + 1;
    bool any = false;
    std::vector<std::byte> bytes;
    for (const auto& range : added) {
        writer->WriteRange(range.address, range.bytes, range.kind, range.protection, range.access);
        ranges.push_back(range);
        ++lateRanges;
        if (!range.readable) {
            unreadableBytes += range.bytes;
            continue;
        }
        for (std::uint64_t at = 0; at < range.bytes;) {
            bytes.resize(static_cast<std::size_t>(std::min<std::uint64_t>(FrameCaptureWriter::BlockBytes, range.bytes - at)));
            GuestMemory::ReadCommittedUnsynced(range.address + at, bytes);
            const bool zero = std::all_of(bytes.begin(), bytes.end(), [](std::byte value) { return value == std::byte{0}; });
            if (!zero) {
                writer->WriteDelta(group, completed, range.address + at, bytes);
                deltaBytes += bytes.size();
                any = true;
            }
            at += bytes.size();
        }
    }
    for (const auto page : hostPages) {
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(page)), HostPageBytes)) continue;
        bytes.resize(HostPageBytes);
        std::memcpy(bytes.data(), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(page)), HostPageBytes);
        const auto hash = contentHash(bytes);
        auto& known = hostPageHashes[page];
        if (known == hash) continue;
        known = hash;
        writer->WriteDelta(group, completed, page, bytes);
        deltaBytes += bytes.size();
        any = true;
    }
    for (const auto& [begin, end] : blocks) {
        bytes.resize(static_cast<std::size_t>(end - begin));
        GuestMemory::ReadCommittedUnsynced(begin, bytes);
        writer->WriteDelta(group, completed, begin, bytes);
        deltaBytes += bytes.size();
        any = true;
    }
    if (!any) return;
    deltaGroups = group;
    auto call = make(FrameCallKind::Memory);
    call.ordinal = ++ordinal;
    call.accepted = accepted;
    call.completed = completed;
    call.values = {group};
    writer->WriteCall(call);
}

void FrameRecorder::snapshot(const Range& range) {
    writer->WriteRange(range.address, range.bytes, range.kind, range.protection, range.access);
    if (!range.readable) {
        unreadableBytes += range.bytes;
        return;
    }
    std::vector<std::byte> bytes;
    for (std::uint64_t at = 0; at < range.bytes; at += SnapshotPieceBytes) {
        if (abandonIfDiskLow()) return;
        bytes.resize(static_cast<std::size_t>(std::min<std::uint64_t>(SnapshotPieceBytes, range.bytes - at)));
        GuestMemory::ReadCommitted(range.address + at, bytes);
        snapshotBytes += writer->WriteSnapshot(range.address + at, bytes);
    }
}

bool FrameRecorder::abandonIfDiskLow() {
    std::error_code error;
    const auto space = std::filesystem::space(directory, error);
    if (error || space.available >= MinimumFreeBytes) return false;
    stop("stopped: less than 4 GiB free on the capture volume");
    return true;
}

void FrameRecorder::stop(const char* reason) {
    warnings.push_back(reason);
    std::fprintf(stderr, "[frame-capture] %s\n", reason);
    try {
        writer->Finish(header());
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "[frame-capture] %s\n", failure.what());
    }
    writer.reset();
    state = State::Done;
}

void FrameRecorder::start() {
    ++startAttempts;
    auto& driver = DriverDetail::Driver::Get();
    if (!driver.WaitIdleFor(std::chrono::seconds(10))) {
        std::fprintf(stderr, "[frame-capture] the driver did not go idle within 10 s at frame boundary %llu (attempt %llu)\n", static_cast<unsigned long long>(boundaries), static_cast<unsigned long long>(startAttempts));
        if (startAttempts >= 5) {
            std::fprintf(stderr, "[frame-capture] giving up: no idle frame boundary\n");
            state = State::Done;
        }
        return;
    }
    startedAt = std::chrono::steady_clock::now();
    startBoundary = boundaries;
    allocationsGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    ranges = committedRanges();
    writer = std::make_unique<FrameCaptureWriter>(directory);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> written;
    for (const auto& range : ranges) {
        if (range.kind == FrameRangeKind::Watched || range.kind == FrameRangeKind::Unwatched) GuestMemory::CollectWrittenBlocks(range.address, static_cast<std::size_t>(range.bytes), std::numeric_limits<std::uint64_t>::max(), written);
    }
    generation = GuestMemory::WriteGeneration();
    for (const auto& range : ranges) {
        snapshot(range);
        if (!writer) return;
    }
    for (const auto& call : prologue) writer->WriteCall(call);
    std::uintptr_t ringBase = 0;
    std::uint32_t ringSize = 0;
    sceAgcDriverGetTFRing(&ringBase, &ringSize);
    auto ring = make(FrameCallKind::SetTessellationFactorRing);
    ring.ordinal = ++ordinal;
    ring.address = ringBase;
    ring.size = ringSize;
    writer->WriteCall(ring);
    const auto pending = driver.PendingEopFlipTokens();
    const std::set<std::uint64_t> open(pending.begin(), pending.end());
    for (const auto& call : reservations) {
        if (open.contains(call.values.back())) writer->WriteCall(call);
    }
    auto begin = make(FrameCallKind::FrameStart);
    begin.ordinal = ++ordinal;
    std::tie(begin.accepted, begin.completed) = driver.SerialProgress();
    begin.values = {boundaries};
    writer->WriteCall(begin);
    state = State::Recording;
    recordedFrames = 0;
    std::fprintf(stderr, "[frame-capture] started at frame boundary %llu: %zu ranges, %.1f MiB snapshot in %.1f s\n", static_cast<unsigned long long>(boundaries), ranges.size(),
                 static_cast<double>(snapshotBytes) / (1024.0 * 1024.0), std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count());
}

void FrameRecorder::finish() {
    emitDelta();
    if (!writer) return;
    auto end = make(FrameCallKind::End);
    end.ordinal = ++ordinal;
    std::tie(end.accepted, end.completed) = DriverDetail::Driver::Get().SerialProgress();
    writer->WriteCall(end);
    writeReferenceFrames();
    writer->Finish(header());
    std::fprintf(stderr, "[frame-capture] %llu frames, %llu calls, %llu memory deltas (%.1f MiB), %.1f MiB raw, %.1f MiB stored in %s\n", static_cast<unsigned long long>(recordedFrames),
                 static_cast<unsigned long long>(recordedCalls), static_cast<unsigned long long>(deltaGroups), static_cast<double>(deltaBytes) / (1024.0 * 1024.0),
                 static_cast<double>(writer->RawBytes()) / (1024.0 * 1024.0), static_cast<double>(writer->StoredBytes()) / (1024.0 * 1024.0), directory.string().c_str());
    writer.reset();
    state = State::Done;
}

std::string FrameRecorder::header() const {
    std::uintptr_t arenaBase = 0;
    std::size_t arenaBytes = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
    std::string text = "{\"magic\":\"aps5-frame-capture 1\"";
    text += ",\"commit\":" + jsonString(APS5_TRACE_COMMIT);
    text += std::string(",\"dirty\":") + (APS5_TRACE_DIRTY < 0 ? "null" : APS5_TRACE_DIRTY != 0 ? "true" : "false");
    text += ",\"arena_base\":" + std::to_string(arenaBase) + ",\"arena_bytes\":" + std::to_string(arenaBytes);
    text += ",\"start_boundary\":" + std::to_string(startBoundary) + ",\"frames\":" + std::to_string(recordedFrames);
    text += ",\"calls\":" + std::to_string(recordedCalls) + ",\"prologue_calls\":" + std::to_string(prologue.size());
    text += ",\"ranges\":" + std::to_string(ranges.size()) + ",\"late_ranges\":" + std::to_string(lateRanges) + ",\"snapshot_bytes\":" + std::to_string(snapshotBytes) + ",\"unreadable_bytes\":" + std::to_string(unreadableBytes);
    text += ",\"memory_groups\":" + std::to_string(deltaGroups) + ",\"delta_bytes\":" + std::to_string(deltaBytes);
    text += ",\"capture_seconds\":" + std::to_string(std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count());
    text += ",\"env\":{";
    bool first = true;
    for (const auto& [name, value] : captureEnvironment()) {
        text += (first ? "" : ",") + jsonString(name) + ":" + jsonString(value);
        first = false;
    }
    text += "},\"warnings\":[";
    first = true;
    for (const auto& warning : warnings) {
        text += (first ? "" : ",") + jsonString(warning);
        first = false;
    }
    return text + "]}\n";
}

std::vector<FrameRecorder::Range> FrameRecorder::committedRanges() {
#ifdef _WIN32
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    if (size == 0) throw std::runtime_error("frame capture: the guest arena is not reserved");
    std::vector<Range> result;
    for (const auto& allocation : GuestAllocations::GuestAllocationsAcquireAll_nid_postfix()) {
        if (!allocation->readable) continue;
        const auto first = allocation->address;
        const auto end = first + allocation->bytes;
        const bool host = first < base || end > base + size;
        const auto access = FrameRangeAccess::Readable | (allocation->writable ? FrameRangeAccess::Writable : 0u) | (allocation->gpu ? FrameRangeAccess::Gpu : 0u);
        for (auto address = first; address < end;) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), &info, sizeof(info)) == 0) throw std::runtime_error("frame capture: VirtualQuery failed inside the guest arena");
            const auto regionEnd = std::min<std::uint64_t>(end, reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize);
            if (info.State == MEM_COMMIT) {
                Range range;
                range.address = address;
                range.bytes = regionEnd - address;
                range.protection = info.Protect;
                range.access = access;
                const auto page = info.Protect & 0xffu;
                range.readable = (info.Protect & PAGE_GUARD) == 0 && (page == PAGE_READONLY || page == PAGE_READWRITE || page == PAGE_WRITECOPY || page == PAGE_EXECUTE_READ || page == PAGE_EXECUTE_READWRITE || page == PAGE_EXECUTE_WRITECOPY);
                if (host) range.kind = FrameRangeKind::Host;
                else if (info.Type == MEM_PRIVATE) range.kind = GuestMemory::Watched(range.address, static_cast<std::size_t>(range.bytes)) ? FrameRangeKind::Watched : FrameRangeKind::Unwatched;
                else range.kind = FrameRangeKind::Section;
                auto& last = result.empty() ? range : result.back();
                if (!result.empty() && last.address + last.bytes == range.address && last.kind == range.kind && last.protection == range.protection && last.readable == range.readable && last.access == range.access) last.bytes += range.bytes;
                else result.push_back(range);
            }
            address = regionEnd;
        }
    }
    return result;
#else
    throw std::runtime_error("frame capture: guest memory capture is implemented on Windows only");
#endif
}

}
