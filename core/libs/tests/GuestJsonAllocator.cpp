#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct AllocationTrace {
    int budget = -1;
    bool failOnce = false;
    Parameter* changeInput = nullptr;
    void* replacementContext = nullptr;
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    std::vector<std::size_t> notifications;
};
static void* APS5_VABI AllocateTracked(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<AllocationTrace*>(context);
    trace.requests.push_back(bytes);
    if (trace.changeInput && trace.requests.size() == 1) {
        trace.changeInput->context = trace.replacementContext;
        trace.changeInput->bufferSize = 32768;
    }
    if (trace.budget == 0) {
        if (trace.failOnce) trace.budget = -1;
        return nullptr;
    }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes ? bytes : 1);
    Require(memory != nullptr, "The tracking allocator must have host memory");
    std::memset(memory, 0xa5, bytes);
    trace.live.emplace(memory, bytes);
    return memory;
}
static void APS5_VABI FreeTracked(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<AllocationTrace*>(context);
    Require(trace.live.erase(memory) == 1, "Free must use the allocator and context that own the resource");
    std::free(memory);
}
static void APS5_VABI NotifyTracked(void*, int error, std::size_t bytes, void* context) {
    Require(static_cast<unsigned>(error) == 0x80848102, "Allocation notification must carry the Json2 out-of-memory error");
    auto& trace = *static_cast<AllocationTrace*>(context);
    Require(!trace.requests.empty() && trace.requests.back() == bytes, "Notification size must match the failed allocation request");
    trace.notifications.push_back(bytes);
}
static void* const Vtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateTracked), reinterpret_cast<void*>(FreeTracked), reinterpret_cast<void*>(NotifyTracked)};
struct TrackingAllocator { void* const* vtable = Vtable; };
static bool Owned(const AllocationTrace& trace, const void* memory, std::size_t bytes) {
    const auto found = trace.live.find(const_cast<void*>(memory));
    return found != trace.live.end() && found->second == bytes;
}
static void CheckResources(AllocationTrace& trace) {
    Require(trace.live.size() == 12, "SDK 6 initialization must own its actual twelve allocated resources");
    const std::vector<std::size_t> requests{24, 8, 24, 8, 32, 48, 8, 32, 104, 32, 8, 8};
    Require(trace.requests.size() >= requests.size() && std::equal(requests.begin(), requests.end(), trace.requests.end() - requests.size()),
            "Initialization must allocate its actual context, defaults, fallback and synchronization handles in reference order");
    bool contextFound = false;
    for (const auto& [memory, bytes] : trace.live) {
        if (bytes != 24) continue;
        std::uintptr_t words[3]; std::memcpy(words, memory, sizeof(words));
        if (words[1] == reinterpret_cast<std::uintptr_t>(&trace) && words[2] == 16384) contextFound = true;
    }
    Require(contextFound, "Initialization must retain the supplied context and buffer setting in its owned context record");
    Value null;
    _ZN3sce4Json5ValueC1Ev(&null);
    const auto* string = _ZNK3sce4Json5Value9getStringEv(&null);
    const auto* array = _ZNK3sce4Json5Value8getArrayEv(&null);
    const auto* object = _ZNK3sce4Json5Value9getObjectEv(&null);
    Require(Owned(trace, string, 8) && Owned(trace, string->text, 24), "The default string must use the configured allocator");
    Require(Owned(trace, array, 8) && Owned(trace, array->items, 32) && Owned(trace, *reinterpret_cast<void**>(array->items), 48),
            "The default array must own its allocator-backed header and sentinel");
    Require(Owned(trace, object, 8) && Owned(trace, object->items, 32) && Owned(trace, *reinterpret_cast<void**>(object->items), 104),
            "The default object must own its allocator-backed header and tree sentinel");
    Require(Owned(trace, _ZNK3sce4Json5ValueixEPKc(&null, "missing"), 32), "Missing access must return the allocated module fallback");
    _ZN3sce4Json5ValueD1Ev(&null);
    String text{};
    _ZN3sce4Json6StringC1EPKc(&text, "abc");
    Require(Owned(trace, text.text, 24) && Owned(trace, *reinterpret_cast<void**>(text.text), 4) &&
                _ZNK3sce4Json6String4sizeEv(&text) == 3 && _ZNK3sce4Json6String8capacityEv(&text) == 4,
            "String construction must allocate the native header and exact initial character capacity");
    _ZN3sce4Json6String7reserveEm(&text, 17);
    Require(Owned(trace, *reinterpret_cast<void**>(text.text), 17) && _ZNK3sce4Json6String8capacityEv(&text) == 17,
            "Reserve must apply the SDK-dependent buffer growth rule");
    _ZN3sce4Json6String6resizeEm(&text, 18);
    const char* data = _ZNK3sce4Json6String5c_strEv(&text);
    Require(Owned(trace, data, 25) && _ZNK3sce4Json6String4sizeEv(&text) == 18 && std::memcmp(data, "abc", 3) == 0,
            "Resize must retain the prefix and allocate the configured character capacity");
    for (unsigned index = 3; index <= 18; ++index)
        Require(data[index] == 0, "Growing a string must zero the added bytes and terminator");
    _ZN3sce4Json6String5clearEv(&text);
    Require(Owned(trace, text.text, 24) && _ZNK3sce4Json6String4sizeEv(&text) == 0 && _ZNK3sce4Json6String8capacityEv(&text) == 0,
            "Clear must release characters while retaining the string header");
    _ZN3sce4Json6StringD1Ev(&text);
    Require(trace.live.size() == 12, "Temporary string operations must leave only initialization resources alive");
}
static void CheckSingleFailures() {
    for (int point = 0; point < 12; ++point) {
        AllocationTrace trace;
        trace.budget = point;
        trace.failOnce = true;
        TrackingAllocator allocator;
        Parameter parameter{&allocator, &trace, 16384, 2, {}};
        std::uint8_t initializer = 0;
        _ZN3sce4Json11InitializerC1Ev(&initializer);
        const int result = _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter));
        const bool optional = point == 2 || point == 4 || point == 5 || point == 7 || point == 8;
        Require(result == (optional ? 0 : static_cast<int>(0x80848102)) && initializer == (optional ? 1 : 0),
                "Single inner-default allocation failures must retain the observed admission result");
        Require(trace.notifications.size() == (point == 0 ? 0 : 1), "Each failed request must produce its observed notification count");
        if (optional) {
            Value null;
            _ZN3sce4Json5ValueC1Ev(&null);
            if (point == 2) {
                const auto* string = _ZNK3sce4Json5Value9getStringEv(&null);
                Require(Owned(trace, string, 8) && !string->text && _ZNK3sce4Json6String4sizeEv(string) == 0 &&
                            _ZNK3sce4Json6String8capacityEv(string) == 0 && _ZNK3sce4Json6String5c_strEv(string)[0] == 0,
                        "A failed default string header must retain its usable empty outer handle");
            } else if (point == 4 || point == 5) {
                const auto* array = _ZNK3sce4Json5Value8getArrayEv(&null);
                Require(Owned(trace, array, 8) && _ZNK3sce4Json5Array4sizeEv(array) == 0,
                        "A failed default array resource must retain an empty outer handle");
                Require(point == 4 ? !array->items : Owned(trace, array->items, 32),
                        "Array admission must preserve the observed partial header ownership");
            } else {
                const auto* object = _ZNK3sce4Json5Value9getObjectEv(&null);
                Require(Owned(trace, object, 8) && !object->items && _ZNK3sce4Json6Object4sizeEv(object) == 0,
                        "A failed default object resource must retain its empty outer handle and reclaim partial tree storage");
            }
            _ZN3sce4Json5ValueD1Ev(&null);
            Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate partial-default initialization");
        }
        Require(trace.live.empty(), "Both admitted partial defaults and refused initialization must release all allocated resources");
        trace.budget = -1;
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0,
                "A subsequent full initialization must work after every single failure schedule");
        CheckResources(trace);
        Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Release resources after retry");
    }
}

