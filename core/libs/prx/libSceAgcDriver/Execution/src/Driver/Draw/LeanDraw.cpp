#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/LeanDraw.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DirectDrawReport.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRecords.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::uint64_t FnvPrime = 0x100000001b3ull;
constexpr std::array<std::pair<std::uint32_t, std::size_t>, 4> KeyedBases{{{0x00cu, 0}, {0x04cu, 1}, {0x08cu, 2}, {0x10cu, 4}}};
constexpr std::array<std::pair<std::size_t, std::size_t>, 5> IdentityBases{{{0, 0}, {2, 2}, {3, 3}, {4, 4}, {5, 5}}};

std::uint64_t fold(std::uint64_t key, std::span<const std::uint64_t> values) {
    for (const auto value : values) key = (key ^ value) * FnvPrime;
    return key;
}

std::uint32_t userWordCount(std::uint32_t resources) {
    return ((resources >> 1u) & 0x1fu) | (((resources >> 27u) & 1u) << 5u);
}

bool allUserWords() {
    static const bool all = std::getenv("APS5_DRAW_KEY_ALL_USER_WORDS") != nullptr;
    return all;
}

bool runtimeRegister(std::uint32_t offset) {
    return offset - 0x00cu < 32u || offset - 0x04cu < 16u || offset - 0x08cu < 32u || offset - 0x10cu < 32u || offset == 0x082u || offset == 0x083u || offset == 0x102u || offset == 0x103u;
}

std::uint64_t presentRun(const DrawUserWords& words, std::size_t index, std::uint32_t count) {
    if (count == 0) return 0;
    const auto bits = words.present[index / 64] >> (index % 64);
    return count >= 64 ? bits : bits & ((std::uint64_t{1} << count) - 1u);
}

template <typename TVisit>
void forEachPresent(const DrawUserWords& words, std::uint32_t base, std::size_t index, std::uint32_t count, TVisit&& visit) {
    for (auto bits = presentRun(words, index, count); bits != 0; bits &= bits - 1) {
        const auto word = static_cast<std::uint32_t>(std::countr_zero(bits));
        visit(base + word, words.values[index + word]);
    }
}

std::size_t verdictIndex(const Pm4::DrawParameters& parameters) {
    if (!parameters.indexed) return 0;
    return parameters.indexSize == 2 ? 1 : parameters.indexSize == 4 ? 2 : 3;
}

std::string where(const Submission& submission, std::size_t packetOffset) {
    return "draw at submission " + std::to_string(submission.serial) + " DWORD " + std::to_string(packetOffset);
}

std::string hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

bool leanQualified(const DrawDecode& decode) {
    if (decode.roles.size() != decode.programs.size()) return false;
    for (std::size_t i = 0; i < decode.programs.size(); ++i) {
        const auto& program = decode.programs[i];
        if (program.snapshot == nullptr || program.snapshot->header.empty() || program.tailContinuation != nullptr) return false;
        if (program.userData.size() < program.hiddenUserWords || program.userData.size() - program.hiddenUserWords > 32u) return false;
        if (!program.nullPixel && DrawUserWords::ResourceIndex(program.resourceRegister) < 0) return false;
        if (program.userData.size() > program.hiddenUserWords && std::find(DrawUserWords::Bases.begin(), DrawUserWords::Bases.end(), program.userDataBase) == DrawUserWords::Bases.end()) return false;
        if (program.merged && (DrawUserWords::Index(program.mergedPointer) < 0 || DrawUserWords::Index(program.mergedPointer + 1) < 0)) return false;
        if (decode.roles[i] != ShaderRecompiler::ProgramRole::GeometryBack && ProgramWritesMemory(*program.snapshot, program.codeOffset)) return false;
    }
    return true;
}

}

std::shared_ptr<DrawDecode> LeanRefreshedDecode(const DrawDecode& from, const DrawUserWords& words) {
    auto refreshed = std::make_shared<DrawDecode>(from);
    for (auto& program : refreshed->programs) {
        ReadUserWordsFrom(words, program);
        if (program.merged) InitializeMergedFrom(words, program, program.mergedPointer, program.mergedPointerRequired);
    }
    return refreshed;
}

