#include "Pm4Analysis.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/LabelStores.hpp"
#include <algorithm>
#include <cstdarg>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace AgcDriver::Pm4Analysis {
namespace {

constexpr std::uint64_t SmallStoreBytes = 4096;
constexpr std::size_t ShownValues = 4;
constexpr std::size_t MarkerCharacters = 48;
constexpr std::size_t ListedWaits = 32;
constexpr std::size_t LastSubmissionsPerQueue = 2;
constexpr std::size_t ShownPackets = 48;
constexpr std::size_t ContextBefore = 8;
constexpr std::size_t ContextAfter = 4;
constexpr const char* CompareNames[8] = {"always", "<", "<=", "==", "!=", ">=", ">", "reserved"};

std::string format(const char* text, ...) {
    va_list arguments;
    va_start(arguments, text);
    va_list measured;
    va_copy(measured, arguments);
    const int length = std::vsnprintf(nullptr, 0, text, measured);
    va_end(measured);
    std::string result(length < 0 ? 0 : static_cast<std::size_t>(length), '\0');
    const int written = length < 0 ? length : std::vsnprintf(result.data(), result.size() + 1, text, arguments);
    va_end(arguments);
    if (length < 0 || written != length) throw std::runtime_error("PM4 analysis: cannot format a line");
    return result;
}

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("PM4 analysis: " + reason);
}

std::uint64_t address(std::uint32_t low, std::uint32_t high) {
    return low | (static_cast<std::uint64_t>(high) << 32u);
}

std::string hex(std::uint64_t value) {
    return format("0x%llx", static_cast<unsigned long long>(value));
}

std::string values(std::span<const std::uint32_t> words) {
    std::string text;
    for (std::size_t i = 0; i < words.size() && i < ShownValues; ++i) text += format(i == 0 ? "0x%x" : " 0x%x", words[i]);
    if (words.size() > ShownValues) text += format(" (+%zu)", words.size() - ShownValues);
    return text;
}

std::string marker(std::span<const std::uint32_t> payload) {
    std::string text;
    for (const auto byte : std::as_bytes(payload)) {
        const auto character = std::to_integer<unsigned char>(byte);
        if (character == 0 || text.size() == MarkerCharacters) break;
        text.push_back(character >= 0x20 && character < 0x7f ? static_cast<char>(character) : '?');
    }
    return "\"" + text + "\"";
}

std::string describeNop(std::span<const std::uint32_t> p) {
    const auto at = [&](std::size_t index) { return index < p.size() ? p[index] : 0u; };
    switch ((p[0] >> 2u) & 0x3fu) {
        case 0: return format("%zu dwords", p.size() - 1);
        case 0x06: return format("output %u index %d", at(1), static_cast<std::int32_t>(at(2)));
        case 0x0b: return marker(p.subspan(1));
        case 0x17: return format("output %u index %d mode %u argument %s", at(1), static_cast<std::int32_t>(at(2)), at(3), hex(address(at(4), at(5))).c_str());
        case 0x1a: return format("operation %u", at(1));
        case 0x09: case 0x0c: return {};
        default: return values(p.subspan(1));
    }
}

std::string describeDmaEnd(std::uint32_t select, std::uint32_t low, std::uint32_t high, bool destination) {
    if (select == Pm4::DmaSelectGds) return format("GDS offset 0x%x", low);
    if (destination && select == Pm4::DmaDestinationNowhere) return "nothing (prefetch)";
    return hex(address(low, high));
}

