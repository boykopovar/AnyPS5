#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::set<void*> live;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (!pointer)
        throw std::bad_alloc();
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

struct FakeStats {
    const void* const* vtable;
    char name;
};

std::string deletions;

void APS5_VABI FakeStatsCompleteDestructor(const FakeStats*) { std::abort(); }

void APS5_VABI FakeStatsDeletingDestructor(const FakeStats* stats) {
    deletions.push_back(stats->name);
    _ZdlPv_nid_postfix(const_cast<FakeStats*>(stats));
}

const void* const FakeStatsVtable[] = {
    reinterpret_cast<const void*>(&FakeStatsCompleteDestructor),
    reinterpret_cast<const void*>(&FakeStatsDeletingDestructor),
};

const void* const NoDeleteVtable[] = {
    reinterpret_cast<const void*>(&FakeStatsCompleteDestructor),
    nullptr,
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RTCStatsReport::ConstIterator check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

WebRtcRtcStatsReportConstIterator* Next(WebRtcRtcStatsReportConstIterator* iterator) {
    return _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(iterator);
}

WebRtcRtcStatsReportConstIterator* NextPostfix(WebRtcRtcStatsReportConstIterator* iterator) {
    return _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(iterator, 0);
}

bool NotEqual(const WebRtcRtcStatsReportConstIterator& left, const WebRtcRtcStatsReportConstIterator& right) {
    return _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&left, &right);
}

const void* Dereference(const WebRtcRtcStatsReportConstIterator& iterator) {
    return _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&iterator);
}

const void* Arrow(const WebRtcRtcStatsReportConstIterator& iterator) {
    return _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&iterator);
}

WebRtcStatsMapNode* End(WebRtcRtcStatsReport& report) {
    return reinterpret_cast<WebRtcStatsMapNode*>(&report.stats.root);
}

struct Fixture {
    WebRtcRtcStatsReport report{};
    std::array<WebRtcStatsMapNode, 5> nodes{};
    std::array<int, 5> stats{};

    Fixture() {
        auto& [a, b, c, d, e] = nodes;
        report.refCount = 1;
        report.timestampUs = 1234;
        report.stats.root = &b;
        report.stats.beginNode = &a;
        report.stats.size = nodes.size();
        b.links = {&a, &d, End(report), true};
        a.links = {nullptr, nullptr, &b, true};
        d.links = {&c, &e, &b, false};
        c.links = {nullptr, nullptr, &d, true};
        e.links = {nullptr, nullptr, &d, true};
        for (std::size_t index = 0; index < nodes.size(); ++index)
            nodes[index].stats = &stats[index];
    }

    WebRtcRtcStatsReportConstIterator Begin() { return {&report, report.stats.beginNode}; }
    WebRtcRtcStatsReportConstIterator EndIterator() { return {&report, End(report)}; }
};

void TestWalksInKeyOrder() {
    Fixture fixture;
    auto iterator = fixture.Begin();
    const auto end = fixture.EndIterator();
    std::size_t index = 0;
    while (NotEqual(iterator, end)) {
        Require(index < fixture.stats.size());
        Require(Arrow(iterator) == &fixture.stats[index]);
        Require(Dereference(iterator) == &fixture.stats[index]);
        Require((index % 2 == 0 ? Next(&iterator) : NextPostfix(&iterator)) == &iterator);
        Require(iterator.report == &fixture.report);
        ++index;
    }
    Require(index == fixture.stats.size());
    Require(iterator.node == end.node);
    Require(!NotEqual(iterator, end));
}

void TestSingleEntryReachesEnd() {
    WebRtcRtcStatsReport report{};
    WebRtcStatsMapNode node{};
    int stats = 0;
    node.links = {nullptr, nullptr, End(report), true};
    node.stats = &stats;
    report.stats = {&node, &node, 1};
    WebRtcRtcStatsReportConstIterator iterator{&report, &node};
    Require(Next(&iterator) == &iterator);
    Require(iterator.node == End(report));
}

void TestNotEqualComparesPositionsOnly() {
    Fixture fixture;
    Fixture other;
    const WebRtcRtcStatsReportConstIterator first{&fixture.report, &fixture.nodes[2]};
    const WebRtcRtcStatsReportConstIterator second{&other.report, &fixture.nodes[2]};
    const WebRtcRtcStatsReportConstIterator third{&fixture.report, &fixture.nodes[3]};
    Require(!NotEqual(first, second));
    Require(NotEqual(first, third));
}

void TestArrowReturnsANullEntry() {
    Fixture fixture;
    fixture.nodes[1].stats = nullptr;
    const WebRtcRtcStatsReportConstIterator iterator{&fixture.report, &fixture.nodes[1]};
    Require(Arrow(iterator) == nullptr);
    RequireInvalidArgument([&] { Dereference(iterator); });
}

