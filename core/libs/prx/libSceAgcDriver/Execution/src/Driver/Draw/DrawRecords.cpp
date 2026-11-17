#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRecords.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DirectDraws.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/LeanDraw.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Execution/include/DrawCommitViewSlot.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <string>
#include <type_traits>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::uint64_t FnvPrime = 0x100000001b3ull;

std::uint64_t mixWord(std::uint64_t key, std::uint64_t value) {
    key ^= value + 0x9e3779b97f4a7c15ull + (key << 6u) + (key >> 2u);
    key *= 0xff51afd7ed558ccdull;
    return key ^ (key >> 33u);
}

std::string hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

std::string keyText(const DrawRecordKey& key) {
    return "shape " + hex(key.shape) + " index " + hex(key.indexAddress) + "+" + std::to_string(key.indexCount) + (key.indexed ? " indexed" : " auto") + " instances " + std::to_string(key.instanceCount) + " first vertex " + std::to_string(key.firstVertex) + " first instance " + std::to_string(key.firstInstance);
}

}

DrawRecordKey DrawRecordKey::For(std::uint64_t shape, const Pm4::DrawParameters& parameters) {
    DrawRecordKey key;
    key.shape = shape;
    key.indexAddress = parameters.indexAddress;
    key.indexCount = parameters.indexCount;
    key.indexSize = parameters.indexSize;
    key.instanceCount = parameters.instanceCount;
    key.flags = parameters.flags;
    key.firstVertex = parameters.firstVertex;
    key.firstInstance = parameters.firstInstance;
    key.indexed = parameters.indexed;
    return key;
}

std::uint64_t DrawRecordKey::Probe() const {
    std::uint64_t probe = 0xcbf29ce484222325ull;
    const auto mix = [&](std::uint64_t value) { probe = (probe ^ value) * FnvPrime; };
    mix(shape);
    mix(indexAddress);
    mix((static_cast<std::uint64_t>(indexCount) << 32u) | indexSize);
    mix((static_cast<std::uint64_t>(instanceCount) << 32u) | flags);
    mix((static_cast<std::uint64_t>(firstVertex) << 32u) | firstInstance);
    mix(indexed ? 1u : 0u);
    return probe == 0 ? 1 : probe;
}

DrawRecordHash HashDrawRecordWords(std::span<const std::uint32_t> first, std::span<const std::uint32_t> second) {
    std::uint64_t low = 0xcbf29ce484222325ull;
    std::uint64_t high = 0x84222325cbf29ce4ull ^ first.size();
    const auto visit = [&](std::span<const std::uint32_t> words) {
        for (const auto word : words) {
            low = (low ^ word) * FnvPrime;
            high = mixWord(high, word);
        }
    };
    visit(first);
    low = (low ^ 0xfffffffffull) * FnvPrime;
    high = mixWord(high, second.size() | 0x100000000ull);
    visit(second);
    return {low, high};
}

void DrawRecordSink::Reset() {
    queued = false;
    verify = false;
    shape = 0;
    contentKey.clear();
    vertexLayout.clear();
    vertexInputs.clear();
    fragmentOutputs.reset();
    writes.clear();
}

void DrawVertexLayout(std::span<const std::optional<ShaderRecompiler::ShaderVertexStageInfo>> infos, std::vector<std::uint32_t>& layout, std::vector<std::uint32_t>& inputs) {
    layout.clear();
    inputs.clear();
    for (const auto& info : infos) {
        if (!info) {
            layout.push_back(0);
            continue;
        }
        const auto count = std::min<std::uint32_t>(info->resourcesNum, ShaderRecompiler::ShaderVertexStageInfo::MaxResources);
        layout.insert(layout.end(), {0x80000000u | count, info->fetchAttribReg, info->fetchBufferReg, info->fetchEmbedded ? 1u : 0u, info->paClVsOutCntl, info->depthRemapMode, info->pointList ? 1u : 0u, std::bit_cast<std::uint32_t>(info->pointSize), std::bit_cast<std::uint32_t>(info->pointSizeMin), std::bit_cast<std::uint32_t>(info->pointSizeMax)});
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto& fields = info->resources[index].fields;
            const auto& destination = info->resourcesDst[index];
            layout.insert(layout.end(), {fields[1] & 0xffff0000u, fields[3], static_cast<std::uint32_t>(destination.registerStart), static_cast<std::uint32_t>(destination.registersNum), static_cast<std::uint32_t>(destination.attrId), destination.fetchIndex});
            inputs.insert(inputs.end(), {fields[0], fields[1] & 0xffffu, fields[2]});
        }
    }
}

void DrawRecordSink::NoteVertices(std::span<const std::optional<ShaderRecompiler::ShaderVertexStageInfo>> infos) {
    DrawVertexLayout(infos, vertexLayout, vertexInputs);
}

