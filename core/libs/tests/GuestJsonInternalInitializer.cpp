#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t fileBuffer; };
using InfoCallback = void (APS5_VABI*)(int, std::int32_t, void*);
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct EntryTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    std::vector<void*> frees;
    void* expectedAllocator = nullptr;
    bool reject = false;
    int budget = -1;
    unsigned notices = 0;
    std::size_t failedSize = 0;
    int notifiedState = 0;
    int firstRequestState = 0;
    int laterRequestState = 0;
    void* owner = nullptr;
    unsigned allocationReentries = 0;
    unsigned notificationReentries = 0;
    unsigned releaseReentries = 0;
};
static void CheckReentry(EntryTrace& trace) {
    std::uint8_t other[]{0, 0x19, 0x28, 0x37};
    const auto active = *reinterpret_cast<const std::uint8_t*>(trace.owner);
    constexpr auto already = static_cast<int>(0x80848111);
    constexpr auto missing = static_cast<int>(0x80848110);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(trace.owner, nullptr) == already &&
            _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(trace.owner, nullptr) == already,
            "Allocator callback reentry must reject both duplicate initialization forms before parameter validation");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(other, nullptr) == already &&
            _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(other, nullptr) == already,
            "An allocator callback must not publish a competing initializer while the module context is owned");
    Require(_ZN3sce4Json11Initializer9terminateEv(other) == missing && other[0] == 0 && other[1] == 0x19 && other[2] == 0x28 && other[3] == 0x37,
            "An inactive callback-local initializer must not terminate another initializer's resources");
    Require(*reinterpret_cast<const std::uint8_t*>(trace.owner) == active, "Rejected callback lifecycle operations must preserve the owning initializer active byte");
}
static void APS5_VABI SdkInfo(int, ValueType, void*) { Require(false, "The reference allocator-info setter must not install or invoke the supplied callback"); }
static int InfoState() { return _ZN3sce4Json11Initializer24setAllocatorInfoCallBackEPFviNS0_9ValueTypeEPvES3_(nullptr, SdkInfo, nullptr); }
static void* APS5_VABI EntryAllocate(void* self, std::size_t size, void* context) {
    auto& trace = *static_cast<EntryTrace*>(context);
    Require(self == trace.expectedAllocator, "Allocation dispatch must preserve the allocator receiver and user context");
    const int admission = InfoState();
    if (trace.requests.empty()) trace.firstRequestState = admission;
    else trace.laterRequestState = admission;
    trace.requests.push_back(size);
    if (trace.owner && admission == 0) { CheckReentry(trace); ++trace.allocationReentries; }
    if (trace.reject || trace.budget == 0) return nullptr;
    if (trace.budget > 0) --trace.budget;
    void* pointer = std::malloc(size ? size : 1);
    Require(pointer != nullptr && trace.live.emplace(pointer, size).second, "Tracking allocator must provide independently owned host storage");
    return pointer;
}
static void APS5_VABI EntryFree(void* self, void* memory, void* context) {
    auto& trace = *static_cast<EntryTrace*>(context);
    Require(self == trace.expectedAllocator, "Free dispatch must preserve the allocator receiver and user context");
    trace.frees.push_back(memory);
    if (trace.owner) { CheckReentry(trace); ++trace.releaseReentries; }
    if (memory) { Require(trace.live.erase(memory) == 1, "Every allocation must be released exactly once through its allocator"); std::free(memory); }
}
static void APS5_VABI EntryNotify(void* self, int error, std::size_t size, void* context) {
    auto& trace = *static_cast<EntryTrace*>(context);
    Require(self == trace.expectedAllocator && static_cast<unsigned>(error) == 0x80848102, "Allocation failure must notify the owning allocator with the Json2 error");
    ++trace.notices; trace.failedSize = size; trace.notifiedState = InfoState();
    if (trace.owner) { CheckReentry(trace); ++trace.notificationReentries; }
}
int main() {
    constexpr auto already = static_cast<int>(0x80848111);
    constexpr auto missing = static_cast<int>(0x80848110);
    constexpr auto invalid = static_cast<int>(0x80848120);
    constexpr auto noMemory = static_cast<int>(0x80848102);
    void* table[]{nullptr, nullptr, reinterpret_cast<void*>(&EntryAllocate), reinterpret_cast<void*>(&EntryFree), reinterpret_cast<void*>(&EntryNotify)};
    void** allocator = table;
    unsigned allocationReentries = 0, notificationReentries = 0, releaseReentries = 0;
    for (std::int32_t option : {0, 1, 2, -1}) {
        for (int budget = 0; budget <= 12; ++budget) {
            EntryTrace trace; trace.expectedAllocator = &allocator; trace.budget = budget;
            Parameter parameter{&allocator, &trace, 8192};
            std::uint8_t object[]{0xa5, 0x19, 0x28, 0x37};
            if (budget % 2) _ZN3sce4Json19InternalInitializerC1Ev(object);
            else _ZN3sce4Json19InternalInitializerC2Ev(object);
            Require(object[0] == 0 && object[1] == 0x19 && object[2] == 0x28 && object[3] == 0x37, "Internal constructors must initialize only the active byte");
            Require(_ZN3sce4Json19InternalInitializer19terminate_staticlibEv(object) == missing, "Inactive internal termination must return not-initialized");
            Require(_ZN3sce4Json19InternalInitializer20initialize_staticlibEPKNS0_13InitParameterENS1_10InitOptionE(object, nullptr, option) == invalid, "Internal initialization must reject a missing parameter");
            trace.owner = object;
            Parameter empty{};
            Require(_ZN3sce4Json19InternalInitializer20initialize_staticlibEPKNS0_13InitParameterENS1_10InitOptionE(object, &empty, option) == noMemory, "Legacy internal initialization must report a missing allocator as out-of-memory");
            const int result = _ZN3sce4Json19InternalInitializer20initialize_staticlibEPKNS0_13InitParameterENS1_10InitOptionE(object, &parameter, option);
            if (budget < 12) {
                Require(result == noMemory && object[0] == 0 && trace.live.empty(), "Every required-resource allocation failure must roll back ownership and clear the active byte");
                Require((budget == 0) == (trace.notices == 0), "The initial context failure must omit notification while resource failures notify");
                trace.budget = -1;
                Require(_ZN3sce4Json19InternalInitializer20initialize_staticlibEPKNS0_13InitParameterENS1_10InitOptionE(object, &parameter, option) == 0, "Retry must succeed without reconstructing the failed internal initializer");
            } else Require(result == 0, "Internal initialization must admit every option value after allocating all resources");
            trace.budget = -1;
            Require(object[0] == 1 && trace.live.size() == 12, "Internal initialization must own the same twelve required resources as legacy public initialization");
            String text{};
            _ZN3sce4Json6StringC1EPKc(&text, "abc");
            _ZN3sce4Json6String7reserveEm(&text, 5);
            Require(_ZNK3sce4Json6String8capacityEv(&text) == 21, "Successful initialization must replace the requested growth option with the compiled SDK policy");
            _ZN3sce4Json6StringD1Ev(&text);
            for (std::int32_t rejectedOption : {0, 1, 2, -1}) {
                std::uint8_t other = 0;
                Require(_ZN3sce4Json19InternalInitializer20initialize_staticlibEPKNS0_13InitParameterENS1_10InitOptionE(&other, nullptr, rejectedOption) == already && other == 0 && object[0] == 1, "Global-context admission must precede parameter validation without taking ownership in another initializer");
                _ZN3sce4Json6StringC1EPKc(&text, "abc");
                _ZN3sce4Json6String7reserveEm(&text, 5);
                Require(_ZNK3sce4Json6String8capacityEv(&text) == (rejectedOption == 1 ? 21 : 5), "Even a rejected internal initialization must update the string-growth option");
                _ZN3sce4Json6StringD1Ev(&text);
                Require(_ZN3sce4Json19InternalInitializer19terminate_staticlibEv(&other) == missing && trace.live.size() == 12, "An inactive initializer must not terminate another initializer's resources");
            }
            if (budget % 3 == 0) _ZN3sce4Json19InternalInitializerD1Ev(object);
            else if (budget % 3 == 1) _ZN3sce4Json19InternalInitializerD2Ev(object);
            else Require(_ZN3sce4Json19InternalInitializer19terminate_staticlibEv(object) == 0, "Explicit internal termination must succeed for its owning initializer");
            Require(object[0] == 0 && object[1] == 0x19 && object[2] == 0x28 && object[3] == 0x37 && trace.live.empty(), "Every internal teardown entry point must release all resources exactly once and preserve adjacent bytes");
            const auto frees = trace.frees.size();
            _ZN3sce4Json19InternalInitializerD1Ev(object);
            _ZN3sce4Json19InternalInitializerD2Ev(object);
            Require(trace.frees.size() == frees && InfoState() == missing, "Repeated internal destruction must not dispatch stale frees");
            allocationReentries += trace.allocationReentries;
            notificationReentries += trace.notificationReentries;
            releaseReentries += trace.releaseReentries;
        }
    }
    Require(allocationReentries && notificationReentries && releaseReentries, "Lifecycle rejection must be exercised through actual allocate, notify and free callbacks");
    return 0;
}
