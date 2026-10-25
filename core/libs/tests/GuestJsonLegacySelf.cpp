#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <string>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; int mode; unsigned reserved[3]; };
using NullHandler = const Value& (APS5_VABI*)(ValueType, const Value*, void*);
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static const std::uint32_t words[]{0, 0, 0, 0, 0x05000000};
    return words;
}
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct LegacySelfTrace {
    std::map<void*, std::size_t> live;
    std::map<void*, unsigned> generations;
    std::vector<std::size_t> requests;
    std::vector<void*> releases;
    unsigned nextGeneration = 0;
    unsigned notices = 0;
    unsigned nullFrees = 0;
    int budget = -1;
    bool once = false;
    std::vector<unsigned> releaseIds;
};
static void* APS5_VABI AllocateSelf(void*, std::size_t size, void* data) {
    auto& trace = *static_cast<LegacySelfTrace*>(data); trace.requests.push_back(size);
    if (!trace.budget) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(size);
    Require(memory && trace.live.emplace(memory, size).second && trace.generations.emplace(memory, ++trace.nextGeneration).second, "Self-setting allocations require distinct live generations");
    std::memset(memory, 0xa5, size); return memory;
}
static void APS5_VABI ReleaseSelf(void*, void* memory, void* data) {
    auto& trace = *static_cast<LegacySelfTrace*>(data);
    if (!memory) { ++trace.nullFrees; return; }
    trace.releaseIds.push_back(trace.generations.at(memory));
    Require(trace.live.erase(memory) == 1 && trace.generations.erase(memory) == 1, "Legacy self-setting must release each owned allocation exactly once");
    trace.releases.push_back(memory); std::free(memory);
}
static void APS5_VABI NotifySelf(void*, int error, std::size_t size, void* data) {
    auto& trace = *static_cast<LegacySelfTrace*>(data);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == size, "Legacy self-setting failure must identify the rejected sentinel");
    ++trace.notices;
}
static void* const SelfTable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateSelf), reinterpret_cast<void*>(ReleaseSelf), reinterpret_cast<void*>(NotifySelf)};
struct LegacySelfAllocator { void* const* table = SelfTable; };
struct SelfListLink { SelfListLink* previous; SelfListLink* next; Value value; };
struct SelfTreeLink {
    SelfTreeLink* parent; SelfTreeLink* left; SelfTreeLink* right;
    const char* key; unsigned length; unsigned capacity; std::uint64_t reserved;
    void* publicKey; std::uint64_t padding; Value value; unsigned char color; unsigned char nil; unsigned char tail[6];
};
static_assert(sizeof(SelfListLink) == 48 && sizeof(SelfTreeLink) == 104);
static std::string SelfItemState(const Value& value, const Value* owner) {
    return std::to_string(value.type) + (value.parent == owner ? "p" : value.parent ? "x" : "n") + std::to_string(value.payload);
}
static void SelfTreeState(const SelfTreeLink* node, const Value* owner, std::string& output, unsigned depth = 0) {
    Require(depth < 8, "Replacement tree traversal must remain bounded");
    if (node->nil) return;
    SelfTreeState(node->left, owner, output, depth + 1);
    output += std::string(node->key) + "/" + (node->publicKey ? *reinterpret_cast<const char* const*>(node->publicKey) : "?") + "=" + SelfItemState(node->value, owner) + ";";
    SelfTreeState(node->right, owner, output, depth + 1);
}
static std::string SelfState(const Value& value, bool object) {
    auto* words = *reinterpret_cast<const std::uintptr_t* const*>(value.payload);
    std::string output = std::to_string(words[1]) + "[";
    if (!words[0]) return output + "missing]";
    if (object) {
        auto* head = reinterpret_cast<const SelfTreeLink*>(words[0]); SelfTreeState(head->parent, &value, output);
    } else {
        auto* head = reinterpret_cast<const SelfListLink*>(words[0]); unsigned count = 0;
        for (auto* node = head->next; node != head; node = node->next) {
            Require(++count < 8, "Replacement list traversal must remain bounded"); output += SelfItemState(node->value, &value) + ";";
        }
    }
    return output + "]";
}
template<class T>
static std::uint64_t SelfOrder(const std::vector<T>& values, std::size_t begin) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (auto i = begin; i < values.size(); ++i) { hash ^= values[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}
struct NonemptySelfCase {
    bool object; bool once; unsigned budget;
    std::size_t requests; std::uint64_t requestOrder; unsigned notices; std::size_t releases; std::uint64_t releaseOrder; const char* state;
    std::size_t retryRequests; std::uint64_t retryRequestOrder; std::size_t retryReleases; std::uint64_t retryReleaseOrder; const char* retryState;
};
static const NonemptySelfCase NonemptySelfCases[]{
    {0, 0, 0, 1, UINT64_C(12638135523509116079), 1, 3, UINT64_C(6554792551179442466), "0[missing]", 1, UINT64_C(12638135523509116079), 0, UINT64_C(14695981039346656037), "0[]"},
    {0, 0, 1, 2, UINT64_C(575361273772712493), 1, 3, UINT64_C(6554792551179442466), "0[]", 1, UINT64_C(12638135523509116079), 1, UINT64_C(12638167409346334198), "0[]"},
    {0, 0, 2, 3, UINT64_C(5656224367409952583), 1, 3, UINT64_C(6554792551179442466), "1[3n11;]", 2, UINT64_C(575361273772712493), 2, UINT64_C(613651766217591568), "1[3n11;]"},
    {0, 0, 3, 3, UINT64_C(5656224367409952583), 0, 3, UINT64_C(6554792551179442466), "2[3n11;3n22;]", 3, UINT64_C(5656224367409952583), 3, UINT64_C(12213400556975252055), "2[3n11;3n22;]"},
    {0, 1, 0, 1, UINT64_C(12638135523509116079), 1, 3, UINT64_C(6554792551179442466), "0[missing]", 1, UINT64_C(12638135523509116079), 0, UINT64_C(14695981039346656037), "0[]"},
    {0, 1, 1, 2, UINT64_C(575361273772712493), 1, 3, UINT64_C(6554792551179442466), "0[]", 1, UINT64_C(12638135523509116079), 1, UINT64_C(12638167409346334198), "0[]"},
    {0, 1, 2, 3, UINT64_C(5656224367409952583), 1, 3, UINT64_C(6554792551179442466), "1[3n11;]", 2, UINT64_C(575361273772712493), 2, UINT64_C(613651766217591568), "1[3n11;]"},
    {0, 1, 3, 3, UINT64_C(5656224367409952583), 0, 3, UINT64_C(6554792551179442466), "2[3n11;3n22;]", 3, UINT64_C(5656224367409952583), 3, UINT64_C(12213400556975252055), "2[3n11;3n22;]"},
    {1, 0, 0, 1, UINT64_C(12638197096160295895), 1, 9, UINT64_C(7899594896565220821), "0[missing]", 1, UINT64_C(12638197096160295895), 0, UINT64_C(14695981039346656037), "0[]"},
    {1, 0, 1, 2, UINT64_C(628920684195530125), 1, 9, UINT64_C(7899594896565220821), "0[]", 1, UINT64_C(12638197096160295895), 1, UINT64_C(12638126727416090391), "0[]"},
    {1, 0, 2, 5, UINT64_C(17406812458522649141), 3, 9, UINT64_C(7899594896565220821), "2[/?=3n11;]", 2, UINT64_C(628920684195530125), 2, UINT64_C(566743301632497700), "2[/?=3n11;]"},
    {1, 0, 3, 5, UINT64_C(17406812458522649141), 2, 9, UINT64_C(7899594896565220821), "2[a/?=3n11;]", 3, UINT64_C(3708358159552328189), 3, UINT64_C(1887292020941694844), "2[a/?=3n11;]"},
    {1, 0, 4, 6, UINT64_C(10307126579844094685), 2, 9, UINT64_C(7899594896565220821), "2[a/=3n11;]", 4, UINT64_C(8967656197913659423), 4, UINT64_C(12385604588108498361), "2[a/=3n11;]"},
    {1, 0, 5, 6, UINT64_C(10307126579844094685), 1, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;]", 5, UINT64_C(17406713502476110151), 5, UINT64_C(16291384495448556647), "2[a/a=3n11;]"},
    {1, 0, 6, 8, UINT64_C(8732740249579296847), 2, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;/?=3n22;]", 6, UINT64_C(10307126579844094685), 6, UINT64_C(6796161341782386540), "2[a/a=3n11;/?=3n22;]"},
    {1, 0, 7, 8, UINT64_C(8732740249579296847), 1, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;b/?=3n22;]", 7, UINT64_C(12001570155012663533), 7, UINT64_C(16058915569980617336), "2[a/a=3n11;b/?=3n22;]"},
    {1, 0, 8, 9, UINT64_C(15869276825689737943), 1, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;b/=3n22;]", 8, UINT64_C(8732740249579296847), 8, UINT64_C(13124253386643929941), "2[a/a=3n11;b/=3n22;]"},
    {1, 0, 9, 9, UINT64_C(15869276825689737943), 0, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;b/b=3n22;]", 9, UINT64_C(15869276825689737943), 9, UINT64_C(5256107193438867751), "2[a/a=3n11;b/b=3n22;]"},
    {1, 1, 0, 1, UINT64_C(12638197096160295895), 1, 9, UINT64_C(7899594896565220821), "0[missing]", 1, UINT64_C(12638197096160295895), 0, UINT64_C(14695981039346656037), "0[]"},
    {1, 1, 1, 2, UINT64_C(628920684195530125), 1, 9, UINT64_C(7899594896565220821), "0[]", 1, UINT64_C(12638197096160295895), 1, UINT64_C(12638126727416090391), "0[]"},
    {1, 1, 2, 9, UINT64_C(15869276825689737943), 1, 9, UINT64_C(7899594896565220821), "2[/a=3n11;b/b=3n22;]", 8, UINT64_C(16025805325695402919), 8, UINT64_C(13124253386643929941), "2[/a=3n11;b/b=3n22;]"},
    {1, 1, 3, 8, UINT64_C(10981429471021977615), 1, 9, UINT64_C(7899594896565220821), "2[a/?=3n11;b/b=3n22;]", 7, UINT64_C(12136851201209200173), 7, UINT64_C(16058915569980617336), "2[a/?=3n11;b/b=3n22;]"},
    {1, 1, 4, 9, UINT64_C(15869276825689737943), 1, 9, UINT64_C(7899594896565220821), "2[a/=3n11;b/b=3n22;]", 8, UINT64_C(10981429471021977615), 8, UINT64_C(13124253386643929941), "2[a/=3n11;b/b=3n22;]"},
    {1, 1, 5, 6, UINT64_C(10307126579844094685), 1, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;]", 5, UINT64_C(17406713502476110151), 5, UINT64_C(16291384495448556647), "2[a/a=3n11;]"},
    {1, 1, 6, 9, UINT64_C(15869276825689737943), 1, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;/b=3n22;]", 8, UINT64_C(8707858301437801767), 8, UINT64_C(13124253386643929941), "2[a/a=3n11;/b=3n22;]"},
    {1, 1, 7, 8, UINT64_C(8732740249579296847), 1, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;b/?=3n22;]", 7, UINT64_C(12001570155012663533), 7, UINT64_C(16058915569980617336), "2[a/a=3n11;b/?=3n22;]"},
    {1, 1, 8, 9, UINT64_C(15869276825689737943), 1, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;b/=3n22;]", 8, UINT64_C(8732740249579296847), 8, UINT64_C(13124253386643929941), "2[a/a=3n11;b/=3n22;]"},
    {1, 1, 9, 9, UINT64_C(15869276825689737943), 0, 9, UINT64_C(7899594896565220821), "2[a/a=3n11;b/b=3n22;]", 9, UINT64_C(15869276825689737943), 9, UINT64_C(5256107193438867751), "2[a/a=3n11;b/b=3n22;]"},
};

static void CheckNonemptySelfUse() {
    for (const auto& expected : NonemptySelfCases) for (bool setter : {false, true}) {
        LegacySelfTrace trace; LegacySelfAllocator allocator; Parameter parameter{&allocator, &trace, 16384, 2, {}};
        std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize nonempty legacy self-use");
        const auto module = trace.generations;
        Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
        const char* text = expected.object ? "{\"a\":11,\"b\":22}" : "[11,22]";
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text, std::strlen(text)) == 0 &&
                _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&value, nullptr, &trace) == 0, "Prepare nonempty collection and callback context");
        const Value original = value; void* header = *reinterpret_cast<void**>(value.payload);
        auto protectedGenerations = module;
        for (void* pointer : {reinterpret_cast<void*>(value.payload), static_cast<void*>(value.auxiliary), reinterpret_cast<void**>(value.auxiliary)[2]}) protectedGenerations.emplace(pointer, trace.generations.at(pointer));
        std::map<void*, std::vector<unsigned char>> snapshots;
        for (const auto& [pointer, generation] : protectedGenerations) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(pointer); snapshots.emplace(pointer, std::vector<unsigned char>{bytes, bytes + trace.live.at(pointer)});
        }
        const auto requests = trace.requests.size(); const auto releases = trace.releaseIds.size(); const auto notices = trace.notices; const auto nullFrees = trace.nullFrees;
        trace.budget = static_cast<int>(expected.budget); trace.once = expected.once;
        if (setter) _ZN3sce4Json5Value3setERKS1_(&value, &value);
        else Require(_ZN3sce4Json5ValueaSERKS1_(&value, &value) == &value, "Nonempty legacy assignment must retain its receiver identity");
        trace.budget = -1; trace.once = false;
        Require(trace.requests.size() == requests + expected.requests && SelfOrder(trace.requests, requests) == expected.requestOrder && trace.notices == notices + expected.notices &&
                trace.releaseIds.size() == releases + expected.releases && SelfOrder(trace.releaseIds, releases) == expected.releaseOrder && trace.nullFrees == nullFrees && SelfState(value, expected.object) == expected.state,
                "Legacy self-use must preserve exact admission/release ordering, partial contents, key failures, parent links and retained counts");
        const auto retryRequests = trace.requests.size(); const auto retryReleases = trace.releaseIds.size(); const auto retryNotices = trace.notices;
        if (setter) _ZN3sce4Json5Value3setERKS1_(&value, &value);
        else Require(_ZN3sce4Json5ValueaSERKS1_(&value, &value) == &value, "Partial self-use retry must retain its receiver identity");
        Require(trace.requests.size() == retryRequests + expected.retryRequests && SelfOrder(trace.requests, retryRequests) == expected.retryRequestOrder && trace.notices == retryNotices &&
                trace.releaseIds.size() == retryReleases + expected.retryReleases && SelfOrder(trace.releaseIds, retryReleases) == expected.retryReleaseOrder && trace.nullFrees == nullFrees && SelfState(value, expected.object) == expected.retryState,
                "Independent retry must clone the actual partial collection without inventing lost entries or repairing observed count/key differences");
        Require(std::memcmp(&value, &original, sizeof(value)) == 0 && *reinterpret_cast<void**>(value.payload) == header, "Both self-use attempts must retain Value, header and callback ownership");
        for (const auto& [pointer, generation] : protectedGenerations) Require(trace.generations.at(pointer) == generation && std::memcmp(pointer, snapshots.at(pointer).data(), snapshots.at(pointer).size()) == 0,
                "Self-use failure and retry must preserve every module, handle, callback and fallback generation and byte");
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.live.empty() && trace.generations.empty(), "All partial nonempty collections and module allocations must be reclaimed");
    }
}