std::shared_ptr<DrawRecord> DrawRecord::Build(const DrawRecordKey& key, std::shared_ptr<const LeanShape> shape, std::uint64_t deviceSerial, std::uint64_t present, const DrawRecordSink& sink) {
    auto record = std::make_shared<DrawRecord>();
    record->key = key;
    record->probe = key.Probe();
    record->shape = std::move(shape);
    record->deviceSerial = deviceSerial;
    record->builtPresent = present;
    record->content = sink.Content();
    record->inputs = sink.Inputs();
    record->contentKey = sink.contentKey;
    record->vertexLayout = sink.vertexLayout;
    record->vertexInputs = sink.vertexInputs;
    record->fragmentOutputs = sink.fragmentOutputs;
    record->writes = sink.writes;
    return record;
}

std::size_t DrawRecord::Bytes() const {
    return sizeof(DrawRecord) + 4 * (contentKey.capacity() + vertexLayout.capacity() + vertexInputs.capacity()) + sizeof(DrawPipeline::Range) * writes.capacity();
}

namespace DrawRecords {

namespace {

constexpr std::size_t FieldCount = static_cast<std::size_t>(Field::Count);
constexpr std::size_t AllowedKinds = static_cast<std::size_t>(Allowed::Count);
constexpr std::size_t CounterCount = static_cast<std::size_t>(Counter::Count);
constexpr std::uint64_t PrintedPerWindow = 8;
constexpr std::int64_t WindowMs = 10000;

struct FieldInfo {
    Item item;
    const char* name;
};

constexpr std::array<const char*, ItemCount> ItemNames{"answer", "commit"};

constexpr std::array<FieldInfo, FieldCount> FieldInfos{{
    {Item::Answer, "content keys"},
    {Item::Answer, "vertex layouts"},
    {Item::Answer, "write templates"},
    {Item::Answer, "fragment outputs"},
    {Item::Answer, "variants"},
    {Item::Answer, "push constants"},
    {Item::Answer, "binding words"},
    {Item::Answer, "vertex attributes"},
    {Item::Answer, "image views"},
    {Item::Answer, "draw parameters"},
    {Item::Answer, "read ranges"},
    {Item::Commit, "templates"},
    {Item::Commit, "pipelines"},
    {Item::Commit, "framebuffers"},
    {Item::Commit, "target views"},
    {Item::Commit, "targets"},
    {Item::Commit, "pass keys"},
    {Item::Commit, "moved buffers"},
    {Item::Commit, "push constants"},
    {Item::Commit, "vertex inputs"},
    {Item::Commit, "shader stages"},
    {Item::Commit, "fragment outputs"},
}};

constexpr std::array<const char*, AllowedKinds> AllowedNames{"fragment outputs known on one side only", "today's draw not queued", "the record's commit fell back", "today's commit built no recipe", "today's commit served another template"};

constexpr std::array<const char*, CounterCount> CounterNames{
    "recorded", "targets held by the pass", "moved buffers trusted", "committer halves built", "verified on today's path",
    "no committer half", "not recordable", "targets", "template gone", "objects gone", "texture proof", "moved buffers", "no recipe built", "demoted",
};

static_assert(ItemNames.back() != nullptr && FieldInfos.back().name != nullptr && AllowedNames.back() != nullptr && CounterNames.back() != nullptr, "every draw record item, field, by-design case and counter needs a name");

struct Tally {
    std::atomic<std::uint64_t> value{0};
    std::uint64_t reported = 0;