void TestEndAndInvalidIteratorsThrow() {
    Fixture fixture;
    auto end = fixture.EndIterator();
    RequireInvalidArgument([&] { Next(&end); });
    RequireInvalidArgument([&] { NextPostfix(&end); });
    RequireInvalidArgument([&] { Dereference(end); });
    RequireInvalidArgument([&] { Arrow(end); });
    Require(end.node == End(fixture.report));

    WebRtcRtcStatsReportConstIterator noReport{nullptr, &fixture.nodes[0]};
    RequireInvalidArgument([&] { Next(&noReport); });
    RequireInvalidArgument([&] { Arrow(noReport); });
    WebRtcRtcStatsReportConstIterator noNode{&fixture.report, nullptr};
    RequireInvalidArgument([&] { Next(&noNode); });
    RequireInvalidArgument([&] { Dereference(noNode); });

    const auto begin = fixture.Begin();
    RequireInvalidArgument([&] { Next(nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&begin, nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(nullptr, &begin); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(nullptr); });
}

void TestMalformedTreesThrow() {
    Fixture orphan;
    orphan.nodes[4].links.parent = nullptr;
    WebRtcRtcStatsReportConstIterator last{&orphan.report, &orphan.nodes[4]};
    RequireInvalidArgument([&] { Next(&last); });
    Require(last.node == &orphan.nodes[4]);

    Fixture detached;
    detached.report.stats.root = &detached.nodes[3];
    WebRtcRtcStatsReportConstIterator lastDetached{&detached.report, &detached.nodes[4]};
    RequireInvalidArgument([&] { Next(&lastDetached); });

    Fixture cycle;
    cycle.nodes[3].links.left = &cycle.nodes[3];
    WebRtcRtcStatsReportConstIterator second{&cycle.report, &cycle.nodes[1]};
    RequireInvalidArgument([&] { Next(&second); });

    Fixture undersized;
    undersized.report.stats.size = 1;
    WebRtcRtcStatsReportConstIterator first{&undersized.report, &undersized.nodes[1]};
    RequireInvalidArgument([&] { Next(&first); });
}

WebRtcRtcStatsReportConstIterator Begin(const WebRtcRtcStatsReport& report) {
    WebRtcRtcStatsReportConstIterator result{};
    Require(_ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&result, &report) == &result);
    return result;
}

WebRtcRtcStatsReportConstIterator EndOf(const WebRtcRtcStatsReport& report) {
    WebRtcRtcStatsReportConstIterator result{};
    Require(_ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&result, &report) == &result);
    return result;
}

void Destroy(WebRtcRtcStatsReportConstIterator& iterator) {
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&iterator);
}

void InitGuestString(WebRtcGuestString& target, std::string_view text) {
    target.reserved = 0;
    target.size = text.size();
    if (text.size() <= WebRtcGuestStringInlineCapacity) {
        std::memset(target.buffer, 0, sizeof(target.buffer));
        std::memcpy(target.buffer, text.data(), text.size());
        target.capacity = WebRtcGuestStringInlineCapacity;
        return;
    }
    const std::size_t capacity = text.size() | WebRtcGuestStringInlineCapacity;
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(data, text.data(), text.size());
    data[text.size()] = '\0';
    target.pointer = data;
    target.capacity = capacity;
}

struct HeapReport {
    WebRtcRtcStatsReport* report;
    std::array<WebRtcStatsMapNode*, 5> nodes;

    HeapReport() {
        report = new (_Znwm_nid_postfix(sizeof(WebRtcRtcStatsReport))) WebRtcRtcStatsReport{};
        report->refCount = 1;
        report->timestampUs = 99;
        constexpr std::array<std::string_view, 5> ids{
            "RTCCodec_audio_111", "a", "RTCInboundRTPAudioStream_1234567890", "b", "RTCTransport_0_1"};
        constexpr std::array<char, 5> names{'a', 'b', 'c', 'd', 'e'};
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            nodes[index] = new (_Znwm_nid_postfix(sizeof(WebRtcStatsMapNode))) WebRtcStatsMapNode{};
            InitGuestString(nodes[index]->id, ids[index]);
            nodes[index]->stats = new (_Znwm_nid_postfix(sizeof(FakeStats))) FakeStats{FakeStatsVtable, names[index]};
        }
        auto [a, b, c, d, e] = nodes;
        WebRtcStatsMapNode* const end = End(*report);
        report->stats = {a, b, nodes.size()};
        b->links = {a, d, end, true};
        a->links = {nullptr, nullptr, b, true};
        d->links = {c, e, b, false};
        c->links = {nullptr, nullptr, d, true};
        e->links = {nullptr, nullptr, d, true};
    }
};

void TestBeginAndEndAddReferences() {
    Fixture fixture;
    auto begin = Begin(fixture.report);
    Require(begin.report == &fixture.report && begin.node == fixture.report.stats.beginNode);
    Require(fixture.report.refCount == 2);
    auto end = EndOf(fixture.report);
    Require(end.report == &fixture.report && end.node == End(fixture.report));
    Require(fixture.report.refCount == 3);
    std::size_t count = 0;
    for (auto iterator = Begin(fixture.report); NotEqual(iterator, end); Next(&iterator)) {
        Require(Dereference(iterator) == &fixture.stats[count]);
        ++count;
    }
    Require(count == fixture.stats.size());
    Require(fixture.report.refCount == 4);

    errno = 41;
    Destroy(begin);
    Destroy(end);
    Require(errno == 41);
    Require(fixture.report.refCount == 2);
}

