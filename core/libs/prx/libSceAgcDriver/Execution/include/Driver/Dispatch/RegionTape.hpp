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
    std::vector<std::pair<std::uint32_t, std::uint32_t>> baseFields; // {flat word index, owning run}: dwords that hold a chained run's base
    std::vector<std::uint32_t> oldUserData;                    // user words at capture time
    std::vector<std::uint32_t> oldWords;                       // flattened captured words at capture time
    bool rebasable = false;
};

inline std::uint64_t TapeAlign(std::uint32_t word) { return word & ~std::uint32_t{3}; }

// Builds the tape from the captured variant state. runBases[r] >= 0 means the
// SRT walk derived run r's base from user word runBases[r] (see
// tagVariantRuns); otherwise the base dword must be found verbatim in exactly
// one earlier run (chained pointer). A run that is the source of a chained
// derivation holds descriptors/tables, so it is Structural and must be
// compared byte-for-byte on rebind; leaf runs are Data and are live-filled.
// Anything unrepresentable -> rebasable = false (fail-closed).
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
            // chained pointer: the run's base dword must appear verbatim in exactly
            // one earlier run. A repeated value leaves the owning run ambiguous, so
            // the delta would be a guess: fail closed instead.
            const auto baseLow = static_cast<std::uint32_t>(begin);
            std::size_t source = runs.size();
            std::int32_t dword = -1;
            std::size_t matches = 0;
            for (std::size_t s = 0; s < r; ++s) {
                const auto count = static_cast<std::uint32_t>((runs[s].second - runs[s].first) / 4u);
                for (std::uint32_t k = 0; k < count; ++k) {
                    const auto at = tape.runWordOffset[s] + k;
                    if (at < flatWords.size() && flatWords[at] == baseLow) {
                        if (matches++ == 0) {
                            source = s;
                            dword = static_cast<std::int32_t>(k);
                        }
                    }
                }
            }
            if (matches != 1) {
                tape.rebasable = false;
                break;
            }
            d.source = TapeSource::RegionDword;
            d.index = static_cast<std::uint32_t>(source);
            d.dword = dword;
            d.addend = 0;
            d.kind = TapeField::Data;
        }
        tape.runWordOffset.push_back(offset);
        tape.regions.push_back(d);
        offset += static_cast<std::uint32_t>(bytes / 4u);
    }
    if (tape.rebasable && offset != flatWords.size()) tape.rebasable = false;
    if (tape.regions.size() != tape.runs.size()) tape.rebasable = false;
    if (tape.rebasable) {
        // A run that some later run dereferences is a descriptor/table: its content
        // (format, stride, sizes, ...) decides the codegen and must match, only the
        // base-address dwords may move. Leaf runs are loaded data and are refilled.
        for (const auto& d : tape.regions) {
            if (d.source != TapeSource::RegionDword) continue;
            tape.regions[d.index].kind = TapeField::Structural;
            tape.baseFields.emplace_back(tape.runWordOffset[d.index] + static_cast<std::uint32_t>(d.dword), d.index);
        }
    }
    tape.oldUserData = userData;
    tape.oldWords = flatWords;
    return tape;
}