std::uint64_t LeanExactKey(const LeanShape& shape, const DrawUserWords& words) {
    auto exact = shape.shape;
    const auto mix = [&](std::uint32_t offset, std::uint32_t value) {
        exact = (exact ^ offset) * FnvPrime;
        exact = (exact ^ value) * FnvPrime;
    };
    for (std::size_t base = 0; base < KeyedBases.size(); ++base) {
        const auto [first, resource] = KeyedBases[base];
        const auto limit = first == 0x04cu ? 16u : 32u;
        forEachPresent(words, first, 32u * base, allUserWords() ? limit : std::min(userWordCount(words.resources[resource]), limit), mix);
    }
    forEachPresent(words, 0x082u, 128, 2, mix);
    forEachPresent(words, 0x102u, 130, 2, mix);
    return fold(exact, shape.keySuffix);
}

std::uint64_t LeanIdentityKey(const LeanShape& shape, const DrawUserWords& words) {
    std::uint64_t key = 0x84222325cbf29ce4ull;
    const auto mix = [&](std::uint64_t value) { key = (key ^ value) * FnvPrime; };
    const auto& set = shape.shaderSet;
    for (const auto [index, resource] : IdentityBases) {
        if (!set.present[index]) {
            mix(0);
            continue;
        }
        mix(set.low[index]);
        mix(set.high[index]);
        mix(words.resources[resource]);
        for (const auto value : set.mixes[index]) mix(value);
    }
    return key == 0 ? 1 : key;
}

void LeanAliasKey(const LeanShape& shape, const DrawUserWords& words, const DrawRelocation* relocation, std::uint64_t deviceSerial, std::uint64_t& alias) {
    if (relocation == nullptr || relocation->words.empty()) return;
    bool readable = true;
    std::uint64_t other = 0x6c62272e07bb0142ull;
    const auto mixOther = [&](std::uint64_t value) { other = (other ^ value) * FnvPrime; };
    mixOther(shape.shape);
    const auto visit = [&](std::uint32_t offset, std::uint32_t value) {
        mixOther(offset);
        const auto word = std::find_if(relocation->words.begin(), relocation->words.end(), [&](const RelocatableUserWord& candidate) { return candidate.reg == offset; });
        mixOther(word != relocation->words.end() ? Driver::relocatedContentKey(*word, value, readable) : value);
    };
    for (const std::size_t base : {std::size_t{0}, std::size_t{2}, std::size_t{3}}) {
        const auto [first, resource] = KeyedBases[base];
        forEachPresent(words, first, 32u * base, allUserWords() ? 32u : std::min(userWordCount(words.resources[resource]), 32u), visit);
    }
    forEachPresent(words, 0x082u, 128, 2, visit);
    forEachPresent(words, 0x102u, 130, 2, visit);
    mixOther(deviceSerial);
    other = fold(other, shape.aliasSuffix);
    if (readable) alias = other == 0 ? 1 : other;
}

ShapeRecord* LeanDraws::Find(std::uint64_t shape, std::uint64_t deviceSerial) {
    if (records.empty()) return nullptr;
    const auto set = static_cast<std::size_t>((shape ^ (deviceSerial * 0x9e3779b97f4a7c15ull)) % Sets) * Ways;
    for (std::size_t way = 0; way < Ways; ++way) {
        auto& record = records[set + way];
        if (!record.used || record.shape != shape || record.deviceSerial != deviceSerial) continue;
        record.lastUse = ++uses;
        return &record;
    }
    return nullptr;
}

ShapeRecord& LeanDraws::Place(std::uint64_t shape, std::uint64_t deviceSerial, std::uint64_t front) {
    if (records.empty()) records.resize(Sets * Ways);
    const auto set = static_cast<std::size_t>((shape ^ (deviceSerial * 0x9e3779b97f4a7c15ull)) % Sets) * Ways;
    auto* victim = &records[set];
    for (std::size_t way = 0; way < Ways; ++way) {
        auto& record = records[set + way];
        if (!record.used) {
            victim = &record;
            break;
        }
        if (record.lastUse < victim->lastUse) victim = &record;
    }
    if (victim->used) {
        Retire(*victim, front);
        LeanFront::AddOnWorker(LeanFront::Counter::ShapeRecordsEvicted);
    }
    *victim = ShapeRecord{};
    victim->used = true;
    victim->shape = shape;
    victim->deviceSerial = deviceSerial;
    victim->lastUse = ++uses;
    return *victim;
}

