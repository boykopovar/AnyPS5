#include "prx/libSceAgcDriver/State/include/Configuration.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/TraceRevision.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
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
extern char** environ;
#endif
#ifdef __linux__
#include "prx/libc/include/specifics/linux/HostMapping.hpp"
#include <sys/uio.h>
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

#ifdef __linux__
std::uint32_t windowsProtection(const HostMapping::Mapping& mapping) {
    if (mapping.executable) return mapping.writable ? 0x40u : mapping.readable ? 0x20u : 0x10u;
    if (mapping.writable) return 0x04u;
    return mapping.readable ? 0x02u : 0x01u;
}

bool pageReadable(std::uint64_t address) {
    char byte = 0;
    iovec local{&byte, 1};
    iovec remote{reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), 1};
    const auto read = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
    if (read == 1) return true;
    if (read < 0 && errno == EFAULT) return false;
    throw std::runtime_error(std::string("frame capture: cannot probe a file mapping with process_vm_readv: ") + (read < 0 ? std::strerror(errno) : "nothing was read"));
}

std::uint64_t fileBackedEnd(const HostMapping::Mapping& mapping) {
    if (mapping.inode == 0 || !mapping.readable) return mapping.end;
    const auto page = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
    if (pageReadable(mapping.end - page)) return mapping.end;
    if (!pageReadable(mapping.start)) return mapping.start;
    std::uint64_t readable = 0;
    std::uint64_t unreadable = (mapping.end - mapping.start) / page - 1;
    while (unreadable - readable > 1) {
        const auto middle = readable + (unreadable - readable) / 2;
        if (pageReadable(mapping.start + middle * page)) readable = middle;
        else unreadable = middle;
    }
    return mapping.start + unreadable * page;
}
#endif

std::uint64_t contentHash(std::span<const std::byte> bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const auto value : bytes) hash = (hash ^ static_cast<std::uint64_t>(value)) * 0x100000001b3ull;
    return hash == 0 ? 1 : hash;
}

constexpr std::array<const char*, 17> HoldReasonNames{"", "unwatched", "late_or_host", "threads", "parked", "driver_page", "label", "snapshot", "storage", "shadow", "queue0", "queued", "awaits_title", "parked_on_title", "driver_page_since_full", "title_wait_unpassed", "wait_dword_in_delta"};

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
    : directory(std::move(directory)), trigger(std::move(trigger)), atBoundary(atBoundary), frames(frames), origin(std::chrono::steady_clock::now()) {
    observing.store(this, std::memory_order_release);
}

std::atomic<FrameRecorder*> FrameRecorder::observing{nullptr};

void FrameRecorder::observeDriverStore(std::uint64_t begin, std::uint64_t end, std::uint64_t serial) {
    if (auto* const recorder = observing.load(std::memory_order_acquire)) recorder->noteDriverStore(begin, end, serial);
}

void FrameRecorder::noteDriverStore(std::uint64_t begin, std::uint64_t end, std::uint64_t serial) {
    if (end <= begin) return;
    std::lock_guard lock(storeMutex);
    if (serial == 0) ++unattributedStores;
    ++driverStores;
    for (auto page = begin & ~std::uint64_t{HostPageBytes - 1}; page < end; page += HostPageBytes) {
        driverPages[page] = driverStores;
        if (storeHazard.group != 0 || serial == GuestMemory::TitleCallStoreSerial) continue;
        const auto found = storeOwners.find(page);
        if (found != storeOwners.end() && (serial == 0 || serial <= found->second.accepted)) storeHazard = {found->second.group, page, serial};
    }
}

std::uint32_t FrameRecorder::holdMask(std::span<const std::pair<std::uint64_t, std::uint64_t>> blocks, std::uint64_t& checked) {
    std::uint32_t mask = 0;
    auto& driver = DriverDetail::Driver::Get();
    for (const auto& wait : driver.AwaitedWaits()) {
        if (wait.address == 0) continue;
        mask |= 1u << 4;
        for (auto dword = wait.address; dword < wait.address + std::max<std::uint64_t>(wait.bytes, 4) && (mask & (1u << 13)) == 0; dword += 4) {
            if (!driver.UnfinishedLabelOverlaps(dword, dword + 4)) mask |= 1u << 13;
        }
    }
    {
        std::lock_guard lock(storeMutex);
        checked = driverStores;
        for (const auto& [begin, end] : blocks) {
            for (auto page = begin & ~std::uint64_t{HostPageBytes - 1}; page < end && (mask & (1u << 14)) == 0; page += HostPageBytes) {
                const auto found = driverPages.find(page);
                if (found != driverPages.end()) mask |= found->second > fullDeltaStores ? (1u << 5) | (1u << 14) : 1u << 5;
            }
        }
    }
    for (const auto& [begin, end] : blocks) {
        if ((mask & (1u << 6)) == 0 && driver.UnfinishedLabelOverlaps(begin, end)) mask |= 1u << 6;
        if ((mask & (1u << 7)) == 0 && Graphics::Recorder::SnapshotWriteOverlaps(begin, static_cast<std::size_t>(end - begin))) mask |= 1u << 7;
    }
    if (Graphics::StorageTexture::AnyPendingOverlaps(blocks)) mask |= 1u << 8;
    if (Graphics::AnyShadowedOverlaps(blocks)) mask |= 1u << 9;
    if (DriverDetail::DrawPipeline::Queue0().OverlapsAny(blocks)) mask |= 1u << 10;
    return mask;
}