std::string describe(std::span<const std::uint32_t> p) {
    const auto opcode = (p[0] >> 8u) & 0xffu;
    const auto at = [&](std::size_t index) { return index < p.size() ? p[index] : 0u; };
    switch (opcode) {
        case 0x10: return describeNop(p);
        case 0x3c: case 0x93: {
            const auto wait = Pm4::DecodeWait(p);
            return format("[%s] (%zu bytes) %s 0x%llx mask 0x%llx poll %u", hex(wait.address).c_str(), wait.bytes, CompareNames[wait.function], static_cast<unsigned long long>(wait.reference), static_cast<unsigned long long>(wait.mask), wait.poll);
        }
        case 0x49: return format("event 0x%x data select %u interrupt %u address %s data %s", p[1] & 0x3fu, p[2] >> 29u, (p[2] >> 24u) & 7u, hex(address(p[3], p[4])).c_str(), hex(address(p[5], p[6])).c_str());
        case 0x37: return format("dst %u address %s%s: %s", (p[1] >> 8u) & 0xfu, hex(address(p[2], p[3])).c_str(), (p[1] & 0x10000u) != 0 ? " (one address)" : "", values(p.subspan(4)).c_str());
        case 0x40: {
            const auto source = Pm4::CopyDataSource(p);
            if (Pm4::CopyDataToRegister(p)) return format("source select %u source %s to register 0x%x", source, hex(address(p[2], p[3])).c_str(), p[4]);
            return format("source select %u source %s to %s (%zu bytes)", source, hex(address(p[2], p[3])).c_str(), hex(address(p[4], p[5])).c_str(), Pm4::CopyDataBytes(p));
        }
        case 0x50: {
            const auto from = Pm4::DmaSource(p) == Pm4::DmaSourceData ? format("fill 0x%08x", p[2]) : "copy from " + describeDmaEnd(Pm4::DmaSource(p), p[2], p[3], false);
            return format("%s to %s (%zu bytes)", from.c_str(), describeDmaEnd(Pm4::DmaDestination(p), p[4], p[5], true).c_str(), Pm4::DmaBytes(p));
        }
        case 0x46: {
            auto text = format("event 0x%x index %u", p[1] & 0x3fu, (p[1] >> 8u) & 7u);
            if (Pm4::SampleDump(p)) text += " samples to " + hex(Pm4::SampleDumpAddress(p));
            return text;
        }
        case 0x45: {
            const auto write = Pm4::DecodeConditionalWrite(p);
            return format("[%s] %s 0x%x mask 0x%x: store 0x%x to %s", hex(write.compared).c_str(), CompareNames[p[1] & 7u], p[4], p[5], write.value, hex(write.destination).c_str());
        }
        case 0x83: {
            const auto dump = Pm4::DecodeConstantRamDump(p);
            return format("offset 0x%x %u dwords to %s", dump.offset, dump.words, hex(dump.destination).c_str());
        }
        case 0x59: return (p[1] & Pm4::RewindValid) != 0 ? "valid" : "not valid";
        case 0x68: case 0x69: case 0x76: case 0x78: case 0x79: case 0x7a:
            if (Pm4::IsTagMarker(p)) return format("tag 0x%08x", p[2]);
            return format("0x%x = %s", p[1], values(p.subspan(2)).c_str());
        case 0x11: return format("index %u base %s", at(1), hex(address(at(2), at(3))).c_str());
        case 0x12: case 0x13: case 0x2a: case 0x2f: case 0x42: return format("%u", at(1));
        case 0x15: return format("groups %ux%ux%u initiator 0x%x", at(1), at(2), at(3), at(4));
        case 0x16: return p.size() == 4 ? format("arguments %s initiator 0x%x", hex(address(at(1), at(2))).c_str(), at(3)) : format("offset 0x%x initiator 0x%x", at(1), at(2));
        case 0x20: return format("control 0x%x address %s", at(1), hex(address(at(2), at(3))).c_str());
        case 0x22: return format("[%s] guards %zu dwords", hex(address(at(1), at(2))).c_str(), Pm4::ConditionalWords(p));
        case 0x24: case 0x25: return format("offset 0x%x locations 0x%x 0x%x initiator 0x%x", at(1), at(2), at(3), at(4));
        case 0x2c: case 0x38: return format("offset 0x%x control 0x%x count %u count address %s stride %u initiator 0x%x", at(1), at(4), at(5), hex(address(at(6), at(7))).c_str(), at(8), at(9));
        case 0x26: return format("address %s", hex(address(at(1), at(2))).c_str());
        case 0x27: return format("%u indices at %s max %u initiator 0x%x", at(4), hex(address(at(2), at(3))).c_str(), at(1), at(5));
        case 0x2d: return format("%u vertices initiator 0x%x", at(1), at(2));
        case 0x35: return format("%u indices from %u max %u initiator 0x%x", at(3), at(2), at(1), at(4));
        case 0x3f: return format("predicated, address %s %u dwords", hex(address(at(1), at(2) & 0xffffu)).c_str(), at(3) & 0xfffffu);
        case 0x58: {
            const auto base = ((static_cast<std::uint64_t>(at(5)) << 32u) | at(4)) << 8u;
            const auto bytes = ((static_cast<std::uint64_t>(at(3)) << 32u) | at(2)) << 8u;
            return format("control 0x%x base %s bytes %s", at(1), hex(base).c_str(), hex(bytes).c_str());
        }
        case 0x63: case 0x64: case 0x9f:
            return at(3) == 0x80000000u ? format("%u pairs at %s", at(4), hex(address(at(1), at(2))).c_str()) : format("0x%x x%u from %s", at(3), at(4), hex(address(at(1), at(2))).c_str());
        case 0x81: return format("offset 0x%x %zu dwords", at(1), p.size() > 2 ? p.size() - 2 : 0);
        default: return values(p.subspan(1));
    }
}

bool conditionalStart(std::uint32_t header) {
    const auto opcode = (header >> 8u) & 0xffu;
    return opcode == 0x22 || (opcode == 0x3f && Pm4::Predicated(header));
}

struct Located {
    std::size_t submission = 0;
    std::size_t offset = 0;
    std::uint32_t header = 0;
};

struct Store {
    Located at;
    MemoryStore store;
};

struct Waiter {
    Located at;
    MemoryWait wait;
};

using Key = std::tuple<std::uint64_t, std::uint64_t, std::size_t>;

struct QueueStores {
    std::vector<std::size_t> byExecution;
    std::vector<std::size_t> byEntry;
};

struct RangeStores {
    std::map<std::uint32_t, QueueStores> queues;
    std::size_t count = 0;
};

struct Accepting {
    std::map<std::uint32_t, std::vector<std::size_t>> prefix;
    std::size_t total = 0;
};

using CompareKey = std::tuple<std::uint64_t, std::size_t, std::uint32_t, std::uint64_t, std::uint64_t>;
using Listed = std::vector<std::pair<std::size_t, WriterMatch::Effect>>;

class Matcher {
public:
    Matcher(const std::vector<CapturedSubmission>& submissions, const std::vector<Store>& stores) : submissions(submissions), stores(stores) {
        for (std::size_t index = 0; index < stores.size(); ++index) (stores[index].store.store.bytes <= SmallStoreBytes ? small : large).push_back(index);
        std::sort(small.begin(), small.end(), [&](std::size_t left, std::size_t right) { return stores[left].store.store.address < stores[right].store.store.address; });
    }