// Replays the tape with new user words: produces the rebound runs and a copy of
// the captured words whose base-address dwords are patched. The baseline is the
// tape's own snapshot, never the candidate's current words, so a cached rebound
// copy stays replayable (its words were live-filled at another address).
// Returns false on any step that is not representable (fail-closed).
inline bool ReplayVariantTape(
    const VariantTape& tape,
    std::span<const std::uint32_t> newUser,
    std::vector<std::pair<std::uint64_t, std::uint64_t>>& outRuns,
    std::vector<std::uint32_t>& outFlat) {
    if (!tape.rebasable || tape.regions.size() != tape.runs.size() || tape.runWordOffset.size() != tape.runs.size()) return false;
    outFlat = tape.oldWords;
    outRuns.assign(tape.runs.size(), {0, 0});
    std::vector<std::int64_t> delta(tape.runs.size(), 0);
    for (std::size_t r = 0; r < tape.runs.size(); ++r) {
        const auto& d = tape.regions[r];
        const auto w0 = tape.runWordOffset[r];
        const auto count = d.bytes / 4u;
        if (static_cast<std::size_t>(w0) + count > outFlat.size()) return false;
        std::int64_t base = -1;
        if (d.source == TapeSource::UserWord) {
            if (d.index >= tape.oldUserData.size() || d.index >= newUser.size()) return false;
            const auto oldLow = TapeAlign(tape.oldUserData[d.index]);
            const auto newLow = TapeAlign(newUser[d.index]);
            if (static_cast<std::int64_t>(tape.runs[r].first) != static_cast<std::int64_t>(oldLow) + d.addend) return false;
            base = static_cast<std::int64_t>(newLow) + d.addend;
            delta[r] = static_cast<std::int64_t>(newLow) - static_cast<std::int64_t>(oldLow);
        } else {
            if (d.index >= r) return false; // source run must come earlier
            const auto src = tape.runWordOffset[d.index] + static_cast<std::uint32_t>(d.dword);
            if (src >= tape.oldWords.size()) return false;
            // The base dword moves with the run that holds it, so use the snapshot
            // (never the already-patched output: that would count the delta twice).
            base = static_cast<std::int64_t>(tape.oldWords[src]) + delta[d.index] + d.addend;
            delta[r] = base - static_cast<std::int64_t>(tape.runs[r].first);
        }
        if (base <= 0) return false;
        const auto address = static_cast<std::uint64_t>(base);
        if ((address & 3u) != 0 || address + d.bytes >= (std::uint64_t{1} << 48u)) return false;
        outRuns[r] = {address, address + d.bytes};
    }
    for (const auto& [at, owner] : tape.baseFields) {
        if (at >= outFlat.size() || owner >= delta.size()) return false;
        const auto moved = static_cast<std::int64_t>(tape.oldWords[at]) + delta[owner];
        if (moved <= 0 || moved >= static_cast<std::int64_t>(std::uint64_t{1} << 48u)) return false;
        outFlat[at] = static_cast<std::uint32_t>(moved);
    }
    // Auto-referential base dword (the run's first word holds its own base).
    for (std::size_t r = 0; r < tape.runs.size(); ++r) {
        const auto w0 = tape.runWordOffset[r];
        if (w0 >= outFlat.size()) return false;
        if (outFlat[w0] == static_cast<std::uint32_t>(tape.runs[r].first) && tape.oldWords[w0] == static_cast<std::uint32_t>(tape.runs[r].first)) {
            const bool isBaseField = std::any_of(tape.baseFields.begin(), tape.baseFields.end(), [&](const auto& field) { return field.first == w0; });
            if (!isBaseField) outFlat[w0] = static_cast<std::uint32_t>(outRuns[r].first);
        }
    }
    return true;
}

// Validates a rebound tape against current guest memory through reader:
// Structural runs must match the captured content (base-address dwords excluded
// and patched), Data runs are not compared. reader(address, out4bytes) must
// return false when the address is unmapped. Returns false on any mismatch or
// unrepresentable step (fail-closed).
template <typename Reader>
bool ValidateReboundTape(
    const VariantTape& tape,
    std::span<const std::uint32_t> newUser,
    Reader&& reader) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    std::vector<std::uint32_t> flat;
    if (!ReplayVariantTape(tape, newUser, runs, flat)) return false;
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

// Shape-key eligibility of a data-hit candidate: which changed user words are
// allowed to differ. Eligible: no changed word (plain data hit, content lives at
// the same addresses). CoveredByTape: every changed word is a UserWord source of
// the tape, so the shape entry can be reused after a rebind. UncoveredWord: at
// least one changed word is neither opaque to the draw key (mask bit 0) nor
// covered by the tape, so the draw must use its own exact entry (fail-closed to
// the current path).
enum class RebindEligibility : std::uint8_t { Eligible, CoveredByTape, UncoveredWord };

inline RebindEligibility RebindEligibilityOf(const VariantTape* tape, std::span<const std::uint32_t> was, std::span<const std::uint32_t> now, std::uint64_t userDataMask) {
    bool covered = false;
    const auto count = std::min(was.size(), now.size());
    for (std::size_t w = 0; w < count; ++w) {
        if (now[w] == was[w] || (w < 64u && ((userDataMask >> w) & 1u) == 0)) continue;
        if (tape != nullptr && TapeCoversUserWord(*tape, static_cast<std::uint32_t>(w))) {
            covered = true;
            continue;
        }
        return RebindEligibility::UncoveredWord;
    }
    return covered ? RebindEligibility::CoveredByTape : RebindEligibility::Eligible;
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
