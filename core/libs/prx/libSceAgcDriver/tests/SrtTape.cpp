// Unit tests for the SRT-walk tape + rebind (RegionTape.hpp). Pure logic, no
// GPU and no driver state: replay and structural validation run against an
// injected guest-memory reader. Registered as ctest "agc_srt_tape".

#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/RegionTape.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <span>
#include <vector>

using namespace AgcDriver::DriverDetail;

namespace {

int failures = 0;

void Check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "FAIL line %d: %s\n", line, expression);
        ++failures;
    }
}

#define CHECK(cond) Check((cond), #cond, __LINE__)

// Fake guest memory: maps address -> dword.
struct FakeMemory {
    std::map<std::uint64_t, std::uint32_t> words;

    bool Read(std::uint64_t address, std::span<std::byte> out) const {
        const auto found = words.find(address);
        if (found == words.end() || out.size() != 4) return false;
        std::memcpy(out.data(), &found->second, 4);
        return true;
    }

    void WriteRun(std::uint64_t base, std::span<const std::uint32_t> content) {
        for (std::size_t i = 0; i < content.size(); ++i) words[base + i * 4u] = content[i];
    }
};

// Scene: user word 0 holds a per-object pointer 0x1000 (run 0, the descriptor).
// Run 0's dword 2 holds a chained pointer 0x2000 (run 1, the buffer it
// describes). Run 0 is therefore Structural (its format fields decide the
// codegen) and run 1 is Data (loaded content).
struct Scene {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs{{0x1000, 0x1010}, {0x2000, 0x2008}};
    std::vector<std::int8_t> runBases{0, -1};
    std::vector<std::uint32_t> flat{0x1000, 0x11111111u, 0x2000, 0x33333333u, 0x2000, 0x44444444u};
    std::vector<std::uint32_t> user{0x1000, 0, 0};
};

// Guest memory matching the scene after a rebind by delta (new user word 0).
void PopulateRebound(const Scene& s, std::uint32_t newPointer, FakeMemory& memory, std::uint32_t descriptorWord1 = 0x11111111u) {
    const std::uint32_t delta = newPointer - static_cast<std::uint32_t>(s.runs[0].first);
    const auto buffer = static_cast<std::uint32_t>(s.runs[1].first) + delta;
    const std::array<std::uint32_t, 4> descriptor{newPointer, descriptorWord1, buffer, 0x33333333u};
    const std::array<std::uint32_t, 2> content{buffer, 0x44444444u};
    memory.WriteRun(newPointer, descriptor);
    memory.WriteRun(buffer, content);
}

