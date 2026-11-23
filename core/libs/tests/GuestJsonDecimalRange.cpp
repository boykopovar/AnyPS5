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
#include <utility>
#include <bit>
#include <cmath>
#include <limits>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using LookupCallback = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);
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
int main() {
    ArrayTrace trace; ArrayAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize decimal underflow contracts");
    const auto baseline = trace.live.size();
    struct Case { const char* text; std::uint64_t bits; };
    const Case cases[]{
        {"1e309", UINT64_C(0x7ff0000000000000)}, {"-1e309", UINT64_C(0xfff0000000000000)},
        {"1.25e30", UINT64_C(0x462f8def8808b023)}, {"18446744073709551616", UINT64_C(0x43f0000000000000)},
        {"1e-999", 0}, {"-1e-999", UINT64_C(0x8000000000000000)},
        {"2.4703282292062327e-324", 0}, {"-2.4703282292062327e-324", UINT64_C(0x8000000000000000)},
        {"2.4703282292062328e-324", 0}, {"-2.4703282292062328e-324", UINT64_C(0x8000000000000000)},
        {"4.9406564584124654e-324", 1}, {"-4.9406564584124654e-324", UINT64_C(0x8000000000000001)},
        {"2.2250738585072014e-308", UINT64_C(0x0010000000000003)},
        {"1.7976931348623157e308", UINT64_C(0x7feffffffffffff9)},
    };
    for (const auto& test : cases) {
        Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, test.text, std::strlen(test.text)) == 0, "Decimal range conversion must preserve native admission");
        Require(value.type == 4 && value.payload == test.bits, "Decimal conversion must preserve the reference arithmetic, subnormal bits and the sign of underflowed zero");
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live.size() == baseline, "Underflow parsing must release token, value and auxiliary ownership");
    }
    for (const auto& test : {std::pair{"[-1e309]", "[-inf]"}, std::pair{"{\"a\":[1,1e400]}", "{\"a\":[1,inf]}"}}) {
        Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
        String output{}; _ZN3sce4Json6StringC1EPKc(&output, "");
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, test.first, std::strlen(test.first)) == 0 && _ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0 && std::strcmp(_ZNK3sce4Json6String5c_strEv(&output), test.second) == 0, "Nested decimal overflow must preserve the reference collection and real-conversion behavior");
        _ZN3sce4Json6StringD1Ev(&output); _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live.size() == baseline, "Nested overflow must release all token, collection and auxiliary storage");
    }
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Terminate decimal range allocation context");
    for (int mode : {0, 1}) {
        parameter.mode = mode;
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize overflow conversion settings");
        const auto resources = trace.live.size();
        for (const char* text : {"1e309", "-1e309"}) {
            Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
            Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text, std::strlen(text)) == 0, "Overflow conversion must honor the configured special-float mode");
            if (mode == 0) Require(value.type == 5 && value.payload && std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value.payload)), text[0] == '-' ? "-inf" : "inf") == 0, "String mode must convert overflowing decimal values through the normal real setter");
            else Require(value.type == 0, "Null mode must convert overflowing decimal values to null");
            _ZN3sce4Json5ValueD1Ev(&value);
            Require(trace.live.size() == resources, "Overflow conversion must release admitted value and auxiliary storage");
        }
        Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Terminate overflow conversion settings");
    }
    return 0;
}
