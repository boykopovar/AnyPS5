#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapWalkMemo.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using AgcDriver::DriverDetail::HeapWalkEntry;
using AgcDriver::DriverDetail::HeapWalkEntryBytes;
using AgcDriver::DriverDetail::HeapWalkMemo;
using AgcDriver::DriverDetail::HeapWalkMemoKeys;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

HeapWalkEntry sizedEntry(std::size_t words, std::shared_ptr<const ShaderRecompiler::RecompileResult> result = nullptr) {
    HeapWalkEntry entry;
    entry.values.resize(words);
    entry.values.shrink_to_fit();
    entry.result = std::move(result);
    return entry;
}

std::uint64_t heldBytes(const HeapWalkMemo& memo) {
    std::uint64_t bytes = 0;
    std::unordered_set<const void*> pinned;
    for (const auto* generation : {&memo.current, &memo.previous}) {
        for (const auto& [key, ways] : *generation) {
            for (const auto& entry : ways) {
                bytes += AgcDriver::DriverDetail::HeapWalkEntryOwnBytes(entry);
                if (entry.capture != nullptr && pinned.insert(entry.capture.get()).second) bytes += AgcDriver::DriverDetail::HeapWalkCaptureBytes(*entry.capture);
                if (entry.result != nullptr && pinned.insert(entry.result.get()).second) bytes += AgcDriver::DriverDetail::HeapWalkResultBytes(*entry.result);
                if (entry.words != nullptr && pinned.insert(entry.words.get()).second) bytes += AgcDriver::DriverDetail::HeapWalkWordsBytes(*entry.words);
            }
        }
    }
    require(pinned.size() == memo.pins.size(), "the heap walk memo pins " + std::to_string(memo.pins.size()) + " objects for the " + std::to_string(pinned.size()) + " its entries hold");
    return bytes;
}

void requireWithinBudget(const HeapWalkMemo& memo, const std::string& where) {
    const auto held = heldBytes(memo);
    require(held == memo.heldBytes, where + ": the heap walk memo charged " + std::to_string(memo.heldBytes) + " bytes for the " + std::to_string(held) + " it holds");
    require(held <= memo.budget, where + ": the heap walk memo holds " + std::to_string(held) + " bytes, over its budget of " + std::to_string(memo.budget));
}

void oldestEntriesDroppedTests() {
    constexpr std::size_t words = 4096;
    require(HeapWalkEntryBytes(sizedEntry(words)) == sizeof(HeapWalkEntry) + words * sizeof(std::uint32_t), "a heap walk entry is not charged its struct and the capacity of its vectors");
    const auto entryBytes = HeapWalkEntryBytes(sizedEntry(words, std::make_shared<const ShaderRecompiler::RecompileResult>()));
    HeapWalkMemo memo;
    memo.budget = 16 * entryBytes;
    constexpr std::uint64_t keys = 64;
    std::vector<std::weak_ptr<const ShaderRecompiler::RecompileResult>> results;
    for (std::uint64_t key = 1; key <= keys; ++key) {
        auto result = std::make_shared<const ShaderRecompiler::RecompileResult>();
        results.push_back(result);
        memo.remember(key, sizedEntry(words, std::move(result)));
        requireWithinBudget(memo, "after key " + std::to_string(key));
    }
    require(memo.find(1) == nullptr && memo.find(keys / 2) == nullptr, "the oldest heap walk entries stayed past the budget");
    require(results.front().expired() && results[keys / 2 - 1].expired(), "a heap walk entry the budget dropped still holds its result");
    const auto* newest = memo.find(keys);
    require(newest != nullptr && newest->size() == 1 && newest->front().values.size() == words, "the newest heap walk entry is not findable under the budget");
    require(!results.back().expired(), "the newest heap walk entry lost its result");
    std::cout << "the heap walk memo keeps " << memo.current.size() << " of " << keys << " entries within its budget of " << memo.budget << " bytes\n";
}

void previousGenerationDroppedFirstTests() {
    const auto tinyBytes = HeapWalkEntryBytes(sizedEntry(1));
    constexpr std::size_t largeWords = std::size_t{2} << 20u;
    const auto largeBytes = HeapWalkEntryBytes(sizedEntry(largeWords));
    HeapWalkMemo memo;
    memo.budget = largeBytes + 64 * tinyBytes;
    require((HeapWalkMemoKeys + 1) * tinyBytes < memo.budget, "the generation fixture does not fit its budget");
    for (std::uint64_t key = 1; key <= HeapWalkMemoKeys + 1; ++key) memo.remember(key, sizedEntry(1));
    require(memo.previous.size() == HeapWalkMemoKeys && memo.current.size() == 1, "the heap walk memo did not start a generation at its key cap");
    requireWithinBudget(memo, "after the generation swap");
    memo.remember(HeapWalkMemoKeys + 2, sizedEntry(largeWords));
    requireWithinBudget(memo, "after the large entry");
    require(memo.previous.empty(), "the heap walk memo kept its previous generation over the budget");
    require(memo.find(HeapWalkMemoKeys + 1) != nullptr && memo.find(HeapWalkMemoKeys + 2) != nullptr, "the heap walk memo dropped current entries while its previous generation could go");
    std::cout << "the heap walk memo drops its previous generation before its current entries\n";
}