    void Match(WaitReport& report, const Waiter& waiter) {
        const auto& fields = report.wait.fields;
        const auto& range = rangeFor(fields.address, fields.bytes);
        const auto& accepting = acceptingFor(range, report.wait);
        report.writerCount = range.count;
        const auto queue = submissions[waiter.at.submission].queue;
        const auto found = range.queues.find(queue);
        const bool stored = found != range.queues.end();
        const auto& own = stored ? found->second.byExecution : empty;
        const auto before = lowerExecution(own, execution(waiter.at));
        const auto others = range.count - own.size();
        const auto ownAccepting = stored ? accepting.prefix.at(queue)[before] : 0;
        const auto otherAccepting = accepting.total - (stored ? accepting.prefix.at(queue).back() : 0);
        Listed listed;

        std::optional<std::uint64_t> lastValue;
        bool lastAccepts = false;
        if (before != 0) {
            lastValue = settledValue(own[before - 1], fields);
            lastAccepts = lastValue.has_value() && Pm4::WaitComparesValue(report.wait.packet, *lastValue);
            if (lastAccepts && others == 0) {
                setReleased(report, WaitReport::Route::CurrentValue,
                            format("released: %s, the last store before it on its queue, leaves 0x%llx at %s, which the compare accepts, and no other queue stores to the awaited bytes", where(own[before - 1]).c_str(), static_cast<unsigned long long>(*lastValue), hex(fields.address).c_str()));
                listed.emplace_back(own[before - 1], WriterMatch::Effect::CurrentValue);
                report.writers = writers(listed, waiter);
                return;
            }
        }

        std::optional<std::size_t> kept;
        std::size_t newer = 0;
        for (auto position = before; position != 0;) {
            const auto index = own[--position];
            if (stores[index].at.submission != waiter.at.submission) break;
            if (!keptAccepting(index, report.wait)) continue;
            kept = index;
            newer = before - 1 - position + others;
            break;
        }
        if (kept.has_value() && newer < DriverDetail::LabelStoreHistory) {
            const auto value = storedBytes(stores[*kept].store.store, fields.address, fields.bytes);
            setReleased(report, WaitReport::Route::LabelHistory,
                        format("released: %s stores 0x%llx after the waiting submission entered the driver, and at most %zu other stores to the awaited bytes can run between it and the wait, so the driver's label history, which keeps the last %zu stores per dword over all queues, still holds it when the wait runs",
                               where(*kept).c_str(), static_cast<unsigned long long>(*value), newer, DriverDetail::LabelStoreHistory));
            listed.emplace_back(*kept, WriterMatch::Effect::LabelHistory);
            report.writers = writers(listed, waiter);
            return;
        }

        if (kept.has_value()) listed.emplace_back(*kept, WriterMatch::Effect::Evicted);
        if (before != 0 && own[before - 1] != kept) listed.emplace_back(own[before - 1], lastAccepts ? WriterMatch::Effect::Overwritten : lastValue.has_value() ? WriterMatch::Effect::Rejected : WriterMatch::Effect::Undecided);
        if (stored) {
            const auto& prefix = accepting.prefix.at(queue);
            const auto end = before == 0 ? 0 : before - 1;
            if (const auto earlier = lastAcceptingBefore(prefix, end); earlier.has_value() && own[*earlier] != kept) {
                listed.emplace_back(own[*earlier], settledValue(own[*earlier], fields).has_value() ? WriterMatch::Effect::Overwritten : WriterMatch::Effect::Undecided);
            }
            if (const auto later = firstAcceptingFrom(prefix, before); later.has_value()) listed.emplace_back(own[*later], WriterMatch::Effect::After);
        }
        const auto queues = noteOthers(range, queue, submissions[waiter.at.submission].ordinal, listed);

        const auto awaited = format("%s+%zu", hex(fields.address).c_str(), fields.bytes);
        if (ownAccepting + otherAccepting == 0) {
            report.verdict = WaitReport::Verdict::Unreleased;
            report.reason = unreleasedReason(range, awaited, own.size() - before);
            if (!submissions[waiter.at.submission].heldKnown) {
                report.verdict = WaitReport::Verdict::Unknown;
                report.reason = "unknown: the index has no held column, so its value may have passed the compare at submit; otherwise " + report.reason;
            }
        } else if (others != 0) {
            report.verdict = WaitReport::Verdict::Unknown;
            report.crossQueue = true;
            report.reason = format("unknown: depends on cross-queue timing (stores on %s): %zu of the %zu stores to %s on other queues write a value the compare may accept, and the capture does not fix when a store of another queue runs relative to the wait",
                                   queues.c_str(), otherAccepting, others, awaited.c_str());
            if (lastAccepts) report.reason += format("; %s, the last store before it on its queue, leaves a value the compare accepts, but a store of another queue can replace it", where(own[before - 1]).c_str());
            if (kept.has_value()) report.reason += format("; %s, a label earlier in the waiting submission that the compare accepts, can be pushed out of the driver's label history by %zu stores to the awaited bytes, as many as it keeps per dword", where(*kept).c_str(), newer);
        } else {
            report.verdict = WaitReport::Verdict::Unknown;
            report.reason = "unknown: " + sameQueueReason(own[before - 1], fields, kept, newer);
        }
        report.writers = writers(listed, waiter);
    }

private:
    static constexpr std::size_t ListedPerQueue = 4;

    const std::vector<CapturedSubmission>& submissions;
    const std::vector<Store>& stores;
    const std::vector<std::size_t> empty;
    std::vector<std::size_t> small;
    std::vector<std::size_t> large;
    std::map<std::pair<std::uint64_t, std::size_t>, RangeStores> ranges;
    std::map<CompareKey, Accepting> accepting;

    Key execution(const Located& at) const {
        const auto& submission = submissions[at.submission];
        return {submission.serial, submission.ordinal, at.offset};
    }

    Key entry(const Located& at) const {
        return {submissions[at.submission].ordinal, at.offset, 0};
    }

    std::size_t lowerExecution(const std::vector<std::size_t>& list, const Key& key) const {
        return static_cast<std::size_t>(std::lower_bound(list.begin(), list.end(), key, [&](std::size_t index, const Key& value) { return execution(stores[index].at) < value; }) - list.begin());
    }

    static bool covers(const Store& store, std::uint64_t at, std::size_t bytes) {
        return at >= store.store.store.address && at - store.store.store.address <= store.store.store.bytes && bytes <= store.store.store.bytes - (at - store.store.store.address);
    }