void FrameRecorder::countUnwatched(std::vector<std::pair<std::uint64_t, std::uint64_t>>& unwatched, std::vector<std::pair<std::uint64_t, std::uint64_t>>& blocks) {
    std::vector<std::byte> bytes;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> held;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unchanged;
    for (const auto& [begin, end] : unwatched) {
        bytes.resize(static_cast<std::size_t>(end - begin));
        ReadAroundRanges(begin, bytes, Graphics::HostOnlySampleRanges(begin, bytes.size()), GuestMemory::ReadCommittedUnsynced);
        bool changed = false;
        for (auto page = begin & ~std::uint64_t{HostPageBytes - 1}; page < end && !changed; page += HostPageBytes) {
            const auto first = std::max(page, begin);
            const auto found = unwatchedPages.find(page);
            changed = found == unwatchedPages.end() || found->second.hash != contentHash(std::span<const std::byte>(bytes).subspan(first - begin, std::min<std::uint64_t>(page + HostPageBytes, end) - first));
        }
        bool stored = false;
        {
            std::lock_guard lock(storeMutex);
            for (auto page = begin & ~std::uint64_t{HostPageBytes - 1}; page < end && !stored; page += HostPageBytes) {
                const auto store = driverPages.find(page);
                const auto found = unwatchedPages.find(page);
                stored = store != driverPages.end() && store->second > (found != unwatchedPages.end() ? found->second.stores : startStores);
            }
        }
        auto& counts = unwatchedCounts[{begin, end}];
        ++counts.deltas;
        counts.changed += changed ? 1 : 0;
        counts.stored += stored ? 1 : 0;
        if (stored) held.emplace_back(begin, end);
        else if (!changed) unchanged.emplace_back(begin, end);
    }
    std::erase_if(blocks, [&](const auto& block) { return std::find(unchanged.begin(), unchanged.end(), block) != unchanged.end(); });
    unwatched = std::move(held);
}

void FrameRecorder::recordUnwatched(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stores) {
    const auto end = address + bytes.size();
    for (auto page = address & ~std::uint64_t{HostPageBytes - 1}; page < end; page += HostPageBytes) {
        const auto first = std::max(page, address);
        unwatchedPages[page] = {contentHash(bytes.subspan(first - address, std::min<std::uint64_t>(page + HostPageBytes, end) - first)), stores};
    }
}

bool FrameRecorder::arm(std::span<const std::pair<std::uint64_t, std::uint64_t>> blocks, std::uint64_t group, std::uint64_t accepted, std::uint64_t checked) {
    std::lock_guard lock(storeMutex);
    for (const auto& [begin, end] : blocks) {
        for (auto page = begin & ~std::uint64_t{HostPageBytes - 1}; page < end; page += HostPageBytes) {
            const auto found = driverPages.find(page);
            if (found != driverPages.end() && found->second > checked) return false;
        }
    }
    for (const auto& [begin, end] : blocks) {
        for (auto page = begin & ~std::uint64_t{HostPageBytes - 1}; page < end; page += HostPageBytes) storeOwners[page] = {group, accepted};
    }
    return true;
}

bool FrameRecorder::stopOnHazard() {
    StoreHazard hazard;
    {
        std::lock_guard lock(storeMutex);
        hazard = storeHazard;
    }
    if (hazard.group == 0) return false;
    const auto reason = "stopped: memory delta " + std::to_string(hazard.group) + " was read while submission " + (hazard.serial == 0 ? std::string("(unknown)") : std::to_string(hazard.serial)) + ", in flight at its read, stored into its page 0x" + [&] {
        char text[32];
        std::snprintf(text, sizeof text, "%llx", static_cast<unsigned long long>(hazard.page));
        return std::string(text);
    }();
    stop(reason.c_str());
    return true;
}

void FrameRecorder::awaitLateStores() {
    auto& driver = DriverDetail::Driver::Get();
    driver.ReleaseForRecorder();
    const GuestMemory::StoreSerialScope unattributed(0);
    std::uint64_t newest = 0;
    {
        std::lock_guard lock(storeMutex);
        if (storeOwners.empty()) return;
        for (const auto& [page, owner] : storeOwners) newest = std::max(newest, owner.accepted);
    }
    if (!driver.WaitCompletedFor(newest, std::chrono::seconds(10))) {
        std::set<std::uint64_t> groups;
        {
            std::lock_guard lock(storeMutex);
            for (const auto& [page, owner] : storeOwners) groups.insert(owner.group);
        }
        unverifiedDeltas = groups.size();
        warnings.push_back(std::to_string(unverifiedDeltas) + " memory deltas read while submissions were in flight were never verified: the submissions in flight at their reads did not complete within 10 s after the capture");
        return;
    }
    driver.WaitDeviceIdle();
    std::vector<std::uint64_t> pages;
    {
        std::lock_guard lock(storeMutex);
        pages.reserve(storeOwners.size());
        for (const auto& [page, owner] : storeOwners) pages.push_back(page);
    }
    std::sort(pages.begin(), pages.end());
    for (std::size_t first = 0; first < pages.size();) {
        auto last = first + 1;
        while (last < pages.size() && pages[last] == pages[last - 1] + HostPageBytes) ++last;
        GuestMemory::FlushGpuWrites(pages[first], static_cast<std::size_t>(pages[last - 1] + HostPageBytes - pages[first]));
        first = last;
    }
}

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
    if (state != State::Recording || stopOnHazard()) return;
    callThreads.insert(call.thread);
    const auto entry = DriverDetail::Driver::Get().StartedProgress();
    {
        const struct Released {
            Released() { DriverDetail::Driver::Get().HoldForRecorder(); }
            ~Released() { DriverDetail::Driver::Get().ReleaseForRecorder(); }
        } released;
        emitDelta(&entry);
    }
    if (state != State::Recording) return;
    call.ordinal = ++ordinal;
    call.accepted = entry.accepted;
    call.completed = entry.completed;
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
    if (!recordLocked(call, prologueCall, shaders)) {
        if (accept) accept();
        return;
    }
    if (accept) {
        auto& driver = DriverDetail::Driver::Get();
        const auto before = driver.SerialProgress().first;
        accept();
        const auto after = driver.SerialProgress().first;
        if (after - before != 1 && (call.kind != FrameCallKind::Flip || after != before)) {
            stop(("stopped: recorded call " + std::to_string(call.ordinal) + " (" + std::string(FrameCallName(call.kind)) + ") moved the driver's accepted serial from " + std::to_string(before) + " to " + std::to_string(after) + ", so the serials of later calls cannot be mapped").c_str());
            return;
        }
        call.values.push_back(after == before ? 0 : after);
    }
    writer->WriteCall(call);
    ++recordedCalls;
}

