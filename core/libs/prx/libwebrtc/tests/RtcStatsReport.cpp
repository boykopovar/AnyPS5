#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

std::set<void*> live;
int statsDestructorCalls = 0;
std::vector<std::int32_t> destroyedIds;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    live.insert(pointer);
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (!pointer)
        return;
    if (live.erase(pointer) != 1)
        std::abort();
    std::free(pointer);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = TestAllocate(count * size);
    std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return 0;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RTCStatsReport check failed at line %u\n", location.line());
    std::abort();
}

template<typename TCall>
bool ThrowsInvalid(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct ReadOnlyPage {
    void* address = nullptr;
    std::size_t size = 0;

    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        address = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(address != nullptr);
        std::memset(address, 0, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(address, size, PAGE_READONLY, &oldProtect) != 0);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        address = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(address != MAP_FAILED);
        Require(mprotect(address, size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!address)
            return;
#ifdef _WIN32
        VirtualFree(address, 0, MEM_RELEASE);
#else
        munmap(address, size);
#endif
    }
};

struct TestStat {
    const void* const* vtable;
    std::int32_t idValue;
};

void APS5_VABI CompleteDestroyStat(TestStat*) {}

void APS5_VABI DeleteStat(TestStat* stat) {
    ++statsDestructorCalls;
    destroyedIds.push_back(stat->idValue);
    _ZdlPv_nid_postfix(stat);
}

const void* const TestStatVtable[] = {
    reinterpret_cast<const void*>(&CompleteDestroyStat),
    reinterpret_cast<const void*>(&DeleteStat),
};

WebRtcRtcStatsMapNode* EndNode(WebRtcRtcStatsMap& map) {
    return reinterpret_cast<WebRtcRtcStatsMapNode*>(&map.root);
}

void InitGuestString(WebRtcGuestString& target, std::string_view text) {
    target.reserved = 0;
    target.size = text.size();
    if (text.size() <= WebRtcGuestStringInlineCapacity) {
        std::memset(target.buffer, 0, sizeof(target.buffer));
        if (!text.empty())
            std::memcpy(target.buffer, text.data(), text.size());
        target.capacity = WebRtcGuestStringInlineCapacity;
        return;
    }
    const std::size_t cap = text.size() | WebRtcGuestStringInlineCapacity;
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(cap + 1));
    std::memcpy(data, text.data(), text.size());
    data[text.size()] = '\0';
    target.pointer = data;
    target.capacity = cap;
}

WebRtcRtcStatsMapNode* MakeStatsNode(std::string_view key, std::int32_t idValue, bool isBlack) {
    auto* statStorage = _Znwm_nid_postfix(sizeof(TestStat));
    auto* stat = new (statStorage) TestStat{TestStatVtable, idValue};
    auto* nodeStorage = _Znwm_nid_postfix(sizeof(WebRtcRtcStatsMapNode));
    auto* node = new (nodeStorage) WebRtcRtcStatsMapNode{};
    node->links.isBlack = isBlack;
    InitGuestString(node->key, key);
    node->value = reinterpret_cast<const WebRtcRtcStats*>(stat);
    return node;
}

WebRtcRtcStatsReport* CreateEmptyReport() {
    auto* storage = _Znwm_nid_postfix(sizeof(WebRtcRtcStatsReport));
    auto* report = new (storage) WebRtcRtcStatsReport{};
    report->refCount.store(1, std::memory_order_relaxed);
    report->timestampUs = 123456;
    report->stats.beginNode = EndNode(report->stats);
    report->stats.root = nullptr;
    report->stats.size = 0;
    return report;
}

WebRtcRtcStatsReport* CreatePopulatedReport() {
    auto* report = CreateEmptyReport();
    auto* n2 = MakeStatsNode("stat-b", 2, true);
    auto* n1 = MakeStatsNode("stat-a-very-long-identifier-string", 1, false);
    auto* n3 = MakeStatsNode("stat-c", 3, false);
    auto* n4 = MakeStatsNode("stat-d", 4, true);
    n2->links.left = n1;
    n2->links.right = n3;
    n2->links.parent = EndNode(report->stats);
    n1->links.parent = n2;
    n3->links.parent = n2;
    n3->links.right = n4;
    n4->links.parent = n3;
    report->stats.root = n2;
    report->stats.beginNode = n1;
    report->stats.size = 4;
    return report;
}

void ReleaseReportRef(WebRtcRtcStatsReport* report) {
    WebRtcRtcStatsReportConstIterator sentinel{report, reinterpret_cast<const WebRtcRtcStatsMapNode*>(&report->stats.root)};
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&sentinel);
}

void TestEmptyReport() {
    auto* report = CreateEmptyReport();
    WebRtcRtcStatsReportConstIterator beginIt{};
    WebRtcRtcStatsReportConstIterator endIt{};
    errno = 61;
    Require(_ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&beginIt, report) == &beginIt);
    Require(_ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&endIt, report) == &endIt);
    Require(errno == 61);
    Require(report->refCount.load() == 3);
    Require(!_ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&beginIt, &endIt));

    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&beginIt);
    Require(report->refCount.load() == 2);
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&endIt);
    Require(report->refCount.load() == 1);
    ReleaseReportRef(report);
    Require(live.empty());
}