void hitEntriesKeptTests() {
    constexpr std::size_t words = 4096;
    const auto entryBytes = HeapWalkEntryBytes(sizedEntry(words));
    HeapWalkMemo memo;
    memo.budget = 4 * entryBytes;
    for (std::uint64_t key = 1; key <= 4; ++key) memo.remember(key, sizedEntry(words));
    auto* hit = memo.find(1);
    require(hit != nullptr && hit->size() == 1, "the first heap walk entry is not findable under the budget");
    memo.touch(hit->front());
    memo.remember(5, sizedEntry(words));
    requireWithinBudget(memo, "after the trim");
    require(memo.find(1) != nullptr, "the heap walk memo dropped an entry that was hit after the entries it kept");
    require(memo.find(2) == nullptr, "the heap walk memo kept its least recently used entry past the budget");
    std::cout << "the heap walk memo keeps its recently hit entries\n";
}

void promotedHitKeptTests() {
    const auto tinyBytes = HeapWalkEntryBytes(sizedEntry(1));
    constexpr std::size_t largeWords = std::size_t{2} << 20u;
    const auto largeBytes = HeapWalkEntryBytes(sizedEntry(largeWords));
    HeapWalkMemo memo;
    memo.budget = largeBytes + 64 * tinyBytes;
    for (std::uint64_t key = 1; key <= HeapWalkMemoKeys + 1; ++key) memo.remember(key, sizedEntry(1));
    auto* promoted = memo.find(1);
    require(promoted != nullptr && memo.current.contains(1), "a hit did not move the heap walk entry into the current generation");
    memo.touch(promoted->front());
    memo.remember(HeapWalkMemoKeys + 2, sizedEntry(largeWords));
    requireWithinBudget(memo, "after the large entry");
    require(memo.find(1) != nullptr, "the heap walk memo dropped an entry promoted by a hit with its previous generation");
    std::cout << "the heap walk memo keeps an entry a hit promoted from its previous generation\n";
}

void sharedResultsChargedOnceTests() {
    constexpr std::size_t resultWords = std::size_t{1} << 16u;
    auto result = std::make_shared<ShaderRecompiler::RecompileResult>();
    result->heapUserWords.resize(resultWords);
    result->heapUserWords.shrink_to_fit();
    const std::shared_ptr<const ShaderRecompiler::RecompileResult> shared = std::move(result);
    std::weak_ptr<const ShaderRecompiler::RecompileResult> watched = shared;
    const auto ownBytes = AgcDriver::DriverDetail::HeapWalkEntryOwnBytes(sizedEntry(1));
    const auto sharedBytes = AgcDriver::DriverDetail::HeapWalkResultBytes(*shared);
    HeapWalkMemo memo;
    memo.budget = 2 * HeapWalkEntryBytes(sizedEntry(1, shared));
    constexpr std::uint64_t keys = 16;
    for (std::uint64_t key = 1; key <= keys; ++key) memo.remember(key, sizedEntry(1, shared));
    require(memo.heldBytes == keys * ownBytes + sharedBytes, "the heap walk memo charged " + std::to_string(memo.heldBytes) + " bytes for " + std::to_string(keys) + " entries that share one result, not " + std::to_string(keys * ownBytes + sharedBytes));
    for (std::uint64_t key = 1; key <= keys; ++key) require(memo.find(key) != nullptr, "the heap walk memo dropped an entry whose shared result it already charged");
    for (std::uint64_t key = keys + 1; key <= keys + 64; ++key) {
        memo.remember(key, sizedEntry(1, std::make_shared<const ShaderRecompiler::RecompileResult>()));
        require(memo.heldBytes <= memo.budget, "after key " + std::to_string(key) + ": the heap walk memo holds " + std::to_string(memo.heldBytes) + " bytes, over its budget of " + std::to_string(memo.budget));
    }
    requireWithinBudget(memo, "after the shared result's entries aged out");
    std::cout << "the heap walk memo charges a result its entries share once\n";
}

}

int main() {
    try {
        oldestEntriesDroppedTests();
        previousGenerationDroppedFirstTests();
        hitEntriesKeptTests();
        promotedHitKeptTests();
        sharedResultsChargedOnceTests();
        std::cout << "heap walk memo budget tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "heap walk memo budget tests failed: " << error.what() << '\n';
        return 1;
    }
}