    std::uint64_t Window() {
        const auto now = value.load(std::memory_order_relaxed);
        const auto since = now - reported;
        reported = now;
        return since;
    }
};

std::array<Tally, ItemCount> checked{};
std::array<Tally, FieldCount> differences{};
std::array<Tally, AllowedKinds> allowed{};
std::array<Tally, CounterCount> counters{};
std::atomic<std::uint64_t> printed{0};
std::atomic<bool> corruptingCommits{false};
std::mutex reportMutex;
std::int64_t reportedAt = 0;

std::string listed(std::size_t count, const auto& value, const auto& name) {
    std::string text;
    for (std::size_t index = 0; index < count; ++index) {
        if (value(index) == 0) continue;
        text += text.empty() ? std::string(": ") : std::string(", ");
        text += name(index);
        text += ' ';
        text += std::to_string(value(index));
    }
    return text;
}

std::string counted(const std::array<std::uint64_t, CounterCount>& counts, Counter first, Counter last) {
    std::string text;
    for (auto index = static_cast<std::size_t>(first); index <= static_cast<std::size_t>(last); ++index) {
        if (counts[index] == 0) continue;
        text += text.empty() ? "" : ", ";
        text += CounterNames[index];
        text += ' ';
        text += std::to_string(counts[index]);
    }
    return text.empty() ? std::string("none") : text;
}

}

void CorruptCommits(bool corrupt) {
    corruptingCommits.store(corrupt, std::memory_order_relaxed);
}

bool CorruptingCommits() {
    return corruptingCommits.load(std::memory_order_relaxed);
}

std::uint64_t VerifyEvery() {
    static const std::uint64_t every = [] {
        const char* text = std::getenv("APS5_VERIFY_DRAW_RECORDS");
        return text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
    }();
    return every;
}

void NoteChecked(Item item) {
    checked[static_cast<std::size_t>(item)].value.fetch_add(1, std::memory_order_relaxed);
}

void NoteDifference(Field field, std::string_view detail) {
    const auto& info = FieldInfos[static_cast<std::size_t>(field)];
    differences[static_cast<std::size_t>(field)].value.fetch_add(1, std::memory_order_relaxed);
    if (printed.fetch_add(1, std::memory_order_relaxed) < PrintedPerWindow) std::fprintf(stderr, "[draw-records-verify] %s %s differs: %.*s\n", ItemName(info.item), info.name, static_cast<int>(detail.size()), detail.data());
}

void NoteAllowed(Allowed kind) {
    allowed[static_cast<std::size_t>(kind)].value.fetch_add(1, std::memory_order_relaxed);
}

void Add(Counter counter, std::uint64_t count) {
    counters[static_cast<std::size_t>(counter)].value.fetch_add(count, std::memory_order_relaxed);
}

void AddOnCommitter(Counter counter, std::uint64_t count) {
    auto& value = counters[static_cast<std::size_t>(counter)].value;
    value.store(value.load(std::memory_order_relaxed) + count, std::memory_order_relaxed);
}

void Report(bool force) {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::lock_guard lock(reportMutex);
    const auto last = reportedAt;
    if (last == 0) {
        reportedAt = now;
        if (!force) return;
    } else if (!force && now - last < WindowMs) {
        return;
    }
    reportedAt = now;
    const double seconds = last == 0 ? 0.0 : static_cast<double>(now - last) / 1000.0;
    std::array<std::uint64_t, CounterCount> counts{};
    for (std::size_t index = 0; index < CounterCount; ++index) counts[index] = counters[index].Window();
    std::array<std::uint64_t, ItemCount> checks{};
    std::uint64_t checksTotal = 0;
    for (std::size_t index = 0; index < ItemCount; ++index) checksTotal += checks[index] = checked[index].Window();
    std::array<std::uint64_t, FieldCount> differing{};
    std::uint64_t differingTotal = 0;
    for (std::size_t index = 0; index < FieldCount; ++index) differingTotal += differing[index] = differences[index].Window();
    std::array<std::uint64_t, AllowedKinds> byDesign{};
    std::uint64_t byDesignTotal = 0;
    for (std::size_t index = 0; index < AllowedKinds; ++index) byDesignTotal += byDesign[index] = allowed[index].Window();
    printed.store(0, std::memory_order_relaxed);
    std::uint64_t commits = 0;
    for (const auto count : counts) commits += count;
    if (commits != 0) {
        AgcDriver::ProfilePrint_nid_no_patch("[draw-records] %.1f s: commit: %s; commit misses: %s\n", seconds, counted(counts, Counter::CommitHits, Counter::CommitVerified).c_str(), counted(counts, Counter::CommitFirst, Counter::CommitDemoted).c_str());
    }
    if (VerifyEvery() == 0 || (checksTotal == 0 && differingTotal == 0 && byDesignTotal == 0)) return;
    const auto fields = listed(FieldCount, [&](std::size_t index) { return differing[index]; }, [&](std::size_t index) { return std::string(ItemName(FieldInfos[index].item)) + " " + FieldInfos[index].name; });
    const auto cases = listed(AllowedKinds, [&](std::size_t index) { return byDesign[index]; }, [&](std::size_t index) { return std::string(AllowedNames[index]); });
    const auto check = [&](Item item) { return static_cast<unsigned long long>(checks[static_cast<std::size_t>(item)]); };
    AgcDriver::ProfilePrint_nid_no_patch("[draw-records-verify] %.1f s: every %llu; checked: answers %llu, commits %llu; differences %llu%s; by design %llu%s\n", seconds,
        static_cast<unsigned long long>(VerifyEvery()), check(Item::Answer), check(Item::Commit), static_cast<unsigned long long>(differingTotal), fields.c_str(), static_cast<unsigned long long>(byDesignTotal), cases.c_str());
}

const char* ItemName(Item item) {
    return ItemNames[static_cast<std::size_t>(item)];
}

const char* FieldName(Field field) {
    return FieldInfos[static_cast<std::size_t>(field)].name;
}

std::uint64_t Checked(Item item) {
    return checked[static_cast<std::size_t>(item)].value.load(std::memory_order_relaxed);
}

std::uint64_t Differences() {
    std::uint64_t total = 0;
    for (const auto& field : differences) total += field.value.load(std::memory_order_relaxed);
    return total;
}

std::uint64_t Differences(Field field) {
    return differences[static_cast<std::size_t>(field)].value.load(std::memory_order_relaxed);
}

std::uint64_t AllowedCount(Allowed kind) {
    return allowed[static_cast<std::size_t>(kind)].value.load(std::memory_order_relaxed);
}

std::uint64_t Total(Counter counter) {
    return counters[static_cast<std::size_t>(counter)].value.load(std::memory_order_relaxed);
}

}