bool FrameRecorder::recordLocked(FrameCall& call, bool prologueCall, std::span<const Shader* const> shaders) {
    if (state == State::Waiting) {
        if (!prologueCall) return false;
        std::set<std::uint64_t> blocks;
        collectShaderPages(shaders, blocks, prologueHostPages);
        call.ordinal = ++ordinal;
        prologue.push_back(std::move(call));
        return false;
    }
    if (state != State::Recording || stopOnHazard()) return false;
    callThreads.insert(call.thread);
    const auto entry = DriverDetail::Driver::Get().StartedProgress();
    const struct Released {
        Released() { DriverDetail::Driver::Get().HoldForRecorder(); }
        ~Released() { DriverDetail::Driver::Get().ReleaseForRecorder(); }
    } released;
    if (prologueCall) emitShaderDelta(shaders, &entry);
    else emitDelta(&entry);
    if (state != State::Recording) return false;
    call.ordinal = ++ordinal;
    call.accepted = entry.accepted;
    call.completed = entry.completed;
    if (call.kind == FrameCallKind::Submit) ++submitDepths[static_cast<std::size_t>(std::min<std::uint64_t>(entry.accepted - entry.completed, submitDepths.size() - 1))];
    return true;
}

void FrameRecorder::FrameBoundary() {
    std::lock_guard lock(mutex);
    frameBoundaryLocked();
}

void FrameRecorder::frameBoundaryLocked() {
    ++boundaries;
    if (state == State::Recording) {
        if (++recordedFrames >= frames) finish();
        return;
    }
    if (state == State::Waiting && triggered()) start();
}

