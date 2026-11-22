#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

struct FakeStats {
    const void* const* vtable;
    char name;
};

std::string destroyed;

void APS5_VABI FakeCompleteDestructor(FakeStats*) { std::abort(); }

void APS5_VABI FakeDeletingDestructor(FakeStats* stats) {
    destroyed.push_back(stats->name);
    _ZdlPv_nid_postfix(stats);
}

const void* const FakeVtable[] = {
    reinterpret_cast<const void*>(&FakeCompleteDestructor),
    reinterpret_cast<const void*>(&FakeDeletingDestructor),
};

const void* const NoDestructorVtable[] = {
    reinterpret_cast<const void*>(&FakeCompleteDestructor),
    nullptr,
};

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
    const std::size_t capacity = text.size() | WebRtcGuestStringInlineCapacity;
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(data, text.data(), text.size());
    data[text.size()] = '\0';
    target.pointer = data;
    target.capacity = capacity;
}

WebRtcStatsMapNode* EndNode(WebRtcStatsReport* report) {
    return reinterpret_cast<WebRtcStatsMapNode*>(&report->stats.root);
}

WebRtcStatsReport* NewReport(std::int32_t references) {
    auto* report = new (_Znwm_nid_postfix(sizeof(WebRtcStatsReport))) WebRtcStatsReport{};
    report->referenceCount = references;
    report->timestampUs = 1234567;
    report->stats.beginNode = EndNode(report);
    return report;
}

WebRtcStatsMapNode* NewNode(std::string_view id, char name) {
    auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcStatsMapNode))) WebRtcStatsMapNode{};
    InitGuestString(node->id, id);
    if (name != 0)
        node->stats = new (_Znwm_nid_postfix(sizeof(FakeStats))) FakeStats{FakeVtable, name};
    return node;
}

void Link(WebRtcStatsMapNode* parent, WebRtcStatsMapNode* left, WebRtcStatsMapNode* right) {
    parent->links.left = left;
    parent->links.right = right;
    if (left)
        left->links.parent = parent;
    if (right)
        right->links.parent = parent;
}

struct Tree {
    WebRtcStatsReport* report;
    std::array<WebRtcStatsMapNode*, 7> nodes;
};

Tree NewBalancedReport(std::int32_t references, bool withSpecialNodes) {
    Tree tree{NewReport(references), {}};
    const std::string longId(40, 'c');
    const std::string largeId(5000, 'e');
    const std::array<std::string, 7> ids{"a", "b", withSpecialNodes ? longId : "c", "d", withSpecialNodes ? largeId : "e", "f", "g"};
    for (std::size_t index = 0; index < tree.nodes.size(); ++index) {
        const char name = static_cast<char>('a' + index);
        tree.nodes[index] = NewNode(ids[index], withSpecialNodes && name == 'f' ? 0 : name);
    }
    Link(tree.nodes[3], tree.nodes[1], tree.nodes[5]);
    Link(tree.nodes[1], tree.nodes[0], tree.nodes[2]);
    Link(tree.nodes[5], tree.nodes[4], tree.nodes[6]);
    tree.nodes[3]->links.parent = EndNode(tree.report);
    tree.nodes[3]->links.isBlack = true;
    tree.report->stats.root = tree.nodes[3];
    tree.report->stats.beginNode = tree.nodes[0];
    tree.report->stats.size = tree.nodes.size();
    return tree;
}

WebRtcStatsReportConstIterator Begin(const WebRtcStatsReport* report) {
    WebRtcStatsReportConstIterator iterator{};
    Require(_ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&iterator, report) == &iterator);
    return iterator;
}

WebRtcStatsReportConstIterator End(const WebRtcStatsReport* report) {
    WebRtcStatsReportConstIterator iterator{};
    Require(_ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&iterator, report) == &iterator);
    return iterator;
}

bool Differ(const WebRtcStatsReportConstIterator& first, const WebRtcStatsReportConstIterator& second) {
    return _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&first, &second);
}

void Destroy(WebRtcStatsReportConstIterator& iterator) {
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&iterator);
}

void TestEmptyReport() {
    WebRtcStatsReport* report = NewReport(1);
    WebRtcStatsReportConstIterator begin = Begin(report);
    WebRtcStatsReportConstIterator end = End(report);
    Require(report->referenceCount == 3);
    Require(begin.report == report && end.report == report);
    Require(begin.node == EndNode(report) && end.node == EndNode(report));
    Require(!Differ(begin, end));
    Destroy(begin);
    Require(report->referenceCount == 2);
    Destroy(end);
    Require(report->referenceCount == 1 && live.count(report) == 1);
    WebRtcStatsReportConstIterator last = Begin(report);
    report->referenceCount = 1;
    Destroy(last);
    Require(live.empty());
}