namespace {

std::string drawText(const LeanDrawJob& job, const DrawRecordKey& key) {
    return "draw at submission " + std::to_string(job.serial) + " DWORD " + std::to_string(job.packetOffset) + " (" + keyText(key) + ")";
}

template <typename T>
std::string firstDifference(const std::vector<T>& record, const std::vector<T>& today) {
    if (record.size() != today.size()) return std::to_string(record.size()) + " words in the record, " + std::to_string(today.size()) + " today";
    for (std::size_t index = 0; index < record.size(); ++index) {
        if (record[index] == today[index]) continue;
        if constexpr (std::is_integral_v<T>) return "word " + std::to_string(index) + " is " + hex(record[index]) + " in the record, " + hex(today[index]) + " today";
        else return "entry " + std::to_string(index) + " differs";
    }
    return "equal";
}

std::string bindingDifference(const std::vector<std::uint32_t>& record, const std::vector<std::uint32_t>& today) {
    constexpr std::size_t HeaderWords = 6;
    std::size_t at = 0;
    for (std::size_t binding = 0; at + HeaderWords <= record.size() && at + HeaderWords <= today.size(); ++binding) {
        const auto words = record[at + HeaderWords - 1];
        if (!std::equal(record.begin() + static_cast<std::ptrdiff_t>(at), record.begin() + static_cast<std::ptrdiff_t>(at + HeaderWords), today.begin() + static_cast<std::ptrdiff_t>(at))) return "binding " + std::to_string(binding) + " (role " + std::to_string(record[at + 1]) + ", kind " + std::to_string(record[at]) + ") has another shape: " + std::to_string(words) + " words in the record, " + std::to_string(today[at + HeaderWords - 1]) + " today";
        for (std::size_t word = 0; word < words && at + HeaderWords + word < record.size(); ++word) {
            const auto kept = record[at + HeaderWords + word];
            const auto now = today[at + HeaderWords + word];
            if (kept != now) return "binding " + std::to_string(binding) + " (role " + std::to_string(record[at + 1]) + ", kind " + std::to_string(record[at]) + ", set " + std::to_string(record[at + 2]) + ", binding " + std::to_string(record[at + 3]) + ") word " + std::to_string(word) + " is " + hex(kept) + " in the record, " + hex(now) + " today";
        }
        at += HeaderWords + words;
    }
    return firstDifference(record, today);
}

}

void DrawRecordView::Capture(std::span<const Graphics::CompiledShader> shaders, const Pm4::DrawParameters& drawParameters) {
    stages.clear();
    for (const auto& shader : shaders) {
        auto& view = stages.emplace_back();
        if (shader.program == nullptr) continue;
        view.variant = shader.program->variantId;
        view.stage = static_cast<std::uint32_t>(shader.stage);
        view.pushOffset = shader.pushConstantOffset;
        for (const auto& binding : shader.program->bindings) {
            const auto& words = Graphics::DrawGuestWords(shader, binding);
            view.bindings.insert(view.bindings.end(), {static_cast<std::uint32_t>(binding.kind), static_cast<std::uint32_t>(binding.role), binding.descriptorSet, binding.binding, binding.count, static_cast<std::uint32_t>(words.size())});
            view.bindings.insert(view.bindings.end(), words.begin(), words.end());
        }
        for (const auto& attribute : Graphics::DrawVertexAttributes(shader)) {
            const auto& fields = attribute.resource.fields;
            view.vertexAttributes.insert(view.vertexAttributes.end(), {attribute.location, attribute.components, fields[0], fields[1], fields[2], fields[3], attribute.fetchIndex, attribute.formatComponents});
        }
        if (shader.registeredImageViews != nullptr) view.views = *shader.registeredImageViews;
    }
    pushed = false;
    try {
        push = Graphics::AssemblePushConstants(shaders);
        pushed = true;
    } catch (const std::exception&) {
        push = {};
    }
    parameters = {drawParameters.indexAddress, drawParameters.indexCount | static_cast<std::uint64_t>(drawParameters.indexSize) << 32u, drawParameters.instanceCount | static_cast<std::uint64_t>(drawParameters.flags) << 32u, drawParameters.firstVertex | static_cast<std::uint64_t>(drawParameters.firstInstance) << 32u, drawParameters.indexed ? 1u : 0u, drawParameters.indirect.has_value() ? 1u : 0u};
}