void FrameRecorder::RewindTailFlips(std::uint64_t flips) {
    std::lock_guard lock(mutex);
    while (rewindTailFlips < flips) {
        ++rewindTailFlips;
        frameBoundaryLocked();
    }
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

void FrameRecorder::emitDelta(const StartedProgressSnapshot* decision) {
    std::vector<Span> spans;
    std::vector<std::size_t> owners;
    for (std::size_t index = 0; index < ranges.size(); ++index) {
        const auto& range = ranges[index];
        if (!range.readable || ((range.access & FrameRangeAccess::Writable) == 0 && range.kind != FrameRangeKind::Watched && range.kind != FrameRangeKind::Unwatched)) continue;
        spans.push_back({range.address, range.bytes, range.since});
        owners.push_back(index);
    }
    const auto added = newRanges();
    writeDelta(spans, {}, added, decision);
    for (std::size_t index = 0; index < owners.size(); ++index) ranges[owners[index]].since = spans[index].since;
    fullDeltaStores = readStores;
    finishedWaitDwords.clear();
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

void FrameRecorder::collectShaderPages(std::span<const Shader* const> shaders, std::set<std::uint64_t>& blocks, std::set<std::uint64_t>& hostPages) {
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
}

void FrameRecorder::emitShaderDelta(std::span<const Shader* const> shaders, const StartedProgressSnapshot* decision) {
    std::set<std::uint64_t> blocks;
    std::set<std::uint64_t> hostPages;
    collectShaderPages(shaders, blocks, hostPages);
    if (blocks.empty() && hostPages.empty()) return;
    const auto added = newRanges();
    std::map<std::uint64_t, std::uint64_t> captured;
    for (const auto& list : {std::span<const Range>(ranges), std::span<const Range>(added)}) {
        for (const auto& range : list) {
            if (!range.readable) continue;
            auto& end = captured[range.address];
            end = std::max(end, range.address + range.bytes);
        }
    }
    const auto inside = [&](std::uint64_t begin, std::uint64_t end, const auto& visit) {
        auto range = captured.upper_bound(begin);
        if (range != captured.begin()) --range;
        for (; range != captured.end() && range->first < end; ++range) {
            const auto first = std::max(begin, range->first);
            const auto stop = std::min(end, range->second);
            if (first < stop) visit(first, stop);
        }
    };
    std::vector<Span> spans;
    for (const auto block : blocks) {
        const auto since = blockSince(block);
        inside(block, block + FrameCaptureWriter::BlockBytes, [&](std::uint64_t first, std::uint64_t stop) { spans.push_back({first, stop - first, since}); });
    }
    const auto blockSpans = spans.size();
    std::vector<std::uint64_t> pages;
    for (const auto page : hostPages) {
        std::uint64_t covered = 0;
        inside(page, page + HostPageBytes, [&](std::uint64_t first, std::uint64_t stop) { covered += stop - first; });
        if (covered == HostPageBytes) pages.push_back(page);
        else inside(page, page + HostPageBytes, [&](std::uint64_t first, std::uint64_t stop) { spans.push_back({first, stop - first, startGeneration}); });
    }
    writeDelta(spans, pages, added, decision);
    std::map<std::uint64_t, std::uint64_t> marks;
    for (std::size_t index = 0; index < blockSpans; ++index) {
        const auto block = spans[index].address & ~std::uint64_t{FrameCaptureWriter::BlockBytes - 1};
        const auto [found, inserted] = marks.try_emplace(block, spans[index].since);
        if (!inserted) found->second = std::min(found->second, spans[index].since);
    }
    for (const auto& [block, since] : marks) shaderMarks[block] = since;
}

std::uint64_t FrameRecorder::blockSince(std::uint64_t block) const {
    const auto end = block + FrameCaptureWriter::BlockBytes;
    const auto found = shaderMarks.find(block);
    const std::uint64_t mark = found != shaderMarks.end() ? found->second : startGeneration;
    std::uint64_t covered = 0;
    auto since = std::numeric_limits<std::uint64_t>::max();
    for (const auto& range : ranges) {
        if ((range.kind != FrameRangeKind::Watched && range.kind != FrameRangeKind::Unwatched) || range.address >= end || block >= range.address + range.bytes) continue;
        covered += std::min(end, range.address + range.bytes) - std::max(block, range.address);
        since = std::min(since, range.since);
    }
    return covered == FrameCaptureWriter::BlockBytes ? std::max(mark, since) : mark;
}

void FrameRecorder::writeDelta(std::span<Span> spans, std::span<const std::uint64_t> hostPages, std::span<const Range> added, const StartedProgressSnapshot* decision) {
    if (abandonIfDiskLow()) return;
    auto& driver = DriverDetail::Driver::Get();
    const struct Hold {
        DriverDetail::Driver& driver;
        ~Hold() { driver.ReleaseSettled(); }
    } hold{driver};
    const auto entry = decision != nullptr ? *decision : driver.StartedProgress();
    const auto awaited = driver.TakeAwaitedDwords();
    finishedWaitDwords.insert(finishedWaitDwords.end(), awaited.finished.begin(), awaited.finished.end());
    auto accepted = entry.accepted;
    auto completed = entry.completed;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> blocks;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> written;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwatched;
    std::vector<std::uint64_t> marks;
    for (const auto& span : spans) marks.push_back(span.since);
    const auto collect = [&] {
        blocks.clear();
        unwatched.clear();
        for (std::size_t index = 0; index < spans.size(); ++index) {
            auto& span = spans[index];
            if (const auto since = GuestMemory::CollectWrittenBlocks(span.address, static_cast<std::size_t>(span.bytes), marks[index], written); since != 0) span.since = since;
            blocks.insert(blocks.end(), written.begin(), written.end());
            for (std::uint64_t offset = 0; offset < span.bytes;) {
                const auto at = span.address + offset;
                const auto bytes = static_cast<std::size_t>(std::min<std::uint64_t>(FrameCaptureWriter::BlockBytes, span.bytes - offset));
                if (!GuestMemory::Watched(at, bytes)) {
                    blocks.emplace_back(at, at + bytes);
                    unwatched.emplace_back(at, at + bytes);
                }
                offset += bytes;
            }
        }
    };
    collect();
    const auto group = deltaGroups + 1;
    std::vector<std::uint64_t> started;
    bool inFlightRead = false;
    if (completed < accepted && !unwatched.empty()) countUnwatched(unwatched, blocks);
    if (completed < accepted && (!added.empty() || !blocks.empty() || !hostPages.empty())) {
        const auto holdsWait = [&](std::span<const std::pair<std::uint64_t, std::uint64_t>> waits) {
            return std::any_of(waits.begin(), waits.end(), [&](const auto& wait) { return std::any_of(blocks.begin(), blocks.end(), [&](const auto& block) { return wait.first < block.second && block.first < wait.second; }); });
        };
        const auto awaitsTitle = (awaited.title ? 1u << 15 : 0u) | (holdsWait(awaited.unfinished) || holdsWait(finishedWaitDwords) ? 1u << 16 : 0u);
        std::uint64_t checked = 0;
        auto mask = holdMask(blocks, checked) | (unwatched.empty() ? 0u : 1u << 1) | (!added.empty() || !hostPages.empty() ? 1u << 2 : 0u) | (callThreads.size() > 1 ? 1u << 3 : 0u) | (entry.queued != 0 ? 1u << 11 : 0u) | (awaitsTitle != 0 ? awaitsTitle | (1u << 12) : 0u);
        if (mask == 0 && !arm(blocks, group, accepted, checked)) mask = (1u << 5) | (1u << 14);
        ++holdMasks[mask];
        for (std::size_t reason = 1; reason < holdReasons.size(); ++reason) holdReasons[reason] += (mask >> reason) & 1u;
        if (mask != 0) {
            driver.ReleaseForRecorder();
            if (!driver.WaitSettledFor(std::chrono::seconds(10))) {
                stop("stopped: the GPU work submitted before a memory delta neither finished nor waited for the title within 10 s");
                return;
            }
            std::tie(accepted, completed) = driver.SerialProgress();
            ++heldDeltas;
            collect();
        } else {
            started = entry.started;
            inFlightRead = true;
            ++inFlightDeltas;
            ++readDepths[static_cast<std::size_t>(std::min<std::uint64_t>(accepted - completed, readDepths.size() - 1))];
        }
    }
    std::vector<std::array<std::uint64_t, 4>> parked;
    if (!inFlightRead) {
        for (const auto& wait : driver.AwaitedWaits()) {
            if (wait.bytes == 0) parked.push_back({wait.address, 0, 0, 0});
            else parked.push_back({wait.address, wait.reference, wait.mask, wait.function | (std::uint64_t{wait.bytes} << 32u)});
        }
        std::ranges::sort(parked);
    }
    std::uint64_t stores = 0;
    {
        std::lock_guard lock(storeMutex);
        stores = driverStores;
    }
    readStores = stores;
    bool any = false;
    std::vector<std::byte> bytes;
    std::vector<std::pair<std::size_t, std::size_t>> runs;
    const auto writeTitleWords = [&](std::uint64_t address, bool zero = false) {
        keepTitleWords(address, bytes, runs, zero);
        for (const auto& [offset, length] : runs) {
            writer->WriteDelta(group, completed, address + offset, std::span<const std::byte>(bytes).subspan(offset, length));
            deltaBytes += length;
            any = true;
        }
    };
    const auto forget = [&](std::uint64_t begin, std::uint64_t end) {
        hostPageHashes.erase(hostPageHashes.lower_bound(begin & ~std::uint64_t{HostPageBytes - 1}), hostPageHashes.lower_bound(end));
        auto found = unwatchedHashes.lower_bound({begin - std::min<std::uint64_t>(begin, FrameCaptureWriter::BlockBytes), 0});
        while (found != unwatchedHashes.end() && found->first.first < end) {
            if (found->first.first + found->first.second > begin) found = unwatchedHashes.erase(found);
            else ++found;
        }
    };
    for (const auto& range : added) {
        writer->WriteRange(range.address, range.bytes, range.kind, range.protection, range.access);
        auto& stored = ranges.emplace_back(range);
        const bool collected = stored.kind == FrameRangeKind::Watched || stored.kind == FrameRangeKind::Unwatched;
        if (collected) stored.since = GuestMemory::CollectWrittenBlocks(stored.address, static_cast<std::size_t>(stored.bytes), std::numeric_limits<std::uint64_t>::max(), written);
        ++lateRanges;
        if (!range.readable) {
            unreadableBytes += range.bytes;
            continue;
        }
        for (std::uint64_t at = 0; at < range.bytes;) {
            bytes.resize(static_cast<std::size_t>(std::min<std::uint64_t>(FrameCaptureWriter::BlockBytes, range.bytes - at)));
            readGuest(range.address + at, bytes);
            const bool zero = std::all_of(bytes.begin(), bytes.end(), [](std::byte value) { return value == std::byte{0}; });
            writeTitleWords(range.address + at, zero && !collected);
            if (!GuestMemory::Watched(range.address + at, bytes.size())) recordUnwatched(range.address + at, bytes, stores);
            at += bytes.size();
        }
    }
    for (const auto page : hostPages) {
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(page)), HostPageBytes)) continue;
        bytes.resize(HostPageBytes);
        std::memcpy(bytes.data(), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(page)), HostPageBytes);
        const auto hash = contentHash(bytes);
        if (const auto found = hostPageHashes.find(page); found != hostPageHashes.end() && found->second == hash) continue;
        forget(page, page + HostPageBytes);
        hostPageHashes[page] = hash;
        writer->WriteDelta(group, completed, page, bytes);
        recordUnwatched(page, bytes, stores);
        deltaBytes += bytes.size();
        any = true;
    }
    for (const auto& [begin, end] : blocks) {
        bytes.resize(static_cast<std::size_t>(end - begin));
        readGuest(begin, bytes);
        if (accepted == completed && !GuestMemory::Watched(begin, bytes.size())) {
            const auto key = std::pair{begin, static_cast<std::uint64_t>(bytes.size())};
            const auto sample = std::pair{contentHash(bytes), accepted};
            const auto found = unwatchedHashes.find(key);
            if (found != unwatchedHashes.end() && found->second == sample) continue;
            forget(begin, end);
            unwatchedHashes[key] = sample;
        } else {
            forget(begin, end);
        }
        if (!GuestMemory::Watched(begin, bytes.size())) recordUnwatched(begin, bytes, stores);
        writeTitleWords(begin);
    }
    if (!inFlightRead) {
        std::lock_guard lock(storeMutex);
        for (const auto& [begin, end] : blocks) {
            for (auto page = begin & ~std::uint64_t{HostPageBytes - 1}; page < end && !storeOwners.empty(); page += HostPageBytes) {
                const auto found = storeOwners.find(page);
                if (found != storeOwners.end() && found->second.accepted <= completed) storeOwners.erase(found);
            }
        }
    }
    if (!any && !inFlightRead) return;
    deltaGroups = group;
    auto call = make(FrameCallKind::Memory);
    call.ordinal = ++ordinal;
    call.accepted = accepted;
    call.completed = completed;
    if (inFlightRead) {
        call.values = {group};
        call.values.insert(call.values.end(), started.begin(), started.end());
        call.flags = InFlightDeltaFlag;
    } else {
        call.values = {group, parked.size()};
        for (const auto& wait : parked) call.values.insert(call.values.end(), wait.begin(), wait.end());
    }
    writer->WriteCall(call);
}

