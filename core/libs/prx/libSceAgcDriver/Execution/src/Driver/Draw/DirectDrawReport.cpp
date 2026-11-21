#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DirectDrawReport.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace AgcDriver::DriverDetail::DirectDraws {

namespace {

using DrawRecords::Allowed;
using DrawRecords::Field;
using DrawRecords::Item;

constexpr std::size_t CounterCount = static_cast<std::size_t>(Counter::Count);
constexpr std::size_t StepCount = static_cast<std::size_t>(Step::Count);
constexpr std::size_t FieldCount = static_cast<std::size_t>(Field::Count);
constexpr std::size_t AllowedKinds = static_cast<std::size_t>(Allowed::Count);
constexpr std::uint64_t PrintedPerWindow = 8;
constexpr std::uint64_t BuildLinesPerReason = 16;
constexpr std::int64_t WindowMs = 10000;

constexpr std::array<const char*, CounterCount> CounterNames{
    "draws", "direct",
    "shape", "heap", "invocation", "sample retry", "walk", "torn reads", "tables", "poisoned", "no patch", "addressed", "side", "push constants", "unsettled", "views not current", "memory changed", "labels", "parameter overflow", "class new", "class refused", "verify",
    "classes built", "not queued", "writes differ", "verify differs", "build differs", "classes evicted", "stages built", "stages evicted", "invocations rebuilt", "walks retried", "verify checked", "verify differing",
    "commit hits", "first commits", "commit misses", "commits demoted",
    "held by the record", "held by the pass", "first use", "new pass", "collect epoch", "proof serial", "pending stores", "depth surfaces", "target changed",
    "buffers prepared", "no plan", "classification", "not accessible", "over the upload limit", "size budget", "not settled", "blob bytes",
    "buffers used", "plan differs", "moved-range hazard", "unmoved-range hazard", "depth guards", "dropped", "buffers verified", "buffers differing", "buffer memory changed", "buffers differing unwatched",
    "inputs prepared", "no published recipe", "outside the shape", "not imported in place", "inputs not accessible", "inputs threw", "indices not settled",
    "inputs used", "inputs rechecked", "import changed", "commit shape", "refused in place", "inputs dropped", "inputs verified", "inputs differing",
    "settle ranges before merging", "settle ranges after merging", "merged settles passed", "merged settles refused", "merged refusals settled per range",
    "verified with a merged settle", "differing with a merged settle", "memory changed with a merged settle", "unwatched with a merged settle",
    "merged settles rechecked per range", "merged settles refused per range",
    "verified with exact capacities", "differing with exact capacities", "memory changed with exact capacities", "unwatched with exact capacities",
    "inputs verified with exact capacities", "inputs differing with exact capacities",
    "blobs appended",
    "memo hits", "memo stored", "no pattern", "gather failed", "no entry", "other inputs", "other values", "views", "order", "unsettled", "side", "untraced", "patterns full", "memo cleared", "memo verify checked", "memo verify differing",
};

constexpr std::array<const char*, StepCount> StepNames{"request", "invocation", "walk", "key", "side", "class", "checks", "buffers", "inputs", "hand-off"};

static_assert(CounterNames.back() != nullptr && StepNames.back() != nullptr, "every direct draw counter and step needs a name");

struct Block {
    std::array<std::atomic<std::uint64_t>, CounterCount> counters{};
    std::array<std::atomic<std::uint64_t>, StepCount> stepNanoseconds{};
    std::array<std::atomic<std::uint64_t>, StepCount> stepDraws{};
};

struct Registry {
    std::mutex mutex;
    std::vector<Block*> live;
    Block retired;
};

Registry& registry() {
    static auto* kept = new Registry;
    return *kept;
}

void bump(std::atomic<std::uint64_t>& value, std::uint64_t count) {
    value.store(value.load(std::memory_order_relaxed) + count, std::memory_order_relaxed);
}

void fold(Block& into, const Block& from) {
    for (std::size_t index = 0; index < CounterCount; ++index) bump(into.counters[index], from.counters[index].load(std::memory_order_relaxed));
    for (std::size_t index = 0; index < StepCount; ++index) {
        bump(into.stepNanoseconds[index], from.stepNanoseconds[index].load(std::memory_order_relaxed));
        bump(into.stepDraws[index], from.stepDraws[index].load(std::memory_order_relaxed));
    }
}

struct BlockHolder {
    Block* block = nullptr;