void Driver::commitRecordDraw(const LeanDrawJob& job, const LeanAnchor& anchor, std::shared_ptr<DrawRecordHit> hit) {
    auto writes = hit->record->writes;
    auto reads = std::move(hit->reads);
    const auto& graphics = job.shape->decode->state;
    const VulkanDevice::DrawTraceContext trace{0, job.serial, job.queue, static_cast<std::size_t>(job.packetOffset), graphics.hasColorTarget ? graphics.color.address : 0};
    auto commit = [this, hit = std::move(hit), localDevice = anchor.device, decode = job.shape->decode, queue = job.queue, opcode = job.opcode, epoch = job.epoch, trace, shape = job.shapeKey] {
        DrawPipeline::FollowEpoch(epoch);
        GuestMemory::SetCurrentPacket(opcode, queue);
        commitRecord(localDevice, queue, decode->state, *hit, trace, shape);
    };
    DrawPipeline::Queue0().Enqueue(DrawPipeline::CommitKind::Draw, std::move(commit), std::move(writes), 0, {}, std::move(reads), true);
}

namespace {

DrawRecords::Counter CommitMiss(Graphics::DrawRecipeMiss miss) {
    using DrawRecords::Counter;
    switch (miss) {
    case Graphics::DrawRecipeMiss::NotRecordable: return Counter::CommitNotRecordable;
    case Graphics::DrawRecipeMiss::TargetGone: return Counter::CommitTargets;
    case Graphics::DrawRecipeMiss::TemplateGone: return Counter::CommitTemplate;
    case Graphics::DrawRecipeMiss::Proof: return Counter::CommitProof;
    case Graphics::DrawRecipeMiss::Buffers: return Counter::CommitMoved;
    default: return Counter::CommitObjects;
    }
}

void NoteDirectCommit(const DrawRecordCommit& commit, bool recorded) {
    using DirectDraws::Counter;
    DirectDraws::Add(recorded ? Counter::CommitHits : Counter::CommitMisses);
    const auto hold = commit.effects.hold;
    if (commit.effects.sharedHold) DirectDraws::Add(Counter::HoldShared);
    else if (hold == DrawRecordHold::Held) DirectDraws::Add(Counter::HoldOwn);
    else if (hold != DrawRecordHold::Count) DirectDraws::Add(static_cast<Counter>(static_cast<std::size_t>(Counter::HoldFirst) + static_cast<std::size_t>(hold) - static_cast<std::size_t>(DrawRecordHold::First)));
}

void NoteDirectBuffers(const DrawRecordCommit& commit, bool prepared) {
    using DirectDraws::Counter;
    const auto& effects = commit.effects;
    switch (effects.buffers) {
    case DrawRecordBuffers::None:
        if (prepared) DirectDraws::Add(Counter::BuffersDropped);
        break;
    case DrawRecordBuffers::Used: DirectDraws::Add(Counter::BuffersUsed); break;
    case DrawRecordBuffers::PlanDiffers: DirectDraws::Add(Counter::BuffersPlanDiffers); break;
    case DrawRecordBuffers::MovedHazard: DirectDraws::Add(Counter::BuffersMovedHazard); break;
    case DrawRecordBuffers::UnmovedHazard: DirectDraws::Add(Counter::BuffersUnmovedHazard); break;
    case DrawRecordBuffers::DepthGuards: DirectDraws::Add(Counter::BuffersDepthGuards); break;
    case DrawRecordBuffers::Checked: DirectDraws::Add(Counter::BuffersVerifyChecked); break;
    case DrawRecordBuffers::Differs:
        DirectDraws::Add(Counter::BuffersVerifyChecked);
        DirectDraws::Add(Counter::BuffersVerifyDiffering);
        DirectDraws::NoteBufferDifference(effects.bufferDifference.what != nullptr ? effects.bufferDifference.what : "buffers", effects.bufferDifference.detail);
        break;
    case DrawRecordBuffers::MemoryChanged:
        DirectDraws::Add(Counter::BuffersVerifyChecked);
        DirectDraws::Add(Counter::BuffersMemoryChanged);
        break;
    case DrawRecordBuffers::Unwatched:
        DirectDraws::Add(Counter::BuffersVerifyChecked);
        DirectDraws::Add(Counter::BuffersUnwatched);
        DirectDraws::NoteBufferDifference(effects.bufferDifference.what != nullptr ? effects.bufferDifference.what : "buffers", effects.bufferDifference.detail);
        break;
    default: break;
    }
}

void NoteDirectInputs(const DrawRecordCommit& commit, bool prepared) {
    using DirectDraws::Counter;
    switch (commit.effects.inputs) {
    case DrawRecordInputs::None:
        if (prepared) DirectDraws::Add(Counter::InputsDropped);
        break;
    case DrawRecordInputs::Used: DirectDraws::Add(Counter::InputsUsed); break;
    case DrawRecordInputs::Rechecked:
        DirectDraws::Add(Counter::InputsUsed);
        DirectDraws::Add(Counter::InputsRechecked);
        break;
    case DrawRecordInputs::ImportChanged: DirectDraws::Add(Counter::InputsImportChanged); break;
    case DrawRecordInputs::Shape: DirectDraws::Add(Counter::InputsCommitShape); break;
    case DrawRecordInputs::Refused: DirectDraws::Add(Counter::InputsRefused); break;
    case DrawRecordInputs::Checked: DirectDraws::Add(Counter::InputsVerifyChecked); break;
    case DrawRecordInputs::Differs:
        DirectDraws::Add(Counter::InputsVerifyChecked);
        DirectDraws::Add(Counter::InputsVerifyDiffering);
        break;
    default: break;
    }
}

void PublishInputsRecipe(DrawRecordCommit& commit) {
    if (commit.inputsRecipeAsked || commit.recipe == nullptr || !Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::DirectInputs)) return;
    commit.inputsRecipeAsked = true;
    commit.inputsRecipe.store(commit.recipe);
}