void LeanDraws::Retire(ShapeRecord& record, std::uint64_t front) {
    if (record.built != nullptr && record.lastSeq >= front) retired.emplace_back(std::move(record.built), record.lastSeq);
    record.built = nullptr;
}

void LeanDraws::Clear(std::uint64_t front) {
    for (auto& record : records) {
        if (!record.used) continue;
        Retire(record, front);
        record = ShapeRecord{};
    }
}

void LeanDraws::Release(std::uint64_t front, std::uint64_t issued) {
    if (!retired.empty()) std::erase_if(retired, [&](const auto& entry) { return entry.second < front; });
    while (anchorTail - anchorHead > 1 && anchors[static_cast<std::size_t>((anchorHead + 1) % Anchors)].firstSeq <= front) {
        anchors[static_cast<std::size_t>(anchorHead % Anchors)] = LeanAnchor{};
        ++anchorHead;
    }
    if (anchorTail - anchorHead == 1 && front >= issued && anchors[static_cast<std::size_t>(anchorHead % Anchors)].device != device) {
        anchors[static_cast<std::size_t>(anchorHead % Anchors)] = LeanAnchor{};
        ++anchorHead;
    }
}

void LeanDraws::ReleaseDevice() {
    for (; anchorHead != anchorTail; ++anchorHead) anchors[static_cast<std::size_t>(anchorHead % Anchors)] = LeanAnchor{};
    device = nullptr;
    deviceStores = ~std::uint64_t{0};
}

void LeanDraws::Reset() {
    ReleaseDevice();
    records.clear();
    retired.clear();
    registry = nullptr;
    ++registryEpoch;
}

std::optional<std::uint64_t> LeanDraws::AnchorFor(const std::shared_ptr<const ShaderRegistry>& heldRegistry, const std::shared_ptr<VulkanDevice>& heldDevice, std::uint64_t issued) {
    if (anchorTail != anchorHead) {
        const auto& current = anchors[static_cast<std::size_t>((anchorTail - 1) % Anchors)];
        if (current.registry == heldRegistry && current.device == heldDevice) return anchorTail - 1;
    }
    if (anchorTail - anchorHead >= Anchors) return std::nullopt;
    anchors[static_cast<std::size_t>(anchorTail % Anchors)] = LeanAnchor{heldRegistry, heldDevice, issued};
    return anchorTail++;
}

void Driver::prepareLeanDraws() {
    auto& finisher = DrawFinisher::Get();
    finisher.Adopt(true);
    const auto front = finisher.Front();
    if (frontLatch.Renew(leanDraws.stamp)) leanDraws.Clear(front);
    if (leanDraws.device != nullptr && device.Stores() != leanDraws.deviceStores) leanDraws.device = nullptr;
    leanDraws.Release(front, finisher.Issued());
}

bool Driver::leanDrawDevice() {
    auto& lean = leanDraws;
    const auto stores = device.Stores();
    if (stores != lean.deviceStores || lean.device == nullptr) {
        lean.device = device.Load();
        lean.deviceStores = stores;
    }
    return lean.device != nullptr;
}

bool Driver::leanShapeCurrent(const LeanShape& shape, const QueueState& queue, const ShaderRegistry& registry) {
    if (ReadDrawShaderSet(queue, registry) != shape.shaderSet) return false;
    return std::all_of(shape.decode->programs.begin(), shape.decode->programs.end(), [&](const DrawProgram& program) { return ProgramSnapshot(registry, program.binary.codeAddress, {program.snapshot->type}) == program.snapshot; });
}

ShapeRecord* Driver::leanShapeRecord(const QueueState& queue, const Submission& submission) {
    auto& lean = leanDraws;
    if (submission.shaders.get() != lean.registry.get()) {
        lean.registry = submission.shaders;
        ++lean.registryEpoch;
    }
    const auto serial = lean.device->Serial();
    const auto shape = RegisterShapeKey(queue);
    if (auto* record = lean.Find(shape, serial)) {
        if (record->epoch == lean.registryEpoch) return record;
        record->epoch = lean.registryEpoch;
        record->refused = false;
        if (record->built == nullptr) return record;
        try {
            if (leanShapeCurrent(*record->built, queue, *submission.shaders)) {
                LeanFront::AddOnWorker(LeanFront::Counter::ShapeRecordsRestamped);
                return record;
            }
        } catch (const std::exception&) {
        }
        lean.Retire(*record, DrawFinisher::Get().Front());
        return record;
    }
    auto& record = lean.Place(shape, serial, DrawFinisher::Get().Front());
    record.epoch = lean.registryEpoch;
    return &record;
}