void TestIterationAndLifetime() {
    statsDestructorCalls = 0;
    destroyedIds.clear();
    auto* report = CreatePopulatedReport();

    WebRtcRtcStatsReportConstIterator it{};
    WebRtcRtcStatsReportConstIterator endIt{};
    _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&it, report);
    _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&endIt, report);
    Require(report->refCount.load() == 3);

    ReleaseReportRef(report);
    Require(report->refCount.load() == 2);

    std::vector<std::int32_t> visited;
    while (_ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&it, &endIt)) {
        const auto* deref = reinterpret_cast<const TestStat*>(_ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&it));
        const auto* arrow = reinterpret_cast<const TestStat*>(_ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&it));
        Require(deref == arrow);
        visited.push_back(deref->idValue);
        if (visited.size() % 2 == 1)
            Require(_ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&it) == &it);
        else
            Require(_ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(&it, 0) == &it);
    }
    Require((visited == std::vector<std::int32_t>{1, 2, 3, 4}));
    Require(!_ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&it, &endIt));

    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&it);
    Require(statsDestructorCalls == 0);
    Require(!live.empty());

    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&endIt);
    Require(statsDestructorCalls == 4);
    Require(destroyedIds.size() == 4);
    Require(live.empty());
}

void TestInvalidInputs() {
    ReadOnlyPage readOnly;
    auto* roIter = static_cast<WebRtcRtcStatsReportConstIterator*>(readOnly.address);
    auto* roReport = static_cast<const WebRtcRtcStatsReport*>(readOnly.address);
    auto* badIter = reinterpret_cast<WebRtcRtcStatsReportConstIterator*>(0x10);
    auto* badReport = reinterpret_cast<const WebRtcRtcStatsReport*>(0x10);

    auto* report = CreatePopulatedReport();
    WebRtcRtcStatsReportConstIterator endIt{};
    _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&endIt, report);
    const auto liveBaseline = live;
    const std::int32_t refsBefore = report->refCount.load();

    errno = 62;
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(nullptr, report); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(roIter, report); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&endIt, nullptr); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&endIt, badReport); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&endIt, roReport); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(badIter, report); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&endIt, nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(roIter); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&endIt); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(&endIt, 0); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&endIt); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&endIt); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(nullptr, &endIt); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&endIt, badIter); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(roIter); }));
    Require(report->refCount.load() == refsBefore);
    Require(errno == 62);

    WebRtcRtcStatsReportConstIterator invalidDtor{report, endIt.it};
    report->stats.size = 99;
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&endIt);
    Require(report->refCount.load() == 1);
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&invalidDtor); }));
    Require(report->refCount.load() == 1);
    Require(live == liveBaseline);
    report->stats.size = 4;

    report->refCount.store(0, std::memory_order_relaxed);
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&endIt, report); }));
    Require(report->refCount.load() == 0);
    report->refCount.store(std::numeric_limits<std::int32_t>::max(), std::memory_order_relaxed);
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&endIt, report); }));
    Require(report->refCount.load() == std::numeric_limits<std::int32_t>::max());
    report->refCount.store(1, std::memory_order_relaxed);

    alignas(WebRtcRtcStatsReport) std::byte unalignedStorage[sizeof(WebRtcRtcStatsReport) + 4]{};
    const auto* misalignedReport = reinterpret_cast<const WebRtcRtcStatsReport*>(unalignedStorage + 1);
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&endIt, misalignedReport); }));

    WebRtcRtcStatsMapNode* leftLeaf = const_cast<WebRtcRtcStatsMapNode*>(report->stats.beginNode);
    leftLeaf->links.left = leftLeaf;
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&invalidDtor); }));
    leftLeaf->links.left = nullptr;

    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&invalidDtor);
    Require(live.empty());
}

void TestConcurrentIterators() {
    statsDestructorCalls = 0;
    destroyedIds.clear();
    auto* report = CreatePopulatedReport();
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([report] {
            for (int i = 0; i < 64; ++i) {
                WebRtcRtcStatsReportConstIterator beginIt{};
                WebRtcRtcStatsReportConstIterator endIt{};
                _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&beginIt, report);
                _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&endIt, report);
                Require(_ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&beginIt, &endIt));
                _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&beginIt);
                _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&endIt);
            }
        });
    }
    for (auto& worker : workers)
        worker.join();
    Require(report->refCount.load() == 1);
    ReleaseReportRef(report);
    Require(statsDestructorCalls == 4);
    Require(live.empty());
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestEmptyReport();
    TestIterationAndLifetime();
    TestInvalidInputs();
    TestConcurrentIterators();
    std::puts("libwebrtc RTCStatsReport tests passed");
    return 0;
}