int main() {
    CheckNonemptySelfUse();
    for (bool object : {false, true}) for (bool setter : {false, true}) for (unsigned budget = 0; budget <= 2; ++budget) {
        LegacySelfTrace trace; LegacySelfAllocator allocator; Parameter parameter{&allocator, &trace, 16384, 2, {}};
        std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, reinterpret_cast<const InitParameter2*>(&parameter)) == 0 && trace.live.size() == 11, "Initialize the eleven-resource legacy module");
        Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, object ? "{}" : "[]", 2) == 0 &&
                _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&value, nullptr, &trace) == 0, "Prepare legacy collection ownership and auxiliary callback context");
        const Value before = value; const auto generations = trace.generations;
        void* header = *reinterpret_cast<void**>(value.payload); void* sentinel = *reinterpret_cast<void**>(header);
        Require(trace.live.at(header) == 24 && trace.live.at(sentinel) == (object ? 104u : 48u), "Legacy collections must retain their 24-byte header and actual sentinel");
        std::map<void*, std::vector<unsigned char>> snapshots;
        for (const auto& [pointer, size] : trace.live) {
            if (pointer == header || pointer == sentinel) continue;
            const auto* bytes = reinterpret_cast<const unsigned char*>(pointer); snapshots.emplace(pointer, std::vector<unsigned char>{bytes, bytes + size});
        }
        const auto requests = trace.requests.size(); const auto releases = trace.releases.size(); const auto notices = trace.notices; const auto nullFrees = trace.nullFrees;
        trace.budget = static_cast<int>(budget);
        if (setter) _ZN3sce4Json5Value3setERKS1_(&value, &value);
        else Require(_ZN3sce4Json5ValueaSERKS1_(&value, &value) == &value, "Legacy self-assignment must return its receiver");
        trace.budget = -1;
        auto* words = reinterpret_cast<std::uintptr_t*>(header);
        Require(std::memcmp(&value, &before, sizeof(value)) == 0 && *reinterpret_cast<void**>(value.payload) == header && words[1] == 0 &&
                trace.requests.size() == requests + 1 && trace.requests.back() == (object ? 104u : 48u) && trace.notices == notices + unsigned(!budget) &&
                trace.releases.size() == releases + 1 && trace.releases.back() == sentinel && trace.nullFrees == nullFrees,
                "Legacy self-use must keep its header and metadata while releasing the original sentinel even when replacement admission fails");
        Require((words[0] != 0) == bool(budget), "Failed legacy replacement must retain a missing sentinel rather than restore the old one");
        if (budget) Require(reinterpret_cast<void*>(words[0]) != sentinel && trace.live.at(reinterpret_cast<void*>(words[0])) == (object ? 104u : 48u), "Successful legacy replacement must publish a fresh sentinel");
        for (const auto& [pointer, generation] : generations) if (pointer != sentinel) Require(trace.generations.at(pointer) == generation, "Self-use must preserve all non-sentinel allocation generations");
        for (const auto& [pointer, bytes] : snapshots) Require(std::memcmp(pointer, bytes.data(), bytes.size()) == 0, "Self-use must preserve callback, fallback, handle and module bytes");
        const auto retryRequests = trace.requests.size(); const auto retryReleases = trace.releases.size(); const auto retryNullFrees = trace.nullFrees; const auto retryNotices = trace.notices;
        const auto previousSentinel = words[0];
        if (setter) _ZN3sce4Json5Value3setERKS1_(&value, &value);
        else Require(_ZN3sce4Json5ValueaSERKS1_(&value, &value) == &value, "Legacy self-use retry must return its original receiver");
        Require(words[0] && words[0] != previousSentinel && words[1] == 0 && std::memcmp(&value, &before, sizeof(value)) == 0 &&
                trace.requests.size() == retryRequests + 1 && trace.requests.back() == (object ? 104u : 48u) && trace.releases.size() == retryReleases + unsigned(previousSentinel != 0) &&
                trace.nullFrees == retryNullFrees && trace.notices == retryNotices,
                "Same-Value retry must repair a missing sentinel without fabricating a null free or replacing its header and metadata");
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.live.empty() && trace.generations.empty(), "Partial legacy collections and module resources must remain destructible");
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, reinterpret_cast<const InitParameter2*>(&parameter)) == 0 && _ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && trace.live.empty(), "A completed legacy self-use case must permit a fresh module generation");
    }
    return 0;
}