    static std::optional<std::uint64_t> storedBytes(const Pm4::PacketStore& store, std::uint64_t at, std::size_t bytes) {
        if (store.value == Pm4::PacketStore::Value::RunTime || at < store.address || at - store.address > store.bytes || bytes > store.bytes - (at - store.address)) return std::nullopt;
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < bytes; ++i) {
            const auto index = static_cast<std::size_t>(at - store.address) + i;
            const auto byte = store.value == Pm4::PacketStore::Value::Bytes ? store.data.at(index) : store.data.at(index % store.data.size());
            value |= std::to_integer<std::uint64_t>(byte) << (8u * i);
        }
        return value;
    }

    std::optional<std::uint64_t> settledValue(std::size_t index, const Pm4::WaitFields& fields) const {
        if (stores[index].store.conditional) return std::nullopt;
        return storedBytes(stores[index].store.store, fields.address, fields.bytes);
    }

    bool mayAccept(std::size_t index, const MemoryWait& wait) const {
        const auto value = storedBytes(stores[index].store.store, wait.fields.address, wait.fields.bytes);
        return !value.has_value() || Pm4::WaitComparesValue(wait.packet, *value);
    }

    bool keptAccepting(std::size_t index, const MemoryWait& wait) const {
        const auto& store = stores[index];
        const auto& fields = wait.fields;
        if (!store.store.label || store.store.conditional || !DriverDetail::KeepsLabelStore(store.store.store.address, store.store.store.bytes)) return false;
        if (!covers(store, fields.address, fields.bytes) || (fields.address - store.store.store.address) % 4 != 0) return false;
        const auto value = storedBytes(store.store.store, fields.address, fields.bytes);
        return value.has_value() && Pm4::WaitComparesValue(wait.packet, *value);
    }

    static std::optional<std::size_t> lastAcceptingBefore(const std::vector<std::size_t>& prefix, std::size_t end) {
        if (prefix[end] == 0) return std::nullopt;
        return static_cast<std::size_t>(std::lower_bound(prefix.begin(), prefix.begin() + static_cast<std::ptrdiff_t>(end) + 1, prefix[end]) - prefix.begin()) - 1;
    }

    static std::optional<std::size_t> firstAcceptingFrom(const std::vector<std::size_t>& prefix, std::size_t from) {
        if (prefix.back() == prefix[from]) return std::nullopt;
        return static_cast<std::size_t>(std::upper_bound(prefix.begin() + static_cast<std::ptrdiff_t>(from), prefix.end(), prefix[from]) - prefix.begin()) - 1;
    }

    std::string where(std::size_t index) const {
        const auto& at = stores[index].at;
        return format("#%llu +0x%04zx %s", static_cast<unsigned long long>(submissions[at.submission].ordinal), at.offset, Pm4::Name(at.header).c_str());
    }

    static void setReleased(WaitReport& report, WaitReport::Route route, std::string reason) {
        report.verdict = WaitReport::Verdict::Released;
        report.route = route;
        report.reason = std::move(reason);
    }

    std::string noteOthers(const RangeStores& range, std::uint32_t queue, std::uint64_t entered, Listed& listed) const {
        std::string text;
        for (const auto& [storeQueue, lists] : range.queues) {
            if (storeQueue == queue) continue;
            const auto& list = lists.byEntry;
            const auto ordinal = [&](std::size_t position) { return submissions[stores[list[position]].at.submission].ordinal; };
            auto high = static_cast<std::size_t>(std::lower_bound(list.begin(), list.end(), entered, [&](std::size_t index, std::uint64_t value) { return submissions[stores[index].at.submission].ordinal < value; }) - list.begin());
            auto low = high;
            std::vector<std::size_t> nearest;
            while (nearest.size() < ListedPerQueue && (low != 0 || high != list.size())) {
                const bool takeLow = high == list.size() || (low != 0 && entered - ordinal(low - 1) <= ordinal(high) - entered);
                nearest.push_back(takeLow ? list[--low] : list[high++]);
            }
            std::sort(nearest.begin(), nearest.end(), [&](std::size_t left, std::size_t right) { return entry(stores[left].at) < entry(stores[right].at); });
            std::vector<std::uint64_t> ordinals;
            for (const auto index : nearest) {
                listed.emplace_back(index, WriterMatch::Effect::OtherQueue);
                const auto at = submissions[stores[index].at.submission].ordinal;
                if (ordinals.empty() || ordinals.back() != at) ordinals.push_back(at);
            }
            text += format("%squeue 0x%x at", text.empty() ? "" : "; ", storeQueue);
            for (std::size_t i = 0; i < ordinals.size(); ++i) text += format("%s #%llu", i == 0 ? "" : ",", static_cast<unsigned long long>(ordinals[i]));
            if (list.size() > nearest.size()) text += format(" (+%zu more)", list.size() - nearest.size());
        }
        return text;
    }

    std::string sameQueueReason(std::size_t last, const Pm4::WaitFields& fields, const std::optional<std::size_t>& kept, std::size_t newer) const {
        const auto& store = stores[last];
        const auto lastStore = where(last) + ", the last store before it on its queue,";
        if (store.store.conditional) return lastStore + " is conditional (COND_WRITE, inside a COND_EXEC range or a predicated packet), so the value the wait reads depends on run time";
        if (store.store.store.value == Pm4::PacketStore::Value::RunTime) return lastStore + " stores a value known only when the GPU runs it";
        const auto value = storedBytes(store.store.store, fields.address, fields.bytes);
        if (!value.has_value()) return lastStore + " writes only part of the awaited bytes";
        auto text = format("%s leaves 0x%llx, which the compare rejects, and the capture does not prove that the driver's label history still holds an earlier store the compare accepts when the wait runs", lastStore.c_str(), static_cast<unsigned long long>(*value));
        if (kept.has_value()) return text + format(": %s, a label earlier in the waiting submission that the compare accepts, can be pushed out of it by %zu newer stores to the awaited bytes", where(*kept).c_str(), newer);
        return text + ": no store the compare accepts is a RELEASE_MEM or WRITE_DATA label the history keeps earlier in the waiting submission, and whether an earlier one ran after the submission entered the driver depends on when it ran";
    }

    static std::string unreleasedReason(const RangeStores& range, const std::string& awaited, std::size_t later) {
        const std::string prefix = "unreleased by the GPU; a CPU store could release it (CPU and shader stores are not captured): its value at submit fails the compare and ";
        if (range.count == 0) return prefix + "no packet in the capture writes " + awaited;
        if (range.count == later) return prefix + format("the %zu stores to %s in the capture are later on its queue and run after it", range.count, awaited.c_str());
        auto text = prefix + format("none of the %zu stores to %s in the capture that can run before it writes a value the compare accepts", range.count - later, awaited.c_str());
        if (later != 0) text += format(" (%zu more are later on its queue and run after it)", later);
        return text;
    }

    std::vector<WriterMatch> writers(Listed& listed, const Waiter& waiter) const {
        std::stable_sort(listed.begin(), listed.end(), [](const auto& left, const auto& right) { return left.second < right.second; });
        std::vector<WriterMatch> result;
        std::set<std::size_t> seen;
        const auto& waiting = submissions[waiter.at.submission];
        for (const auto& [index, effect] : listed) {
            if (result.size() == Analysis::ListedWriters) break;
            if (!seen.insert(index).second) continue;
            const auto& at = stores[index].at;
            const auto& submission = submissions[at.submission];
            WriterMatch match;
            match.at = {submission.ordinal, submission.queue, submission.serial, at.offset, Pm4::Name(at.header)};
            if (submission.queue == waiting.queue) match.order = execution(at) < execution(waiter.at) ? WriterMatch::Order::EarlierOnQueue : WriterMatch::Order::LaterOnQueue;
            match.effect = effect;
            match.stored = storedBytes(stores[index].store.store, waiter.wait.fields.address, waiter.wait.fields.bytes);
            match.accepted = mayAccept(index, waiter.wait);
            result.push_back(std::move(match));
        }
        return result;
    }

    const Accepting& acceptingFor(const RangeStores& range, const MemoryWait& wait) {
        const auto& fields = wait.fields;
        const auto [found, inserted] = accepting.try_emplace({fields.address, fields.bytes, fields.function, fields.reference, fields.mask});
        if (!inserted) return found->second;
        auto& result = found->second;
        for (const auto& [queue, lists] : range.queues) {
            auto& prefix = result.prefix[queue];
            prefix.push_back(0);
            for (const auto index : lists.byExecution) prefix.push_back(prefix.back() + (mayAccept(index, wait) ? 1 : 0));
            result.total += prefix.back();
        }
        return result;
    }

    const RangeStores& rangeFor(std::uint64_t at, std::size_t bytes) {
        const auto [found, inserted] = ranges.try_emplace({at, bytes});
        if (!inserted) return found->second;
        auto& range = found->second;
        const auto overlaps = [&](std::size_t index) {
            const auto& store = stores[index].store.store;
            return store.address < at + bytes && at < store.address + store.bytes;
        };
        std::vector<std::size_t> matches;
        const auto first = std::lower_bound(small.begin(), small.end(), at >= SmallStoreBytes ? at - SmallStoreBytes + 1 : 0, [&](std::size_t index, std::uint64_t value) { return stores[index].store.store.address < value; });
        for (auto it = first; it != small.end() && stores[*it].store.store.address < at + bytes; ++it) {
            if (overlaps(*it)) matches.push_back(*it);
        }
        for (const auto index : large) {
            if (overlaps(index)) matches.push_back(index);
        }
        range.count = matches.size();
        for (const auto index : matches) {
            auto& lists = range.queues[submissions[stores[index].at.submission].queue];
            lists.byExecution.push_back(index);
            lists.byEntry.push_back(index);
        }
        for (auto& [queue, lists] : range.queues) {
            std::sort(lists.byExecution.begin(), lists.byExecution.end(), [&](std::size_t left, std::size_t right) { return execution(stores[left].at) < execution(stores[right].at); });
            std::sort(lists.byEntry.begin(), lists.byEntry.end(), [&](std::size_t left, std::size_t right) { return entry(stores[left].at) < entry(stores[right].at); });
        }
        return range;
    }
};