LeanVerdict Driver::leanVerdict(const QueueState& queue, ShapeRecord& record, const Pm4::DrawParameters& parameters) {
    const auto index = verdictIndex(parameters);
    if (index >= record.verdicts.size()) return LeanVerdict::Live;
    auto& verdict = record.verdicts[index];
    if (verdict != LeanVerdict::Unknown) return verdict;
    std::vector<Graphics::RegisterRead> reads;
    {
        const Graphics::RegisterReadLogScope log(&reads);
        verdict = precheckVerdict(queue, parameters.indexed, parameters.indexSize);
    }
    if (verdict == LeanVerdict::Normal || verdict == LeanVerdict::Nothing) {
        const bool keyed = std::all_of(reads.begin(), reads.end(), [](const Graphics::RegisterRead& read) { return Graphics::DrawKeyCovers(read) && !(read.bank == Graphics::RegisterBank::Shader && runtimeRegister(read.offset)); });
        if (!keyed) verdict = LeanVerdict::Live;
    }
    return verdict;
}

bool Driver::buildLeanShape(const QueueState& queue, const Submission& submission, ShapeRecord& record) {
    if (record.built != nullptr) return true;
    if (record.refused) return false;
    auto built = std::make_shared<LeanShape>();
    try {
        std::vector<Graphics::RegisterRead> reads;
        std::shared_ptr<DrawDecode> decode;
        {
            const Graphics::RegisterReadLogScope log(&reads);
            decode = decodeDraw(queue, submission, leanDraws.device->graphicsContext());
        }
        const bool keyed = std::all_of(reads.begin(), reads.end(), [](const Graphics::RegisterRead& read) { return Graphics::DrawKeyCovers(read); });
        if (!keyed || !leanQualified(*decode)) {
            record.refused = true;
            LeanFront::AddOnWorker(LeanFront::Counter::ShapesRefused);
            return false;
        }
        built->shape = record.shape;
        built->shaderSet = ReadDrawShaderSet(queue, *submission.shaders);
        built->keySuffix.push_back(record.deviceSerial);
        for (std::size_t index = 0; index < DrawShaderSet::Bases.size(); ++index) {
            const auto& set = built->shaderSet;
            if (!set.present[index]) {
                built->keySuffix.push_back(0);
                continue;
            }
            built->keySuffix.insert(built->keySuffix.end(), set.mixes[index].begin(), set.mixes[index].end());
            if (set.registered[index]) built->aliasSuffix.insert(built->aliasSuffix.end(), set.mixes[index].begin(), set.mixes[index].end());
        }
        built->finalShape = fold(record.shape, built->keySuffix);
        built->reg207 = readRegister(queue.context, 0x207);
        built->skipped = std::any_of(decode->programs.begin(), decode->programs.end(), [](const DrawProgram& program) { return SkippedShader(program.binary.codeAddress); });
        built->writes = DrawWriteRanges(decode->state, {}, ExactDrawWrites());
        built->decode = std::move(decode);
    } catch (const std::exception&) {
        record.refused = true;
        LeanFront::AddOnWorker(LeanFront::Counter::ShapesRefused);
        return false;
    }
    record.built = std::move(built);
    LeanFront::AddOnWorker(LeanFront::Counter::ShapeRecordsBuilt);
    return true;
}

