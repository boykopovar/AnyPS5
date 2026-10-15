#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawAhead.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#ifndef _WIN32
#include <pthread.h>
#endif

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::size_t MaxQueuedJobs = 4096;
std::atomic<void (*)(bool)> pinnedReadHook{nullptr};
std::atomic<bool> compileAheadEnabled{true};

void NameThread(const char* name) {
#if defined(__APPLE__)
    pthread_setname_np(name);
#elif !defined(_WIN32)
    pthread_setname_np(pthread_self(), name);
#else
    static_cast<void>(name);
#endif
}

Graphics::RegisterBank BankOf(std::uint32_t opcode) {
    if (opcode == 0x69 || opcode == 0x9f || opcode == 0x40) return Graphics::RegisterBank::Context;
    if (opcode == 0x76 || opcode == 0x63) return Graphics::RegisterBank::Shader;
    return Graphics::RegisterBank::UserConfig;
}

const Registers& BankRegisters(const QueueState& queue, Graphics::RegisterBank bank) {
    return bank == Graphics::RegisterBank::Context ? queue.context : bank == Graphics::RegisterBank::Shader ? queue.shader : queue.userConfig;
}

bool SetsRegisters(std::span<const std::uint32_t> packet, std::uint32_t opcode) {
    return (opcode == 0x69 || opcode == 0x76 || opcode == 0x79 || opcode == 0x7a) && packet.size() >= 2 && !Pm4::IsTagMarker(packet);
}

}

DrawScan::DrawScan(std::span<const std::uint32_t> commands, const std::map<std::size_t, std::vector<std::uint32_t>>& registerLists, const std::map<std::size_t, std::size_t>& conditionalEnds, QueueState& state) : commands(commands), registerLists(registerLists), conditionalEnds(conditionalEnds), state(state) {}

void DrawScan::write(Graphics::RegisterBank bank, std::uint32_t offset, bool maybe) {
    if (maybe) divergent.insert({bank, offset});
    else if (!divergent.empty()) divergent.erase({bank, offset});
}

void DrawScan::clearContext() {
    std::erase_if(divergent, [](const auto& entry) { return entry.first == Graphics::RegisterBank::Context; });
}

void DrawScan::track(std::span<const std::uint32_t> packet, std::uint32_t opcode, bool maybe, const std::vector<std::uint32_t>* pairs) {
    if (SetsRegisters(packet, opcode)) {
        const auto first = Pm4::RegisterOffset(packet[1]);
        for (std::size_t i = 2; i < packet.size(); ++i) write(BankOf(opcode), first + static_cast<std::uint32_t>(i - 2), maybe);
        return;
    }
    if (Pm4::IndirectRegisterOpcode(opcode)) {
        for (std::size_t i = 0; i + 1 < pairs->size(); i += 2) write(BankOf(opcode), Pm4::RegisterOffset((*pairs)[i]), maybe);
        return;
    }
    switch (opcode) {
        case 0x69: case 0x76: case 0x79: case 0x7a:
        case 0x11: case 0x13: case 0x26: case 0x28: case 0x2a: case 0x2f: case 0x59: case 0x81:
            return;
        case 0x12:
            if (maybe) lost = true;
            else clearContext();
            return;
        case 0x10:
            switch ((packet[0] >> 2u) & 0x3fu) {
                case 0: case 0x0b: case 0x0c:
                    return;
                case 0x09:
                    if (maybe) {
                        lost = true;
                        return;
                    }
                    divergent.clear();
                    saved.clear();
                    lost = false;
                    return;
                case 0x1a:
                    if (maybe) {
                        lost = true;
                        return;
                    }
                    if (packet[1] == 0) {
                        clearContext();
                    } else if (packet[1] == 1 || packet[1] == 3) {
                        saved.clear();
                        for (const auto& entry : divergent) {
                            if (entry.first == Graphics::RegisterBank::Context) saved.insert(entry);
                        }
                        if (packet[1] == 3) clearContext();
                    } else {
                        clearContext();
                        divergent.insert(saved.begin(), saved.end());
                        saved.clear();
                    }
                    return;
                default:
                    lost = true;
                    return;
            }
        default:
            if (maybe) lost = true;
            return;
    }
}