void matchRewind(WaitReport& report, const std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<std::uint64_t>>& segments) {
    const auto& ordinals = segments.at({report.at.queue, report.at.serial});
    const auto self = std::find(ordinals.begin(), ordinals.end(), report.at.ordinal);
    if (self != ordinals.end() && self + 1 != ordinals.end()) {
        report.verdict = WaitReport::Verdict::Released;
        report.route = WaitReport::Route::RewindTail;
        report.reason = format("released: its tail entered the driver as #%llu", static_cast<unsigned long long>(*(self + 1)));
        return;
    }
    if (report.wait.released) {
        report.verdict = WaitReport::Verdict::Unknown;
        report.route = WaitReport::Route::None;
        report.reason = "unknown: the valid bit was set when the submission entered the driver, but the driver reads it again when the queue reaches the REWIND and the capture has no tail";
        return;
    }
    report.verdict = WaitReport::Verdict::Unreleased;
    report.reason = "unreleased: no rewind tail in the capture, so the title had not set the REWIND valid bit when the capture ended";
}

const char* verdictName(WaitReport::Verdict verdict) {
    switch (verdict) {
        case WaitReport::Verdict::Released: return "released";
        case WaitReport::Verdict::Unknown: return "UNKNOWN";
        case WaitReport::Verdict::Unreleased: return "UNRELEASED";
    }
    throw std::runtime_error("PM4 analysis: unknown verdict");
}