    ~BlockHolder() {
        if (block == nullptr) return;
        auto& kept = registry();
        std::lock_guard lock(kept.mutex);
        fold(kept.retired, *block);
        std::erase(kept.live, block);
        delete block;
    }
};

struct BlockStorage {};

Block& own() {
    auto& holder = HostThreadLocal<BlockHolder, BlockStorage>();
    if (holder.block != nullptr) return *holder.block;
    auto block = std::make_unique<Block>();
    auto& kept = registry();
    std::lock_guard lock(kept.mutex);
    kept.live.push_back(block.get());
    holder.block = block.release();
    return *holder.block;
}

template <typename Read>
std::uint64_t summed(const Read& read) {
    auto& kept = registry();
    std::lock_guard lock(kept.mutex);
    auto total = read(kept.retired);
    for (const auto* block : kept.live) total += read(*block);
    return total;
}

struct Window {
    std::array<std::uint64_t, CounterCount> counters{};
    std::array<std::uint64_t, StepCount> stepNanoseconds{};
    std::array<std::uint64_t, StepCount> stepDraws{};
};

Window totals() {
    Window now;
    auto& kept = registry();
    std::lock_guard lock(kept.mutex);
    const auto add = [&](const Block& block) {
        for (std::size_t index = 0; index < CounterCount; ++index) now.counters[index] += block.counters[index].load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < StepCount; ++index) {
            now.stepNanoseconds[index] += block.stepNanoseconds[index].load(std::memory_order_relaxed);
            now.stepDraws[index] += block.stepDraws[index].load(std::memory_order_relaxed);
        }
    };
    add(kept.retired);
    for (const auto* block : kept.live) add(*block);
    return now;
}

std::array<std::atomic<std::uint64_t>, FieldCount> differences{};
std::array<std::atomic<std::uint64_t>, FieldCount> buildDifferences{};
std::array<std::atomic<std::uint64_t>, AllowedKinds> allowed{};
std::atomic<std::uint64_t> printed{0};
std::atomic<std::uint64_t> bufferPrinted{0};
std::atomic<std::uint64_t> classesKnown{0};
std::atomic<std::uint64_t> classBytes{0};
std::atomic<std::uint64_t> stagesKnown{0};
std::mutex reportMutex;
std::int64_t reportedAt = 0;
Window reported;

bool Printed() {
    static const bool stats = std::getenv("APS5_PIPELINE_STATS") != nullptr;
    return stats;
}

std::string reasons(const std::array<std::uint64_t, CounterCount>& counts, Counter first, Counter last) {
    std::string text;
    for (auto index = static_cast<std::size_t>(first); index <= static_cast<std::size_t>(last); ++index) {
        if (counts[index] == 0) continue;
        text += text.empty() ? " (" : ", ";
        text += CounterNames[index];
        text += ' ';
        text += std::to_string(counts[index]);
    }
    return text.empty() ? text : text + ")";
}

}

std::uint64_t VerifyEvery() {
    static const std::uint64_t every = [] {
        const char* text = std::getenv("APS5_VERIFY_DIRECT_DRAWS");
        return text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
    }();
    return every;
}

std::uint64_t BufferVerifyEvery() {
    static const std::uint64_t every = [] {
        const char* text = std::getenv("APS5_VERIFY_DIRECT_BUFFERS");
        return text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
    }();
    return every;
}

std::uint64_t InputVerifyEvery() {
    static const std::uint64_t every = [] {
        const char* text = std::getenv("APS5_VERIFY_DIRECT_INPUTS");
        return text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
    }();
    return every;
}

void Add(Counter counter, std::uint64_t count) {
    bump(own().counters[static_cast<std::size_t>(counter)], count);
}

void AddStep(Step step, std::uint64_t nanoseconds) {
    auto& block = own();
    bump(block.stepNanoseconds[static_cast<std::size_t>(step)], nanoseconds);
    bump(block.stepDraws[static_cast<std::size_t>(step)], 1);
}

void NoteClasses(std::size_t known, std::size_t bytes) {
    classesKnown.store(known, std::memory_order_relaxed);
    classBytes.store(bytes, std::memory_order_relaxed);
}

void NoteStages(std::size_t known) {
    stagesKnown.store(known, std::memory_order_relaxed);
}

void NoteDifference(Field field, std::string_view detail) {
    differences[static_cast<std::size_t>(field)].fetch_add(1, std::memory_order_relaxed);
    if (printed.fetch_add(1, std::memory_order_relaxed) < PrintedPerWindow) std::fprintf(stderr, "[direct-draws-verify] %s %s differs: %.*s\n", DrawRecords::ItemName(Item::Answer), DrawRecords::FieldName(field), static_cast<int>(detail.size()), detail.data());
}