bool DrawScan::Next(const std::function<bool(const Draw&)>& draw) {
    while (cursor < commands.size()) {
        const auto header = commands[cursor];
        if (Pm4::FillerPacket(header)) {
            ++cursor;
            continue;
        }
        const auto count = Pm4::PacketWords(header);
        if (cursor + count > commands.size()) throw std::runtime_error("draw-ahead scan found a packet past the end of its commands");
        const auto at = cursor;
        const auto packet = commands.subspan(at, count);
        cursor += count;
        const auto opcode = (header >> 8u) & 0xffu;
        const bool predicated = Pm4::Predicated(header) && state.predication.operation != 0;
        const bool maybe = at < maybeUntil || predicated;
        if (opcode == 0x22 || (opcode == 0x3f && predicated)) {
            const auto end = conditionalEnds.find(at);
            if (end == conditionalEnds.end()) lost = true;
            else maybeUntil = std::max(maybeUntil, end->second);
            continue;
        }
        if (opcode == 0x15 || opcode == 0x16) {
            Pm4::NoteDispatchRegisters(packet, state);
            for (std::uint32_t offset = 0x200; offset < 0x204; ++offset) write(Graphics::RegisterBank::Shader, offset, maybe);
            continue;
        }
        if (header == FlipPacketHeader || header == RenderingWaitPacketHeader || opcode == 0x3f) continue;
        if (Pm4::DrawOpcode(opcode)) {
            if (!draw({at, packet, state, !lost && divergent.size() <= MaxRegisterChecks})) return false;
            continue;
        }
        if (Pm4::IndirectRegisterOpcode(opcode)) {
            const auto found = registerLists.find(at);
            if (found == registerLists.end()) throw std::runtime_error("draw-ahead scan found an indirect register packet without its register list");
            Pm4::ExecuteIndirectRegisters(packet, found->second, state);
            track(packet, opcode, maybe, &found->second);
            continue;
        }
        if (const auto copied = Pm4::CopyDataContextRegisters(packet)) {
            for (std::uint32_t i = 0; i < copied->second; ++i) write(Graphics::RegisterBank::Context, copied->first + i, true);
            continue;
        }
        if (Pm4::AccessesMemory(header) || opcode == 0x42 || opcode == 0x46 || opcode == 0x58) continue;
        Pm4::Execute(packet, state);
        track(packet, opcode, maybe, nullptr);
    }
    return true;
}

void DrawScan::Capture(std::vector<PublishedDraw::RegisterCheck>& checks) const {
    checks.clear();
    checks.reserve(divergent.size());
    for (const auto& [bank, offset] : divergent) {
        const auto& registers = BankRegisters(state, bank);
        const auto found = registers.find(offset);
        checks.push_back({bank, offset, found != registers.end(), found != registers.end() ? (*found).second : 0u});
    }
}

bool DrawScan::Matches(const PublishedDraw& record, const QueueState& queue) {
    if (queue.indexBase != record.indexBase || queue.indexType != record.indexType || queue.instanceCount != record.instanceCount) return false;
    return std::all_of(record.checks.begin(), record.checks.end(), [&](const PublishedDraw::RegisterCheck& check) {
        const auto& registers = BankRegisters(queue, check.bank);
        const auto found = registers.find(check.offset);
        return (found != registers.end()) == check.present && (!check.present || (*found).second == check.value);
    });
}

DrawAhead::Speculation::Speculation(DrawAhead& owner, std::uint64_t serial, std::size_t offset, bool compile) : owner(owner), serial(serial), offset(offset), compile(compile) {}

void DrawAhead::Speculation::Enter() {
    if (!owner.pin(serial, offset)) throw GuestMemory::SpeculativeReadRefused("draw-ahead speculation outlived its draw");
    if (const auto hook = pinnedReadHook.load(std::memory_order_acquire)) hook(compile);
}