void DifferentPointersSameLayoutReuses() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    CHECK(tape.rebasable);
    CHECK(tape.regions.size() == 2);
    CHECK(tape.regions[0].source == TapeSource::UserWord && tape.regions[0].index == 0);
    CHECK(tape.regions[1].source == TapeSource::RegionDword && tape.regions[1].index == 0 && tape.regions[1].dword == 2);
    // BUG-1: the descriptor is the source of a chained derivation -> Structural,
    // the pointed buffer is a leaf -> Data.
    CHECK(tape.regions[0].kind == TapeField::Structural);
    CHECK(tape.regions[1].kind == TapeField::Data);
    CHECK(tape.baseFields.size() == 1 && tape.baseFields[0].first == 2 && tape.baseFields[0].second == 0);
    CHECK(TapeCoversUserWord(tape, 0));
    CHECK(!TapeCoversUserWord(tape, 1));

    const std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    std::vector<std::pair<std::uint64_t, std::uint64_t>> outRuns;
    std::vector<std::uint32_t> outFlat;
    CHECK(ReplayVariantTape(tape, newUser, outRuns, outFlat));
    CHECK(outRuns.size() == 2);
    CHECK(outRuns[0].first == 0x5000 && outRuns[0].second == 0x5010); // moved descriptor
    CHECK(outRuns[1].first == 0x6000 && outRuns[1].second == 0x6008); // chained buffer shifted by the same delta
    CHECK(outFlat[0] == 0x5000); // auto-referential base dword of run 0 patched
    CHECK(outFlat[2] == 0x6000); // BUG-1/3: the descriptor's base field is patched exactly once
    CHECK(outFlat[4] == 0x6000); // base dword of the leaf run patched

    FakeMemory memory;
    PopulateRebound(s, 0x5000, memory);
    CHECK(ValidateReboundTape(tape, newUser, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
}

void DescriptorFieldChangeIsDiffering() {
    // BUG-1 regression: a field of the UserWord-sourced descriptor that changes the
    // codegen (here a format/stride dword) must be caught, not live-filled away.
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    const std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    FakeMemory memory;
    PopulateRebound(s, 0x5000, memory, 0x99999999u);
    CHECK(!ValidateReboundTape(tape, newUser, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
}

void ChainedPointerFollowsTheDelta() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    const std::vector<std::uint32_t> newUser{0x9000, 0, 0};
    std::vector<std::pair<std::uint64_t, std::uint64_t>> outRuns;
    std::vector<std::uint32_t> outFlat;
    CHECK(ReplayVariantTape(tape, newUser, outRuns, outFlat));
    CHECK(outRuns[1].first == 0x2000 + (0x9000 - 0x1000)); // 0xA000: chained pointer moves with its object
}

void LayoutDifferenceIsDiffering() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    const std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    FakeMemory memory;
    PopulateRebound(s, 0x5000, memory);
    memory.words[0x5004] = 0x99999999u; // descriptor field (format/stride) changed -> must NOT rebind
    CHECK(!ValidateReboundTape(tape, newUser, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
    // A leaf's contents are data: they are refilled, not compared.
    memory.words[0x5004] = 0x11111111u;
    memory.words[0x6004] = 0x99999999u;
    CHECK(ValidateReboundTape(tape, newUser, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
}

void UnregisteredStepFailsClosed() {
    Scene s;
    s.runs = {{0x1000, 0x1010}, {0x9000, 0x9008}};
    s.runBases = {0, -1};
    s.flat = {0x1000, 0x11111111u, 0x22222222u, 0x33333333u, 0x9000, 0x44444444u}; // 0x9000 not present in run 0
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    CHECK(!tape.rebasable);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> outRuns;
    std::vector<std::uint32_t> outFlat;
    const std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    CHECK(!ReplayVariantTape(tape, newUser, outRuns, outFlat));
    CHECK(!TapeCoversUserWord(tape, 0));
    FakeMemory memory;
    CHECK(!ValidateReboundTape(tape, newUser, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
}

void AmbiguousChainedSourceFailsClosed() {
    // BUG-4: the base value of the chained run appears twice, so which run owns
    // the delta is a guess. Fail closed instead of guessing.
    Scene s;
    s.flat = {0x1000, 0x2000, 0x2000, 0x33333333u, 0x2000, 0x44444444u};
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    CHECK(!tape.rebasable);
}

void UnmappedStructuralReadFailsClosed() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    const std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    FakeMemory memory; // empty: the structural run is unmapped at the new address
    CHECK(!ValidateReboundTape(tape, newUser, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
}

void RebindPatchesCarryNewAddresses() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    std::vector<std::uint32_t> newUser{0x5000, 0x7777, 0};
    const std::vector<std::array<std::uint32_t, 3>> valueSlots{{0, 2, 8}, {1, 3, 4}};
    const std::vector<std::uint32_t> forwarded{0, 1, 5};
    std::vector<std::array<std::uint32_t, 3>> descriptorPatches;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> shaderDataPatches;
    RebindPatches(tape, tape.oldUserData, newUser, valueSlots, forwarded, descriptorPatches, shaderDataPatches);
    CHECK(descriptorPatches.size() == 1);
    const std::array<std::uint32_t, 3> expectedSlot{2, 8, 0x5000};
    CHECK(descriptorPatches[0] == expectedSlot);
    CHECK(shaderDataPatches.size() == 1);
    CHECK(shaderDataPatches[0].first == 0 && shaderDataPatches[0].second == 0x5000);
}

void ChainedRebindIsRepeatable() {
    // BUG-2 regression: the tape replays from its own snapshot, so replaying it a
    // second time with the same new words gives the same answer (a cached rebound
    // copy carries moved words, which must not poison the next rebind).
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    const std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    std::vector<std::pair<std::uint64_t, std::uint64_t>> firstRuns, secondRuns;
    std::vector<std::uint32_t> firstFlat, secondFlat;
    CHECK(ReplayVariantTape(tape, newUser, firstRuns, firstFlat));
    CHECK(ReplayVariantTape(tape, newUser, secondRuns, secondFlat));
    CHECK(firstRuns == secondRuns);
    CHECK(firstFlat == secondFlat);
}

void ShapeKeyReusesTheShapeEntryForTrackedWords() {
    // Two draws with the same shape and different per-object pointers: the only
    // changed user word is a tape source, so the draw resolves to the same shape
    // entry and the tape rebuilds the regions at the new addresses.
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    CHECK(tape.rebasable);
    const std::vector<std::uint32_t> was{0x1000, 0x7777, 0};
    const std::vector<std::uint32_t> now{0x5000, 0x7777, 0};
    CHECK(RebindEligibilityOf(&tape, was, now, ~std::uint64_t{0}) == RebindEligibility::CoveredByTape);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> outRuns;
    std::vector<std::uint32_t> outFlat;
    CHECK(ReplayVariantTape(tape, now, outRuns, outFlat));
    CHECK(outRuns[0].first == 0x5000);
}

void ShapeKeyKeepsUntrackedWordsInTheKey() {
    // A changed word the tape does not cover must stay in the draw key, so the draw
    // gets its own entry instead of a (wrong) rebind from the shape's entry.
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    const std::vector<std::uint32_t> was{0x1000, 0x7777, 0};
    const std::vector<std::uint32_t> changed{0x1000, 0x7777, 0x9999};
    CHECK(!TapeCoversUserWord(tape, 2));
    CHECK(RebindEligibilityOf(&tape, was, changed, ~std::uint64_t{0}) == RebindEligibility::UncoveredWord);
    CHECK(RebindEligibilityOf(nullptr, was, changed, ~std::uint64_t{0}) == RebindEligibility::UncoveredWord);
    // A word the draw key already treats as data (mask bit clear) may still differ.
    CHECK(RebindEligibilityOf(&tape, was, changed, ~(std::uint64_t{1} << 2u)) == RebindEligibility::Eligible);
    CHECK(RebindEligibilityOf(&tape, was, was, ~std::uint64_t{0}) == RebindEligibility::Eligible);
}

} // namespace

int main() {
    DifferentPointersSameLayoutReuses();
    DescriptorFieldChangeIsDiffering();
    ChainedPointerFollowsTheDelta();
    LayoutDifferenceIsDiffering();
    UnregisteredStepFailsClosed();
    AmbiguousChainedSourceFailsClosed();
    UnmappedStructuralReadFailsClosed();
    RebindPatchesCarryNewAddresses();
    ChainedRebindIsRepeatable();
    ShapeKeyReusesTheShapeEntryForTrackedWords();
    ShapeKeyKeepsUntrackedWordsInTheKey();
    if (failures != 0) {
        std::fprintf(stderr, "agc_srt_tape: %d check(s) failed\n", failures);
        return 1;
    }
    std::printf("agc_srt_tape: all checks passed\n");
    return 0;
}