std::string describeWriter(const WriterMatch& match) {
    const char* order = match.order == WriterMatch::Order::EarlierOnQueue ? "earlier on this queue" : match.order == WriterMatch::Order::LaterOnQueue ? "later on this queue" : "on another queue";
    std::string text = format("#%llu queue 0x%x serial %llu +0x%04zx %s, %s", static_cast<unsigned long long>(match.at.ordinal), match.at.queue, static_cast<unsigned long long>(match.at.serial), match.at.offset, match.at.name.c_str(), order);
    text += match.stored.has_value() ? format(", stores 0x%llx", static_cast<unsigned long long>(*match.stored)) : ", stores part of the awaited bytes or a value known only at run time";
    switch (match.effect) {
        case WriterMatch::Effect::CurrentValue: return text + ": the value the wait reads, releases it";
        case WriterMatch::Effect::LabelHistory: return text + ": still in the driver's label history when the wait runs, releases it";
        case WriterMatch::Effect::OtherQueue: return text + (match.accepted ? ": the compare may accept it" : ": the compare rejects it") + ", and the capture does not fix when it runs relative to the wait";
        case WriterMatch::Effect::Undecided: return text + ": conditional, or its value is known only at run time";
        case WriterMatch::Effect::Overwritten: return text + ": the compare accepts it, but another store to the awaited bytes can replace it before the wait";
        case WriterMatch::Effect::Evicted: return text + ": the compare accepts it, but newer stores to the awaited bytes can push it out of the driver's label history";
        case WriterMatch::Effect::Rejected: return text + ": the compare rejects it";
        case WriterMatch::Effect::After: return text + (match.accepted ? ": the compare may accept it" : "") + ", but it runs after the wait";
    }
    throw std::runtime_error("PM4 analysis: unknown writer effect");
}

std::string waitLine(const WaitReport& report) {
    std::string text = format("#%llu queue 0x%x serial %llu +0x%04zx %s", static_cast<unsigned long long>(report.at.ordinal), report.at.queue, static_cast<unsigned long long>(report.at.serial), report.at.offset, report.at.name.c_str());
    if (report.wait.kind == MemoryWait::Kind::Memory) {
        const auto& fields = report.wait.fields;
        text += format(" [%s] (%zu bytes) %s 0x%llx mask 0x%llx", hex(fields.address).c_str(), fields.bytes, CompareNames[fields.function], static_cast<unsigned long long>(fields.reference), static_cast<unsigned long long>(fields.mask));
    }
    if (report.wait.conditional) text += " (conditional)";
    return text;
}

std::string submissionLine(const CapturedSubmission& submission) {
    const char* kind = submission.kind == SubmissionKind::Rewind ? " (rewind tail)" : submission.kind == SubmissionKind::Suspend ? " (suspend point)" : "";
    return format("#%llu queue 0x%x %s serial %llu at %llu.%03llu ms: %zu dwords%s", static_cast<unsigned long long>(submission.ordinal), submission.queue, SubmissionCapture::QueueType(submission.queue).data(), static_cast<unsigned long long>(submission.serial),
                  static_cast<unsigned long long>(submission.timeUs / 1000), static_cast<unsigned long long>(submission.timeUs % 1000), submission.dwords, kind);
}

void printPackets(std::FILE* output, const std::vector<DecodedPacket>& packets, std::size_t from, std::size_t to, std::size_t marked) {
    for (std::size_t i = from; i < to && i < packets.size(); ++i) {
        const auto& packet = packets[i];
        std::fprintf(output, "  %s+0x%04zx %-26s %s%s\n", packet.offset == marked ? "> " : "  ", packet.offset, packet.name.c_str(), packet.operands.c_str(), packet.conditional && (!packet.stores.empty() || packet.wait.has_value()) ? "  (conditional)" : "");
    }
}

