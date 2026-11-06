#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DISPATCH_REGIONTAPE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DISPATCH_REGIONTAPE_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

// Tape of the SRT walk: for every captured run it records how the run's guest
// base address derives from the draw's user words (UserWord) or from a dword
// of an earlier captured run (RegionDword, chained pointer), plus a field
// kind that decides what a rebind must re-validate. Pure data + pure
// functions, so the rebind is unit-testable without a GPU. Any step that
// cannot be represented makes the whole tape non-rebasable (fail-closed).

enum class TapeSource : std::uint8_t { UserWord, RegionDword };
enum class TapeField : std::uint8_t { Data, Structural };

struct RegionDerivation {
    TapeSource source = TapeSource::UserWord;
    std::uint32_t index = 0;   // UserWord: user word index; RegionDword: source run index (must be < own run)
    std::int32_t dword = 0;    // RegionDword: dword offset inside the source run's captured words
    std::int64_t addend = 0;   // added to the sourced 32-bit value
    std::uint32_t bytes = 0;   // run length in bytes
    TapeField kind = TapeField::Structural; // Data: content skipped on rebind; Structural: must match
};

struct VariantTape {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs; // capture-time runs
    std::vector<std::uint32_t> runWordOffset;                  // flattened-word offset of each run
    std::vector<RegionDerivation> regions;                     // one per run
    bool rebasable = false;
};

inline std::uint64_t TapeAlign(std::uint32_t word) { return word & ~std::uint32_t{3}; }

// Builds the tape from the captured variant state. runBases[r] >= 0 means the
// SRT walk derived run r's base from user word runBases[r] (see
// tagVariantRuns); otherwise the base dword must be found verbatim inside an
// earlier run (chained pointer). Anything else -> rebasable = false.
inline VariantTape BuildVariantTape(
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs,
    const std::vector<std::int8_t>& runBases,
    const std::vector<std::uint32_t>& flatWords,
    const std::vector<std::uint32_t>& userData) {
    VariantTape tape;
    tape.runs = runs;
    tape.rebasable = true;
    std::uint32_t offset = 0;
    for (std::size_t r = 0; r < runs.size(); ++r) {
        const auto begin = runs[r].first;
        const auto bytes = runs[r].second - runs[r].first;
        if (begin == 0 || (begin & 3u) != 0 || bytes == 0 || (bytes & 3u) != 0 ||
            static_cast<std::size_t>(offset) + bytes / 4u > flatWords.size()) {
            tape.rebasable = false;
            break;
        }
        RegionDerivation d;
        d.bytes = static_cast<std::uint32_t>(bytes);
        if (r < runBases.size() && runBases[r] >= 0 && static_cast<std::size_t>(runBases[r]) < userData.size()) {
            d.source = TapeSource::UserWord;
            d.index = static_cast<std::uint32_t>(runBases[r]);
            d.addend = static_cast<std::int64_t>(begin) - static_cast<std::int64_t>(TapeAlign(userData[d.index]));
            d.kind = TapeField::Data;
        } else {
            // chained pointer: the run's base dword must appear verbatim in an earlier run
            const auto baseLow = static_cast<std::uint32_t>(begin);
            std::size_t source = runs.size();
            std::int32_t dword = -1;
            for (std::size_t s = 0; s < r && source == runs.size(); ++s) {
                const auto count = static_cast<std::uint32_t>((runs[s].second - runs[s].first) / 4u);
                for (std::uint32_t k = 0; k < count; ++k) {
                    const auto at = tape.runWordOffset[s] + k;
                    if (at < flatWords.size() && flatWords[at] == baseLow) {
                        source = s;
                        dword = static_cast<std::int32_t>(k);
                        break;
                    }
                }
            }
            if (source == runs.size()) {
                tape.rebasable = false;
                break;
            }
            d.source = TapeSource::RegionDword;
            d.index = static_cast<std::uint32_t>(source);
            d.dword = dword;
            d.addend = 0;
            d.kind = TapeField::Structural;
        }
        tape.runWordOffset.push_back(offset);
        tape.regions.push_back(d);
        offset += static_cast<std::uint32_t>(bytes / 4u);
    }
    if (tape.rebasable && offset != flatWords.size()) tape.rebasable = false;
    if (tape.regions.size() != tape.runs.size()) tape.rebasable = false;
    return tape;
}