void DrawAhead::SetPinnedReadHook(void (*hook)(bool compile)) {
    pinnedReadHook.store(hook, std::memory_order_release);
}

void DrawAhead::SetCompileAhead(bool enabled) {
    compileAheadEnabled.store(enabled, std::memory_order_release);
}

void DrawAhead::Speculation::Leave() {
    owner.unpin(serial, offset);
}

bool DrawAhead::Speculation::Passed() const {
    return owner.passed(serial, offset);
}

bool DrawAhead::Speculation::Commit(const std::function<void()>& action) {
    if (!owner.pin(serial, offset)) return false;
    struct Unpin {
        DrawAhead& owner;
        std::uint64_t serial;
        std::size_t offset;
        ~Unpin() { owner.unpin(serial, offset); }
    } unpin{owner, serial, offset};
    action();
    return true;
}

std::size_t DrawAhead::Workers() {
    if (!GuestMemory::SpeculativeReadsSupported()) return 0;
    return std::clamp<std::size_t>(std::thread::hardware_concurrency() / 4, 1, 4);
}

bool DrawAhead::HasDirectDraws(std::span<const std::uint32_t> commands) {
    for (std::size_t cursor = 0; cursor < commands.size();) {
        const auto header = commands[cursor];
        if (Pm4::FillerPacket(header)) {
            ++cursor;
            continue;
        }
        const auto opcode = (header >> 8u) & 0xffu;
        if (Pm4::DrawOpcode(opcode) && !Pm4::IndirectDrawOpcode(opcode)) return true;
        cursor += Pm4::PacketWords(header);
    }
    return false;
}

bool DrawAhead::CompilesAhead(const Graphics::State& state, const Pm4::DrawParameters& parameters) {
    return !state.stages.mesh && !parameters.indirect && !(state.stages.tessellation && parameters.instanceCount > 1u);
}

DrawAhead::DrawAhead(Driver& owner, std::size_t workers) : driver(owner), workerCount(workers) {
    threads.emplace_back([this](std::stop_token token) {
        NameThread("agcdrawscan");
        scanLoop(token);
    });
    for (std::size_t index = 0; index < workers; ++index) {
        threads.emplace_back([this, index](std::stop_token token) {
            char name[16];
            std::snprintf(name, sizeof(name), "agcdraw%zu", index);
            NameThread(name);
            workLoop(token);
        });
    }
}

DrawAhead::~DrawAhead() {
    for (auto& thread : threads) thread.request_stop();
    scansReady.notify_all();
    jobsReady.notify_all();
    threads.clear();
}

void DrawAhead::Post(const Submission& submission, const QueueState& state) {
    if (!HasDirectDraws(submission.commands)) {
        current.reset();
        return;
    }
    {
        std::lock_guard lock(mutex);
        if (scanning || !scans.empty()) {
            current.reset();
            return;
        }
    }
    Scan work;
    work.serial = submission.serial;
    work.commands = submission.commands;
    work.registerLists = submission.registerLists;
    work.conditionalEnds = submission.conditionalEnds;
    work.shaders = submission.shaders;
    work.state = std::make_unique<QueueState>(state);
    auto publication = std::make_shared<Publication>();
    publication->serial = submission.serial;
    publication->commands = submission.commands.data();
    publication->shaders = submission.shaders.get();
    work.publication = publication;
    current = std::move(publication);
    {
        std::lock_guard lock(mutex);
        scans.push_back(std::move(work));
    }
    scansReady.notify_one();
}

void DrawAhead::Progress(std::uint64_t serial, std::size_t cursor) {
    pass(serial, cursor + 1);
}

void DrawAhead::Skip(std::uint64_t serial, std::size_t end) {
    pass(serial, end);
}