void TestEmptyReportBeginIsEnd() {
    WebRtcRtcStatsReport report{};
    report.refCount = 1;
    report.stats.beginNode = End(report);
    auto begin = Begin(report);
    auto end = EndOf(report);
    Require(!NotEqual(begin, end));
    Require(report.refCount == 3);
    Destroy(end);
    Destroy(begin);
    Require(report.refCount == 1);
}

void TestDestroyingTheLastReferenceDeletesTheReport() {
    HeapReport heap;
    auto iterator = Begin(*heap.report);
    auto end = EndOf(*heap.report);
    Require(heap.report->refCount == 3);
    heap.report->refCount = 2;
    deletions.clear();
    Destroy(end);
    Require(heap.report->refCount == 1);
    Require(deletions.empty());
    Require(Dereference(iterator) == heap.nodes[0]->stats);
    Destroy(iterator);
    Require(deletions == "acedb");
    Require(live.empty());
}

void TestEmptyHeapReportIsDeleted() {
    auto* report = new (_Znwm_nid_postfix(sizeof(WebRtcRtcStatsReport))) WebRtcRtcStatsReport{};
    report->stats.beginNode = End(*report);
    auto iterator = EndOf(*report);
    Require(report->refCount == 1);
    Destroy(iterator);
    Require(live.empty());
}

void TestNullStatsEntriesAreSkipped() {
    HeapReport heap;
    _ZdlPv_nid_postfix(const_cast<void*>(heap.nodes[2]->stats));
    heap.nodes[2]->stats = nullptr;
    heap.report->refCount = 0;
    auto iterator = Begin(*heap.report);
    deletions.clear();
    Destroy(iterator);
    Require(deletions == "aedb");
    Require(live.empty());
}

void TestMovedFromIteratorReleasesNothing() {
    WebRtcRtcStatsReportConstIterator moved{nullptr, nullptr};
    Destroy(moved);
}

void TestInvalidCallsThrowWithoutChanges() {
    Fixture fixture;
    WebRtcRtcStatsReportConstIterator result{};
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(nullptr, &fixture.report); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(nullptr, &fixture.report); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&result, nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&result, nullptr); });
    fixture.report.stats.beginNode = nullptr;
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&result, &fixture.report); });
    Require(fixture.report.refCount == 1);
    Require(result.report == nullptr && result.node == nullptr);

    RequireInvalidArgument([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(nullptr); });
    fixture.report.refCount = 0;
    WebRtcRtcStatsReportConstIterator unowned{&fixture.report, End(fixture.report)};
    RequireInvalidArgument([&] { Destroy(unowned); });
    Require(fixture.report.refCount == 0);
}

void TestMalformedLastReferenceThrowsBeforeReleasing() {
    HeapReport heap;
    const auto baseline = live;
    auto iterator = Begin(*heap.report);
    heap.report->refCount = 1;
    deletions.clear();

    heap.report->stats.size = 4;
    RequireInvalidArgument([&] { Destroy(iterator); });
    heap.report->stats.size = 6;
    RequireInvalidArgument([&] { Destroy(iterator); });
    heap.report->stats.size = 5;

    heap.nodes[4]->links.parent = heap.nodes[4];
    RequireInvalidArgument([&] { Destroy(iterator); });
    heap.nodes[4]->links.parent = heap.nodes[3];

    heap.report->stats.beginNode = heap.nodes[2];
    RequireInvalidArgument([&] { Destroy(iterator); });
    heap.report->stats.beginNode = heap.nodes[0];

    heap.nodes[3]->id.size = heap.nodes[3]->id.capacity + 1;
    RequireInvalidArgument([&] { Destroy(iterator); });
    heap.nodes[3]->id.size = 1;

    auto* stats = static_cast<FakeStats*>(const_cast<void*>(heap.nodes[1]->stats));
    stats->vtable = NoDeleteVtable;
    RequireInvalidArgument([&] { Destroy(iterator); });
    stats->vtable = FakeStatsVtable;

    Require(heap.report->refCount == 1);
    Require(deletions.empty());
    Require(live == baseline);
    Destroy(iterator);
    Require(deletions == "acedb");
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
    TestWalksInKeyOrder();
    TestSingleEntryReachesEnd();
    TestNotEqualComparesPositionsOnly();
    TestArrowReturnsANullEntry();
    TestEndAndInvalidIteratorsThrow();
    TestMalformedTreesThrow();
    TestBeginAndEndAddReferences();
    TestEmptyReportBeginIsEnd();
    TestDestroyingTheLastReferenceDeletesTheReport();
    TestEmptyHeapReportIsDeleted();
    TestNullStatsEntriesAreSkipped();
    TestMovedFromIteratorReleasesNothing();
    TestInvalidCallsThrowWithoutChanges();
    TestMalformedLastReferenceThrowsBeforeReleasing();
    return 0;
}