void NoteBuildDifference(Field field, std::string_view detail) {
    if (buildDifferences[static_cast<std::size_t>(field)].fetch_add(1, std::memory_order_relaxed) < BuildLinesPerReason) std::fprintf(stderr, "[direct-draws-build] %s %s differs: %.*s\n", DrawRecords::ItemName(Item::Answer), DrawRecords::FieldName(field), static_cast<int>(detail.size()), detail.data());
}

void NoteBufferDifference(std::string_view what, std::string_view detail) {
    if (bufferPrinted.fetch_add(1, std::memory_order_relaxed) < PrintedPerWindow) std::fprintf(stderr, "[direct-buffers-verify] %.*s differs: %.*s\n", static_cast<int>(what.size()), what.data(), static_cast<int>(detail.size()), detail.data());
}

void NoteAllowed(Allowed kind) {
    allowed[static_cast<std::size_t>(kind)].fetch_add(1, std::memory_order_relaxed);
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
    const auto current = totals();
    Window since;
    for (std::size_t index = 0; index < CounterCount; ++index) since.counters[index] = current.counters[index] - reported.counters[index];
    for (std::size_t index = 0; index < StepCount; ++index) {
        since.stepNanoseconds[index] = current.stepNanoseconds[index] - reported.stepNanoseconds[index];
        since.stepDraws[index] = current.stepDraws[index] - reported.stepDraws[index];
    }
    reported = current;
    printed.store(0, std::memory_order_relaxed);
    bufferPrinted.store(0, std::memory_order_relaxed);
    const auto& counts = since.counters;
    const auto at = [&](Counter counter) { return counts[static_cast<std::size_t>(counter)]; };
    const auto draws = at(Counter::Draws);
    std::uint64_t fallbacks = 0;
    for (auto index = static_cast<std::size_t>(Counter::FallbackShape); index <= static_cast<std::size_t>(Counter::FallbackVerify); ++index) fallbacks += counts[index];
    std::uint64_t refused = 0;
    for (auto index = static_cast<std::size_t>(Counter::RefusedNotQueued); index <= static_cast<std::size_t>(Counter::RefusedDiffers); ++index) refused += counts[index];
    if (!Printed() || (draws == 0 && at(Counter::ClassesBuilt) == 0 && at(Counter::ClassesEvicted) == 0 && at(Counter::StagesBuilt) == 0 && at(Counter::StagesEvicted) == 0)) return;
    const auto perDraw = [&](Step step) {
        const auto index = static_cast<std::size_t>(step);
        return since.stepDraws[index] == 0 ? 0.0 : static_cast<double>(since.stepNanoseconds[index]) / static_cast<double>(since.stepDraws[index]) / 1000.0;
    };
    const auto direct = at(Counter::Direct);
    AgcDriver::ProfilePrint_nid_no_patch("[direct-draws] %llu draws: %llu direct (%.1f%%), %llu fallback%s; classes %llu known, %llu built, %llu refused%s, %llu evicted, %.1f KiB; stages %llu known, %llu built; invocations %llu rebuilt; walks %llu retried; verify %llu checked, %llu differing; us per direct draw: request %.2f, invocation %.2f, walk %.2f, key %.2f, side %.2f, class %.2f, checks %.2f, buffers %.2f, inputs %.2f, hand-off %.2f\n",
        static_cast<unsigned long long>(draws), static_cast<unsigned long long>(direct), draws != 0 ? 100.0 * static_cast<double>(direct) / static_cast<double>(draws) : 0.0,
        static_cast<unsigned long long>(fallbacks), reasons(counts, Counter::FallbackShape, Counter::FallbackVerify).c_str(),
        static_cast<unsigned long long>(classesKnown.load(std::memory_order_relaxed)), static_cast<unsigned long long>(at(Counter::ClassesBuilt)), static_cast<unsigned long long>(refused), reasons(counts, Counter::RefusedNotQueued, Counter::RefusedDiffers).c_str(),
        static_cast<unsigned long long>(at(Counter::ClassesEvicted)), static_cast<double>(classBytes.load(std::memory_order_relaxed)) / 1024.0,
        static_cast<unsigned long long>(stagesKnown.load(std::memory_order_relaxed)), static_cast<unsigned long long>(at(Counter::StagesBuilt)),
        static_cast<unsigned long long>(at(Counter::InvocationsRebuilt)), static_cast<unsigned long long>(at(Counter::WalksRetried)), static_cast<unsigned long long>(at(Counter::VerifyChecked)), static_cast<unsigned long long>(at(Counter::VerifyDiffering)),
        perDraw(Step::Request), perDraw(Step::Invocation), perDraw(Step::Walk), perDraw(Step::Key), perDraw(Step::Side), perDraw(Step::Class), perDraw(Step::Checks), perDraw(Step::Buffers), perDraw(Step::Inputs), perDraw(Step::HandOff));
    const auto commits = at(Counter::CommitHits) + at(Counter::CommitFirst) + at(Counter::CommitMisses);
    if (commits != 0) {
        std::uint64_t refreshed = 0;
        for (auto index = static_cast<std::size_t>(Counter::HoldFirst); index <= static_cast<std::size_t>(Counter::HoldTarget); ++index) refreshed += counts[index];
        AgcDriver::ProfilePrint_nid_no_patch("[direct-commits] %llu commits: %llu from the record (%.1f%%), %llu first commits, %llu misses, %llu demoted; targets: %llu held by the record, %llu held by the pass, %llu refreshed%s; stages evicted %llu\n",
            static_cast<unsigned long long>(commits), static_cast<unsigned long long>(at(Counter::CommitHits)), 100.0 * static_cast<double>(at(Counter::CommitHits)) / static_cast<double>(commits),
            static_cast<unsigned long long>(at(Counter::CommitFirst)), static_cast<unsigned long long>(at(Counter::CommitMisses)), static_cast<unsigned long long>(at(Counter::CommitDemoted)),
            static_cast<unsigned long long>(at(Counter::HoldOwn)), static_cast<unsigned long long>(at(Counter::HoldShared)), static_cast<unsigned long long>(refreshed), reasons(counts, Counter::HoldFirst, Counter::HoldTarget).c_str(),
            static_cast<unsigned long long>(at(Counter::StagesEvicted)));
    }
    const auto sum = [&](Counter first, Counter last) {
        std::uint64_t total = 0;
        for (auto index = static_cast<std::size_t>(first); index <= static_cast<std::size_t>(last); ++index) total += counts[index];
        return total;
    };
    if (sum(Counter::BuffersPrepared, Counter::BuffersUnwatched) != 0) {
        const auto prepared = at(Counter::BuffersPrepared);
        std::string shortcuts;
        const auto verified = [&](const char* name, Counter first) {
            const auto index = static_cast<std::size_t>(first);
            if (counts[index] == 0) return;
            shortcuts += "; verify of " + std::string(name) + " " + std::to_string(counts[index]) + " checked, " + std::to_string(counts[index + 1]) + " differing, " + std::to_string(counts[index + 2]) + " memory changed, " + std::to_string(counts[index + 3]) + " differing in unwatched memory";
        };
        verified("merged settles", Counter::VerifyMergedChecked);
        if (at(Counter::MergedSettlesRechecked) != 0) shortcuts += "; merged settles rechecked per range " + std::to_string(at(Counter::MergedSettlesRechecked)) + ", " + std::to_string(at(Counter::MergedSettlesRecheckRefused)) + " refused per range";
        verified("exact capacities", Counter::VerifyCapacityChecked);
        AgcDriver::ProfilePrint_nid_no_patch("[direct-commits] buffers: %llu hits prepared, %llu used, %llu not prepared%s, %llu not used%s; %.1f blob bytes per prepared hit; verify %llu checked, %llu differing, %llu memory changed, %llu differing in unwatched memory%s\n",
            static_cast<unsigned long long>(prepared), static_cast<unsigned long long>(at(Counter::BuffersUsed)),
            static_cast<unsigned long long>(sum(Counter::BuffersNoPlan, Counter::BuffersUnsettled)), reasons(counts, Counter::BuffersNoPlan, Counter::BuffersUnsettled).c_str(),
            static_cast<unsigned long long>(sum(Counter::BuffersPlanDiffers, Counter::BuffersDropped)), reasons(counts, Counter::BuffersPlanDiffers, Counter::BuffersDropped).c_str(),
            prepared != 0 ? static_cast<double>(at(Counter::BuffersBlobBytes)) / static_cast<double>(prepared) : 0.0,
            static_cast<unsigned long long>(at(Counter::BuffersVerifyChecked)), static_cast<unsigned long long>(at(Counter::BuffersVerifyDiffering)), static_cast<unsigned long long>(at(Counter::BuffersMemoryChanged)), static_cast<unsigned long long>(at(Counter::BuffersUnwatched)), shortcuts.c_str());
    }
    if (sum(Counter::SettleRangesBefore, Counter::MergedSettlesRecovered) != 0) {
        AgcDriver::ProfilePrint_nid_no_patch("[direct-commits] buffer shortcuts: settle ranges %llu before, %llu after merging; merged settles %llu passed, %llu refused (%llu of them settled per range)\n",
            static_cast<unsigned long long>(at(Counter::SettleRangesBefore)), static_cast<unsigned long long>(at(Counter::SettleRangesAfter)),
            static_cast<unsigned long long>(at(Counter::MergedSettlesPassed)), static_cast<unsigned long long>(at(Counter::MergedSettlesRefused)), static_cast<unsigned long long>(at(Counter::MergedSettlesRecovered)));
    }
    if (sum(Counter::InputsPrepared, Counter::InputsVerifyDiffering) != 0) {
        const auto capacities = at(Counter::InputsVerifyCapacityChecked) == 0 ? std::string() : "; verify of exact capacities " + std::to_string(at(Counter::InputsVerifyCapacityChecked)) + " checked, " + std::to_string(at(Counter::InputsVerifyCapacityDiffering)) + " differing";
        AgcDriver::ProfilePrint_nid_no_patch("[direct-commits] inputs: %llu hits prepared, %llu used (%llu after an import recheck), %llu not prepared%s, %llu not used%s; verify %llu checked, %llu differing%s\n",
            static_cast<unsigned long long>(at(Counter::InputsPrepared)), static_cast<unsigned long long>(at(Counter::InputsUsed)), static_cast<unsigned long long>(at(Counter::InputsRechecked)),
            static_cast<unsigned long long>(sum(Counter::InputsNoRecipe, Counter::InputsUnsettled)), reasons(counts, Counter::InputsNoRecipe, Counter::InputsUnsettled).c_str(),
            static_cast<unsigned long long>(sum(Counter::InputsImportChanged, Counter::InputsDropped)), reasons(counts, Counter::InputsImportChanged, Counter::InputsDropped).c_str(),
            static_cast<unsigned long long>(at(Counter::InputsVerifyChecked)), static_cast<unsigned long long>(at(Counter::InputsVerifyDiffering)), capacities.c_str());
    }
    if (at(Counter::BlobAppends) != 0) AgcDriver::ProfilePrint_nid_no_patch("[direct-draws] finisher reuse: buffers%s\n", reasons(counts, Counter::BlobAppends, Counter::BlobAppends).c_str());
    if (sum(Counter::StageMemoHits, Counter::StageMemoVerifyDiffering) != 0) {
        AgcDriver::ProfilePrint_nid_no_patch("[direct-draws] stage memo: %llu hits, %llu stored, %llu not adopted%s; not stored%s; %llu cleared; verify %llu checked, %llu differing\n",
            static_cast<unsigned long long>(at(Counter::StageMemoHits)), static_cast<unsigned long long>(at(Counter::StageMemoStored)), static_cast<unsigned long long>(sum(Counter::StageMemoNoPattern, Counter::StageMemoSide)), reasons(counts, Counter::StageMemoNoPattern, Counter::StageMemoSide).c_str(),
            reasons(counts, Counter::StageMemoUntraced, Counter::StageMemoPatternsFull).c_str(), static_cast<unsigned long long>(at(Counter::StageMemoCleared)), static_cast<unsigned long long>(at(Counter::StageMemoVerifyChecked)), static_cast<unsigned long long>(at(Counter::StageMemoVerifyDiffering)));
    }
}

const char* CounterName(Counter counter) {
    return CounterNames[static_cast<std::size_t>(counter)];
}

std::uint64_t Total(Counter counter) {
    const auto index = static_cast<std::size_t>(counter);
    return summed([&](const Block& block) { return block.counters[index].load(std::memory_order_relaxed); });
}

std::uint64_t Differences(Field field) {
    return differences[static_cast<std::size_t>(field)].load(std::memory_order_relaxed);
}

std::uint64_t BuildDifferences(Field field) {
    return buildDifferences[static_cast<std::size_t>(field)].load(std::memory_order_relaxed);
}

std::uint64_t AllowedCount(Allowed kind) {
    return allowed[static_cast<std::size_t>(kind)].load(std::memory_order_relaxed);
}

}