std::vector<DecodedPacket> decodePackets(const CapturedSubmission& submission, bool text) {
    const std::span<const std::uint32_t> commands = submission.commands;
    require(commands.size() == submission.dwords, "the commands do not hold the dword count of the index");
    std::vector<DecodedPacket> packets;
    std::vector<std::size_t> boundaries;
    std::vector<std::size_t> guarded;
    auto range = submission.conditionalRanges.begin();
    auto held = submission.heldAtSubmit.begin();
    for (std::size_t cursor = 0; cursor < commands.size();) {
        std::erase_if(guarded, [cursor](std::size_t end) { return end <= cursor; });
        boundaries.push_back(cursor);
        const auto header = commands[cursor];
        DecodedPacket packet;
        packet.offset = cursor;
        packet.header = header;
        if (text) packet.name = Pm4::Name(header);
        if (Pm4::FillerPacket(header)) {
            std::size_t run = 1;
            while (cursor + run < commands.size() && Pm4::FillerPacket(commands[cursor + run])) ++run;
            for (std::size_t inner = 1; inner < run; ++inner) boundaries.push_back(cursor + inner);
            packet.words = run;
            if (text) packet.operands = format("x%zu", run);
            packets.push_back(std::move(packet));
            cursor += run;
            continue;
        }
        if ((header >> 30u) != 3u) require(false, format("header 0x%08x at DWORD %zu is not a type-3 packet", header, cursor));
        const auto words = Pm4::PacketWords(header);
        if (words > commands.size() - cursor) require(false, format("%s at DWORD %zu needs %zu dwords, %zu remain", Pm4::Name(header).c_str(), cursor, words, commands.size() - cursor));
        const auto span = commands.subspan(cursor, words);
        try {
            Pm4::Validate(span, submission.queue);
        } catch (const std::exception& error) {
            throw std::runtime_error(format("PM4 analysis: %s at DWORD %zu is a packet the driver rejects, so the capture is corrupt: ", Pm4::Name(header).c_str(), cursor) + error.what());
        }
        packet.words = words;
        packet.conditional = !guarded.empty() || Pm4::Predicated(header);
        if (conditionalStart(header) && !submission.conditionalKnown) {
            guarded.push_back(commands.size());
        } else if (conditionalStart(header)) {
            if (range == submission.conditionalRanges.end() || range->first != cursor) require(false, format("%s at DWORD %zu has no conditional range in the index", Pm4::Name(header).c_str(), cursor));
            if (range->second < cursor + words) require(false, format("the conditional range of %s at DWORD %zu ends inside it", Pm4::Name(header).c_str(), cursor));
            guarded.push_back(range->second);
            ++range;
        }
        if (range != submission.conditionalRanges.end() && range->first <= cursor) require(false, format("a conditional range of the index starts at DWORD %zu, which is no COND_EXEC or predicated INDIRECT_BUFFER", range->first));
        const auto opcode = (header >> 8u) & 0xffu;
        if (text) packet.operands = describe(span);
        for (auto& store : Pm4::DecodeStores(span)) {
            const bool conditional = packet.conditional || store.conditional;
            packet.stores.push_back({std::move(store), conditional, opcode == 0x49 || opcode == 0x37});
        }
        const bool heldHere = held != submission.heldAtSubmit.end() && *held == cursor;
        if (opcode == 0x3c || opcode == 0x93) {
            MemoryWait wait;
            wait.fields = Pm4::DecodeWait(span);
            wait.heldAtSubmit = heldHere;
            wait.conditional = packet.conditional;
            wait.packet.assign(span.begin(), span.end());
            packet.wait = std::move(wait);
        } else if (opcode == 0x59) {
            MemoryWait wait;
            wait.kind = MemoryWait::Kind::Rewind;
            wait.released = (span[1] & Pm4::RewindValid) != 0;
            wait.packet.assign(span.begin(), span.end());
            packet.wait = std::move(wait);
        }
        if (heldHere && (!packet.wait.has_value() || packet.wait->kind != MemoryWait::Kind::Memory)) require(false, format("the index marks DWORD %zu as a wait held at submit, but %s is no WAIT_REG_MEM", cursor, Pm4::Name(header).c_str()));
        if (heldHere) ++held;
        if (held != submission.heldAtSubmit.end() && *held <= cursor) require(false, format("the index marks DWORD %zu as a wait held at submit, which is inside a packet", *held));
        packets.push_back(std::move(packet));
        cursor += words;
    }
    require(range == submission.conditionalRanges.end(), "a conditional range of the index starts past the commands");
    require(held == submission.heldAtSubmit.end(), "a wait held at submit lies past the commands");
    for (const auto& [start, end] : submission.conditionalRanges) {
        if (end != commands.size() && !std::binary_search(boundaries.begin(), boundaries.end(), end)) require(false, format("the conditional range at DWORD %zu ends inside a packet", start));
    }
    return packets;
}

std::vector<DecodedPacket> decodeSubmission(const std::filesystem::path& directory, CapturedSubmission submission, bool text) {
    try {
        submission.commands = SubmissionCapture::ReadCommands(directory, submission);
        return decodePackets(submission, text);
    } catch (const std::exception& error) {
        throw std::runtime_error(format("submission #%llu: ", static_cast<unsigned long long>(submission.ordinal)) + error.what());
    }
}

}

std::vector<DecodedPacket> Decode(const CapturedSubmission& submission) {
    return decodePackets(submission, true);
}

std::size_t Analysis::Count(WaitReport::Verdict verdict) const {
    return static_cast<std::size_t>(std::count_if(waits.begin(), waits.end(), [verdict](const WaitReport& report) { return report.verdict == verdict; }));
}

std::size_t Analysis::CrossQueue() const {
    return static_cast<std::size_t>(std::count_if(waits.begin(), waits.end(), [](const WaitReport& report) { return report.crossQueue; }));
}

Analysis Analyze(const std::filesystem::path& directory) {
    Analysis analysis;
    analysis.submissions = SubmissionCapture::ReadIndex(directory);
    const auto& submissions = analysis.submissions;
    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<std::uint64_t>> segments;
    for (const auto& submission : submissions) {
        if (submission.kind != SubmissionKind::Suspend) segments[{submission.queue, submission.serial}].push_back(submission.ordinal);
    }
    std::vector<Store> stores;
    std::vector<Waiter> waiters;
    for (std::size_t index = 0; index < submissions.size(); ++index) {
        if (submissions[index].dwords == 0) continue;
        auto packets = decodeSubmission(directory, submissions[index], false);
        analysis.packets += packets.size();
        for (auto& packet : packets) {
            for (auto& store : packet.stores) stores.push_back({{index, packet.offset, packet.header}, std::move(store)});
            if (packet.wait.has_value()) waiters.push_back({{index, packet.offset, packet.header}, std::move(*packet.wait)});
        }
    }
    analysis.stores = stores.size();
    Matcher matcher(submissions, stores);
    for (const auto& waiter : waiters) {
        WaitReport report;
        const auto& submission = submissions[waiter.at.submission];
        report.at = {submission.ordinal, submission.queue, submission.serial, waiter.at.offset, Pm4::Name(waiter.at.header)};
        report.wait = waiter.wait;
        if (report.wait.kind == MemoryWait::Kind::Rewind) {
            matchRewind(report, segments);
        } else if (report.wait.fields.function == 0) {
            report.verdict = WaitReport::Verdict::Released;
            report.route = WaitReport::Route::Always;
            report.reason = "released: the compare function always passes";
        } else if (report.wait.heldAtSubmit) {
            report.verdict = WaitReport::Verdict::Released;
            report.route = WaitReport::Route::HeldAtSubmit;
            report.reason = "released: its value passed the compare when the submission entered the driver (held at submit)";
        } else {
            matcher.Match(report, waiter);
        }
        analysis.waits.push_back(std::move(report));
    }
    return analysis;
}