DrawVerdict Driver::drawLean(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::size_t packetOffset, std::string& rejected) {
    using LeanFront::Counter;
    static const bool allowed = [] {
        for (const char* name : {"APS5_PROFILE_DRAW", "APS5_LOCKED_DRAW_PREPARE", "APS5_DUMP_GRAPHICS_SHADERS", "APS5_DUMP_DRAW_SHADERS", "APS5_DUMP_DRAW_SLOT1", "APS5_TRACE_DRAW_PROGRAMS", "APS5_TRACE_SHADER_CORRELATION", "APS5_VERIFY_REGISTER_KEYS"}) {
            if (std::getenv(name) != nullptr) return false;
        }
        return drawEntries() && registerKeyEnabled() && !verifyDrawDataHits() && !verifyDrawRecipe() && !verifyDrawEntries() && !traceDrawCache() && DrawFinisher::Enabled() && DrawFinisher::Overlap();
    }();
    auto& finisher = DrawFinisher::Get();
    const auto today = [&](Counter counter) {
        LeanFront::AddOnWorker(counter);
        if (drawRelocationEnabled() && DrawFinisher::Busy() && finisher.Front() < leanDraws.published) finisher.Drain();
        return draw(queue, packet, submission, packetOffset, rejected);
    };
    if (!allowed || !finisher.RingStore() || !DrawPipeline::Active() || ShaderRecompiler::DebugProbeActive() || Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::TableSnapshot)) return today(Counter::DrawsOnTodaysPath);
    Pm4::DrawParameters parameters{};
    try {
        parameters = Pm4::ResolveDraw(packet, queue);
    } catch (const std::exception&) {
        return today(Counter::DrawsOnTodaysPath);
    }
    if (parameters.indirect) return today(Counter::IndirectDrawsOnTodaysPath);
    if (!parameters.indexed && (parameters.indexCount == 0 || parameters.instanceCount == 0)) return DrawVerdict::Nothing;
    if (!deferredLabels().labels.empty() || !leanDrawDevice()) return today(Counter::DrawsOnTodaysPath);
    auto& record = *leanShapeRecord(queue, submission);
    const auto verdict = leanVerdict(queue, record, parameters);
    if (verdict == LeanVerdict::Fallback) return today(Counter::DrawsOnTodaysPath);
    if (verdict == LeanVerdict::Nothing) {
        if (LeanFront::Due(frontLatch, LeanFront::Item::Draw)) return verifyLeanDraw(queue, packet, submission, packetOffset, rejected, record, verdict, parameters, nullptr);
        LeanFront::AddOnWorker(Counter::LeanDraws);
        return DrawVerdict::Nothing;
    }
    if (!buildLeanShape(queue, submission, record)) return today(Counter::DrawsOnTodaysPath);
    if (!drawEntriesThisDraw()) return today(Counter::DrawsOnTodaysPath);
    const auto& shape = *record.built;
    if (LeanFront::Due(frontLatch, LeanFront::Item::Draw)) {
        LeanDrawJob job{};
        job.shape = &shape;
        job.serial = submission.serial;
        job.queue = submission.queue;
        job.packetOffset = static_cast<std::uint64_t>(packet.data() - submission.commands.data());
        job.opcode = (packet[0] >> 8u) & 0xffu;
        job.parameters = parameters;
        job.words.Capture(queue.shader);
        job.drawKey = LeanExactKey(shape, job.words);
        job.shapeKey = shape.finalShape;
        job.deviceSerial = record.deviceSerial;
        return verifyLeanDraw(queue, packet, submission, packetOffset, rejected, record, verdict, parameters, &job);
    }
    if (verdict == LeanVerdict::Live) {
        LeanFront::AddOnWorker(Counter::PrecheckedLeanDraws);
        bool traceIndirect = false;
        if (const auto prechecked = precheckDraw(queue, submission, packetOffset, packet, parameters, rejected, traceIndirect)) return *prechecked;
    }
    if (shape.skipped) return DrawVerdict::Drawn;
    auto* payload = finisher.ReserveLean();
    const auto anchor = payload != nullptr ? leanDraws.AnchorFor(submission.shaders, leanDraws.device, finisher.Issued()) : std::nullopt;
    std::optional<LeanDrawJob> local;
    auto* job = payload != nullptr && anchor ? ::new (payload) LeanDrawJob : &local.emplace();
    job->shape = &shape;
    job->anchor = anchor.value_or(0);
    job->serial = submission.serial;
    job->queue = submission.queue;
    job->epoch = DrawPipeline::EpochToken().load(std::memory_order_relaxed);
    job->packetOffset = static_cast<std::uint64_t>(packet.data() - submission.commands.data());
    job->opcode = (packet[0] >> 8u) & 0xffu;
    job->parameters = parameters;
    job->words.Capture(queue.shader);
    job->drawKey = LeanExactKey(shape, job->words);
    job->shapeKey = shape.finalShape;
    job->deviceSerial = record.deviceSerial;
    job->recordFlags = 0;
    DrawPipeline::NoteDrawKey(job->drawKey, job->shapeKey);
    LeanFront::AddOnWorker(Counter::LeanDraws);
    if (local.has_value()) {
        finishLeanDraw(*local, LeanAnchor{submission.shaders, leanDraws.device, 0});
        return DrawVerdict::Drawn;
    }
    if (frontLatch.directDraws) {
        job->recordFlags = DrawRecordFlags::Direct;
        if (frontLatch.directBuffers) job->recordFlags |= DrawRecordFlags::DirectBuffers;
        if (frontLatch.directInputs) job->recordFlags |= DrawRecordFlags::DirectInputs;
        if (DirectDraws::Due(frontLatch)) job->recordFlags |= DrawRecordFlags::VerifyDue;
    }
    record.lastSeq = finisher.Issued();
    finisher.PublishLean(&Driver::runLeanDraw, shape.writes, job->drawKey, job->shapeKey);
    leanDraws.published = finisher.Issued();
    return DrawVerdict::Drawn;
}

