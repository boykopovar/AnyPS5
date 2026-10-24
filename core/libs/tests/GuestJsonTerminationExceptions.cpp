#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
}
#if defined(JSON2_TERMINATION_LEGACY_SDK)
static constexpr bool Legacy = true;
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static const std::uint32_t words[]{0, 0, 0, 0, 0x05000000};
    return words;
}
#else
static constexpr bool Legacy = false;
#endif
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct TerminationMarker { const void* identity; };
struct TerminationTrace {
    void* allocator = nullptr;
    unsigned target = 0;
    bool after = false;
    std::vector<void*> allocations;
    std::map<void*, unsigned> live;
    std::vector<unsigned> releases;
};
static void* APS5_VABI AllocateTerminationTracked(void* self, std::size_t size, void* data) {
    auto& trace = *static_cast<TerminationTrace*>(data);
    Require(self == trace.allocator, "Initialization must preserve the allocator receiver and context");
    void* memory = std::malloc(size);
    Require(memory && trace.live.emplace(memory, trace.allocations.size() + 1).second, "Each initialization resource must have an independent live identity");
    std::memset(memory, 0xa5, size);
    trace.allocations.push_back(memory);
    return memory;
}
static void APS5_VABI ReleaseTerminationTracked(void* self, void* memory, void* data) {
    auto& trace = *static_cast<TerminationTrace*>(data);
    Require(self == trace.allocator && (!memory || trace.live.contains(memory)), "Teardown must preserve allocator identity and release only live resources");
    trace.releases.push_back(memory ? trace.live.at(memory) : 0);
    if (trace.releases.size() == trace.target && !trace.after) throw TerminationMarker{&trace};
    if (memory) { trace.live.erase(memory); std::free(memory); }
    if (trace.releases.size() == trace.target && trace.after) throw TerminationMarker{&trace};
}
static void APS5_VABI NotifyTerminationTracked(void*, int, std::size_t, void*) { Require(false, "Release exceptions must not fabricate allocation errors"); }
static void* const Table[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateTerminationTracked), reinterpret_cast<void*>(ReleaseTerminationTracked), reinterpret_cast<void*>(NotifyTerminationTracked)};
struct TerminationAllocator { void* const* table = Table; };
int main(int argc, char** argv) {
    Require(argc == 3 || argc == 4, "Every interrupted teardown requires an independent module process");
    TerminationAllocator allocator; TerminationTrace trace; trace.allocator = &allocator;
    trace.target = static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)); trace.after = std::strcmp(argv[2], "after") == 0;
    Require(trace.target <= 12 && (trace.after || std::strcmp(argv[2], "before") == 0), "The schedule must identify a release boundary");
    Parameter parameter{&allocator, &trace, 16384, 0, {}};
    std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, &parameter) == 0 && owner && trace.live.size() == (Legacy ? 11u : 12u), "Initialize complete module ownership before release failure");
    const auto baseline = trace.live;
    bool caught = false;
    try { Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0, "Uninterrupted teardown must complete"); }
    catch (const TerminationMarker& marker) { Require(marker.identity == &trace, "Free callback exceptions must preserve their identity"); caught = true; }
    const std::array<unsigned, 12> order = Legacy ? std::array<unsigned, 12>{0, 10, 11, 3, 2, 6, 5, 4, 9, 8, 7, 1}
                                                 : std::array<unsigned, 12>{10, 12, 11, 3, 2, 6, 5, 4, 9, 8, 7, 1};
    const unsigned calls = trace.target ? trace.target : 12;
    Require(caught == bool(trace.target) && owner == unsigned(caught) && trace.releases == std::vector<unsigned>(order.begin(), order.begin() + calls),
            "Interrupted teardown must retain the active owner and exact release-generation prefix");
    auto expected = baseline;
    const unsigned completed = caught ? calls - unsigned(!trace.after) : calls;
    for (unsigned i = 0; i < completed; ++i) if (order[i]) expected.erase(trace.allocations[order[i] - 1]);
    Require(trace.live == expected, "Only frees completed before the exception may change resource ownership");
    const auto releases = trace.releases;
    std::uint8_t other = 0; _ZN3sce4Json11InitializerC1Ev(&other);
    if (caught) {
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, &parameter) == static_cast<int>(0x80848111) &&
                _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&other, &parameter) == static_cast<int>(0x80848111) &&
                !other && trace.live == expected && trace.releases == releases,
                "Both owners must reject initialization after incomplete teardown without further allocator effects");
        if (argc == 4 && (std::strcmp(argv[3], "refuse-mutex") == 0 || std::strcmp(argv[3], "refuse-header") == 0)) {
            if (std::strcmp(argv[3], "refuse-header") == 0) Require(trace.target == 4 || trace.target == 7 || trace.target == 10, "Header refusal requires completed child destruction");
            else Require((Legacy && trace.target == 3) || (!Legacy && (trace.target == 2 || trace.target == 3)), "Refusal probes require a destroyed mutex slot");
            trace.target = 0;
            for (unsigned attempt = 0; attempt < 2; ++attempt) {
                bool refused = false;
                try { _ZN3sce4Json11Initializer9terminateEv(&owner); }
                catch (const std::runtime_error& error) {
                    refused = std::string(error.what()).find("_ZN3sce4Json11Initializer9terminateEv") != std::string::npos;
                }
                Require(refused && owner && trace.live == expected && trace.releases == releases,
                        "Retry must explicitly refuse retired ownership before kernel use or further release effects");
            }
            trace.target = calls;
        } else if (argc == 4) {
            Require(std::strcmp(argv[3], "retry") == 0, "The additional schedule must request a returning reference retry");
            std::vector<unsigned> tail;
            switch (trace.target) {
                case 1: Require(!trace.after || Legacy, "Only the legacy null-free after-throw retry returns"); tail.assign(order.begin(), order.end()); break;
                case 2: Require(Legacy && !trace.after, "Only the legacy fallback before-throw retry returns"); tail.assign(order.begin(), order.end()); break;
                case 5: tail = {0, 0, 0, 2, 6, 5, 4, 9, 8, 7, 1}; break;
                case 6: tail = {0, 0, 0, 0, 6, 5, 4, 9, 8, 7, 1}; break;
                case 8: tail = {0, 0, 0, 0, 4, 9, 8, 7, 1}; break;
                case 9: tail = {0, 0, 0, 0, 0, 9, 8, 7, 1}; break;
                case 11: tail = {0, 0, 0, 0, 0, 7, 1}; break;
                case 12: tail = {0, 0, 0, 0, 0, 0, 1}; break;
                default: Require(false, "This retry has no returning reference contract");
            }
            Require(!trace.after || (Legacy && trace.target == 1), "Returning resource retries must precede the rejected release");
            auto expectedRetry = releases; expectedRetry.insert(expectedRetry.end(), tail.begin(), tail.end());
            trace.target = 0;
            Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.live.empty() && trace.releases == expectedRetry,
                    "Returning teardown retry must reclaim the retained resources in exact reference order including null frees");
            Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == static_cast<int>(0x80848110) && trace.releases == expectedRetry,
                    "Completed retry must not repeat any release");
            Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&other, &parameter) == 0 && other,
                    "Completed retry must permit a fresh owner generation");
            Require(_ZN3sce4Json11Initializer9terminateEv(&other) == 0 && !other && trace.live.empty(), "Fresh ownership must retain complete cleanup after interrupted retry");
            return 0;
        }
        if (!Legacy && trace.target < 2) Require(scePthreadMutexDestroy(static_cast<PthreadMutex*>(trace.allocations[11])) == 0, "The fixture must separately reclaim the untouched recovery mutex");
        if (trace.target < 3) Require(scePthreadMutexDestroy(static_cast<PthreadMutex*>(trace.allocations[10])) == 0, "The fixture must separately reclaim the untouched internal mutex");
        for (const auto& [pointer, generation] : trace.live) std::free(pointer);
        trace.live.clear();
    } else {
        Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == static_cast<int>(0x80848110) && trace.releases == releases, "Completed teardown must remain inactive without repeated frees");
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&other, &parameter) == 0 && other, "Completed teardown must permit a new module generation");
        Require(_ZN3sce4Json11Initializer9terminateEv(&other) == 0 && !other && trace.live.empty(), "A fresh generation must retain complete cleanup");
    }
    return 0;
}