static void CheckContextSnapshot() {
    AllocationTrace trace, replacement;
    TrackingAllocator allocator;
    Parameter parameter{&allocator, &trace, 16384, 2, {}};
    trace.changeInput = &parameter;
    trace.replacementContext = &replacement;
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize with a reconfigured input during context allocation");
    bool found = false;
    for (const auto& [memory, bytes] : trace.live) {
        if (bytes != 24) continue;
        std::uintptr_t words[3]; std::memcpy(words, memory, sizeof(words));
        if (words[0] == reinterpret_cast<std::uintptr_t>(&allocator) && words[1] == reinterpret_cast<std::uintptr_t>(&trace) && words[2] == 32768) found = true;
    }
    Require(found && trace.live.size() == 12 && replacement.requests.empty(), "Context admission must cache allocator and user data before allocation and read the buffer setting afterward");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty() && replacement.live.empty(), "Termination must release the admitted context snapshot");
}
int main() {
    for (int budget = 0; budget <= 12; ++budget) {
        AllocationTrace trace;
        trace.budget = budget;
        TrackingAllocator allocator;
        Parameter parameter{&allocator, &trace, 16384, 2, {}};
        std::uint8_t initializer = 0;
        _ZN3sce4Json11InitializerC1Ev(&initializer);
        const int result = _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter));
        if (budget < 12) {
            Require(static_cast<unsigned>(result) == 0x80848102 && initializer == 0 && trace.live.empty(),
                    "Every initialization allocation failure must report OOM, roll back ownership and leave retry legal");
            Require((budget == 0) == trace.notifications.empty(), "Only the first context-allocation failure omits notification");
            trace.budget = -1;
            Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0,
                    "Initialization must succeed after failure rollback without reconstructing the initializer");
        } else {
            Require(result == 0 && initializer == 1, "Initialization must succeed after its final resource allocation");
            trace.budget = -1;
        }
        CheckResources(trace);
        AllocationTrace unrelated;
        parameter.context = &unrelated;
        Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && initializer == 0 && trace.live.empty() && unrelated.live.empty(),
                "Termination must release the snapshotted allocation context and every resource");
    }
    CheckSingleFailures();
    CheckContextSnapshot();
    return 0;
}