void FrameRecorder::endGpuStores() {
    notingGpuStores = false;
    DriverDetail::Driver::Get().TrackAwaitedDwords(false);
    std::lock_guard lock(gpuStoresMutex);
    gpuWords.clear();
    gpuFills.clear();
}

void FrameRecorder::noteGpuStore(std::uint64_t address, std::span<const std::byte> bytes) {
    if (bytes.empty()) return;
    const auto end = address + bytes.size();
    std::lock_guard lock(gpuStoresMutex);
    forgetGpuStores(address, end);
    if (bytes.size() > GpuStoreWordLimit) return;
    for (auto word = (address + 3) & ~std::uint64_t{3}; word + 4 <= end; word += 4) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data() + (word - address), sizeof(value));
        gpuWords.emplace_hint(gpuWords.end(), word, value);
    }
}

void FrameRecorder::noteGpuFill(std::uint64_t address, std::uint64_t bytes, const std::array<std::uint32_t, 4>& pattern) {
    if (bytes == 0) return;
    std::lock_guard lock(gpuStoresMutex);
    forgetGpuStores(address, address + bytes);
    if (address % 16 == 0 && bytes % 16 == 0) gpuFills.emplace(address, GpuFill{address + bytes, pattern});
}

void FrameRecorder::forgetGpuStores(std::uint64_t begin, std::uint64_t end) {
    const auto low = begin & ~std::uint64_t{3};
    const auto high = (end + 3) & ~std::uint64_t{3};
    gpuWords.erase(gpuWords.lower_bound(low), gpuWords.lower_bound(end));
    auto fill = gpuFills.upper_bound(begin);
    if (fill != gpuFills.begin()) --fill;
    while (fill != gpuFills.end() && fill->first < end) {
        if (fill->second.end <= begin) {
            ++fill;
            continue;
        }
        const auto start = fill->first;
        const auto kept = fill->second;
        fill = gpuFills.erase(fill);
        if (start < low) gpuFills.emplace(start, GpuFill{low, kept.pattern});
        if (high < kept.end) gpuFills.emplace(high, GpuFill{kept.end, kept.pattern});
    }
}