int Report(const std::filesystem::path& directory, bool full, std::FILE* output) {
    const auto analysis = Analyze(directory);
    const auto& submissions = analysis.submissions;
    std::map<std::uint32_t, std::size_t> perQueue;
    std::size_t tails = 0, suspends = 0, dwords = 0;
    for (const auto& submission : submissions) {
        dwords += submission.dwords;
        if (submission.kind == SubmissionKind::Suspend) ++suspends;
        else ++perQueue[submission.queue];
        if (submission.kind == SubmissionKind::Rewind) ++tails;
    }
    std::fprintf(output, "capture %s: submissions %zu (rewind tails %zu, suspend points %zu), dwords %zu, packets %zu, memory stores %zu\n", directory.string().c_str(), submissions.size(), tails, suspends, dwords, analysis.packets, analysis.stores);
    for (const auto& [queue, count] : perQueue) std::fprintf(output, "  queue 0x%x %s: submissions %zu\n", queue, SubmissionCapture::QueueType(queue).data(), count);
    std::map<WaitReport::Route, std::size_t> routes;
    for (const auto& report : analysis.waits) {
        if (report.verdict == WaitReport::Verdict::Released) ++routes[report.route];
    }
    const auto unreleased = analysis.Count(WaitReport::Verdict::Unreleased);
    const auto unknown = analysis.Count(WaitReport::Verdict::Unknown);
    std::fprintf(output, "waits %zu: released %zu (compare always %zu, held at submit %zu, current value %zu, label history %zu, rewind tail %zu), unknown %zu (cross-queue timing %zu), unreleased %zu\n", analysis.waits.size(), analysis.Count(WaitReport::Verdict::Released),
                 routes[WaitReport::Route::Always], routes[WaitReport::Route::HeldAtSubmit], routes[WaitReport::Route::CurrentValue], routes[WaitReport::Route::LabelHistory], routes[WaitReport::Route::RewindTail], unknown, analysis.CrossQueue(), unreleased);
    std::fprintf(output, "(released only where the capture proves it; the tool does not model cross-queue timing, so a wait that stores of another queue decide is unknown)\n");
    std::fprintf(output, "(stores are the PM4 packets of the capture; CPU and shader stores are not captured)\n");
    if (std::any_of(submissions.begin(), submissions.end(), [](const CapturedSubmission& submission) { return !submission.heldKnown || !submission.conditionalKnown; })) {
        std::fprintf(output, "(the index has no held and conditional columns: a wait no store of the capture can release is unknown instead of unreleased, and every packet after a COND_EXEC or predicated INDIRECT_BUFFER is conditional to the end of its submission)\n");
    }

    std::map<std::uint64_t, std::size_t> byOrdinal;
    for (std::size_t index = 0; index < submissions.size(); ++index) byOrdinal.emplace(submissions[index].ordinal, index);
    std::vector<const WaitReport*> shown;
    if (full) {
        for (const auto& report : analysis.waits) shown.push_back(&report);
    } else {
        for (const auto verdict : {WaitReport::Verdict::Unreleased, WaitReport::Verdict::Unknown}) {
            for (auto it = analysis.waits.rbegin(); it != analysis.waits.rend() && shown.size() < ListedWaits; ++it) {
                if (it->verdict == verdict) shown.push_back(&*it);
            }
        }
        if (!shown.empty()) std::fprintf(output, "\nunreleased waits, then unknown waits, newest first:\n");
    }
    for (const auto* report : shown) {
        std::fprintf(output, "\n%s %s\n  %s\n", verdictName(report->verdict), waitLine(*report).c_str(), report->reason.c_str());
        for (const auto& writer : report->writers) std::fprintf(output, "    writer %s\n", describeWriter(writer).c_str());
        if (report->writerCount > report->writers.size()) std::fprintf(output, "    (%zu stores to the awaited bytes in the capture)\n", report->writerCount);
        if (full) continue;
        const auto& submission = submissions[byOrdinal.at(report->at.ordinal)];
        const auto packets = decodeSubmission(directory, submission, true);
        const auto at = static_cast<std::size_t>(std::find_if(packets.begin(), packets.end(), [&](const DecodedPacket& packet) { return packet.offset == report->at.offset; }) - packets.begin());
        std::fprintf(output, "  in %s\n", submissionLine(submission).c_str());
        printPackets(output, packets, at > ContextBefore ? at - ContextBefore : 0, at + ContextAfter + 1, report->at.offset);
    }
    if (!full && unreleased + unknown > shown.size()) std::fprintf(output, "\n(+%zu more unreleased and unknown waits; --full lists every wait)\n", unreleased + unknown - shown.size());

    std::vector<std::size_t> order;
    for (std::size_t index = 0; index < submissions.size(); ++index) {
        if (submissions[index].kind != SubmissionKind::Suspend) order.push_back(index);
    }
    std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        return std::tie(submissions[left].queue, submissions[left].serial, submissions[left].ordinal) < std::tie(submissions[right].queue, submissions[right].serial, submissions[right].ordinal);
    });
    std::fprintf(output, full ? "\nsubmissions in execution order per queue:\n" : "\nlast submissions of each queue (--full prints every submission and packet):\n");
    for (std::size_t begin = 0; begin < order.size();) {
        auto end = begin;
        while (end < order.size() && submissions[order[end]].queue == submissions[order[begin]].queue) ++end;
        for (auto index = full || end - begin <= LastSubmissionsPerQueue ? begin : end - LastSubmissionsPerQueue; index < end; ++index) {
            const auto& submission = submissions[order[index]];
            std::fprintf(output, "%s\n", submissionLine(submission).c_str());
            const auto packets = submission.dwords == 0 ? std::vector<DecodedPacket>{} : decodeSubmission(directory, submission, true);
            const auto limit = full ? packets.size() : std::min(packets.size(), ShownPackets);
            printPackets(output, packets, 0, limit, std::numeric_limits<std::size_t>::max());
            if (limit < packets.size()) std::fprintf(output, "    (+%zu packets)\n", packets.size() - limit);
        }
        begin = end;
    }
    return unreleased == 0 ? 0 : 1;
}

}
