#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

extern "C" {
int APS5_VABI malloc_stats_fast_nid_postfix(void*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int statsResult = 0x2a;

alignas(64) std::array<std::byte, 256> storage{};
unsigned statsCalls = 0;
void* lastStats = nullptr;
bool statsAllocate = false;

void* APS5_VABI allocate(std::size_t) { return storage.data(); }
void APS5_VABI release(void*) {}
void* APS5_VABI reallocate(void*, std::size_t) { return storage.data(); }
void* APS5_VABI allocateZeroed(std::size_t, std::size_t) { return storage.data(); }
void* APS5_VABI align(std::size_t, std::size_t) { return storage.data(); }
void* APS5_VABI realign(void*, std::size_t, std::size_t) { return storage.data(); }

int APS5_VABI posixAlign(void** pointer, std::size_t, std::size_t) {
    *pointer = storage.data();
    return 0;
}

int APS5_VABI statsFast(void* stats) {
    ++statsCalls;
    lastStats = stats;
    if (statsAllocate) ApplicationHeapAllocate_nid_no_patch(16);
    return statsResult;
}

template<typename TValue, std::size_t TSize>
void Write(std::array<std::byte, TSize>& data, std::size_t offset, TValue value) {
    Require(offset <= data.size() && sizeof(value) <= data.size() - offset, "metadata write stays inside its table");
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

struct HeapMetadata {
    std::array<std::byte, 0x40> process{};
    std::array<std::byte, 0x38> libc{};
    std::array<std::byte, 0x78> replacement{};

    HeapMetadata() {
        Write(process, 0, std::uint64_t{0x40});
        Write(process, 8, std::uint32_t{0x4942524f});
        Write(process, 0x38, libc.data());
        Write(libc, 0, std::uint64_t{0x38});
        Write(libc, 0x30, replacement.data());
        Write(replacement, 0, std::uint64_t{0x78});
        Write(replacement, 8, std::uint64_t{2});
        Write(replacement, 0x20, &allocate);
        Write(replacement, 0x28, &release);
        Write(replacement, 0x30, &allocateZeroed);
        Write(replacement, 0x38, &reallocate);
        Write(replacement, 0x40, &align);
        Write(replacement, 0x48, &realign);
        Write(replacement, 0x50, &posixAlign);
        Write(replacement, 0x60, &statsFast);
    }
};

HeapMetadata& Metadata() {
    static HeapMetadata metadata;
    return metadata;
}

template<typename TOperation>
void RequireRejected(const TOperation& operation, std::string_view message) {
    try {
        operation();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception&) {
        return;
    }
    Testing::Fail(std::string(message) + ": did not throw");
}

void RequireMode(std::string_view mode) {
    const auto& arguments = Testing::Arguments();
    const std::string_view selected = arguments.empty() ? std::string_view{} : std::string_view(arguments.front());
    if (selected != mode) {
        Testing::Skip("needs its own process, selected by the argument '" + std::string(mode) + "'");
    }
}

void RequireUninitializedStatsRejects() {
    std::array<std::byte, 0x40> stats{};
    RequireRejected([&] { malloc_stats_fast_nid_postfix(stats.data()); }, "stats before initialization");
}

void InitializeReplacementHeapOnce() {
    static bool initialized = false;
    if (initialized) return;
    RequireUninitializedStatsRejects();
    ApplicationHeapInitialize_nid_no_patch(Metadata().process.data());
    initialized = true;
}

class ReplacementHeap {
public:
    ReplacementHeap() {
        RequireMode("");
        InitializeReplacementHeapOnce();
        statsAllocate = false;
    }

    ~ReplacementHeap() {
        statsAllocate = false;
    }

    ReplacementHeap(const ReplacementHeap&) = delete;
    ReplacementHeap& operator=(const ReplacementHeap&) = delete;
};

const Case forwards{"MallocStatsFast_ReplacementCallback_ForwardsPointerAndResult", [] {
    const ReplacementHeap heap;
    std::array<std::byte, 0x40> stats{};
    const unsigned callsBefore = statsCalls;
    RequireEqual(malloc_stats_fast_nid_postfix(stats.data()), statsResult, "stats result");
    RequireEqual(statsCalls, callsBefore + 1, "callback calls");
    Require(lastStats == stats.data(), "callback receives the stats buffer");
    RequireEqual(malloc_stats_fast_nid_postfix(nullptr), statsResult, "stats result for a null buffer");
    RequireEqual(statsCalls, callsBefore + 2, "callback calls for a null buffer");
    Require(lastStats == nullptr, "callback receives the null buffer");
}};

const Case reentrant{"MallocStatsFast_CallbackAllocates_ThrowsAndRecovers", [] {
    const ReplacementHeap heap;
    std::array<std::byte, 0x40> stats{};
    const unsigned callsBefore = statsCalls;
    statsAllocate = true;
    RequireRejected([&] { malloc_stats_fast_nid_postfix(stats.data()); }, "allocation inside the callback");
    statsAllocate = false;
    RequireEqual(statsCalls, callsBefore + 1, "callback calls after the reentrant failure");
    RequireEqual(malloc_stats_fast_nid_postfix(stats.data()), statsResult, "stats after the reentrant failure");
    RequireEqual(statsCalls, callsBefore + 2, "callback calls after recovery");
    Require(ApplicationHeapAllocate_nid_no_patch(16) == storage.data(), "allocation after recovery");
}};

const Case defaultHeap{"MallocStatsFast_DefaultHeap_Throws", [] {
    RequireMode("default");
    RequireUninitializedStatsRejects();
    auto& metadata = Metadata();
    std::memset(metadata.replacement.data() + 0x20, 0, 10 * sizeof(void*));
    ApplicationHeapInitialize_nid_no_patch(metadata.process.data());
    Require(ApplicationHeapAllocate_nid_no_patch(16) != nullptr, "default heap allocation");
    std::array<std::byte, 0x40> stats{};
    RequireRejected([&] { malloc_stats_fast_nid_postfix(stats.data()); }, "stats on the default heap");
}};

const Case missingCallback{"MallocStatsFast_ReplacementWithoutStatsCallback_Throws", [] {
    RequireMode("missing");
    RequireUninitializedStatsRejects();
    auto& metadata = Metadata();
    Write(metadata.replacement, 0x60, static_cast<void*>(nullptr));
    ApplicationHeapInitialize_nid_no_patch(metadata.process.data());
    Require(ApplicationHeapAllocate_nid_no_patch(16) == storage.data(), "replacement heap allocation");
    std::array<std::byte, 0x40> stats{};
    RequireRejected([&] { malloc_stats_fast_nid_postfix(stats.data()); }, "stats without a callback");
    RequireEqual(statsCalls, 0u, "stats callback calls");
}};

} // namespace