DrawVerdict Driver::verifyLeanDraw(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::size_t packetOffset, std::string& rejected, ShapeRecord& record, LeanVerdict verdict, const Pm4::DrawParameters& parameters, const LeanDrawJob* job) {
    using LeanFront::Field;
    LeanDrawSink sink;
    struct SinkScope {
        LeanDraws& lean;
        ~SinkScope() { lean.sink = nullptr; }
    } scope{leanDraws};
    auto& finisher = DrawFinisher::Get();
    if (drawRelocationEnabled() && DrawFinisher::Busy() && finisher.Front() < leanDraws.published) finisher.Drain();
    leanDraws.sink = &sink;
    const auto result = draw(queue, packet, submission, packetOffset, rejected);
    leanDraws.sink = nullptr;
    LeanFront::NoteChecked(LeanFront::Item::Draw);
    const auto at = where(submission, packetOffset);
    const auto variant = verdictIndex(parameters);
    bool demote = false;
    if (verdict == LeanVerdict::Normal && (!sink.prechecked || !sink.passed)) {
        LeanFront::NoteDifference(Field::DrawVerdicts, at + ": the shape record says the precheck passes, today's precheck " + (sink.prechecked ? "returned verdict " + std::to_string(static_cast<int>(sink.verdict)) : std::string("did not run")));
        if (variant < record.verdicts.size()) record.verdicts[variant] = LeanVerdict::Live;
    } else if (verdict == LeanVerdict::Nothing && (!sink.prechecked || sink.passed || sink.verdict != DrawVerdict::Nothing)) {
        LeanFront::NoteDifference(Field::DrawVerdicts, at + ": the shape record says the draw draws nothing, today's precheck " + (!sink.prechecked ? std::string("did not run") : sink.passed ? std::string("passed") : "returned verdict " + std::to_string(static_cast<int>(sink.verdict))));
        if (variant < record.verdicts.size()) record.verdicts[variant] = LeanVerdict::Live;
    }
    if (job == nullptr || !sink.keyed) return result;
    const auto& shape = *job->shape;
    if (job->drawKey != sink.key || job->shapeKey != sink.shape) {
        LeanFront::NoteDifference(Field::DrawKeys, at + ": lean key " + hex(job->drawKey) + " shape " + hex(job->shapeKey) + ", today's key " + hex(sink.key) + " shape " + hex(sink.shape));
        demote = true;
    }
    if (drawRelocationEnabled()) {
        const auto identity = LeanIdentityKey(shape, job->words);
        if (identity != sink.identity) {
            LeanFront::NoteDifference(Field::DrawIdentity, at + ": lean identity " + hex(identity) + ", today's " + hex(sink.identity));
            demote = true;
        }
    }
    if (sink.aliasAsked) {
        std::uint64_t alias = 0;
        LeanAliasKey(shape, job->words, sink.relocation.get(), job->deviceSerial, alias);
        if (alias != sink.alias) {
            LeanFront::NoteDifference(Field::AliasKeys, at + ": lean alias " + hex(alias) + ", today's " + hex(sink.alias));
            demote = true;
        } else if (alias != 0) {
            std::shared_ptr<DrawEntry> todayEntry, leanEntry;
            std::shared_ptr<const DrawDecode> todayDonor, leanDonor;
            std::vector<DrawUserPatch> todayPatches, leanPatches;
            {
                std::lock_guard cacheLock(drawCacheMutex);
                relocateDrawEntry(queue, alias, todayEntry, todayDonor, todayPatches);
                relocateDrawEntry(job->words, alias, leanEntry, leanDonor, leanPatches);
            }
            const bool samePatches = std::equal(todayPatches.begin(), todayPatches.end(), leanPatches.begin(), leanPatches.end(), [](const DrawUserPatch& a, const DrawUserPatch& b) { return a.program == b.program && a.index == b.index && a.value == b.value; });
            if (todayDonor != leanDonor || !samePatches) {
                LeanFront::NoteDifference(Field::Patches, at + ": relocating alias " + hex(alias) + " with the job's user words gives " + std::to_string(leanPatches.size()) + " patches" + (todayDonor != leanDonor ? " from another donor" : "") + ", with the queue's registers " + std::to_string(todayPatches.size()));
                demote = true;
            }
        }
    }
    for (std::uint32_t offset = 0; offset < 0x200u; ++offset) {
        if (DrawUserWords::Index(offset) < 0 && DrawUserWords::ResourceIndex(offset) < 0) continue;
        const bool held = queue.shader.contains(offset);
        const auto value = readUserData(queue.shader, offset);
        if ((DrawUserWords::Index(offset) >= 0 && job->words.Contains(offset) != held) || job->words.Value(offset) != value) {
            LeanFront::NoteDifference(Field::UserWords, at + ": user word register " + hex(offset) + " captured as " + hex(job->words.Value(offset)) + (job->words.Contains(offset) ? "" : " (absent)") + ", the queue holds " + hex(value) + (held ? "" : " (absent)"));
            demote = true;
            break;
        }
    }
    try {
        const auto lean = LeanRefreshedDecode(*shape.decode, job->words);
        const auto fresh = decodeDraw(queue, submission, leanDraws.device->graphicsContext());
        if (!sameDecode(*lean, *fresh)) {
            LeanFront::NoteDifference(Field::Decodes, at + ": the shape record's decode with the draw's user words differs from a fresh decode (shape " + hex(job->shapeKey) + ")");
            demote = true;
        }
    } catch (const std::exception& error) {
        LeanFront::NoteDifference(Field::Decodes, at + ": decoding the draw for the check failed: " + error.what());
        demote = true;
    }
    if (sink.handed) {
        if (sink.held.unknown) {
            LeanFront::NoteAllowed(LeanFront::Allowed::HeldTargetsOnly);
        } else if (sink.held.writes != shape.writes) {
            LeanFront::NoteDifference(Field::WriteTemplates, at + ": the shape's write template holds " + std::to_string(shape.writes.size()) + " ranges, today's held writes " + std::to_string(sink.held.writes.size()));
            demote = true;
        }
        const auto& today = sink.parameters;
        const auto& mine = job->parameters;
        const bool sameParameters = today.indexAddress == mine.indexAddress && today.indexCount == mine.indexCount && today.indexSize == mine.indexSize && today.instanceCount == mine.instanceCount && today.flags == mine.flags && today.indexed == mine.indexed && today.firstVertex == mine.firstVertex && today.firstInstance == mine.firstInstance && !today.indirect && !mine.indirect;
        if (!sameParameters || sink.held.key != job->drawKey || sink.held.shape != job->shapeKey || sink.reg207 != shape.reg207) {
            LeanFront::NoteDifference(Field::Jobs, at + ": the lean job's " + (!sameParameters ? "draw parameters" : sink.reg207 != shape.reg207 ? "PA_CL_VS_OUT_CNTL" : "held key") + " differ from today's hand-off");
            demote = true;
        }
        if (sink.device != leanDraws.device.get()) LeanFront::NoteDifference(Field::Jobs, at + ": the lean job's device differs from the device today's draw used");
        if (sink.dataEntry != nullptr && sink.decode != nullptr) {
            try {
                if (!sameDecode(*LeanRefreshedDecode(*sink.dataEntry->decode, job->words), *sink.decode)) {
                    LeanFront::NoteDifference(Field::Decodes, at + ": the data entry's decode refreshed with the job's user words differs from today's refresh (shape " + hex(job->shapeKey) + ")");
                    demote = true;
                }
            } catch (const std::exception& error) {
                LeanFront::NoteDifference(Field::Decodes, at + ": refreshing the data entry's decode with the job's user words failed: " + error.what());
                demote = true;
            }
        }
    }
    if (demote) {
        leanDraws.Retire(record, DrawFinisher::Get().Front());
        record.refused = true;
    }
    return result;
}

}
