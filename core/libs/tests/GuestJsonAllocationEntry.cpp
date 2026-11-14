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
    unsigned notices = 0;
    std::size_t failedSize = 0;
    int notifiedState = 0;
    int firstRequestState = 0;
    int laterRequestState = 0;
};
static void APS5_VABI SdkInfo(int, ValueType, void*) { Require(false, "The reference allocator-info setter must not install or invoke the supplied callback"); }
static int InfoState() { return _ZN3sce4Json11Initializer24setAllocatorInfoCallBackEPFviNS0_9ValueTypeEPvES3_(nullptr, SdkInfo, nullptr); }
static void* APS5_VABI EntryAllocate(void* self, std::size_t size, void* context) {
    auto& trace = *static_cast<EntryTrace*>(context);
    Require(self == trace.expectedAllocator, "Allocation dispatch must preserve the allocator receiver and user context");
    if (trace.requests.empty()) trace.firstRequestState = InfoState();
    else trace.laterRequestState = InfoState();
    trace.requests.push_back(size);
    if (trace.reject) return nullptr;
    void* pointer = std::malloc(size ? size : 1);
    Require(pointer != nullptr && trace.live.emplace(pointer, size).second, "Tracking allocator must provide independently owned host storage");
    return pointer;
}
static void APS5_VABI EntryFree(void* self, void* memory, void* context) {
    auto& trace = *static_cast<EntryTrace*>(context);
    Require(self == trace.expectedAllocator, "Free dispatch must preserve the allocator receiver and user context");
    trace.frees.push_back(memory);
    if (memory) { Require(trace.live.erase(memory) == 1, "Every allocation must be released exactly once through its allocator"); std::free(memory); }
}
static void APS5_VABI EntryNotify(void* self, int error, std::size_t size, void* context) {
    auto& trace = *static_cast<EntryTrace*>(context);
    Require(self == trace.expectedAllocator && static_cast<unsigned>(error) == 0x80848102, "Allocation failure must notify the owning allocator with the Json2 error");
    ++trace.notices; trace.failedSize = size; trace.notifiedState = InfoState();
}
int main() {
    std::uint64_t allocatorBytes[]{UINT64_C(0x1122334455667788), UINT64_C(0xaabbccddeeff0099)};
    const auto first = allocatorBytes[0], second = allocatorBytes[1];
    _ZN3sce4Json12MemAllocatorD2Ev(allocatorBytes);
    Require(allocatorBytes[0] == first && allocatorBytes[1] == second, "Base allocator destruction must not rewrite the vtable or adjacent bytes");
    _ZN3sce4Json12MemAllocatorD1Ev(allocatorBytes);
    Require(allocatorBytes[0] == first && allocatorBytes[1] == second, "Both non-deleting allocator destructors must leave the complete object unchanged");
    constexpr int notInitialized = static_cast<int>(0x80848110);
    Require(InfoState() == notInitialized && _ZN3sce4Json6MallocEm(17) == nullptr, "Allocation entry points must expose missing context before initialization");
    _ZN3sce4Json4FreeEPv(nullptr);
    void* table[]{nullptr, nullptr, reinterpret_cast<void*>(&EntryAllocate), reinterpret_cast<void*>(&EntryFree), reinterpret_cast<void*>(&EntryNotify)};
    void** allocator = table;
    EntryTrace trace; trace.expectedAllocator = &allocator;
    Parameter parameter{&allocator, &trace, 8192};
    std::uint8_t initializer = 0xa5;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&initializer, &parameter) == 0, "Entry-point tests require successful production initialization");
    Require(trace.firstRequestState == notInitialized && trace.laterRequestState == 0 && InfoState() == 0, "Allocator-info admission must follow context publication, including during resource initialization");
    std::uint8_t unrelated = 0;
    Require(_ZN3sce4Json11Initializer24setAllocatorInfoCallBackEPFviNS0_9ValueTypeEPvES3_(&unrelated, nullptr, &trace) == 0, "Allocator-info admission must ignore the receiver, callback and context arguments");
    for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{257}}) {
        const auto requests = trace.requests.size(), frees = trace.frees.size(), live = trace.live.size();
        void* memory = _ZN3sce4Json6MallocEm(size);
        Require(memory && trace.requests.size() == requests + 1 && trace.requests.back() == size && trace.live.at(memory) == size, "Malloc must forward exact size and return the allocator's unmodified pointer");
        _ZN3sce4Json4FreeEPv(memory);
        Require(trace.frees.size() == frees + 1 && trace.frees.back() == memory && trace.live.size() == live, "Free must forward the exact allocation without retaining ownership");
    }
    const auto frees = trace.frees.size();
    _ZN3sce4Json4FreeEPv(nullptr);
    Require(trace.frees.size() == frees + 1 && trace.frees.back() == nullptr, "An active allocator must receive even a null Free argument");
    trace.reject = true;
    Require(_ZN3sce4Json6MallocEm(4097) == nullptr && trace.notices == 1 && trace.failedSize == 4097 && trace.notifiedState == 0, "Failed Malloc must notify once with the original size while keeping the allocation context active");
    trace.reject = false;
    void* retry = _ZN3sce4Json6MallocEm(9);
    Require(retry != nullptr && trace.notices == 1, "A failed direct allocation must permit a later successful retry");
    _ZN3sce4Json4FreeEPv(retry);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Termination must release all required resources through the same allocator");
    const auto finalRequests = trace.requests.size(), finalFrees = trace.frees.size();
    Require(InfoState() == notInitialized && _ZN3sce4Json6MallocEm(13) == nullptr, "Termination must withdraw the allocation context");
    _ZN3sce4Json4FreeEPv(nullptr);
    Require(trace.requests.size() == finalRequests && trace.frees.size() == finalFrees && trace.notices == 1, "Missing-context calls must not dispatch stale allocator callbacks");
    return 0;
}