void TestIteration() {
    Tree tree = NewBalancedReport(1, false);
    WebRtcStatsReportConstIterator iterator = Begin(tree.report);
    WebRtcStatsReportConstIterator end = End(tree.report);
    Require(tree.report->referenceCount == 3);
    Require(iterator.node == tree.nodes[0] && end.node == EndNode(tree.report));
    std::string visited;
    for (std::size_t index = 0; Differ(iterator, end); ++index) {
        Require(index < tree.nodes.size() && iterator.node == tree.nodes[index]);
        const void* reference = _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&iterator);
        const void* pointer = _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&iterator);
        Require(reference == tree.nodes[index]->stats && pointer == reference);
        visited.push_back(static_cast<const FakeStats*>(reference)->name);
        if (index % 2 == 0)
            Require(_ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&iterator) == &iterator);
        else
            Require(_ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(&iterator, 0) == &iterator);
    }
    Require(visited == "abcdefg");
    Require(iterator.node == end.node && iterator.report == tree.report);
    Require(tree.report->referenceCount == 3 && destroyed.empty());

    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&iterator); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(&iterator, 0); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&end); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&end); }));
    Require(iterator.node == end.node);

    WebRtcStatsReportConstIterator moved{nullptr, tree.nodes[6]};
    Require(Differ(moved, iterator));
    Require(_ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&moved) == &moved);
    Require(moved.node == EndNode(tree.report) && !Differ(moved, end));
    Destroy(moved);
    Require(tree.report->referenceCount == 3);

    Destroy(iterator);
    Destroy(end);
    Require(tree.report->referenceCount == 1 && destroyed.empty());
    WebRtcStatsReportConstIterator last = Begin(tree.report);
    tree.report->referenceCount = 1;
    Destroy(last);
    Require(destroyed == "acbegfd");
    Require(live.empty());
    destroyed.clear();
}

void TestChains() {
    WebRtcStatsReport* report = NewReport(0);
    std::array<WebRtcStatsMapNode*, 4> nodes{NewNode("a", 'a'), NewNode("b", 'b'), NewNode("c", 'c'), NewNode("d", 'd')};
    Link(nodes[3], nodes[0], nullptr);
    Link(nodes[0], nullptr, nodes[1]);
    Link(nodes[1], nullptr, nodes[2]);
    nodes[3]->links.parent = EndNode(report);
    report->stats.root = nodes[3];
    report->stats.beginNode = nodes[0];
    report->stats.size = nodes.size();
    WebRtcStatsReportConstIterator iterator = Begin(report);
    WebRtcStatsReportConstIterator end = End(report);
    std::string visited;
    while (Differ(iterator, end)) {
        visited.push_back(static_cast<const FakeStats*>(_ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&iterator))->name);
        _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&iterator);
    }
    Require(visited == "abcd");
    Destroy(iterator);
    Require(report->referenceCount == 1 && destroyed.empty());
    Destroy(end);
    Require(destroyed == "cbad");
    Require(live.empty());
    destroyed.clear();
}

void TestLastReference() {
    Tree tree = NewBalancedReport(0, true);
    Require(tree.nodes[2]->id.capacity > WebRtcGuestStringInlineCapacity);
    Require(tree.nodes[4]->id.capacity + 1 >= GuestStringStorage::LargeAllocation);
    Require(tree.nodes[5]->stats == nullptr);
    WebRtcStatsReportConstIterator iterator = Begin(tree.report);
    Require(tree.report->referenceCount == 1);
    Destroy(iterator);
    Require(destroyed == "acbegd");
    Require(live.empty());
    destroyed.clear();
}

void TestInvalid() {
    Tree tree = NewBalancedReport(0, false);
    WebRtcStatsReportConstIterator iterator{};
    const ReadOnlyPage readOnly;
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(nullptr, tree.report); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&iterator, nullptr); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(nullptr, tree.report); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&iterator, nullptr); }));
    Require(ThrowsInvalid([&] {
        _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(static_cast<WebRtcStatsReportConstIterator*>(readOnly.address), tree.report);
    }));
    Require(ThrowsInvalid([&] {
        _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&iterator, static_cast<const WebRtcStatsReport*>(readOnly.address));
    }));
    Require(tree.report->referenceCount == 0 && iterator.report == nullptr && iterator.node == nullptr);
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&iterator); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&iterator); }));
    Require(ThrowsInvalid([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&iterator, nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(nullptr); }));
    Destroy(iterator);

    iterator = Begin(tree.report);
    Require(tree.report->referenceCount == 1);
    tree.report->stats.size = 6;
    Require(ThrowsInvalid([&] { Destroy(iterator); }));
    tree.report->stats.size = 8;
    Require(ThrowsInvalid([&] { Destroy(iterator); }));
    tree.report->stats.size = 7;
    tree.nodes[6]->links.parent = tree.nodes[3];
    Require(ThrowsInvalid([&] { Destroy(iterator); }));
    tree.nodes[6]->links.parent = tree.nodes[5];
    static_cast<FakeStats*>(tree.nodes[2]->stats)->vtable = NoDestructorVtable;
    Require(ThrowsInvalid([&] { Destroy(iterator); }));
    static_cast<FakeStats*>(tree.nodes[2]->stats)->vtable = FakeVtable;
    tree.nodes[1]->id.size = tree.nodes[1]->id.capacity + 1;
    Require(ThrowsInvalid([&] { Destroy(iterator); }));
    tree.nodes[1]->id.size = 1;
    Require(tree.report->referenceCount == 1 && destroyed.empty() && live.count(tree.report) == 1);
    Destroy(iterator);
    Require(destroyed == "acbegfd");
    Require(live.empty());
    destroyed.clear();
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
    TestIteration();
    TestChains();
    TestLastReference();
    TestInvalid();
    std::puts("libwebrtc RTCStatsReport tests passed");
    return 0;
}