void PublishBufferPlan(DrawRecordCommit& commit, std::span<const Graphics::CompiledShader> stages) {
    if (commit.bufferPlanAsked || commit.recipe == nullptr || !Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::DirectBuffers)) return;
    commit.bufferPlanAsked = true;
    if (const auto resources = commit.recipe->templateRef.lock(); resources != nullptr) commit.bufferPlan.store(resources->DirectBufferPlan(stages));
}

bool SameVertexInput(const Graphics::VertexInputLayout& a, const Graphics::VertexInputLayout& b) {
    const auto sameBinding = [](const VkVertexInputBindingDescription& x, const VkVertexInputBindingDescription& y) { return x.binding == y.binding && x.stride == y.stride && x.inputRate == y.inputRate; };
    const auto sameAttribute = [](const VkVertexInputAttributeDescription& x, const VkVertexInputAttributeDescription& y) { return x.location == y.location && x.binding == y.binding && x.format == y.format && x.offset == y.offset; };
    return std::ranges::equal(a.bindings, b.bindings, sameBinding) && std::ranges::equal(a.attributes, b.attributes, sameAttribute);
}

}

void Driver::commitRecord(std::shared_ptr<VulkanDevice> localDevice, std::uint32_t queue, const Graphics::State& graphics, DrawRecordHit& hit, const VulkanDevice::DrawTraceContext& trace, std::uint64_t shape) {
    using DrawRecords::Counter;
    const auto* tracePtr = trace.token != 0 ? &trace : nullptr;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    if (auto current = device.Load(); current != nullptr && current != localDevice) localDevice = std::move(current);
    struct SubmitDue {
        Driver& driver;
        const std::shared_ptr<VulkanDevice>& device;
        ~SubmitDue() noexcept(false) {
            if (std::uncaught_exceptions() == 0) driver.submitDueAfterCommit(device.get());
        }
    } submitDue{*this, localDevice};
    recordLabelsForPacket(localDevice.get(), queue);
    noteDrawWriters(hit.stages, queue);
    auto& commit = *hit.record->commit;
    const auto& record = *hit.record;
    const auto* keyWords = record.contentKey.empty() ? nullptr : &record.contentKey;
    static std::uint64_t ordinal = 0;
    const auto every = DrawRecords::VerifyEvery();
    const bool verify = commit.recipe != nullptr && every != 0 && ++ordinal % every == 0;
    if (commit.recipe == nullptr) {
        DrawRecords::AddOnCommitter(Counter::CommitFirst);
        if (hit.direct) DirectDraws::Add(DirectDraws::Counter::CommitFirst);
    } else if (!verify) {
        const auto* prepared = hit.preparedBuffers.has_value() ? &*hit.preparedBuffers : nullptr;
        static std::uint64_t bufferOrdinal = 0;
        const auto bufferEvery = DirectDraws::BufferVerifyEvery();
        commit.verifyBuffers = prepared != nullptr && bufferEvery != 0 && ++bufferOrdinal % bufferEvery == 0;
        auto* inputs = hit.preparedInputs.has_value() ? &*hit.preparedInputs : nullptr;
        static std::uint64_t inputOrdinal = 0;
        const auto inputEvery = DirectDraws::InputVerifyEvery();
        commit.verifyInputs = inputs != nullptr && inputEvery != 0 && ++inputOrdinal % inputEvery == 0;
        commit.effects = {};
        const auto outcome = localDevice->DrawFromRecord(graphics, hit.parameters, hit.stages, commit, tracePtr, nullptr, prepared, inputs);
        if (hit.direct) {
            NoteDirectCommit(commit, outcome.recorded);
            NoteDirectBuffers(commit, prepared != nullptr);
            NoteDirectInputs(commit, inputs != nullptr);
        }
        if (outcome.recorded) {
            if (hit.direct) {
                PublishBufferPlan(commit, hit.stages);
                PublishInputsRecipe(commit);
            }
            const auto& effects = commit.effects;
            DrawRecords::AddOnCommitter(Counter::CommitHits);
            if (effects.passHeld) DrawRecords::AddOnCommitter(Counter::CommitPassHeld);
            if (effects.movedTrusted != 0) DrawRecords::AddOnCommitter(Counter::CommitMovedTrusted, effects.movedTrusted);
            return;
        }
        DrawRecords::AddOnCommitter(CommitMiss(outcome.miss));
    }
    if ((commit.recipe == nullptr || verify) && hit.preparedBuffers.has_value()) DirectDraws::Add(DirectDraws::Counter::BuffersDropped);
    if ((commit.recipe == nullptr || verify) && hit.preparedInputs.has_value()) DirectDraws::Add(DirectDraws::Counter::InputsDropped);
    DrawCommitView answer;
    if (verify) static_cast<void>(localDevice->DrawFromRecord(graphics, hit.parameters, hit.stages, commit, tracePtr, &answer));
    DrawCommitView today;
    struct ViewScope {
        ~ViewScope() { DrawCommitViewSlot::Set(nullptr); }
    } viewScope;
    if (verify) DrawCommitViewSlot::Set(&today);
    std::shared_ptr<const DrawRecipe> built;
    localDevice->Draw(graphics, hit.parameters, hit.stages, hit.snapshots, &built, tracePtr, shape, keyWords, true);
    DrawCommitViewSlot::Set(nullptr);
    bool differs = false;
    if (verify) {
        DrawRecords::AddOnCommitter(Counter::CommitVerified);
        differs = verifyRecordCommit(record, answer, today);
    }
    if (differs) {
        commit.Adopt(nullptr);
        DrawRecords::AddOnCommitter(Counter::CommitDemoted);
        if (hit.direct) RefuseDirectClass(hit.owner);
        return;
    }
    if (built != nullptr && (!verify || !answer.filled || answer.resources != today.resources)) {
        commit.Adopt(std::move(built));
        DrawRecords::AddOnCommitter(Counter::CommitBuilt);
    } else if (!verify) {
        DrawRecords::AddOnCommitter(Counter::CommitUnbuilt);
    }
}

