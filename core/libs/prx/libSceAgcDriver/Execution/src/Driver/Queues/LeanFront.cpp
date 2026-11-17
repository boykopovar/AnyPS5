#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DirectDrawReport.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRecordReport.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

namespace AgcDriver::DriverDetail {

namespace LeanFront {

namespace {

constexpr std::size_t FieldCount = static_cast<std::size_t>(Field::Count);
constexpr std::size_t AllowedKinds = static_cast<std::size_t>(Allowed::Count);
constexpr std::size_t CounterCount = static_cast<std::size_t>(Counter::Count);
constexpr std::size_t CoreCounters = static_cast<std::size_t>(Counter::SwitchCopies) + 1;
constexpr std::uint64_t PrintedPerWindow = 8;
constexpr std::int64_t WindowMs = 10000;
constexpr auto SwitchMoveWait = std::chrono::milliseconds(50);

struct FieldInfo {
    Item item;
    const char* name;
};

constexpr std::array<const char*, ItemCount> ItemNames{"segment", "register list", "packet", "draw", "switch read"};

constexpr std::array<FieldInfo, FieldCount> FieldInfos{{
    {Item::Segment, "words"},
    {Item::Segment, "conditional ends"},
    {Item::Segment, "rewind tail"},
    {Item::Segment, "released tails"},
    {Item::Segment, "flips"},
    {Item::Segment, "rendering waits"},
    {Item::Segment, "label writes"},
    {Item::Segment, "held waits"},
    {Item::Segment, "packet table"},
    {Item::RegisterList, "lists"},
    {Item::RegisterList, "cache"},
    {Item::Packet, "pipeline verdicts"},
    {Item::Packet, "flush records"},
    {Item::Packet, "flush submits"},
    {Item::Packet, "flush caps"},
    {Item::Packet, "flush reaps"},
    {Item::Draw, "verdicts"},
    {Item::Draw, "keys"},
    {Item::Draw, "identity"},
    {Item::Draw, "decodes"},
    {Item::Draw, "user words"},
    {Item::Draw, "alias keys"},
    {Item::Draw, "patches"},
    {Item::Draw, "write templates"},
    {Item::Draw, "jobs"},
    {Item::Draw, "slot reuse"},
    {Item::SwitchRead, "values"},
}};

constexpr std::array<const char*, AllowedKinds> AllowedNames{"batch cap moved since the gate's copy", "early flush checks", "switches moved in the segment", "lists rewritten inside a collect epoch", "held writes known from the shape where today's are unknown"};

constexpr std::array<const char*, CounterCount> CounterNames{
    "lean segments", "today's segments", "mode changes", "switch copies", "segments looped", "prepared copies used", "prepared copies missed",
    "list cache hits", "list cache misses", "list cache invalidations", "flush checks skipped", "kick wakes", "kick timeouts", "settles that waited",
    "prepared copies refused", "late tail epoch hoists", "lean register lists", "list cache admits", "list cache bypasses", "list cache evictions",
    "rewind waits", "tails released without a kick", "kicks with no waiter", "idle polls woken by a submission", "flush checks called", "packet tables built",
    "segment slots created", "segment slots held past their segment", "lean draws", "draws on today's path", "indirect draws on today's path",
    "shape records built", "shape records restamped", "shape records evicted", "shapes refused", "finisher ring room waits",
    "finisher store switches", "lean draws that ran the precheck", "tails with their lists read directly",
};

static_assert(ItemNames.back() != nullptr && FieldInfos.back().name != nullptr && AllowedNames.back() != nullptr && CounterNames.back() != nullptr, "every lean front item, field, by-design case and counter needs a name");

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
Tally checkedHits{};
std::array<Tally, FieldCount> differences{};
std::array<Tally, AllowedKinds> allowed{};
std::array<Tally, CounterCount> counters{};
std::atomic<std::uint32_t> deepest{0};
std::atomic<std::uint64_t> printed{0};
std::mutex reportMutex;
std::int64_t reportedAt = 0;

std::string listed(std::string_view lead, std::size_t count, const auto& value, const auto& name) {
    std::string text;
    for (std::size_t index = 0; index < count; ++index) {
        if (value(index) == 0) continue;
        text += text.empty() ? std::string(lead) : std::string(", ");
        text += name(index);
        text += ' ';
        text += std::to_string(value(index));
    }
    return text;
}

}

std::uint64_t VerifyEvery() {
    static const std::uint64_t every = [] {
        const char* text = std::getenv("APS5_VERIFY_LEAN_FRONT");
        return text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
    }();
    return every;
}

FrontMode Take(Latch& latch, bool eligible) {
    if (!eligible) {
        latch.mode = FrontMode::Today;
        latch.verifyEvery = 0;
        latch.checkSwitches = false;
        latch.directDraws = false;
        latch.directBuffers = false;
        latch.directInputs = false;
        latch.directVerifyEvery = 0;
        return FrontMode::Today;
    }
    const auto generation = Graphics::RuntimeSwitches::Generation();
    if (generation != latch.generation) {
        latch.switches = Graphics::RuntimeSwitches::Snapshot();
        latch.generation = generation;
        AddOnWorker(Counter::SwitchCopies);
    }
    const auto mode = latch.switches.Set(Graphics::RuntimeSwitch::LeanFront) ? FrontMode::Lean : FrontMode::Today;
    if (latch.taken != FrontMode::Unlatched && latch.taken != mode) AddOnWorker(Counter::ModeChanges);
    latch.taken = mode;
    latch.mode = mode;
    latch.verifyEvery = mode == FrontMode::Lean ? VerifyEvery() : 0;
    latch.checkSwitches = Due(latch, Item::SwitchRead);
    const bool direct = mode == FrontMode::Lean && latch.switches.Set(Graphics::RuntimeSwitch::DirectDraws);
    latch.directDraws = direct;
    latch.directBuffers = direct && latch.switches.Set(Graphics::RuntimeSwitch::DirectBuffers);
    latch.directInputs = direct && latch.switches.Set(Graphics::RuntimeSwitch::DirectInputs);
    latch.directVerifyEvery = direct ? DirectDraws::VerifyEvery() : 0;
    AddOnWorker(mode == FrontMode::Lean ? Counter::LeanSegments : Counter::TodaySegments);
    if ((++latch.latches & 15u) == 0) {
        Report(false);
        DrawRecords::Report(false);
        DirectDraws::Report(false);
    }
    return mode;
}

bool CheckedSwitch(const Latch& latch, Graphics::RuntimeSwitch id) {
    const bool live = Graphics::RuntimeSwitches::Set(id);
    NoteChecked(Item::SwitchRead);
    if (live == latch.switches.Set(id)) return live;
    const auto deadline = std::chrono::steady_clock::now() + SwitchMoveWait;
    while (Graphics::RuntimeSwitches::Generation() == latch.generation && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    if (Graphics::RuntimeSwitches::Generation() != latch.generation) {
        NoteAllowed(Allowed::SwitchesMoved);
    } else {
        NoteDifference(Field::SwitchValues, std::string(Graphics::RuntimeSwitches::Name(id)) + " reads " + (live ? "set" : "unset") + ", the segment's switch copy has it " + (live ? "unset" : "set") + " at the same switch generation " + std::to_string(latch.generation));
    }
    return live;
}

SegmentNesting::SegmentNesting(Latch* owner) : latch(owner) {
    if (latch == nullptr) return;
    ++latch->depth;
    if (latch->mode == FrontMode::Lean && latch->depth > deepest.load(std::memory_order_relaxed)) deepest.store(latch->depth, std::memory_order_relaxed);
}

void NoteChecked(Item item, bool cacheHit) {
    checked[static_cast<std::size_t>(item)].value.fetch_add(1, std::memory_order_relaxed);
    if (cacheHit) checkedHits.value.fetch_add(1, std::memory_order_relaxed);
}

void NoteDifference(Field field, std::string_view detail) {
    const auto& info = FieldInfos[static_cast<std::size_t>(field)];
    differences[static_cast<std::size_t>(field)].value.fetch_add(1, std::memory_order_relaxed);
    if (printed.fetch_add(1, std::memory_order_relaxed) < PrintedPerWindow) std::fprintf(stderr, "[lean-front-verify] %s %s differs: %.*s\n", ItemName(info.item), info.name, static_cast<int>(detail.size()), detail.data());
}

void NoteAllowed(Allowed kind) {
    allowed[static_cast<std::size_t>(kind)].value.fetch_add(1, std::memory_order_relaxed);
}

void Add(Counter counter, std::uint64_t count) {
    counters[static_cast<std::size_t>(counter)].value.fetch_add(count, std::memory_order_relaxed);
}

void AddOnWorker(Counter counter, std::uint64_t count) {
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
    const auto nesting = deepest.exchange(0, std::memory_order_relaxed);
    std::array<std::uint64_t, ItemCount> checks{};
    std::uint64_t checksTotal = 0;
    for (std::size_t index = 0; index < ItemCount; ++index) checksTotal += checks[index] = checked[index].Window();
    const auto hits = checkedHits.Window();
    std::array<std::uint64_t, FieldCount> differing{};
    std::uint64_t differingTotal = 0;
    for (std::size_t index = 0; index < FieldCount; ++index) differingTotal += differing[index] = differences[index].Window();
    std::array<std::uint64_t, AllowedKinds> byDesign{};
    std::uint64_t byDesignTotal = 0;
    for (std::size_t index = 0; index < AllowedKinds; ++index) byDesignTotal += byDesign[index] = allowed[index].Window();
    printed.store(0, std::memory_order_relaxed);
    const auto lean = counts[static_cast<std::size_t>(Counter::LeanSegments)];
    if (lean != 0 || counts[static_cast<std::size_t>(Counter::ModeChanges)] != 0) {
        const auto parts = listed("; ", CounterCount - CoreCounters, [&](std::size_t index) { return counts[CoreCounters + index]; }, [&](std::size_t index) { return std::string(CounterNames[CoreCounters + index]); });
        AgcDriver::ProfilePrint_nid_no_patch("[lean-front] %.1f s: lean segments %llu, today's segments %llu, mode changes %llu, switch copies %llu, deepest lean nesting %u%s\n", seconds,
            static_cast<unsigned long long>(lean), static_cast<unsigned long long>(counts[static_cast<std::size_t>(Counter::TodaySegments)]),
            static_cast<unsigned long long>(counts[static_cast<std::size_t>(Counter::ModeChanges)]), static_cast<unsigned long long>(counts[static_cast<std::size_t>(Counter::SwitchCopies)]),
            static_cast<unsigned>(nesting), parts.c_str());
    }
    if (VerifyEvery() == 0 || (lean == 0 && checksTotal == 0 && differingTotal == 0 && byDesignTotal == 0)) return;
    const auto fields = listed(": ", FieldCount, [&](std::size_t index) { return differing[index]; }, [&](std::size_t index) { return std::string(ItemName(FieldInfos[index].item)) + " " + FieldInfos[index].name; });
    const auto cases = listed(": ", AllowedKinds, [&](std::size_t index) { return byDesign[index]; }, [&](std::size_t index) { return std::string(AllowedNames[index]); });
    const auto check = [&](Item item) { return static_cast<unsigned long long>(checks[static_cast<std::size_t>(item)]); };
    AgcDriver::ProfilePrint_nid_no_patch("[lean-front-verify] %.1f s: every %llu; checked: segments %llu, register lists %llu (cache hits %llu), packets %llu, draws %llu, switch reads %llu; differences %llu%s; by design %llu%s\n", seconds,
        static_cast<unsigned long long>(VerifyEvery()), check(Item::Segment), check(Item::RegisterList), static_cast<unsigned long long>(hits), check(Item::Packet), check(Item::Draw), check(Item::SwitchRead),
        static_cast<unsigned long long>(differingTotal), fields.c_str(), static_cast<unsigned long long>(byDesignTotal), cases.c_str());
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

FrontMode Driver::latchFront(std::uint32_t queue, bool suspend) {
    if (queue != 0) return FrontMode::Today;
    return LeanFront::Take(frontLatch, !suspend && DrawPipeline::Depth() != 0);
}

}