void DrawAhead::Retire(std::uint64_t serial) {
    pass(serial, std::numeric_limits<std::size_t>::max());
    std::size_t dropped = 0;
    {
        std::lock_guard lock(mutex);
        dropped = static_cast<std::size_t>(std::erase_if(jobs, [&](const Job& job) { return job.serial <= serial; }));
        std::erase_if(scans, [&](const Scan& scan) { return scan.serial <= serial; });
    }
    driver.drawAheadCounts.dropped.fetch_add(dropped, std::memory_order_relaxed);
    current.reset();
}

const PublishedDraw* DrawAhead::Find(const Submission& submission, std::span<const std::uint32_t> packet, const QueueState& queue, std::uint64_t deviceSerial) {
    auto* publication = current.get();
    const auto* commands = submission.commands.data();
    if (publication == nullptr || publication->serial != submission.serial || publication->commands != commands || publication->shaders != submission.shaders.get()) return nullptr;
    if (packet.data() < commands || packet.data() >= commands + submission.commands.size()) return nullptr;
    const auto offset = static_cast<std::size_t>(packet.data() - commands);
    const auto ready = publication->published.load(std::memory_order_acquire);
    auto& cursor = publication->readCursor;
    while (cursor < ready && publication->records[cursor].offset < offset) ++cursor;
    if (cursor == ready) return nullptr;
    const auto& record = publication->records[cursor];
    if (record.offset != offset || !record.certain || record.deviceSerial != deviceSerial || !DrawScan::Matches(record, queue)) return nullptr;
    driver.drawAheadCounts.used.fetch_add(1, std::memory_order_relaxed);
    return &record;
}

bool DrawAhead::passed(std::uint64_t serial, std::size_t offset) {
    std::lock_guard lock(gateMutex);
    return serial < passedSerial || (serial == passedSerial && offset < passedEnd);
}

bool DrawAhead::pin(std::uint64_t serial, std::size_t offset) {
    std::lock_guard lock(gateMutex);
    if (serial < passedSerial || (serial == passedSerial && offset < passedEnd)) return false;
    pinned.emplace_back(serial, offset);
    return true;
}

void DrawAhead::unpin(std::uint64_t serial, std::size_t offset) {
    {
        std::lock_guard lock(gateMutex);
        const auto found = std::find(pinned.begin(), pinned.end(), std::pair{serial, offset});
        if (found == pinned.end()) throw std::runtime_error("draw-ahead unpinned a speculation it never pinned");
        pinned.erase(found);
    }
    gateLeft.notify_all();
}

void DrawAhead::pass(std::uint64_t serial, std::size_t end) {
    std::unique_lock lock(gateMutex);
    if (serial < passedSerial || (serial == passedSerial && end <= passedEnd)) return;
    passedSerial = serial;
    passedEnd = end;
    gateLeft.wait(lock, [&] {
        return std::none_of(pinned.begin(), pinned.end(), [&](const auto& entry) { return entry.first < serial || (entry.first == serial && entry.second < end); });
    });
}

void DrawAhead::scanLoop(std::stop_token token) {
    while (!token.stop_requested()) {
        Scan work;
        {
            std::unique_lock lock(mutex);
            if (!scansReady.wait(lock, token, [&] { return !scans.empty(); })) return;
            work = std::move(scans.front());
            scans.pop_front();
            scanning = true;
        }
        try {
            scan(work, token);
        } catch (...) {
        }
        std::lock_guard lock(mutex);
        scanning = false;
    }
}

void DrawAhead::queueJob(Job job) {
    {
        std::lock_guard lock(mutex);
        if (jobs.size() >= MaxQueuedJobs) return;
        jobs.push_back(std::move(job));
    }
    jobsReady.notify_one();
}