bool Driver::verifyRecordCommit(const DrawRecord& record, const DrawCommitView& answer, const DrawCommitView& today) {
    using DrawRecords::Field;
    DrawRecords::NoteChecked(DrawRecords::Item::Commit);
    if (!answer.filled) {
        DrawRecords::NoteAllowed(DrawRecords::Allowed::CommitRecordMissed);
        return false;
    }
    if (!today.filled) {
        DrawRecords::NoteAllowed(DrawRecords::Allowed::CommitTodayUnrecorded);
        return false;
    }
    const auto at = keyText(record.key);
    bool differs = false;
    const auto note = [&](Field field, const std::string& detail) {
        DrawRecords::NoteDifference(field, at + ": " + detail);
        differs = true;
    };
    if (answer.resources != today.resources) {
        if (answer.key != today.key) note(Field::Templates, "the record's template key has " + std::to_string(answer.key.size()) + " words, today's " + std::to_string(today.key.size()) + "; " + firstDifference(answer.key, today.key));
        else DrawRecords::NoteAllowed(DrawRecords::Allowed::CommitTemplateRebuilt);
        return differs;
    }
    if (answer.pipeline != today.pipeline) note(Field::Pipelines, "the record binds another pipeline object than today's draw");
    if (answer.framebuffer != today.framebuffer) note(Field::Framebuffers, "the record renders into another framebuffer than today's draw");
    if (answer.views != today.views) note(Field::TargetViews, "the record has " + std::to_string(answer.views.size()) + " attachment views, today's draw " + std::to_string(today.views.size()));
    if (answer.targets != today.targets) note(Field::Targets, "the record's resident targets are other objects than today's");
    if (answer.passKey != today.passKey) note(Field::PassKeys, "the record's pass key " + hex(answer.passKey) + ", today's " + hex(today.passKey));
    if (answer.moved != today.moved || answer.movedSizes != today.movedSizes || answer.movedWords != today.movedWords) {
        std::size_t index = 0;
        while (index < answer.moved.size() && index < today.moved.size() && answer.moved[index] == today.moved[index] && answer.movedSizes[index] == today.movedSizes[index] && answer.movedWords[index] == today.movedWords[index]) ++index;
        const auto describe = [&](const DrawCommitView& view) { return index < view.moved.size() ? "allocation " + std::to_string(view.moved[index].first) + " at " + hex(view.moved[index].second) + " (" + std::to_string(view.movedSizes[index]) + " bytes, " + std::to_string(view.movedWords[index].size()) + " words)" : std::string("nothing"); };
        note(Field::MovedBuffers, "the record moves " + std::to_string(answer.moved.size()) + " buffers, today's draw " + std::to_string(today.moved.size()) + "; at " + std::to_string(index) + " the record has " + describe(answer) + ", today's " + describe(today));
    }
    if (answer.pushStages != today.pushStages || answer.push != today.push) {
        std::size_t dword = 0;
        while (dword * 4 < answer.push.size() && std::memcmp(answer.push.data() + dword * 4, today.push.data() + dword * 4, 4) == 0) ++dword;
        note(Field::CommitPushConstants, "the record's push constants differ at dword " + std::to_string(dword) + " (stages " + hex(answer.pushStages) + ", today's " + hex(today.pushStages) + ")");
    }
    if (!SameVertexInput(answer.vertexInput, today.vertexInput)) note(Field::VertexInputs, "the record's vertex input has " + std::to_string(answer.vertexInput.attributes.size()) + " attributes, today's " + std::to_string(today.vertexInput.attributes.size()));
    if (answer.shaderStages != today.shaderStages || answer.hasDepth != today.hasDepth) note(Field::ShaderStages, "the record's stages " + hex(answer.shaderStages) + ", today's " + hex(today.shaderStages));
    if (answer.fragmentOutputs != today.fragmentOutputs) note(Field::CommitFragmentOutputs, "the record's fragment outputs " + hex(answer.fragmentOutputs) + ", today's " + hex(today.fragmentOutputs));
    if (DrawRecords::CorruptingCommits() && !differs) note(Field::PassKeys, "a corrupted committer answer");
    return differs;
}