void FrameRecorder::keepTitleWords(std::uint64_t address, std::span<const std::byte> bytes, std::vector<std::pair<std::size_t, std::size_t>>& runs, bool zero) {
    runs.clear();
    const auto end = address + bytes.size();
    std::lock_guard lock(gpuStoresMutex);
    auto word = gpuWords.lower_bound(address);
    auto fill = gpuFills.upper_bound(address);
    if (fill != gpuFills.begin()) --fill;
    if (fill != gpuFills.end() && fill->second.end <= address) ++fill;
    const bool words = word != gpuWords.end() && word->first < end;
    const bool fills = fill != gpuFills.end() && fill->first < end;
    if (zero && !words && !fills) return;
    if (address % 4 != 0 || bytes.size() % 4 != 0 || (!words && !fills)) {
        if (!bytes.empty()) runs.emplace_back(0, bytes.size());
        return;
    }
    std::size_t open = bytes.size();
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        const auto at = address + offset;
        std::uint32_t current = 0;
        std::memcpy(&current, bytes.data() + offset, sizeof(current));
        bool driver = false;
        if (word != gpuWords.end() && word->first == at) {
            driver = word->second == current;
            ++word;
        } else {
            while (fill != gpuFills.end() && fill->second.end <= at) ++fill;
            driver = fill != gpuFills.end() && fill->first <= at && fill->second.pattern[(at % 16) / 4] == current;
        }
        if (driver) {
            gpuStoreBytes += 4;
            if (open != bytes.size()) runs.emplace_back(open, offset - open);
            open = bytes.size();
        } else if (open == bytes.size()) {
            open = offset;
        }
    }
    if (open != bytes.size()) runs.emplace_back(open, bytes.size() - open);
    for (const auto& [offset, length] : runs) forgetGpuStores(address + offset, address + offset + length);
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
        readGuest(range.address + at, bytes);
        snapshotBytes += writer->WriteSnapshot(range.address + at, bytes);
    }
}

void FrameRecorder::readGuest(std::uint64_t address, std::span<std::byte> bytes) {
    hostOnlySampleBytes += ReadAroundRanges(address, bytes, Graphics::HostOnlySampleRanges(address, bytes.size()), GuestMemory::ReadCommitted);
}

bool FrameRecorder::abandonIfDiskLow() {
    std::error_code error;
    const auto space = std::filesystem::space(directory, error);
    static const auto minimumFree = readNumber(std::getenv("APS5_FRAME_CAPTURE_MIN_FREE_BYTES"), MinimumFreeBytes);
    if (error || space.available >= minimumFree) return false;
    stop("stopped: less than 4 GiB free on the capture volume");
    return true;
}

void FrameRecorder::stop(const char* reason) {
    GuestMemory::SetOwnStoreCompare(false);
    warnings.push_back(reason);
    std::fprintf(stderr, "[frame-capture] %s\n", reason);
    bool hazard = false;
    {
        std::lock_guard lock(storeMutex);
        hazard = storeHazard.group != 0;
    }
    try {
        if (!hazard) awaitLateStores();
        writer->Finish(header());
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "[frame-capture] %s\n", failure.what());
    }
    GuestMemory::SetDriverStoreObserver(nullptr);
    writer.reset();
    endGpuStores();
    state = State::Done;
}