void DrawAhead::scan(Scan& work, std::stop_token token) {
    const auto device = driver.device.Load();
    if (device == nullptr || work.shaders == nullptr) return;
    const auto deviceSerial = device->Serial();
    const bool listRestart = device->PrimitiveListRestart();
    const auto& commands = work.commands;
    auto& publication = *work.publication;
    if (passed(work.serial, std::numeric_limits<std::size_t>::max() - 1)) return;
    std::size_t draws = 0;
    for (std::size_t cursor = 0; cursor < commands.size();) {
        const auto header = commands[cursor];
        const auto count = Pm4::PacketWords(header);
        if (!Pm4::FillerPacket(header) && Pm4::DrawOpcode((header >> 8u) & 0xffu)) ++draws;
        cursor += count;
    }
    publication.records = std::make_unique<PublishedDraw[]>(draws);
    publication.capacity = draws;
    const bool compileAhead = workerCount != 0 && compileAheadEnabled.load(std::memory_order_acquire) && !Driver::insertCompare() && !Driver::traceDrawCache() && !work.shaders->empty();
    std::unordered_set<std::uint64_t> queued;
    std::string rejected;
    std::size_t index = 0;
    DrawScan scanner(commands, work.registerLists, work.conditionalEnds, *work.state);
    scanner.Next([&](const DrawScan::Draw& draw) {
        if (token.stop_requested() || !pin(work.serial, commands.size())) return false;
        struct Unpin {
            DrawAhead& owner;
            std::uint64_t serial;
            std::size_t end;
            ~Unpin() { owner.unpin(serial, end); }
        } unpin{*this, work.serial, commands.size()};
        if (index >= publication.capacity) throw std::runtime_error("draw-ahead scan found more draws than it counted");
        auto& record = publication.records[index];
        record.offset = draw.offset;
        const auto opcode = (draw.packet[0] >> 8u) & 0xffu;
        Speculation speculation(*this, work.serial, draw.offset, false);
        if (!Pm4::IndirectDrawOpcode(opcode) && !speculation.Passed()) {
            const GuestMemory::SpeculativeReadScope speculative(speculation);
            bool resolved = false;
            try {
                record.parameters = Pm4::ResolveDraw(draw.packet, draw.state);
                record.precheck = PrecheckDrawState(draw.state, record.parameters, listRestart, rejected);
                record.key = Driver::drawRegisterKey(draw.state, *work.shaders, deviceSerial, &record.shape);
                resolved = true;
            } catch (...) {
            }
            record.deviceSerial = deviceSerial;
            record.indexBase = draw.state.indexBase;
            record.indexType = draw.state.indexType;
            record.instanceCount = draw.state.instanceCount;
            record.certain = resolved && draw.certain && (record.precheck == DrawPrecheckOutcome::Proceed || record.precheck == DrawPrecheckOutcome::Nothing);
            if (record.certain) scanner.Capture(record.checks);
            if (compileAhead && resolved && record.precheck == DrawPrecheckOutcome::Proceed && queued.insert(record.key).second) {
                bool cached = false;
                {
                    std::lock_guard cacheLock(driver.drawCacheMutex);
                    cached = driver.drawCache.contains(record.key);
                }
                if (!cached) {
                    try {
                        queueJob({work.serial, draw.offset, record.key, record.shape, driver.decodeDraw(draw.state, *work.shaders), record.parameters, device});
                    } catch (...) {
                        driver.drawAheadCounts.dropped.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }
        publication.published.store(++index, std::memory_order_release);
        driver.drawAheadCounts.published.fetch_add(1, std::memory_order_relaxed);
        return true;
    });
}

void DrawAhead::workLoop(std::stop_token token) {
    while (!token.stop_requested()) {
        Job job;
        {
            std::unique_lock lock(mutex);
            if (!jobsReady.wait(lock, token, [&] { return !jobs.empty(); })) return;
            job = std::move(jobs.front());
            jobs.pop_front();
        }
        Speculation speculation(*this, job.serial, job.offset, true);
        bool inserted = false;
        if (!speculation.Passed()) {
            try {
                inserted = driver.compileDrawAhead(job.key, job.shape, job.decode, job.parameters, job.device, speculation);
            } catch (...) {
            }
        }
        if (!inserted) driver.drawAheadCounts.dropped.fetch_add(1, std::memory_order_relaxed);
    }
}
}
