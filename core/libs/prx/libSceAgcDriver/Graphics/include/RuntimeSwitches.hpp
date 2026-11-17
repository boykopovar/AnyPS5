#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RUNTIMESWITCHES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RUNTIMESWITCHES_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver::Graphics {

enum class RuntimeSwitch : std::size_t {
    NoBarrierBatch,
    NoGuardSpans,
    NoOwnedDrawInputs,
    NoPassAttachmentMemo,
    NoImportSegmentMemo,
    NoSizeOnlySurfaceBytes,
    NoSpaceReuseRecords,
    PipelineDispatchMisses,
    NoMemoPendingCollect,
    NoSharedCollectWalks,
    NoExtendedFastProofs,
    TableSnapshot,
    NoTargetMemo,
    NoTargetMemoOverlapCache,
    LeanFront,
    LeanListDirect,
    DirectDraws,
    DirectRetry,
    DirectPassTargets,
    DirectBuffers,
    DirectInputs,
    Count,
};

class SwitchSnapshot {
public:
    bool Set(RuntimeSwitch id) const {
        const auto index = static_cast<std::size_t>(id);
        return ((bits[index / 64] >> (index % 64)) & 1u) != 0;
    }

    void Assign(RuntimeSwitch id, bool set) {
        const auto index = static_cast<std::size_t>(id);
        const auto bit = std::uint64_t{1} << (index % 64);
        bits[index / 64] = set ? bits[index / 64] | bit : bits[index / 64] & ~bit;
    }

    bool operator==(const SwitchSnapshot&) const = default;

private:
    std::array<std::uint64_t, (static_cast<std::size_t>(RuntimeSwitch::Count) + 63) / 64> bits{};
};

namespace RuntimeSwitches {

const char* Name(RuntimeSwitch id);
bool Set(RuntimeSwitch id);
void Assign(RuntimeSwitch id, bool set);
std::uint64_t Generation();
SwitchSnapshot Snapshot();
std::optional<RuntimeSwitch> Find(std::string_view name);

}

class AbSchedule {
public:
    struct Config {
        double phaseSeconds = 0;
        double settleSeconds = 2;
        std::uint64_t startPresent = 0;
        std::optional<double> startSeconds;
        std::vector<RuntimeSwitch> switches;
    };

    enum class Event { None, Begin, Settled };

    struct Step {
        Event event = Event::None;
        std::uint64_t phase = 0;
        bool b = false;
    };

    static Config Parse(std::string_view ab, std::string_view start, std::string_view settle);

    explicit AbSchedule(Config config);

    Step AtPresent(std::uint64_t present, double seconds);

    const Config& Settings() const { return config; }

private:
    Config config;
    bool started = false;
    bool settled = false;
    std::uint64_t phase = 0;
    double phaseStart = 0;
    std::optional<double> firstSeconds;
};

bool AbConfigured();
std::vector<std::string> AbAtPresent(std::uint64_t present, double seconds);

}

#endif