void FrameRecorder::start() {
    ++startAttempts;
    auto& driver = DriverDetail::Driver::Get();
    if (!driver.WaitIdleUnlessTitleWaitsFor(std::chrono::seconds(10))) {
        if (startAttempts >= 600) {
            std::fprintf(stderr, "[frame-capture] giving up: the driver was not idle at any of %llu frame boundaries\n", static_cast<unsigned long long>(startAttempts));
            state = State::Done;
        }
        return;
    }
    rewindTailFlips = std::max(rewindTailFlips, driver.RewindTailFlips());
    if (!GuestMemory::WriteWatched()) throw std::runtime_error("frame capture: the guest write watch is unavailable, so CPU stores during the frames cannot be recorded");
    GuestMemory::SetOwnStoreCompare(true);
    startedAt = std::chrono::steady_clock::now();
    startBoundary = boundaries;
    allocationsGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    ranges = committedRanges();
    writer = std::make_unique<FrameCaptureWriter>(directory);
    notingGpuStores = true;
    finishedWaitDwords.clear();
    driver.TrackAwaitedDwords(true);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> written;
    for (auto& range : ranges) {
        if (range.kind == FrameRangeKind::Watched || range.kind == FrameRangeKind::Unwatched) range.since = GuestMemory::CollectWrittenBlocks(range.address, static_cast<std::size_t>(range.bytes), std::numeric_limits<std::uint64_t>::max(), written);
    }
    {
        std::lock_guard lock(storeMutex);
        driverPages.clear();
        startStores = driverStores;
        fullDeltaStores = driverStores;
    }
    GuestMemory::SetDriverStoreObserver(&FrameRecorder::observeDriverStore);
    startGeneration = GuestMemory::TrackerGeneration();
    for (const auto& range : ranges) {
        snapshot(range);
        if (!writer) return;
    }
    for (const auto page : prologueHostPages) {
        const auto* address = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(page));
        if (!GuestMemory::Accessible(address, HostPageBytes)) continue;
        const bool covered = std::ranges::any_of(ranges, [&](const auto& range) {
            return range.readable && range.address <= page && page - range.address <= range.bytes && HostPageBytes <= range.bytes - (page - range.address);
        });
        if (covered) continue;
        const bool writable = GuestMemory::Accessible(address, HostPageBytes, true);
        Range range{page, HostPageBytes, FrameRangeKind::Host, writable ? 0x04u : 0x02u,
            FrameRangeAccess::Readable | (writable ? FrameRangeAccess::Writable : 0u), true};
        snapshot(range);
        if (!writer) return;
        ranges.push_back(range);
    }
    prologueHostPages.clear();
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
    callThreads.insert(threadId());
    {
        const auto entry = DriverDetail::Driver::Get().StartedProgress();
        const struct Released {
            Released() { DriverDetail::Driver::Get().HoldForRecorder(); }
            ~Released() { DriverDetail::Driver::Get().ReleaseForRecorder(); }
        } released;
        emitDelta(&entry);
    }
    GuestMemory::SetOwnStoreCompare(false);
    if (!writer) return;
    auto end = make(FrameCallKind::End);
    end.ordinal = ++ordinal;
    std::tie(end.accepted, end.completed) = DriverDetail::Driver::Get().SerialProgress();
    writer->WriteCall(end);
    awaitLateStores();
    writeReferenceFrames();
    if (stopOnHazard()) return;
    writer->Finish(header());
    GuestMemory::SetDriverStoreObserver(nullptr);
    std::fprintf(stderr, "[frame-capture] %llu frames, %llu calls, %llu memory deltas (%.1f MiB), %.1f MiB raw, %.1f MiB stored in %s\n", static_cast<unsigned long long>(recordedFrames),
                 static_cast<unsigned long long>(recordedCalls), static_cast<unsigned long long>(deltaGroups), static_cast<double>(deltaBytes) / (1024.0 * 1024.0),
                 static_cast<double>(writer->RawBytes()) / (1024.0 * 1024.0), static_cast<double>(writer->StoredBytes()) / (1024.0 * 1024.0), directory.string().c_str());
    writer.reset();
    endGpuStores();
    state = State::Done;
}

