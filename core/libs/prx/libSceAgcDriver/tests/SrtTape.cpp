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
};

// Scene: user word 0 holds a per-object pointer 0x1000 (run 0, Data). Run 0's
// dword 2 holds a chained pointer 0x2000 (run 1, Structural descriptor whose
// dword 0 is the base address field).
struct Scene {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs{{0x1000, 0x1010}, {0x2000, 0x2008}};
    std::vector<std::int8_t> runBases{0, -1};
    std::vector<std::uint32_t> flat{0x1000, 0x11111111u, 0x2000, 0x33333333u, 0x2000, 0x44444444u};
    std::vector<std::uint32_t> user{0x1000, 0, 0};
};

void DifferentPointersSameLayoutReuses() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    CHECK(tape.rebasable);
    CHECK(tape.regions.size() == 2);
    CHECK(tape.regions[0].source == TapeSource::UserWord && tape.regions[0].index == 0 && tape.regions[0].kind == TapeField::Data);
    CHECK(tape.regions[1].source == TapeSource::RegionDword && tape.regions[1].index == 0 && tape.regions[1].dword == 2 && tape.regions[1].kind == TapeField::Structural);
    CHECK(TapeCoversUserWord(tape, 0));
    CHECK(!TapeCoversUserWord(tape, 1));

    std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    std::vector<std::pair<std::uint64_t, std::uint64_t>> outRuns;
    std::vector<std::uint32_t> outFlat;
    CHECK(ReplayVariantTape(tape, s.user, newUser, s.flat, outRuns, outFlat));
    CHECK(outRuns.size() == 2);
    CHECK(outRuns[0].first == 0x5000 && outRuns[0].second == 0x5010); // rebound per-object buffer
    CHECK(outRuns[1].first == 0x6000 && outRuns[1].second == 0x6008); // chained pointer shifted by the same delta
    CHECK(outFlat[0] == 0x5000); // base-address dword of run 0 patched
    CHECK(outFlat[4] == 0x6000); // base-address dword of run 1 patched
    CHECK(outFlat[1] == 0x11111111u && outFlat[2] == 0x2000 && outFlat[5] == 0x44444444u); // other content untouched

    // Layout equal at the new addresses (structural dword 1 unchanged, base field excluded) -> rebind valid.
    FakeMemory memory;
    memory.words[0x6000] = 0x6000;
    memory.words[0x6004] = 0x44444444u;
    CHECK(ValidateReboundTape(tape, s.user, newUser, s.flat, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
}

void ChainedPointerFollowsTheDelta() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    std::vector<std::uint32_t> newUser{0x9000, 0, 0};
    std::vector<std::pair<std::uint64_t, std::uint64_t>> outRuns;
    std::vector<std::uint32_t> outFlat;
    CHECK(ReplayVariantTape(tape, s.user, newUser, s.flat, outRuns, outFlat));
    CHECK(outRuns[1].first == 0x2000 + (0x9000 - 0x1000)); // 0xA000: chained pointer moves with its object
}

void LayoutDifferenceIsDiffering() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    FakeMemory memory;
    memory.words[0x6000] = 0x6000;
    memory.words[0x6004] = 0x99999999u; // structural field (e.g. format) changed -> must NOT rebind
    CHECK(!ValidateReboundTape(tape, s.user, newUser, s.flat, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
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
    std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    CHECK(!ReplayVariantTape(tape, s.user, newUser, s.flat, outRuns, outFlat));
    CHECK(!TapeCoversUserWord(tape, 0));
    FakeMemory memory;
    CHECK(!ValidateReboundTape(tape, s.user, newUser, s.flat, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
}

void UnmappedStructuralReadFailsClosed() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    std::vector<std::uint32_t> newUser{0x5000, 0, 0};
    FakeMemory memory; // empty: the structural run is unmapped at the new address
    CHECK(!ValidateReboundTape(tape, s.user, newUser, s.flat, [&](std::uint64_t address, std::span<std::byte> out) { return memory.Read(address, out); }));
}

void RebindPatchesCarryNewAddresses() {
    Scene s;
    const auto tape = BuildVariantTape(s.runs, s.runBases, s.flat, s.user);
    std::vector<std::uint32_t> oldUser{0x1000, 0x7777, 0};
    std::vector<std::uint32_t> newUser{0x5000, 0x7777, 0}; // only word 0 changed
    const std::vector<std::array<std::uint32_t, 3>> valueSlots{{0, 2, 8}, {1, 3, 4}};
    const std::vector<std::uint32_t> forwarded{0, 1, 5};
    std::vector<std::array<std::uint32_t, 3>> descriptorPatches;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> shaderDataPatches;
    RebindPatches(tape, oldUser, newUser, valueSlots, forwarded, descriptorPatches, shaderDataPatches);
    CHECK(descriptorPatches.size() == 1);
    const std::array<std::uint32_t, 3> expectedSlot{2, 8, 0x5000};
    CHECK(descriptorPatches[0] == expectedSlot);
    CHECK(shaderDataPatches.size() == 1);
    CHECK(shaderDataPatches[0].first == 0 && shaderDataPatches[0].second == 0x5000);
}

} // namespace

int main() {
    DifferentPointersSameLayoutReuses();
    ChainedPointerFollowsTheDelta();
    LayoutDifferenceIsDiffering();
    UnregisteredStepFailsClosed();
    UnmappedStructuralReadFailsClosed();
    RebindPatchesCarryNewAddresses();
    if (failures != 0) {
        std::fprintf(stderr, "agc_srt_tape: %d check(s) failed\n", failures);
        return 1;
    }
    std::printf("agc_srt_tape: all checks passed\n");
    return 0;
}