bool Driver::compareRecordAnswer(const LeanDrawJob& job, const DrawRecord& record, const DrawRecordView& answer, const DrawRecordSink& sink, void (*noteDifference)(DrawRecords::Field, std::string_view), void (*noteAllowed)(DrawRecords::Allowed)) {
    using DrawRecords::Field;
    if (!sink.queued) {
        noteAllowed(DrawRecords::Allowed::TodayNotQueued);
        return false;
    }
    const auto& today = sink.today;
    const auto at = drawText(job, record.key);
    bool differs = false;
    const auto note = [&](Field field, const std::string& detail) {
        noteDifference(field, at + ": " + detail);
        differs = true;
    };
    if (answer.stages.size() != today.stages.size()) {
        note(Field::Variants, "the record has " + std::to_string(answer.stages.size()) + " stages, today's draw " + std::to_string(today.stages.size()));
    } else {
        for (std::size_t i = 0; i < answer.stages.size(); ++i) {
            const auto& kept = answer.stages[i];
            const auto& now = today.stages[i];
            const auto stage = "stage " + std::to_string(i) + ": ";
            if (kept.variant != now.variant || kept.stage != now.stage) note(Field::Variants, stage + "the record's variant " + hex(kept.variant) + ", today's " + hex(now.variant));
            if (kept.pushOffset != now.pushOffset) note(Field::PushConstants, stage + "the record pushes at byte " + std::to_string(kept.pushOffset) + ", today's draw at " + std::to_string(now.pushOffset));
            if (kept.bindings != now.bindings) note(Field::Bindings, stage + bindingDifference(kept.bindings, now.bindings));
            if (kept.vertexAttributes != now.vertexAttributes) note(Field::VertexAttributes, stage + firstDifference(kept.vertexAttributes, now.vertexAttributes));
            if (kept.views != now.views) note(Field::ImageViews, stage + "the record holds " + std::to_string(kept.views.size()) + " registered image views, today's draw " + std::to_string(now.views.size()));
        }
    }
    if (answer.pushed != today.pushed || answer.push != today.push) {
        std::size_t dword = 0;
        while (dword * 4 < answer.push.size() && std::memcmp(answer.push.data() + dword * 4, today.push.data() + dword * 4, 4) == 0) ++dword;
        note(Field::PushConstants, "the assembled push constants differ at dword " + std::to_string(dword));
    }
    if (answer.parameters != today.parameters) {
        std::size_t index = 0;
        while (index < answer.parameters.size() && answer.parameters[index] == today.parameters[index]) ++index;
        note(Field::Parameters, "field " + std::to_string(index) + " is " + hex(answer.parameters[index]) + " in the record, " + hex(today.parameters[index]) + " today");
    }
    if (answer.reads != today.reads) note(Field::ReadRanges, "the record reads " + std::to_string(answer.reads.size()) + " ranges, today's draw " + std::to_string(today.reads.size()));
    if (answer.writes != today.writes) note(Field::WriteTemplates, "the record holds " + std::to_string(answer.writes.size()) + " write ranges, today's draw writes " + std::to_string(today.writes.size()));
    if (answer.contentKey != today.contentKey) note(Field::ContentKeys, firstDifference(answer.contentKey, today.contentKey));
    if (record.vertexLayout != sink.vertexLayout) note(Field::VertexLayouts, firstDifference(record.vertexLayout, sink.vertexLayout));
    if (answer.fragmentOutputs.has_value() != today.fragmentOutputs.has_value()) noteAllowed(DrawRecords::Allowed::FragmentOutputsOneSide);
    else if (answer.fragmentOutputs != today.fragmentOutputs) note(Field::FragmentOutputs, "the record's fragment outputs " + hex(*answer.fragmentOutputs) + ", today's " + hex(*today.fragmentOutputs));
    return differs;
}

}