// Replays the tape with new user words: produces the rebound runs and a copy
// of the captured words whose base-address dword (word 0 of each run, when it
// held the old base) is patched to the new base. Returns false on any step
// that is not representable (fail-closed).
inline bool ReplayVariantTape(
    const VariantTape& tape,
    std::span<const std::uint32_t> oldUser,
    std::span<const std::uint32_t> newUser,
    const std::vector<std::uint32_t>& oldFlat,
    std::vector<std::pair<std::uint64_t, std::uint64_t>>& outRuns,
    std::vector<std::uint32_t>& outFlat) {
    if (!tape.rebasable || tape.regions.size() != tape.runs.size() || tape.runWordOffset.size() != tape.runs.size()) return false;
    outFlat = oldFlat;
    outRuns.assign(tape.runs.size(), {0, 0});
    std::vector<std::int64_t> delta(tape.runs.size(), 0);
    for (std::size_t r = 0; r < tape.runs.size(); ++r) {
        const auto& d = tape.regions[r];
        const auto w0 = tape.runWordOffset[r];
        const auto count = d.bytes / 4u;
        if (static_cast<std::size_t>(w0) + count > outFlat.size()) return false;
        std::int64_t base = -1;
        if (d.source == TapeSource::UserWord) {
            if (d.index >= oldUser.size() || d.index >= newUser.size()) return false;
            const auto oldLow = TapeAlign(oldUser[d.index]);
            const auto newLow = TapeAlign(newUser[d.index]);
            if (static_cast<std::int64_t>(tape.runs[r].first) != static_cast<std::int64_t>(oldLow) + d.addend) return false;
            base = static_cast<std::int64_t>(newLow) + d.addend;
            delta[r] = static_cast<std::int64_t>(newLow) - static_cast<std::int64_t>(oldLow);
        } else {
            if (d.index >= r) return false; // source run must come earlier
            const auto src = tape.runWordOffset[d.index] + static_cast<std::uint32_t>(d.dword);
            if (src >= outFlat.size()) return false;
            base = static_cast<std::int64_t>(outFlat[src]) + delta[d.index] + d.addend;
        }
        if (base <= 0) return false;
        const auto address = static_cast<std::uint64_t>(base);
        if ((address & 3u) != 0 || address + d.bytes >= (std::uint64_t{1} << 48u)) return false;
        const auto oldLow = static_cast<std::uint32_t>(tape.runs[r].first);
        if (outFlat[w0] == oldLow) outFlat[w0] = static_cast<std::uint32_t>(address);
        outRuns[r] = {address, address + d.bytes};
    }
    return true;
}

// Validates a rebound tape against current guest memory through reader:
// Structural runs must match the captured content (base-address dword
// excluded), Data runs are not compared. reader(address, out4bytes) must
// return false when the address is unmapped. Returns false on any mismatch
// or unrepresentable step (fail-closed).
template <typename Reader>
bool ValidateReboundTape(
    const VariantTape& tape,
    std::span<const std::uint32_t> oldUser,
    std::span<const std::uint32_t> newUser,
    const std::vector<std::uint32_t>& oldFlat,
    Reader&& reader) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    std::vector<std::uint32_t> flat;
    if (!ReplayVariantTape(tape, oldUser, newUser, oldFlat, runs, flat)) return false;
    std::array<std::byte, 4> current{};
    for (std::size_t r = 0; r < runs.size(); ++r) {
        if (tape.regions[r].kind != TapeField::Structural) continue;
        const auto w0 = tape.runWordOffset[r];
        const auto count = tape.regions[r].bytes / 4u;
        for (std::uint32_t k = 0; k < count; ++k) {
            if (k == 0 && flat[w0] == static_cast<std::uint32_t>(runs[r].first)) continue; // base-address field
            if (!reader(runs[r].first + k * 4u, std::span<std::byte>(current.data(), current.size()))) return false;
            std::uint32_t now = 0;
            std::memcpy(&now, current.data(), sizeof(now));
            if (now != flat[w0 + k]) return false;
        }
    }
    return true;
}

// True when user word w is a UserWord source of the tape, i.e. a candidate
// whose only changed words are covered by the tape may be rebound instead of
// recompiled.
inline bool TapeCoversUserWord(const VariantTape& tape, std::uint32_t word) {
    if (!tape.rebasable) return false;
    for (const auto& d : tape.regions) {
        if (d.source == TapeSource::UserWord && d.index == word) return true;
    }
    return false;
}

// Descriptor/push-constant patches for a rebind: every changed UserWord source
// that feeds a descriptor slot (valueSlots: {userWord, binding, word}) or a
// forwarded push-constant slot (shaderDataUserWords) gets the new value.
inline void RebindPatches(
    const VariantTape& tape,
    std::span<const std::uint32_t> oldUser,
    std::span<const std::uint32_t> newUser,
    const std::vector<std::array<std::uint32_t, 3>>& valueSlots,
    const std::vector<std::uint32_t>& shaderDataUserWords,
    std::vector<std::array<std::uint32_t, 3>>& outDescriptorPatches,
    std::vector<std::pair<std::uint32_t, std::uint32_t>>& outShaderDataPatches) {
    for (const auto& d : tape.regions) {
        if (d.source != TapeSource::UserWord || d.index >= oldUser.size() || d.index >= newUser.size()) continue;
        if (oldUser[d.index] == newUser[d.index]) continue;
        for (const auto& slot : valueSlots) {
            if (slot[0] == d.index) outDescriptorPatches.push_back({slot[1], slot[2], newUser[d.index]});
        }
        for (std::size_t s = 0; s < shaderDataUserWords.size(); ++s) {
            if (shaderDataUserWords[s] == d.index) outShaderDataPatches.emplace_back(static_cast<std::uint32_t>(s), newUser[d.index]);
        }
    }
}

} // namespace AgcDriver::DriverDetail

#endif
