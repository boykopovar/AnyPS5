#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <stdexcept>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; };
static const char* CleanupDocument = nullptr;
static int CleanupBudget = -1;
static bool CleanupCallback = false;
static void Require(bool valid, const char* message) {
    if (!valid) {
        if (CleanupDocument) std::fprintf(stderr, "Parser document=%s callback=%d budget=%d\n", CleanupDocument, CleanupCallback, CleanupBudget);
        std::fprintf(stderr, "%s\n", message); std::abort();
    }
}
struct ParserTrace : std::map<void*, std::size_t> {
    int budget = -1;
    bool once = false;
    unsigned notifications = 0;
    std::size_t requests = 0;
};
static void* APS5_VABI AllocateTracked(void*, std::size_t size, void* context) {
    auto& trace = *static_cast<ParserTrace*>(context);
    ++trace.requests;
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(size);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    static_cast<ParserTrace*>(context)->emplace(memory, size);
    return memory;
}
static void APS5_VABI FreeTracked(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<ParserTrace*>(context)->erase(memory) == 1, "Parser allocations must be freed exactly once by their owning allocator");
    std::free(memory);
}
static void APS5_VABI NotifyTracked(void*, int error, std::size_t, void* context) {
    Require(static_cast<unsigned>(error) == 0x80848102, "Parser allocation failures must report the Json2 out-of-memory error");
    ++static_cast<ParserTrace*>(context)->notifications;
}
static int Parse(Value& value, const char* text) {
    return _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text, std::strlen(text));
}
static int APS5_VABI ReadNext(char& character, void* context) {
    auto& cursor = *static_cast<const char**>(context);
    if (!*cursor) return -1;
    character = *cursor++;
    return 0;
}
static void CheckArrayEntryFailures(ParserTrace& live, std::size_t baseline) {
    for (bool callback : {false, true}) for (bool valid : {false, true}) {
        bool completed = false;
        unsigned failures = 0;
        for (int budget = 0; budget < 32 && !completed; ++budget) {
            Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
            Require(Parse(value, "[]") == 0, "Prepare an owned empty array before element admission");
            const char* document = valid ? "[1]" : "[1,";
            const char* cursor = document;
            const auto notifications = live.notifications;
            live.budget = budget;
            const int status = callback ? _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&value, ReadNext, &cursor) : Parse(value, document);
            live.budget = -1;
            const bool failed = live.notifications != notifications;
            const int expected = failed ? static_cast<int>(0x80848102) : valid ? 0 : callback ? -1 : static_cast<int>(0x80848101);
            Require(status == expected, "Array element allocation and reader failures must preserve their public status");
            if (!failed) {
                completed = true;
                Require(value.type == 6 && _ZNK3sce4Json5Value5countEv(&value) == (valid ? 1 : 2), "Complete and malformed array parsing must retain the admitted entry count");
                const auto* first = _ZNK3sce4Json5Array5frontEv(reinterpret_cast<const Array*>(value.payload));
                Require(first->type == 3 && first->payload == 1 && first->parent == &value, "The parsed element must remain owned by its destination array");
            } else {
                ++failures;
                Require(Parse(value, "false") == 0 && Parse(value, "[]") == 0 && Parse(value, "[1]") == 0 && _ZNK3sce4Json5Value5countEv(&value) == 1,
                        "A failed array parse must permit cleanup and successful reuse of the same value");
            }
            _ZN3sce4Json5ValueD1Ev(&value);
            Require(live.size() == baseline, "Element admission and callback parsing must leave no unreachable guest allocations after cleanup");
        }
        Require(completed && failures != 0, "Every reader and syntax case must exercise guest allocation failure and complete admission");
    }
}
static void CheckOwnedTree(const Value& value, const Value* parent = nullptr, bool partial = false) {
    Require(value.parent == parent || (partial && value.type == TypeNull && !value.parent && !value.auxiliary),
            "Admitted payloads must retain their owner; interrupted null placeholders may remain unbound");
    if (value.type == TypeArray) {
        const auto count = _ZNK3sce4Json5Value5countEv(&value);
        for (std::size_t index = 0; index < count; ++index) {
            const auto* child = _ZNK3sce4Json5ValueixEm(&value, index);
            Require(child != nullptr, "Admitted array entries must remain reachable");
            CheckOwnedTree(*child, &value, partial);
        }
    } else if (value.type == TypeObject) {
        const auto* object = _ZNK3sce4Json5Value9getObjectEv(&value);
        ObjectIterator position{}, end{};
        _ZNK3sce4Json6Object5beginEv(&position, object);
        _ZNK3sce4Json6Object3endEv(&end, object);
        std::size_t count = 0;
        for (; _ZNK3sce4Json6Object8iteratorneERKS2_(&position, &end); _ZN3sce4Json6Object8iteratorppEv(&position)) {
            CheckOwnedTree(_ZNK3sce4Json6Object8iteratordeEv(&position)->value, &value, partial);
            ++count;
        }
        _ZN3sce4Json6Object8iteratorD1Ev(&position);
        _ZN3sce4Json6Object8iteratorD1Ev(&end);
        Require(count == _ZNK3sce4Json5Value5countEv(&value), "Every admitted object entry must remain reachable");
    }
}
static void CheckParserCleanup(ParserTrace& live, std::size_t baseline) {
    struct Case { const char* document; bool valid; const char* retained; };
    const Case cases[]{
        {"[1,{\"a\":[true,null]}", false, "[1,{\"a\":[true,null]}]"},
        {"[]", true, "[]"}, {"{}", true, "{}"}, {"[1]", true, "[1]"},
        {"{\"a\":1}", true, "{\"a\":1}"},
        {"[1,{\"long\":\"a sufficiently long string to require its own allocation\"},[false,null]]", true,
         "[1,{\"long\":\"a sufficiently long string to require its own allocation\"},[false,null]]"},
        {"{\"long\":\"a sufficiently long string to require its own allocation\",\"nested\":[{\"leaf\":\"another sufficiently long string\"},[true,null,17]]}", true,
         "{\"long\":\"a sufficiently long string to require its own allocation\",\"nested\":[{\"leaf\":\"another sufficiently long string\"},[true,null,17]]}"},
        {"{\"a sufficiently long key to require its own allocation\":{\"child\":[1,2]}}", true,
         "{\"a sufficiently long key to require its own allocation\":{\"child\":[1,2]}}"},
        {"{\"a\":[{\"b\":\"a sufficiently long string allocation\"},[true,null,17]]", false,
         "{\"a\":[{\"b\":\"a sufficiently long string allocation\"},[true,null,17]]}"},
        {"{\"a\":[1]}x", true, "{\"a\":[1]}"},
        {"[{\"a\":[1]}", false, "[{\"a\":[1]}]"},
    };
    unsigned failures = 0, refusals = 0;
    for (bool callback : {false, true}) for (const auto& test : cases) {
        bool completed = false;
        for (int budget = 0; budget < 128 && !completed; ++budget) {
            CleanupDocument = test.document; CleanupBudget = budget; CleanupCallback = callback;
            Value value{}; _ZN3sce4Json5ValueC1EPKc(&value, "previous");
            const auto notifications = live.notifications;
            const char* cursor = test.document;
            live.budget = budget;
            int status = 0;
            bool refused = false;
            try {
                status = callback ? _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&value, ReadNext, &cursor) : Parse(value, test.document);
            } catch (const std::runtime_error& error) {
                if (std::strcmp(error.what(), "value not implemented") != 0 || live.notifications == notifications)
                    std::fprintf(stderr, "Parser document=%s callback=%d budget=%d refusal=%s\n", test.document, callback, budget, error.what());
                Require(std::strcmp(error.what(), "value not implemented") == 0 && live.notifications != notifications,
                        "Only the documented allocation-failed array-sentinel path may refuse parsing");
                refused = true;
                ++refusals;
            }
            live.budget = -1;
            const bool failed = live.notifications != notifications;
            if (!refused) {
                const int expected = failed ? static_cast<int>(0x80848102) : test.valid ? 0 : callback ? -1 : static_cast<int>(0x80848101);
                Require(status == expected, "Guest allocation and syntax failures must retain their public status");
                CheckOwnedTree(value, nullptr, failed);
            }
            if (!failed) {
                completed = true;
                String output{}; _ZN3sce4Json6StringC1Ev(&output);
                Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0 &&
                            std::strcmp(_ZNK3sce4Json6String5c_strEv(&output), test.retained) == 0,
                        "Complete and malformed documents must retain the expected tree");
                _ZN3sce4Json6StringD1Ev(&output);
            } else {
                ++failures;
                Require(Parse(value, "false") == 0 && Parse(value, "[1]") == 0,
                        "Interrupted parsing must permit cleanup and reuse of the same destination");
                CheckOwnedTree(value, nullptr, failed);
            }
            _ZN3sce4Json5ValueD1Ev(&value);
            Require(live.size() == baseline, "Parser cleanup must leave no unreachable guest allocations");
        }
        Require(completed, "Every document and reader must reach its allocation-complete result");
    }
    CleanupDocument = nullptr;
    Require(failures != 0, "Parser cleanup must exercise guest allocation failures");
    std::printf("Parser cleanup: %u guest allocation failures, %u documented sentinel refusals, no leaks\n", failures, refusals);
}
int main() {
    JsonTest::Allocator allocator;
    void* const vtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateTracked), reinterpret_cast<void*>(FreeTracked), reinterpret_cast<void*>(NotifyTracked)};
    allocator.vtable = vtable;
    ParserTrace live;
    Parameter parameter{&allocator, &live, 512};
    std::uint64_t initializer[2]{};
    _ZN3sce4Json11InitializerC1Ev(initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer, &parameter) == 0, "Initialize parser state tests");
    const auto baseline = live.size();
    CheckParserCleanup(live, baseline);
    CheckArrayEntryFailures(live, baseline);
    struct FailureCase { bool once; int budget; int type; unsigned notifications; std::size_t requests; const char* text; };
    const FailureCase failures[]{
        {false, 0, 0, 2, 2, ""}, {false, 1, 0, 2, 3, ""},
        {false, 2, 5, 1, 3, ""}, {false, 3, 5, 1, 4, ""},
        {false, 4, 5, 9, 13, ""}, {false, 5, 5, 0, 5, "abcdefghi"},
        {true, 0, 0, 1, 3, ""}, {true, 1, 0, 1, 4, ""},
        {true, 2, 5, 1, 3, ""}, {true, 3, 5, 1, 4, ""},
        {true, 4, 5, 1, 6, "bcdefghi"}, {true, 5, 5, 0, 5, "abcdefghi"},
    };
    for (const auto& failure : failures) {
        Value partial{}; _ZN3sce4Json5ValueC1El(&partial, 77);
        const auto notifications = live.notifications;
        const auto requests = live.requests;
        live.budget = failure.budget; live.once = failure.once;
        const auto status = static_cast<unsigned>(Parse(partial, "\"abcdefghi\""));
        live.budget = -1; live.once = false;
        Require(status == (failure.notifications ? 0x80848102u : 0u) && partial.type == failure.type, "String parser allocation failures must retain the reference type and status");
        Require(live.notifications - notifications == failure.notifications && live.requests - requests == failure.requests, "String parser failures must follow storage-before-metadata admission and character retries");
        if (partial.type == 5) Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(partial.payload)), failure.text) == 0, "Failed string parsing must retain the admitted character suffix");
        _ZN3sce4Json5ValueD1Ev(&partial);
        Require(live.size() == baseline, "String parser failure must reclaim partial storage and metadata");
    }
    struct ArrayFailure { bool once; int budget; int type; std::size_t count; std::uint64_t number; int lastType; const char* text; unsigned notifications; std::size_t requests; };
    const ArrayFailure arrayFailures[]{
        {0, 0, 0, 0, 0, 0, "", 2, 2},
        {0, 1, 0, 0, 0, 0, "", 2, 3},
        {0, 3, 6, 0, 0, 0, "", 2, 5},
        {0, 4, 6, 0, 0, 0, "", 2, 6},
        {0, 5, 6, 0, 0, 0, "", 1, 6},
        {0, 6, 6, 0, 0, 0, "", 2, 8},
        {0, 7, 6, 0, 0, 0, "", 2, 9},
        {0, 8, 6, 0, 0, 0, "", 1, 9},
        {0, 9, 6, 2, 1, 0, "", 1, 10},
        {0, 10, 6, 2, 1, 0, "", 1, 11},
        {0, 11, 6, 2, 1, 5, "", 3, 14},
        {0, 12, 6, 2, 1, 5, "abc", 0, 12},
        {1, 0, 0, 0, 0, 0, "", 1, 3},
        {1, 1, 0, 0, 0, 0, "", 1, 4},
        {1, 3, 6, 0, 0, 0, "", 1, 5},
        {1, 4, 6, 0, 0, 0, "", 1, 6},
        {1, 5, 6, 0, 0, 0, "", 1, 6},
        {1, 6, 6, 1, 0, 0, "", 1, 8},
        {1, 7, 6, 1, 0, 0, "", 1, 9},
        {1, 8, 6, 0, 0, 0, "", 1, 9},
        {1, 9, 6, 2, 1, 0, "", 1, 10},
        {1, 10, 6, 2, 1, 0, "", 1, 11},
        {1, 11, 6, 2, 1, 5, "bc", 1, 13},
        {1, 12, 6, 2, 1, 5, "abc", 0, 12},
    };
    for (const auto& failure : arrayFailures) {
        Value partial{}; _ZN3sce4Json5ValueC1El(&partial, 77);
        const auto notifications = live.notifications;
        const auto requests = live.requests;
        live.budget = failure.budget; live.once = failure.once;
        const auto status = static_cast<unsigned>(Parse(partial, "[1,\"abc\"]"));
        live.budget = -1; live.once = false;
        Require(status == (failure.notifications ? 0x80848102u : 0u) && partial.type == failure.type, "Array parser allocation failures must retain the reference status and type");
        Require(live.notifications - notifications == failure.notifications && live.requests - requests == failure.requests, "Array parser failures must account for numeric-token storage and inherited append errors");
        if (partial.type == 6) {
            Require(_ZNK3sce4Json5Value5countEv(&partial) == failure.count, "Array parser failures must retain the reference partial count");
            if (failure.count) {
                const auto* array = reinterpret_cast<const Array*>(partial.payload);
                const auto* first = _ZNK3sce4Json5Array5frontEv(array);
                const auto* last = _ZNK3sce4Json5Array4backEv(array);
                Require(first->type == 3 && first->payload == failure.number && first->parent == &partial && last->parent == &partial, "Array parser failures must retain numeric values and admitted parent links");
                if (failure.count == 2) {
                    Require(last->type == failure.lastType, "Array parser failures must preserve the final admitted value type");
                    if (last->type == 5) Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(last->payload)), failure.text) == 0, "Array parser failures must retain the admitted string suffix");
                }
            }
        }
        _ZN3sce4Json5ValueD1Ev(&partial);
        Require(live.size() == baseline, "Array parser failure must reclaim all admitted resources");
    }
    for (const char* text : {"", "?", "tru"}) {
        Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
        Require(static_cast<unsigned>(Parse(value, text)) == 0x80848101 && value.type == 2 && value.payload == 77 && !value.auxiliary, "Unrecognized or incomplete scalar input must preserve the previous payload");
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    for (const char* text : {"[1,", "[1,?]"}) {
        Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
        Require(static_cast<unsigned>(Parse(value, text)) == 0x80848101 && value.type == 6 && _ZNK3sce4Json5Value5countEv(&value) == 2, "Failed array parsing must retain admitted entries instead of restoring the destination");
        auto* array = reinterpret_cast<const Array*>(value.payload);
        const auto* first = _ZNK3sce4Json5Array5frontEv(array);
        const auto* last = _ZNK3sce4Json5Array4backEv(array);
        Require(first->type == 3 && first->payload == 1 && first->parent == &value && last->type == 0, "Partial array entries must retain their payload and destination parent");
        Require(Parse(value, "[]") == 0 && _ZNK3sce4Json5Value5countEv(&value) == 2, "Parsing an empty array into an existing array must retain its entries");
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    for (const char* text : {"{\"a\":1,", "{\"a\":?}"}) {
        Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
        Require(static_cast<unsigned>(Parse(value, text)) == 0x80848101 && value.type == 7 && _ZNK3sce4Json5Value5countEv(&value) == 1, "Failed object parsing must retain the admitted member");
        Require(Parse(value, "{}") == 0 && _ZNK3sce4Json5Value5countEv(&value) == 1, "Parsing an empty object into an existing object must retain its members");
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    Value missingMember{}; _ZN3sce4Json5ValueC1El(&missingMember, 77);
    Require(static_cast<unsigned>(Parse(missingMember, "{\"a\":}")) == 0x80848101 &&
                _ZNK3sce4Json5Value7getTypeEv(&missingMember) == 7,
            "A missing member value must report parse failure while retaining the object type");
    Require(Parse(missingMember, "{}") == 0 && _ZNK3sce4Json5Value7getTypeEv(&missingMember) == 7,
            "The retained object must remain usable after a missing-value parse error");
    _ZN3sce4Json5ValueD1Ev(&missingMember);
    Require(live.size() == baseline, "The missing-value error and retry must reclaim admitted storage");
    Value nested{}; _ZN3sce4Json5ValueC1El(&nested, 77);
    Require(static_cast<unsigned>(Parse(nested, "[[1,")) == 0x80848101 && nested.type == 6, "Nested failures must retain the outer collection");
    const auto* inner = _ZNK3sce4Json5Array5frontEv(reinterpret_cast<const Array*>(nested.payload));
    Require(inner->type == 6 && inner->parent == &nested && _ZNK3sce4Json5Value5countEv(inner) == 2, "Nested partial collections must retain their immediate owner");
    const auto* leaf = _ZNK3sce4Json5Array5frontEv(reinterpret_cast<const Array*>(inner->payload));
    Require(leaf->parent == inner && leaf->payload == 1, "Nested leaf parents must point into the retained destination tree");
    Require(Parse(nested, "42") == 0 && nested.type == 3 && nested.payload == 42, "Numeric parsing must replace an existing collection");
    _ZN3sce4Json5ValueD1Ev(&nested);
    Value object{}; _ZN3sce4Json5ValueC1El(&object, 77);
    Require(Parse(object, "{\"a\":9}") == 0 && static_cast<unsigned>(Parse(object, "{\"a\":?}")) == 0x80848101, "Prepare duplicate member failure");
    const auto* retained = _ZNK3sce4Json5ValueixEPKc(&object, "a");
    Require(retained->type == 3 && retained->payload == 9 && retained->parent == &object, "An unrecognized duplicate member value must preserve the prior member");
    _ZN3sce4Json5ValueD1Ev(&object);
    Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
    Require(static_cast<unsigned>(Parse(value, "\"unterminated")) == 0x80848101 && value.type == 5, "Unterminated strings must change the destination type");
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value.payload)), "unterminated") == 0, "Unterminated strings must retain their decoded prefix");
    Require(Parse(value, "\"!\"") == 0 && std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value.payload)), "unterminated!") == 0, "Parsing another string must append to existing string storage");
    Require(Parse(value, "true") == 0 && Parse(value, "false") == 0 && value.type == 1 && !(value.payload & 255), "Parsing false must replace an existing true boolean");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(live.size() == baseline, "Partial parsing and numeric replacement must reclaim all value allocations");
    Require(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0, "Terminate parser state tests");
    Require(live.empty(), "Termination must reclaim the remaining parser context resources");
    return 0;
}