std::string FrameRecorder::header() const {
    std::uintptr_t arenaBase = 0;
    std::size_t arenaBytes = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
    std::string text = "{\"magic\":\"aps5-frame-capture 2\"";
    text += ",\"commit\":" + jsonString(APS5_TRACE_COMMIT);
    text += std::string(",\"dirty\":") + (APS5_TRACE_DIRTY < 0 ? "null" : APS5_TRACE_DIRTY != 0 ? "true" : "false");
    text += ",\"arena_base\":" + std::to_string(arenaBase) + ",\"arena_bytes\":" + std::to_string(arenaBytes);
    text += ",\"start_boundary\":" + std::to_string(startBoundary) + ",\"frames\":" + std::to_string(recordedFrames);
    text += ",\"calls\":" + std::to_string(recordedCalls) + ",\"prologue_calls\":" + std::to_string(prologue.size());
    text += ",\"ranges\":" + std::to_string(ranges.size()) + ",\"late_ranges\":" + std::to_string(lateRanges) + ",\"snapshot_bytes\":" + std::to_string(snapshotBytes) + ",\"unreadable_bytes\":" + std::to_string(unreadableBytes) + ",\"host_only_sample_bytes\":" + std::to_string(hostOnlySampleBytes);
    text += ",\"memory_groups\":" + std::to_string(deltaGroups) + ",\"delta_bytes\":" + std::to_string(deltaBytes) + ",\"gpu_store_bytes\":" + std::to_string(gpuStoreBytes);
    text += ",\"capture_seconds\":" + std::to_string(std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count());
    text += ",\"submit_depths\":[";
    for (std::size_t depth = 0; depth < submitDepths.size(); ++depth) text += (depth == 0 ? "" : ",") + std::to_string(submitDepths[depth]);
    text += "],\"read_depths\":[";
    for (std::size_t depth = 0; depth < readDepths.size(); ++depth) text += (depth == 0 ? "" : ",") + std::to_string(readDepths[depth]);
    text += "]";
    {
        std::lock_guard lock(storeMutex);
        text += ",\"in_flight_deltas\":" + std::to_string(inFlightDeltas) + ",\"held_deltas\":" + std::to_string(heldDeltas) + ",\"hold_reasons\":{" + [&] {
            std::string reasons;
            for (std::size_t index = 1; index < HoldReasonNames.size(); ++index) reasons += std::string(index == 1 ? "" : ",") + "\"" + HoldReasonNames[index] + "\":" + std::to_string(holdReasons[index]);
            return reasons + "}";
        }() + ",\"hold_masks\":{" + [&] {
            std::string masks;
            for (const auto& [mask, count] : holdMasks) {
                std::string names;
                for (std::size_t index = 1; index < HoldReasonNames.size(); ++index) {
                    if (((mask >> index) & 1u) != 0) names += std::string(names.empty() ? "" : "+") + HoldReasonNames[index];
                }
                masks += std::string(masks.empty() ? "\"" : ",\"") + (names.empty() ? std::string("none") : names) + "\":" + std::to_string(count);
            }
            return masks + "}";
        }() + ",\"unwatched_blocks\":[" + [&] {
            std::string list;
            for (const auto& [block, counts] : unwatchedCounts) {
                list += std::string(list.empty() ? "" : ",") + "{\"address\":" + std::to_string(block.first) + ",\"bytes\":" + std::to_string(block.second - block.first) + ",\"deltas\":" + std::to_string(counts.deltas) + ",\"changed\":" + std::to_string(counts.changed) + ",\"driver_stored\":" + std::to_string(counts.stored) + "}";
            }
            return list + "]";
        }() + ",\"stale_delta\":" + std::to_string(storeHazard.group) + ",\"unverified_deltas\":" + std::to_string(unverifiedDeltas) + ",\"unattributed_stores\":" + std::to_string(unattributedStores);
    }
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
    if (hostOnlySampleBytes != 0) text += (first ? "" : ",") + jsonString(std::to_string(hostOnlySampleBytes) + " bytes under multisampled targets whose samples are only on the host are zero in the capture, so replay starts those targets without their earlier contents");
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
        const auto firstRange = result.size();
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
                if (result.size() > firstRange && last.address + last.bytes == range.address && last.kind == range.kind && last.protection == range.protection && last.readable == range.readable && last.access == range.access) last.bytes += range.bytes;
                else result.push_back(range);
            }
            address = regionEnd;
        }
    }
    return result;
#elif defined(__linux__)
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    if (size == 0) throw std::runtime_error("frame capture: the guest arena is not reserved");
    std::vector<Range> result;
    for (const auto& allocation : GuestAllocations::GuestAllocationsAcquireAll_nid_postfix()) {
        if (!allocation->readable) continue;
        const auto first = allocation->address;
        const auto end = first + allocation->bytes;
        const auto firstRange = result.size();
        const bool host = first < base || end > base + size;
        const auto access = FrameRangeAccess::Readable | (allocation->writable ? FrameRangeAccess::Writable : 0u) | (allocation->gpu ? FrameRangeAccess::Gpu : 0u);
        HostMapping::Walk(first, end, "frame capture", [&](const HostMapping::Mapping& mapping) {
            if (!mapping.readable && !mapping.writable && !mapping.executable) return;
            const auto add = [&](std::uint64_t address, std::uint64_t regionEnd, bool readable) {
                if (address >= regionEnd) return;
                Range range;
                range.address = address;
                range.bytes = regionEnd - address;
                range.protection = windowsProtection(mapping);
                range.access = access;
                range.readable = readable;
                if (host) range.kind = FrameRangeKind::Host;
                else if (GuestMemory::Watched(range.address, static_cast<std::size_t>(range.bytes))) range.kind = FrameRangeKind::Watched;
                else if (!mapping.shared && mapping.inode == 0) range.kind = FrameRangeKind::Unwatched;
                else range.kind = FrameRangeKind::Section;
                auto& last = result.empty() ? range : result.back();
                if (result.size() > firstRange && last.address + last.bytes == range.address && last.kind == range.kind && last.protection == range.protection && last.readable == range.readable && last.access == range.access) last.bytes += range.bytes;
                else result.push_back(range);
            };
            const auto address = std::max<std::uint64_t>(first, mapping.start);
            const auto regionEnd = std::min<std::uint64_t>(end, mapping.end);
            const auto backed = std::clamp(fileBackedEnd(mapping), address, regionEnd);
            add(address, backed, mapping.readable);
            add(backed, regionEnd, false);
        });
    }
    return result;
#else
    throw std::runtime_error("frame capture: guest memory capture is implemented on Windows and Linux only");
#endif
}

}
