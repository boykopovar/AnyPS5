#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <initializer_list>
#include <set>
#include <vector>
#include <string>
#include <bit>
#include <limits>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using LegacyNullCallback = const Value& (APS5_VABI*)(ValueType, const Value*, void*);
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct ArrayTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    int budget = -1;
    bool once = false;
    unsigned notifications = 0;
    std::set<void*> headers;
};
static void* APS5_VABI AllocateArray(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ArrayTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); return memory;
}
static void APS5_VABI FreeArray(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<ArrayTrace*>(context);
    if (trace.headers.erase(memory)) {
        const auto* words = reinterpret_cast<const std::uint64_t*>(memory);
        Require(words[0] == 0xffffffff && words[1] == UINT64_MAX, "Array teardown must expose the native header poison before allocator release");
    }
    Require(trace.live.erase(memory) == 1, "Array must release through its owning allocator"); std::free(memory);
}
static void APS5_VABI NotifyArray(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ArrayTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "Array notification must identify the failed allocation"); ++trace.notifications;
}
static void* const ArrayVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateArray), reinterpret_cast<void*>(FreeArray), reinterpret_cast<void*>(NotifyArray)};
struct ArrayAllocator { void* const* vtable = ArrayVtable; };
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    alignas(8) static const std::uint32_t parameters[]{0, 0, 0, 0, 0x05000000, 0};
    return parameters;
}
struct LegacySerializationTrace { std::vector<std::string> chunks; };
static int APS5_VABI ObserveLegacy(String& output, void* context) {
    static_cast<LegacySerializationTrace*>(context)->chunks.emplace_back(_ZNK3sce4Json6String5c_strEv(&output));
    return 23;
}
static const Value& APS5_VABI LegacyNull(ValueType, const Value*, void* context) {
    Require(false, "Legacy serialization must clear the root null-access callback before later recovery");
    return *static_cast<const Value*>(context);
}
int main() {
    ArrayTrace trace; ArrayAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&initializer, &parameter) == 0, "Initialize legacy serialization contracts");
    const auto baseline = trace.live.size();
    struct Case { double value; const char* text; };
    const Case cases[]{
        {0.0, "0"}, {-0.0, "0"}, {1.0, "1"}, {1.25, "1.250000"},
        {0.0000001, "0"}, {-0.0000001, "-0"}, {-0.0000005, "-0"},
        {1.0000005, "1"}, {1.0000015, "1.000001"}, {1234567.875, "1234567.875000"},
        {1e8, "100000000"}, {1e10, "2147483647"}, {1e20, "2147483647"}, {-1e20, "-2147483647"},
        {std::numeric_limits<double>::infinity(), "2147483647"}, {-std::numeric_limits<double>::infinity(), "-2147483647"},
        {std::numeric_limits<double>::quiet_NaN(), "9223372036854775808"},
        {std::bit_cast<double>(UINT64_C(0xfff8000000000001)), "9223372036854775808"},
    };
    for (const auto& test : cases) for (int mode : {0, 1, 2, 3}) {
        Value value{}; _ZN3sce4Json5ValueC1Ed(&value, test.value);
        String output{}; _ZN3sce4Json6StringC1EPKc(&output, "p");
        const auto requests = trace.requests.size();
        LegacySerializationTrace callback;
        int status = 0;
        if (mode == 0) status = _ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output);
        else if (mode == 1) status = _ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(&value, ObserveLegacy, &callback);
        else if (mode == 2) status = _ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(&value, &output, ObserveLegacy, &callback);
        else _ZNK3sce4Json5Value8toStringERNS0_6StringE(&value, &output);
        Require(value.type == 4 && value.payload == std::bit_cast<std::uint64_t>(test.value), "Formatting must preserve the original double payload bits, including signed zero and NaN payloads");
        const std::string expected = std::string("p") + test.text;
        Require(status == (mode == 1 || mode == 2 ? 23 : 0), "Legacy serialization must preserve callback return values");
        Require(std::string(_ZNK3sce4Json6String5c_strEv(&output)) == (mode == 1 ? "p" : expected), "Legacy real formatting must preserve float32 conversion, truncation, sign and range behavior");
        if (mode == 1 || mode == 2) Require(callback.chunks == std::vector<std::string>{mode == 1 ? std::string(test.text) : expected}, "Legacy callback serialization must emit the same formatted scalar through its selected output mode");
        std::vector<std::size_t> allocations;
        const auto length = std::strlen(test.text);
        if (mode != 3) {
            allocations = {24, 24, 32, (length + 1) | 16};
            Require(!value.parent && value.auxiliary && trace.live.at(value.auxiliary) == 24, "Legacy serialization must admit the root's allocator-backed auxiliary record");
            auto* fallback = reinterpret_cast<void**>(value.auxiliary)[2];
            Require(fallback && trace.live.at(fallback) == 32 && reinterpret_cast<Value*>(fallback)->parent == &value, "The legacy root fallback must be allocator-backed and bound to its owning root");
        } else Require(!value.auxiliary, "Direct toString formatting must not perform serialization's metadata admission");
        if (mode != 1) allocations.push_back((length + 2) | 16);
        Require(std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == allocations, "Legacy formatting and metadata admission must preserve the observed allocation order and request sizes");
        _ZN3sce4Json6StringD1Ev(&output); _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live.size() == baseline, "Legacy scalar serialization must release all buffers and root metadata on destruction");
    }
    for (bool once : {false, true}) for (int budget = 0; budget <= 5; ++budget) {
        Value value{}; _ZN3sce4Json5ValueC1Ed(&value, 1.25);
        String output{}; _ZN3sce4Json6StringC1EPKc(&output, "p");
        const auto requests = trace.requests.size(); const auto notices = trace.notifications;
        trace.budget = budget; trace.once = once;
        const int status = _ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output);
        trace.budget = -1; trace.once = false;
        const bool complete = budget >= 5 || (once && (budget == 1 || budget == 2));
        Require(status == 0 && std::string(_ZNK3sce4Json6String5c_strEv(&output)) == (complete ? "p1.250000" : "p"), "Legacy serialization must retain its observed partial output rather than inventing an out-of-memory return");
        const bool hasAuxiliary = budget >= 2 || (once && budget == 0);
        Require((value.auxiliary != nullptr) == hasAuxiliary, "Rejected metadata admission must preserve the native partial auxiliary ownership");
        if (value.auxiliary) Require((reinterpret_cast<void**>(value.auxiliary)[2] != nullptr) == (budget >= 3 || (once && budget == 0)), "Fallback rejection must retain the admitted auxiliary record without fabricating a fallback");
        std::vector<std::size_t> expected{24, 24};
        if (hasAuxiliary) expected.push_back(32);
        if (budget != 0) expected.push_back(25);
        if (budget >= 4 || (once && (budget == 1 || budget == 2))) expected.push_back(26);
        const unsigned failed = once ? budget < 5 ? 1 : 0 : budget < 3 ? 2 : budget < 5 ? 1 : 0;
        Require(std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected && trace.notifications == notices + failed, "Legacy metadata and output failures must preserve exact allocator request order and notification counts");
        _ZN3sce4Json6StringD1Ev(&output); _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live.size() == baseline, "Every partial legacy serialization state must remain destructible without leaked ownership");
    }
    Value registered{}; _ZN3sce4Json5ValueC1El(&registered, 7);
    String registeredOutput{}; _ZN3sce4Json6StringC1EPKc(&registeredOutput, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&registered, &registeredOutput) == 0, "Admit metadata before registering a legacy root callback");
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&registered, LegacyNull, &registered) == 0, "Register an existing legacy root null callback");
    void* registeredAuxiliary = registered.auxiliary;
    void* registeredFallback = reinterpret_cast<void**>(registeredAuxiliary)[2];
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&registered, &registeredOutput) == 0 && registered.auxiliary == registeredAuxiliary && reinterpret_cast<void**>(registeredAuxiliary)[2] == registeredFallback && !reinterpret_cast<void**>(registeredAuxiliary)[0] && !reinterpret_cast<void**>(registeredAuxiliary)[1], "Legacy serialization must reset existing root callback fields while preserving auxiliary and fallback identity");
    _ZN3sce4Json6StringD1Ev(&registeredOutput); _ZN3sce4Json5ValueD1Ev(&registered);
    Require(trace.live.size() == baseline, "Reset legacy root handlers must retain normal cleanup ownership");
    Value root{}; _ZN3sce4Json5ValueC1Ev(&root);
    const char json[] = "[1,2]";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, json, sizeof(json) - 1) == 0, "Parse legacy collection metadata input");
    auto* child = const_cast<Value*>(_ZNK3sce4Json5ValueixEm(&root, 0));
    String first{}; _ZN3sce4Json6StringC1EPKc(&first, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(child, &first) == 0 && !child->parent && child->auxiliary, "Serializing a legacy child as a root must detach its parent and admit root metadata");
    void* childAuxiliary = child->auxiliary;
    void* childFallback = reinterpret_cast<void**>(childAuxiliary)[2];
    String whole{}; _ZN3sce4Json6StringC1EPKc(&whole, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&root, &whole) == 0 && std::strcmp(_ZNK3sce4Json6String5c_strEv(&whole), "[1,2]") == 0, "Legacy collection serialization must preserve its actual contents");
    Require(child->parent == &root && !child->auxiliary && !trace.live.count(childAuxiliary) && !trace.live.count(childFallback), "Serializing the parent must rebind the child and release its former root auxiliary record and fallback");
    _ZN3sce4Json6StringD1Ev(&first); _ZN3sce4Json6StringD1Ev(&whole); _ZN3sce4Json5ValueD1Ev(&root);
    Require(trace.live.size() == baseline && _ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Legacy metadata transitions must leave no outstanding ownership");
    return 0;
}
