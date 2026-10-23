#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
}
#if defined(JSON2_LEGACY_TEST)
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static const std::uint32_t words[]{0, 0, 0, 0, 0x05000000};
    return words;
}
#endif
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct InitializationTrace {
    std::map<void*, std::size_t> ids;
    std::vector<std::size_t> releases;
    std::size_t requests = 0;
    int budget = -1;
    bool once = false;
};
static void* APS5_VABI AllocateTracked(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<InitializationTrace*>(context);
    ++trace.requests;
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes);
    Require(trace.ids.emplace(memory, trace.requests).second, "Each allocation must have an independent live identity");
    return memory;
}
static void APS5_VABI FreeTracked(void*, void* memory, void* context) {
    auto& trace = *static_cast<InitializationTrace*>(context);
    trace.releases.push_back(memory ? trace.ids.at(memory) : 0);
    if (!memory) return;
    Require(trace.ids.erase(memory) == 1, "Cleanup must release each admitted allocation through its owner exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyTracked(void*, int error, std::size_t, void*) {
    Require(static_cast<unsigned>(error) == 0x80848102, "Initialization allocation failures must report the observed error");
}
static void* const Vtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateTracked), reinterpret_cast<void*>(FreeTracked), reinterpret_cast<void*>(NotifyTracked)};
struct InitializationAllocator { void* const* vtable = Vtable; };
struct Expected { bool once; int budget; bool admitted; std::vector<std::size_t> releases; };
static const Expected Cases[]{
#if defined(JSON2_LEGACY_TEST)
    {0, 0, false, {}},
    {0, 1, false, {0,0,0,0,0,0,1}},
    {0, 2, false, {0,0,0,2,0,0,1}},
    {0, 3, false, {0,0,0,3,2,0,0,1}},
    {0, 4, false, {0,0,0,3,2,4,0,1}},
    {0, 5, false, {0,0,0,3,2,5,4,0,1}},
    {0, 6, false, {0,0,0,3,2,6,5,4,0,1}},
    {0, 7, false, {0,0,0,3,2,6,5,4,7,1}},
    {0, 8, false, {8,0,0,0,3,2,6,5,4,7,1}},
    {0, 9, false, {0,0,0,3,2,6,5,4,9,8,7,1}},
    {0, 10, false, {0,10,0,3,2,6,5,4,9,8,7,1}},
    {0, 11, true, {0,10,11,3,2,6,5,4,9,8,7,1}},
    {0, 12, true, {0,10,11,3,2,6,5,4,9,8,7,1}},
    {1, 0, false, {}},
    {1, 1, false, {0,9,10,0,5,4,3,8,7,6,1}},
    {1, 2, true, {0,10,11,2,6,5,4,9,8,7,1}},
    {1, 3, false, {0,8,9,3,2,0,7,6,5,1}},
    {1, 4, true, {0,9,10,3,2,4,8,7,6,1}},
    {1, 5, true, {0,10,11,3,2,5,4,9,8,7,1}},
    {1, 6, false, {0,8,9,3,2,6,5,4,0,1}},
    {1, 7, true, {0,9,10,3,2,6,5,4,7,1}},
    {1, 8, true, {8,0,10,11,3,2,6,5,4,7,1}},
    {1, 9, false, {0,0,11,3,2,6,5,4,9,8,7,1}},
    {1, 10, false, {0,10,0,3,2,6,5,4,9,8,7,1}},
    {1, 11, true, {0,10,11,3,2,6,5,4,9,8,7,1}},
    {1, 12, true, {0,10,11,3,2,6,5,4,9,8,7,1}},
#else
    {0, 0, false, {}},
    {0, 1, false, {0,0,0,0,0,0,1}},
    {0, 2, false, {0,0,0,2,0,0,1}},
    {0, 3, false, {0,0,0,3,2,0,0,1}},
    {0, 4, false, {0,0,0,3,2,4,0,1}},
    {0, 5, false, {0,0,0,3,2,5,4,0,1}},
    {0, 6, false, {0,0,0,3,2,6,5,4,0,1}},
    {0, 7, false, {0,0,0,3,2,6,5,4,7,1}},
    {0, 8, false, {8,0,0,0,3,2,6,5,4,7,1}},
    {0, 9, false, {0,0,0,3,2,6,5,4,9,8,7,1}},
    {0, 10, false, {10,0,0,3,2,6,5,4,9,8,7,1}},
    {0, 11, false, {10,0,11,3,2,6,5,4,9,8,7,1}},
    {0, 12, true, {10,12,11,3,2,6,5,4,9,8,7,1}},
    {1, 0, false, {}},
    {1, 1, false, {9,0,10,0,5,4,3,8,7,6,1}},
    {1, 2, true, {10,12,11,2,6,5,4,9,8,7,1}},
    {1, 3, false, {8,0,9,3,2,0,7,6,5,1}},
    {1, 4, true, {9,11,10,3,2,4,8,7,6,1}},
    {1, 5, true, {10,12,11,3,2,5,4,9,8,7,1}},
    {1, 6, false, {8,0,9,3,2,6,5,4,0,1}},
    {1, 7, true, {9,11,10,3,2,6,5,4,7,1}},
    {1, 8, true, {8,10,12,11,3,2,6,5,4,7,1}},
    {1, 9, false, {0,0,11,3,2,6,5,4,9,8,7,1}},
    {1, 10, false, {10,0,0,3,2,6,5,4,9,8,7,1}},
    {1, 11, false, {10,0,11,3,2,6,5,4,9,8,7,1}},
    {1, 12, true, {10,12,11,3,2,6,5,4,9,8,7,1}},
#endif
};
int main() {
    for (const auto& expected : Cases) {
        InitializationTrace trace; trace.budget = expected.budget; trace.once = expected.once;
        InitializationAllocator allocator; Parameter parameter{&allocator, &trace, 16384, 2, {}};
        std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
        const int status = _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter);
        Require(status == (expected.admitted ? 0 : static_cast<int>(0x80848102)) && initializer == expected.admitted, "Failure admission must preserve the reference status and initializer active byte");
        if (expected.admitted) Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate admitted partial resources");
        Require(trace.ids.empty() && initializer == 0, "Rollback and termination must reclaim every admitted resource");
        Require(trace.releases == expected.releases, "Cleanup must preserve exact allocation-identity order including observable null frees");
        Require(static_cast<unsigned>(_ZN3sce4Json11Initializer9terminateEv(&initializer)) == 0x80848110 && trace.releases == expected.releases, "Repeated termination must report inactive state without allocator effects");
        trace.budget = -1; trace.once = false; trace.requests = 0; trace.releases.clear();
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0 && initializer == 1, "Retry must work on the same initializer after every failure schedule");
        Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && initializer == 0 && trace.ids.empty(), "Retry termination must reclaim complete resource ownership");
        Require(trace.releases == Cases[12].releases, "Retry must restore the full healthy teardown protocol");
    }
    return 0;
}
