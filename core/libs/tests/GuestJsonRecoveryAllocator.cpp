#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <limits>
#include <bit>
#include <string>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static_assert(sizeof(Value) == 32 && sizeof(Parameter) == 40);
using FloatCallback = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
extern "C" {
Value* APS5_VABI _ZN3sce4Json5Value10referValueERKNS0_6StringE(Value*, const String*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value*, double);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value*, String*);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, FloatCallback, void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct RecoveryAllocationTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    int budget = -1;
    bool once = false;
    unsigned notifications = 0;
    std::map<void*, std::size_t> ids;
    std::size_t nextId = 0;
};
static void* APS5_VABI AllocateRecovery(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<RecoveryAllocationTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); trace.ids[memory] = ++trace.nextId; return memory;
}
static void APS5_VABI FreeRecovery(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<RecoveryAllocationTrace*>(context);
    Require(trace.ids.count(memory) == 1, "Freed recovery storage must identify a currently live allocation generation");
    trace.ids.erase(memory);
    Require(trace.live.erase(memory) == 1, "Recovery storage must release through its owning allocator"); std::free(memory);
}
static void APS5_VABI NotifyRecovery(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<RecoveryAllocationTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "Recovery notification must identify the failed allocation"); ++trace.notifications;
}
static void* const RecoveryVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateRecovery), reinterpret_cast<void*>(FreeRecovery), reinterpret_cast<void*>(NotifyRecovery)};
struct RecoveryAllocator { void* const* vtable = RecoveryVtable; };
struct RecoveryContext { const Value* replacement; bool object; unsigned calls = 0; int function = -1; bool errorPointer = false; };
static Value* APS5_VABI RecoverAllocated(Value* result, std::int32_t function, double number, const Value* parent, void* context, bool* error) {
    auto& trace = *static_cast<RecoveryContext*>(context);
    ++trace.calls; trace.function = function; trace.errorPointer = error != nullptr;
    Require(number == std::numeric_limits<double>::infinity() && !parent, "Recovery must receive the original non-finite value and immediate parent");
    if (trace.object) _ZN3sce4Json5ValueC1ERKS1_(result, trace.replacement);
    else _ZN3sce4Json5ValueC1EPKc(result, "recovery");
    if (error) *error = false;
    return result;
}
static std::string Contents(Value& value) {
    if (value.type == 0) return "null";
    if (value.type == 4) { Require(value.payload == std::bit_cast<std::uint64_t>(std::numeric_limits<double>::infinity()), "Serialization recovery must preserve the source real payload bits"); return "inf"; }
    if (value.type == 5) return _ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value.payload));
    String output{}; _ZN3sce4Json6StringC1EPKc(&output, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0, "Partial callback-return objects must remain serializable");
    std::string result = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return result;
}
struct Expected {
    bool object; bool format; bool once; int budget; int type;
    const char* contents; const char* output; std::size_t requests; unsigned notifications; std::uint64_t order; int lookupType;
};
static const Expected Cases[]{
    {0, 0, 0, 0, 0, "null", "p", 1, 1, UINT64_C(0xaf63d54c8601e0a7), -2},
    {0, 0, 0, 1, 0, "null", "p", 3, 2, UINT64_C(0xa9485c18e1e33846), -2},
    {0, 0, 0, 2, 0, "null", "p", 3, 1, UINT64_C(0xa9485c18e1e33846), -2},
    {0, 0, 0, 3, 0, "null", "p", 4, 1, UINT64_C(0x892cdc47d518c7ba), -2},
    {0, 0, 0, 4, 0, "null", "p", 6, 2, UINT64_C(0xaebceda7a971a713), -2},
    {0, 0, 0, 5, 0, "null", "p", 6, 1, UINT64_C(0xaebceda7a971a713), -2},
    {0, 0, 0, 6, 0, "null", "p", 7, 1, UINT64_C(0x5caedee4ec1ed7b1), -2},
    {0, 0, 0, 7, 0, "null", "p", 9, 2, UINT64_C(0x768bd046d9a8a6e0), -2},
    {0, 0, 0, 8, 0, "null", "p", 9, 1, UINT64_C(0x768bd046d9a8a6e0), -2},
    {0, 0, 0, 9, 0, "null", "p", 10, 1, UINT64_C(0x1839e063d993b768), -2},
    {0, 0, 0, 10, 5, "", "p", 11, 1, UINT64_C(0xbe0faaaab60099d3), -2},
    {0, 0, 0, 11, 5, "recovery", "p", 11, 0, UINT64_C(0xbe0faaaab60099d3), -2},
    {0, 0, 1, 0, 0, "null", "p", 1, 1, UINT64_C(0xaf63d54c8601e0a7), -2},
    {0, 0, 1, 1, 5, "", "p", 8, 1, UINT64_C(0xfc01dbf4e232edfa), -2},
    {0, 0, 1, 2, 0, "null", "p", 3, 1, UINT64_C(0xa9485c18e1e33846), -2},
    {0, 0, 1, 3, 0, "null", "p", 4, 1, UINT64_C(0x892cdc47d518c7ba), -2},
    {0, 0, 1, 4, 5, "", "p", 9, 1, UINT64_C(0x768f2646d9ab6ed9), -2},
    {0, 0, 1, 5, 0, "null", "p", 6, 1, UINT64_C(0xaebceda7a971a713), -2},
    {0, 0, 1, 6, 0, "null", "p", 7, 1, UINT64_C(0x5caedee4ec1ed7b1), -2},
    {0, 0, 1, 7, 5, "", "p", 10, 1, UINT64_C(0x1839e063d993b768), -2},
    {0, 0, 1, 8, 0, "null", "p", 9, 1, UINT64_C(0x768bd046d9a8a6e0), -2},
    {0, 0, 1, 9, 0, "null", "p", 10, 1, UINT64_C(0x1839e063d993b768), -2},
    {0, 0, 1, 10, 5, "", "p", 11, 1, UINT64_C(0xbe0faaaab60099d3), -2},
    {0, 0, 1, 11, 5, "recovery", "p", 11, 0, UINT64_C(0xbe0faaaab60099d3), -2},
    {0, 1, 0, 0, 4, "inf", "p", 2, 2, UINT64_C(0x08843007b530e48d), -2},
    {0, 1, 0, 1, 4, "inf", "p", 3, 2, UINT64_C(0xa9823518e2146e48), -2},
    {0, 1, 0, 2, 4, "inf", "p", 5, 3, UINT64_C(0xc691469ccd6898fb), -2},
    {0, 1, 0, 3, 4, "inf", "p", 5, 2, UINT64_C(0xc691469ccd6898fb), -2},
    {0, 1, 0, 4, 4, "inf", "p", 6, 2, UINT64_C(0xd162e67108ad626b), -2},
    {0, 1, 0, 5, 4, "inf", "p", 8, 3, UINT64_C(0xac5d752721a88d90), -2},
    {0, 1, 0, 6, 4, "inf", "p", 8, 2, UINT64_C(0xac5d752721a88d90), -2},
    {0, 1, 0, 7, 4, "inf", "p", 9, 2, UINT64_C(0x8b87da7e318e2776), -2},
    {0, 1, 0, 8, 4, "inf", "p", 11, 3, UINT64_C(0x7c2105433836840e), -2},
    {0, 1, 0, 9, 4, "inf", "p", 19, 10, UINT64_C(0xaef2a03652afb7be), -2},
    {0, 1, 0, 10, 4, "inf", "p", 11, 1, UINT64_C(0x7c20ff43383679dc), -2},
    {0, 1, 0, 11, 4, "inf", "p\"recovery\"", 11, 0, UINT64_C(0x7c20ff43383679dc), -2},
    {0, 1, 1, 0, 4, "inf", "p", 9, 1, UINT64_C(0x8b87d67e318e20aa), -2},
    {0, 1, 1, 1, 4, "inf", "pnull", 4, 1, UINT64_C(0x1cae974828b789ba), -2},
    {0, 1, 1, 2, 4, "inf", "p\"\"", 9, 1, UINT64_C(0x60e103758cc7062a), -2},
    {0, 1, 1, 3, 4, "inf", "pnull", 6, 1, UINT64_C(0xd173e97108bbdab7), -2},
    {0, 1, 1, 4, 4, "inf", "pnull", 7, 1, UINT64_C(0x78700f11be9e5a67), -2},
    {0, 1, 1, 5, 4, "inf", "p\"\"", 10, 1, UINT64_C(0xa609f86e3495cb61), -2},
    {0, 1, 1, 6, 4, "inf", "pnull", 9, 1, UINT64_C(0x8b5b977e31687ab2), -2},
    {0, 1, 1, 7, 4, "inf", "pnull", 10, 1, UINT64_C(0xa5ffa46e348ce820), -2},
    {0, 1, 1, 8, 4, "inf", "p\"\"", 11, 1, UINT64_C(0x7c21074338368774), -2},
    {0, 1, 1, 9, 4, "inf", "precovery\"", 12, 1, UINT64_C(0x22a0063884a26faf), -2},
    {0, 1, 1, 10, 4, "inf", "p", 11, 1, UINT64_C(0x7c20ff43383679dc), -2},
    {0, 1, 1, 11, 4, "inf", "p\"recovery\"", 11, 0, UINT64_C(0x7c20ff43383679dc), -2},
    {1, 0, 0, 0, 0, "null", "p", 1, 1, UINT64_C(0xaf63c54c8601c577), -2},
    {1, 0, 0, 1, 0, "null", "p", 2, 1, UINT64_C(0x084d9807b50252d5), -2},
    {1, 0, 0, 2, 0, "null", "p", 3, 1, UINT64_C(0x1e2c121892f29727), -2},
    {1, 0, 0, 3, 0, "null", "p", 5, 2, UINT64_C(0x7a8da821d2d372b7), -2},
    {1, 0, 0, 4, 0, "null", "p", 6, 2, UINT64_C(0x10e130793c36cb2d), -2},
    {1, 0, 0, 5, 0, "null", "p", 10, 5, UINT64_C(0xb187c4272f9942bf), -2},
    {1, 0, 0, 6, 0, "null", "p", 11, 5, UINT64_C(0x10b56be74c59bf9d), -2},
    {1, 0, 0, 7, 0, "null", "p", 12, 5, UINT64_C(0xe7ef7936b9e1d087), -2},
    {1, 0, 0, 8, 0, "null", "p", 14, 6, UINT64_C(0x2f22e7a49d59a9e5), -2},
    {1, 0, 0, 9, 0, "null", "p", 14, 5, UINT64_C(0x2f22e7a49d59a9e5), -2},
    {1, 0, 0, 10, 0, "null", "p", 16, 6, UINT64_C(0x3eca414f9a84948f), -2},
    {1, 0, 0, 11, 0, "null", "p", 16, 5, UINT64_C(0x3eca414f9a84948f), -2},
    {1, 0, 0, 12, 0, "null", "p", 17, 5, UINT64_C(0xe4f17e1059ea318d), -2},
    {1, 0, 0, 13, 0, "null", "p", 18, 5, UINT64_C(0xb242dac6f307d43f), -2},
    {1, 0, 0, 14, 0, "null", "p", 19, 5, UINT64_C(0x3c673d38f9599c87), -2},
    {1, 0, 0, 15, 0, "null", "p", 19, 4, UINT64_C(0x3c673d38f9599c87), -2},
    {1, 0, 0, 16, 0, "null", "p", 20, 4, UINT64_C(0x28b4cf2275458c17), -2},
    {1, 0, 0, 17, 0, "null", "p", 20, 3, UINT64_C(0x28b4cf2275458c17), -2},
    {1, 0, 0, 18, 0, "null", "p", 21, 3, UINT64_C(0xbd17e68bbdc2b29d), -2},
    {1, 0, 0, 19, 0, "null", "p", 21, 2, UINT64_C(0xbd17e68bbdc2b29d), -2},
    {1, 0, 0, 20, 0, "null", "p", 24, 4, UINT64_C(0x8aa851c4ba2dfe0d), -2},
    {1, 0, 0, 21, 0, "null", "p", 24, 3, UINT64_C(0x8aa851c4ba2dfe0d), -2},
    {1, 0, 0, 22, 0, "null", "p", 25, 3, UINT64_C(0x960824483eb6c5ad), -2},
    {1, 0, 0, 23, 0, "null", "p", 25, 2, UINT64_C(0x960824483eb6c5ad), -2},
    {1, 0, 0, 24, 0, "null", "p", 26, 2, UINT64_C(0xa664ebc29063a4cf), -2},
    {1, 0, 0, 25, 0, "null", "p", 27, 2, UINT64_C(0x211dfc9b594e1afc), -2},
    {1, 0, 0, 26, 0, "null", "p", 27, 1, UINT64_C(0x211dfc9b594e1afc), -2},
    {1, 0, 0, 27, 0, "null", "p", 28, 1, UINT64_C(0x940f17f8bfb7a3d4), -2},
    {1, 0, 0, 28, 0, "null", "p", 29, 1, UINT64_C(0x4d4977adc50b3874), -2},
    {1, 0, 0, 29, 0, "null", "p", 31, 2, UINT64_C(0x96ac3aa3f26673dc), -2},
    {1, 0, 0, 30, 0, "null", "p", 32, 2, UINT64_C(0x6d883894e473365c), -2},
    {1, 0, 0, 31, 0, "null", "p", 36, 5, UINT64_C(0xa689d44f1e8c390a), -2},
    {1, 0, 0, 32, 0, "null", "p", 37, 5, UINT64_C(0xb52aeaa3bc7e29da), -2},
    {1, 0, 0, 33, 0, "null", "p", 38, 5, UINT64_C(0x2bdfebe44514ddfa), -2},
    {1, 0, 0, 34, 0, "null", "p", 40, 6, UINT64_C(0x52ab3c4c65391168), -2},
    {1, 0, 0, 35, 0, "null", "p", 40, 5, UINT64_C(0x52ab3c4c65391168), -2},
    {1, 0, 0, 36, 0, "null", "p", 42, 6, UINT64_C(0xd4a882bc4fea0176), -2},
    {1, 0, 0, 37, 0, "null", "p", 42, 5, UINT64_C(0xd4a882bc4fea0176), -2},
    {1, 0, 0, 38, 0, "null", "p", 43, 5, UINT64_C(0xa7aebec77c7e3806), -2},
    {1, 0, 0, 39, 0, "null", "p", 44, 5, UINT64_C(0x2c2b5af6b31d5a8a), -2},
    {1, 0, 0, 40, 0, "null", "p", 45, 5, UINT64_C(0xe2e498da0c1ac778), -2},
    {1, 0, 0, 41, 0, "null", "p", 45, 4, UINT64_C(0xe2e498da0c1ac778), -2},
    {1, 0, 0, 42, 0, "null", "p", 46, 4, UINT64_C(0x3097224f03ed303a), -2},
    {1, 0, 0, 43, 0, "null", "p", 46, 3, UINT64_C(0x3097224f03ed303a), -2},
    {1, 0, 0, 44, 0, "null", "p", 47, 3, UINT64_C(0xddcbae42c02cbfc6), -2},
    {1, 0, 0, 45, 0, "null", "p", 47, 2, UINT64_C(0xddcbae42c02cbfc6), -2},
    {1, 0, 0, 46, 0, "null", "p", 50, 4, UINT64_C(0xc12186d4776e2450), -2},
    {1, 0, 0, 47, 0, "null", "p", 50, 3, UINT64_C(0xc12186d4776e2450), -2},
    {1, 0, 0, 48, 0, "null", "p", 51, 3, UINT64_C(0x6659e106d2e5e0e6), -2},
    {1, 0, 0, 49, 0, "null", "p", 51, 2, UINT64_C(0x6659e106d2e5e0e6), -2},
    {1, 0, 0, 50, 0, "null", "p", 52, 2, UINT64_C(0xd0d0ac985ccb66fa), -2},
    {1, 0, 0, 51, 0, "null", "p", 53, 2, UINT64_C(0x9dff9ee5ada2d05f), -2},
    {1, 0, 0, 52, 0, "null", "p", 53, 1, UINT64_C(0x9dff9ee5ada2d05f), -2},
    {1, 0, 0, 53, 0, "null", "p", 54, 1, UINT64_C(0x1c2b7f460ba847cd), -2},
    {1, 0, 0, 54, 0, "null", "p", 55, 1, UINT64_C(0x8630e905cef1bd5f), -2},
    {1, 0, 0, 55, 7, "{}", "p", 56, 1, UINT64_C(0xf6d973dea4c4fecd), -1},
    {1, 0, 0, 56, 7, "{}", "p", 57, 1, UINT64_C(0x387e8851fabcb25f), -1},
    {1, 0, 0, 57, 7, "{}", "p", 61, 4, UINT64_C(0x3956b24396957ae5), -1},
    {1, 0, 0, 58, 7, "{}", "p", 62, 4, UINT64_C(0xd13b7fd8c3c27f9f), -1},
    {1, 0, 0, 59, 7, "{}", "p", 63, 4, UINT64_C(0xf2c99a54716bbccd), -1},
    {1, 0, 0, 60, 7, "{}", "p", 65, 5, UINT64_C(0xf2ece4ea49ff28d7), -1},
    {1, 0, 0, 61, 7, "{}", "p", 65, 4, UINT64_C(0xf2ece4ea49ff28d7), -1},
    {1, 0, 0, 62, 7, "{}", "p", 67, 5, UINT64_C(0xa80b0851a72bbbf5), -1},
    {1, 0, 0, 63, 7, "{}", "p", 67, 4, UINT64_C(0xa80b0851a72bbbf5), -1},
    {1, 0, 0, 64, 7, "{}", "p", 68, 4, UINT64_C(0x355849f180ded08f), -1},
    {1, 0, 0, 65, 7, "{}", "p", 69, 4, UINT64_C(0x84428c5dfaf81765), -1},
    {1, 0, 0, 66, 7, "{}", "p", 70, 4, UINT64_C(0x012028e3c8954fdd), -1},
    {1, 0, 0, 67, 7, "{}", "p", 70, 3, UINT64_C(0x012028e3c8954fdd), -1},
    {1, 0, 0, 68, 7, "{}", "p", 71, 3, UINT64_C(0x4b691e0db8a2f8dd), -1},
    {1, 0, 0, 69, 7, "{}", "p", 71, 2, UINT64_C(0x4b691e0db8a2f8dd), -1},
    {1, 0, 0, 70, 7, "{}", "p", 72, 2, UINT64_C(0xc739a650bd76c67f), -1},
    {1, 0, 0, 71, 7, "{}", "p", 72, 1, UINT64_C(0xc739a650bd76c67f), -1},
    {1, 0, 0, 72, 7, "{\"\":null}", "p", 75, 3, UINT64_C(0x9e8b9c317656d9bf), -1},
    {1, 0, 0, 73, 7, "{\"\":null}", "p", 75, 2, UINT64_C(0x9e8b9c317656d9bf), 0},
    {1, 0, 0, 74, 7, "{\"\":null}", "p", 76, 2, UINT64_C(0xbe361b0c15b0d18f), 0},
    {1, 0, 0, 75, 7, "{\"k\":null}", "p", 76, 1, UINT64_C(0xbe361b0c15b0d18f), 0},
    {1, 0, 0, 76, 7, "{\"k\":null}", "p", 77, 1, UINT64_C(0xe6c18c88db742395), 0},
    {1, 0, 0, 77, 7, "{\"k\":\"\"}", "p", 78, 1, UINT64_C(0x8f05688ce6588214), 5},
    {1, 0, 0, 78, 7, "{\"k\":\"recovery\"}", "p", 78, 0, UINT64_C(0x8f05688ce6588214), 5},
    {1, 0, 1, 0, 0, "null", "p", 1, 1, UINT64_C(0xaf63c54c8601c577), -2},
    {1, 0, 1, 1, 0, "null", "p", 2, 1, UINT64_C(0x084d9807b50252d5), -2},
    {1, 0, 1, 2, 0, "null", "p", 3, 1, UINT64_C(0x1e2c121892f29727), -2},
    {1, 0, 1, 3, 7, "{}", "p", 14, 1, UINT64_C(0xd76d5b07238706ad), -1},
    {1, 0, 1, 4, 7, "{}", "p", 15, 1, UINT64_C(0xccfbd0932ed1c61f), -1},
    {1, 0, 1, 5, 7, "{\"k\":\"recovery\"}", "p", 73, 1, UINT64_C(0x15fb0ebd0a33b7ca), 5},
    {1, 0, 1, 6, 7, "{\"k\":\"recovery\"}", "p", 74, 1, UINT64_C(0x68d020c916f67c54), 5},
    {1, 0, 1, 7, 7, "{\"\":\"recovery\"}", "p", 63, 1, UINT64_C(0x2d23dde27a017aa6), 5},
    {1, 0, 1, 8, 7, "{\"\":\"recovery\"}", "p", 67, 1, UINT64_C(0xe8726979e3018f08), 5},
    {1, 0, 1, 9, 7, "{\"\":\"recovery\"}", "p", 65, 1, UINT64_C(0xc4d03eb0173d3988), 5},
    {1, 0, 1, 10, 7, "{\"\":\"recovery\"}", "p", 68, 1, UINT64_C(0x59a5ec0540fd0250), 5},
    {1, 0, 1, 11, 7, "{\"k\":\"recovery\"}", "p", 75, 1, UINT64_C(0xc4135e36c1583f72), 5},
    {1, 0, 1, 12, 7, "{\"k\":\"recovery\"}", "p", 76, 1, UINT64_C(0x8ede4064f5a34ddc), 5},
    {1, 0, 1, 13, 7, "{\"k\":\"\"}", "p", 67, 1, UINT64_C(0x8c6b9268a2c5cfcd), -1},
    {1, 0, 1, 14, 7, "{\"k\":\"\"}", "p", 68, 1, UINT64_C(0x9f6328349f5efe35), -1},
    {1, 0, 1, 15, 7, "{\"\":\"\"}", "p", 64, 1, UINT64_C(0xddef821334a86667), 5},
    {1, 0, 1, 16, 7, "{\"\":\"\"}", "p", 66, 1, UINT64_C(0x9eaa88f7ed6fe65d), 5},
    {1, 0, 1, 17, 7, "{\"k\":null}", "p", 56, 1, UINT64_C(0x8250699960f4e85f), 0},
    {1, 0, 1, 18, 7, "{\"k\":null}", "p", 57, 1, UINT64_C(0x9b6c3d4dacd7a045), 0},
    {1, 0, 1, 19, 7, "{}", "p", 30, 1, UINT64_C(0x81db53d0e75644ef), -1},
    {1, 0, 1, 20, 7, "{\"k\":\"\"}", "p", 69, 1, UINT64_C(0xd73452dd7647589d), -1},
    {1, 0, 1, 21, 7, "{\"\":\"\"}", "p", 66, 1, UINT64_C(0x9eaa88f7ed6fe65d), 5},
    {1, 0, 1, 22, 7, "{\"\":\"\"}", "p", 67, 1, UINT64_C(0x96214c6d9d4d7ffd), 5},
    {1, 0, 1, 23, 7, "{\"k\":null}", "p", 58, 1, UINT64_C(0x5334f2ccc09e7717), 0},
    {1, 0, 1, 24, 7, "{\"k\":null}", "p", 59, 1, UINT64_C(0x82d1353a8c895ebd), 0},
    {1, 0, 1, 25, 7, "{\"k\":\"\"}", "p", 76, 1, UINT64_C(0x1be6a6f37e090450), 5},
    {1, 0, 1, 26, 0, "null", "p", 27, 1, UINT64_C(0x211dfc9b594e1afc), -2},
    {1, 0, 1, 27, 0, "null", "p", 28, 1, UINT64_C(0x940f17f8bfb7a3d4), -2},
    {1, 0, 1, 28, 0, "null", "p", 29, 1, UINT64_C(0x4d4977adc50b3874), -2},
    {1, 0, 1, 29, 7, "{}", "p", 35, 1, UINT64_C(0x961e76ecf8fd871c), -1},
    {1, 0, 1, 30, 7, "{}", "p", 36, 1, UINT64_C(0xef7cb822b58db19c), -1},
    {1, 0, 1, 31, 7, "{\"k\":\"recovery\"}", "p", 73, 1, UINT64_C(0xdbd99fe5ed4883e4), 5},
    {1, 0, 1, 32, 7, "{\"k\":\"recovery\"}", "p", 74, 1, UINT64_C(0x6a386f444a5e4254), 5},
    {1, 0, 1, 33, 7, "{\"\":\"recovery\"}", "p", 67, 1, UINT64_C(0x7e21bf21deb3808c), 5},
    {1, 0, 1, 34, 7, "{\"\":\"recovery\"}", "p", 71, 1, UINT64_C(0x982621b7c5f73376), 5},
    {1, 0, 1, 35, 7, "{\"\":\"recovery\"}", "p", 69, 1, UINT64_C(0x77d0ac84582d5f7a), 5},
    {1, 0, 1, 36, 7, "{\"\":\"recovery\"}", "p", 72, 1, UINT64_C(0xee3d5bf84f6d2a40), 5},
    {1, 0, 1, 37, 7, "{\"k\":\"recovery\"}", "p", 75, 1, UINT64_C(0xc5d9c4afc4709dd4), 5},
    {1, 0, 1, 38, 7, "{\"k\":\"recovery\"}", "p", 76, 1, UINT64_C(0x8cfe23e5d4680904), 5},
    {1, 0, 1, 39, 7, "{\"k\":\"\"}", "p", 71, 1, UINT64_C(0xe7ff3f815d66ff2e), -1},
    {1, 0, 1, 40, 7, "{\"k\":\"\"}", "p", 72, 1, UINT64_C(0x3296cb16b10379e8), -1},
    {1, 0, 1, 41, 7, "{\"\":\"\"}", "p", 69, 1, UINT64_C(0xeb080365bc3f30bc), 5},
    {1, 0, 1, 42, 7, "{\"\":\"\"}", "p", 71, 1, UINT64_C(0xdf67a4e26b3d2a1e), 5},
    {1, 0, 1, 43, 7, "{\"k\":null}", "p", 65, 1, UINT64_C(0xa0086042559c8e1c), 0},
    {1, 0, 1, 44, 7, "{\"k\":null}", "p", 66, 1, UINT64_C(0x51cdd5df396e0ba0), 0},
    {1, 0, 1, 45, 7, "{}", "p", 51, 1, UINT64_C(0x383161da3f804726), -1},
    {1, 0, 1, 46, 7, "{\"k\":\"\"}", "p", 73, 1, UINT64_C(0x7eb80b99470a2eba), -1},
    {1, 0, 1, 47, 7, "{\"\":\"\"}", "p", 71, 1, UINT64_C(0xdf67a4e26b3d2a1e), 5},
    {1, 0, 1, 48, 7, "{\"\":\"\"}", "p", 72, 1, UINT64_C(0x8f4766e11afb39d4), 5},
    {1, 0, 1, 49, 7, "{\"k\":null}", "p", 67, 1, UINT64_C(0x279a838f9bdfaefc), 0},
    {1, 0, 1, 50, 7, "{\"k\":null}", "p", 68, 1, UINT64_C(0x5a342f85a466b390), 0},
    {1, 0, 1, 51, 7, "{\"k\":\"\"}", "p", 77, 1, UINT64_C(0xe6c18c88db742395), 5},
    {1, 0, 1, 52, 0, "null", "p", 53, 1, UINT64_C(0x9dff9ee5ada2d05f), -2},
    {1, 0, 1, 53, 0, "null", "p", 54, 1, UINT64_C(0x1c2b7f460ba847cd), -2},
    {1, 0, 1, 54, 0, "null", "p", 55, 1, UINT64_C(0x8630e905cef1bd5f), -2},
    {1, 0, 1, 55, 7, "{}", "p", 56, 1, UINT64_C(0xf6d973dea4c4fecd), -1},
    {1, 0, 1, 56, 7, "{}", "p", 57, 1, UINT64_C(0x387e8851fabcb25f), -1},
    {1, 0, 1, 57, 7, "{\"k\":\"recovery\"}", "p", 73, 1, UINT64_C(0x95a65de0d1459ffa), 5},
    {1, 0, 1, 58, 7, "{\"k\":\"recovery\"}", "p", 74, 1, UINT64_C(0xa620195a9833c2b4), 5},
    {1, 0, 1, 59, 7, "{\"\":\"recovery\"}", "p", 71, 1, UINT64_C(0x454d188779b08d02), 5},
    {1, 0, 1, 60, 7, "{\"\":\"recovery\"}", "p", 75, 1, UINT64_C(0x22580a8396a3a0ac), 5},
    {1, 0, 1, 61, 7, "{\"\":\"recovery\"}", "p", 73, 1, UINT64_C(0x3acc0530cfa4dcec), 5},
    {1, 0, 1, 62, 7, "{\"\":\"recovery\"}", "p", 76, 1, UINT64_C(0xb9a8b3368d755ba4), 5},
    {1, 0, 1, 63, 7, "{\"k\":\"recovery\"}", "p", 75, 1, UINT64_C(0x91af3c44d0365042), 5},
    {1, 0, 1, 64, 7, "{\"k\":\"recovery\"}", "p", 76, 1, UINT64_C(0x41b64226cd9d271c), 5},
    {1, 0, 1, 65, 7, "{\"k\":\"\"}", "p", 75, 1, UINT64_C(0xbbddd48e5d45fa45), -1},
    {1, 0, 1, 66, 7, "{\"k\":\"\"}", "p", 76, 1, UINT64_C(0x402f245441340d5d), -1},
    {1, 0, 1, 67, 7, "{\"\":\"\"}", "p", 74, 1, UINT64_C(0x66aaf18c794fd2ff), 5},
    {1, 0, 1, 68, 7, "{\"\":\"\"}", "p", 76, 1, UINT64_C(0xbe140f0c1593d6c5), 5},
    {1, 0, 1, 69, 7, "{\"k\":null}", "p", 74, 1, UINT64_C(0x91322a4e151994d7), 0},
    {1, 0, 1, 70, 7, "{\"k\":null}", "p", 75, 1, UINT64_C(0x9e8ba6317656eabd), 0},
    {1, 0, 1, 71, 7, "{}", "p", 72, 1, UINT64_C(0xc739a650bd76c67f), -1},
    {1, 0, 1, 72, 7, "{\"k\":\"\"}", "p", 77, 1, UINT64_C(0xe6c18c88db742395), -1},
    {1, 0, 1, 73, 7, "{\"\":\"\"}", "p", 76, 1, UINT64_C(0xbe140f0c1593d6c5), 5},
    {1, 0, 1, 74, 7, "{\"\":\"\"}", "p", 77, 1, UINT64_C(0xe6c18c88db742395), 5},
    {1, 0, 1, 75, 7, "{\"k\":null}", "p", 76, 1, UINT64_C(0xbe361b0c15b0d18f), 0},
    {1, 0, 1, 76, 7, "{\"k\":null}", "p", 77, 1, UINT64_C(0xe6c18c88db742395), 0},
    {1, 0, 1, 77, 7, "{\"k\":\"\"}", "p", 78, 1, UINT64_C(0x8f05688ce6588214), 5},
    {1, 0, 1, 78, 7, "{\"k\":\"recovery\"}", "p", 78, 0, UINT64_C(0x8f05688ce6588214), 5},
    {1, 1, 0, 0, 4, "inf", "p", 2, 2, UINT64_C(0x08842007b530c95d), -2},
    {1, 1, 0, 1, 4, "inf", "p", 3, 2, UINT64_C(0xa94bb518e1e60558), -2},
    {1, 1, 0, 2, 4, "inf", "p", 4, 2, UINT64_C(0x925e3a47da742eb6), -2},
    {1, 1, 0, 3, 4, "inf", "p", 5, 2, UINT64_C(0x28fb3718324d62b8), -2},
    {1, 1, 0, 4, 4, "inf", "p", 7, 3, UINT64_C(0x5e96ce153c7cfbb8), -2},
    {1, 1, 0, 5, 4, "inf", "p", 8, 3, UINT64_C(0x76f63f179f84d58e), -2},
    {1, 1, 0, 6, 4, "inf", "p", 12, 6, UINT64_C(0xa616d1c6ca4b37b8), -2},
    {1, 1, 0, 7, 4, "inf", "p", 13, 6, UINT64_C(0x7b61fcc43cb7c366), -2},
    {1, 1, 0, 8, 4, "inf", "p", 14, 6, UINT64_C(0x55e9654cf5f40670), -2},
    {1, 1, 0, 9, 4, "inf", "p", 16, 7, UINT64_C(0x03166de64beed216), -2},
    {1, 1, 0, 10, 4, "inf", "p", 16, 6, UINT64_C(0x03166de64beed216), -2},
    {1, 1, 0, 11, 4, "inf", "p", 18, 7, UINT64_C(0x61960677c5c6ef08), -2},
    {1, 1, 0, 12, 4, "inf", "p", 18, 6, UINT64_C(0x61960677c5c6ef08), -2},
    {1, 1, 0, 13, 4, "inf", "p", 19, 6, UINT64_C(0x7fb54dffd01f18d6), -2},
    {1, 1, 0, 14, 4, "inf", "p", 20, 6, UINT64_C(0x3351fbcf95f4f238), -2},
    {1, 1, 0, 15, 4, "inf", "p", 21, 6, UINT64_C(0x1d49b0fdcdfd5028), -2},
    {1, 1, 0, 16, 4, "inf", "p", 21, 5, UINT64_C(0x1d49b0fdcdfd5028), -2},
    {1, 1, 0, 17, 4, "inf", "p", 22, 5, UINT64_C(0x95f8c66497bb53a0), -2},
    {1, 1, 0, 18, 4, "inf", "p", 22, 4, UINT64_C(0x95f8c66497bb53a0), -2},
    {1, 1, 0, 19, 4, "inf", "p", 23, 4, UINT64_C(0xd152956a474667e6), -2},
    {1, 1, 0, 20, 4, "inf", "p", 23, 3, UINT64_C(0xd152956a474667e6), -2},
    {1, 1, 0, 21, 4, "inf", "p", 26, 5, UINT64_C(0xf155ca552e66308e), -2},
    {1, 1, 0, 22, 4, "inf", "p", 26, 4, UINT64_C(0xf155ca552e66308e), -2},
    {1, 1, 0, 23, 4, "inf", "p", 27, 4, UINT64_C(0x3b227d6a79828cd6), -2},
    {1, 1, 0, 24, 4, "inf", "p", 27, 3, UINT64_C(0x3b227d6a79828cd6), -2},
    {1, 1, 0, 25, 4, "inf", "p", 28, 3, UINT64_C(0x73861aec2a4cce28), -2},
    {1, 1, 0, 26, 4, "inf", "p", 29, 3, UINT64_C(0x90c3c74bdb6352f9), -2},
    {1, 1, 0, 27, 4, "inf", "p", 29, 2, UINT64_C(0x90c3c74bdb6352f9), -2},
    {1, 1, 0, 28, 4, "inf", "p", 30, 2, UINT64_C(0x5fd285e5c9a097b5), -2},
    {1, 1, 0, 29, 4, "inf", "p", 31, 2, UINT64_C(0x71deb9759aa6efd1), -2},
    {1, 1, 0, 30, 4, "inf", "p", 35, 5, UINT64_C(0xcc66351878f7eaae), -2},
    {1, 1, 0, 31, 4, "inf", "p", 36, 5, UINT64_C(0x938d793a6f66fdfa), -2},
    {1, 1, 0, 32, 4, "inf", "p", 40, 8, UINT64_C(0x9ac679090babd038), -2},
    {1, 1, 0, 33, 4, "inf", "p", 41, 8, UINT64_C(0xb441ea5b2ee4ef6c), -2},
    {1, 1, 0, 34, 4, "inf", "p", 42, 8, UINT64_C(0xe25b516d74e1ce28), -2},
    {1, 1, 0, 35, 4, "inf", "p", 44, 9, UINT64_C(0x2adccef255d56a56), -2},
    {1, 1, 0, 36, 4, "inf", "p", 44, 8, UINT64_C(0x2adccef255d56a56), -2},
    {1, 1, 0, 37, 4, "inf", "p", 46, 9, UINT64_C(0x661c8ee64bff6094), -2},
    {1, 1, 0, 38, 4, "inf", "p", 46, 8, UINT64_C(0x661c8ee64bff6094), -2},
    {1, 1, 0, 39, 4, "inf", "p", 47, 8, UINT64_C(0xb77b424f5a5c3448), -2},
    {1, 1, 0, 40, 4, "inf", "p", 48, 8, UINT64_C(0xfb8a4df5be8fafe8), -2},
    {1, 1, 0, 41, 4, "inf", "p", 49, 8, UINT64_C(0xb303414a8c5a6cba), -2},
    {1, 1, 0, 42, 4, "inf", "p", 49, 7, UINT64_C(0xb303414a8c5a6cba), -2},
    {1, 1, 0, 43, 4, "inf", "p", 50, 7, UINT64_C(0x0c8984c4638db2a8), -2},
    {1, 1, 0, 44, 4, "inf", "p", 50, 6, UINT64_C(0x0c8984c4638db2a8), -2},
    {1, 1, 0, 45, 4, "inf", "p", 51, 6, UINT64_C(0xc499abf4c8d2d6f8), -2},
    {1, 1, 0, 46, 4, "inf", "p", 51, 5, UINT64_C(0xc499abf4c8d2d6f8), -2},
    {1, 1, 0, 47, 4, "inf", "p", 54, 7, UINT64_C(0xee5659e8ca9e200e), -2},
    {1, 1, 0, 48, 4, "inf", "p", 54, 6, UINT64_C(0xee5659e8ca9e200e), -2},
    {1, 1, 0, 49, 4, "inf", "p", 55, 6, UINT64_C(0x313624099079c388), -2},
    {1, 1, 0, 50, 4, "inf", "p", 55, 5, UINT64_C(0x313624099079c388), -2},
    {1, 1, 0, 51, 4, "inf", "p", 56, 5, UINT64_C(0x4eb737700cb0e2b8), -2},
    {1, 1, 0, 52, 4, "inf", "p", 57, 5, UINT64_C(0xf578edebf5d33e57), -2},
    {1, 1, 0, 53, 4, "inf", "p", 57, 4, UINT64_C(0xf578edebf5d33e57), -2},
    {1, 1, 0, 54, 4, "inf", "p", 58, 4, UINT64_C(0x106c88119e6508ba), -2},
    {1, 1, 0, 55, 4, "inf", "p{", 58, 3, UINT64_C(0x106c88119e6508ba), -2},
    {1, 1, 0, 56, 4, "inf", "p{", 58, 2, UINT64_C(0x106c88119e6508ba), -2},
    {1, 1, 0, 57, 4, "inf", "p{", 74, 17, UINT64_C(0xb580ca8d7702ee9e), -2},
    {1, 1, 0, 58, 4, "inf", "p{", 74, 16, UINT64_C(0xb580ca8d7702ee9e), -2},
    {1, 1, 0, 59, 4, "inf", "p{\"k\":\"recovery\"", 61, 2, UINT64_C(0x303529626abee9ac), -2},
    {1, 1, 0, 60, 4, "inf", "p{\"k\":\"recovery\"", 61, 1, UINT64_C(0x303529626abee9ac), -2},
    {1, 1, 0, 61, 4, "inf", "p{\"k\":\"recovery\"}", 61, 0, UINT64_C(0x303529626abee9ac), -2},
    {1, 1, 1, 0, 4, "inf", "p", 57, 1, UINT64_C(0x357a103f541adf8f), -2},
    {1, 1, 1, 1, 4, "inf", "pnull", 4, 1, UINT64_C(0x91aa0747d9db038a), -2},
    {1, 1, 1, 2, 4, "inf", "pnull", 5, 1, UINT64_C(0x2a4ba818336b39e0), -2},
    {1, 1, 1, 3, 4, "inf", "pnull", 6, 1, UINT64_C(0xf0414c1d797eadaa), -2},
    {1, 1, 1, 4, 4, "inf", "p{}", 15, 1, UINT64_C(0xb8a11f4b85cfdf68), -2},
    {1, 1, 1, 5, 4, "inf", "p{}", 16, 1, UINT64_C(0x515bb1e0a9377c8a), -2},
    {1, 1, 1, 6, 4, "inf", "p{\"k\":\"recovery\"}", 56, 1, UINT64_C(0x315f2eac37d67b5a), -2},
    {1, 1, 1, 7, 4, "inf", "p{\"k\":\"recovery\"}", 57, 1, UINT64_C(0xcbfa0862d979100c), -2},
    {1, 1, 1, 8, 4, "inf", "p{\"\":\"recovery\"}", 50, 1, UINT64_C(0xd905de7dd0c987e6), -2},
    {1, 1, 1, 9, 4, "inf", "p{\"\":\"recovery\"}", 54, 1, UINT64_C(0x88b0fc6aef4df998), -2},
    {1, 1, 1, 10, 4, "inf", "p{\"\":\"recovery\"}", 52, 1, UINT64_C(0x0ed068e5aec81d18), -2},
    {1, 1, 1, 11, 4, "inf", "p{\"\":\"recovery\"}", 55, 1, UINT64_C(0x9ae649625b45ca98), -2},
    {1, 1, 1, 12, 4, "inf", "p{\"k\":\"recovery\"}", 58, 1, UINT64_C(0x0625a9a88129bed2), -2},
    {1, 1, 1, 13, 4, "inf", "p{\"k\":\"recovery\"}", 59, 1, UINT64_C(0x303de3c047695c84), -2},
    {1, 1, 1, 14, 4, "inf", "p{\"k\":\"\"}", 54, 1, UINT64_C(0xb8015f1c36338c04), -2},
    {1, 1, 1, 15, 4, "inf", "p{\"k\":\"\"}", 55, 1, UINT64_C(0xdb7517e493e6a9b4), -2},
    {1, 1, 1, 16, 4, "inf", "p{\"\":\"\"}", 52, 1, UINT64_C(0x7da62e92d17ad572), -2},
    {1, 1, 1, 17, 4, "inf", "p{\"\":\"\"}", 54, 1, UINT64_C(0x8d200b5a3b7e1874), -2},
    {1, 1, 1, 18, 4, "inf", "p{\"k\":null}", 48, 1, UINT64_C(0x573ea7945c700032), -2},
    {1, 1, 1, 19, 4, "inf", "p{\"k\":null}", 49, 1, UINT64_C(0x27b1db6071fdac8c), -2},
    {1, 1, 1, 20, 4, "inf", "p{}", 31, 1, UINT64_C(0xce687ad15e20ed42), -2},
    {1, 1, 1, 21, 4, "inf", "p{\"k\":\"\"}", 56, 1, UINT64_C(0x3dd0ee7536859154), -2},
    {1, 1, 1, 22, 4, "inf", "p{\"\":\"\"}", 54, 1, UINT64_C(0x8d200b5a3b7e1874), -2},
    {1, 1, 1, 23, 4, "inf", "p{\"\":\"\"}", 55, 1, UINT64_C(0x39985a010413873c), -2},
    {1, 1, 1, 24, 4, "inf", "p{\"k\":null}", 50, 1, UINT64_C(0xe2169cee4d02037a), -2},
    {1, 1, 1, 25, 4, "inf", "p{\"k\":null}", 51, 1, UINT64_C(0x6668922033679bd4), -2},
    {1, 1, 1, 26, 4, "inf", "p{\"k\":\"\"}", 60, 1, UINT64_C(0x7a7b3b316939da83), -2},
    {1, 1, 1, 27, 4, "inf", "pnull", 30, 1, UINT64_C(0x5ffe94e5c9c5ec1d), -2},
    {1, 1, 1, 28, 4, "inf", "pnull", 31, 1, UINT64_C(0x735128759be1a9f9), -2},
    {1, 1, 1, 29, 4, "inf", "pnull", 32, 1, UINT64_C(0x2464e9d5c9a96f25), -2},
    {1, 1, 1, 30, 4, "inf", "p{}", 36, 1, UINT64_C(0x2959406bddec2341), -2},
    {1, 1, 1, 31, 4, "inf", "p{}", 37, 1, UINT64_C(0xff7ab32c63614925), -2},
    {1, 1, 1, 32, 4, "inf", "p{\"k\":\"recovery\"}", 56, 1, UINT64_C(0x1d97c31770f583e8), -2},
    {1, 1, 1, 33, 4, "inf", "p{\"k\":\"recovery\"}", 57, 1, UINT64_C(0xf80d82cd3044afcc), -2},
    {1, 1, 1, 34, 4, "inf", "p{\"\":\"recovery\"}", 54, 1, UINT64_C(0xda80aa339eb5c7f8), -2},
    {1, 1, 1, 35, 4, "inf", "p{\"\":\"recovery\"}", 58, 1, UINT64_C(0x625b8d667a7a4bda), -2},
    {1, 1, 1, 36, 4, "inf", "p{\"\":\"recovery\"}", 56, 1, UINT64_C(0xd1b802c73f7ec5a6), -2},
    {1, 1, 1, 37, 4, "inf", "p{\"\":\"recovery\"}", 59, 1, UINT64_C(0x4d6683f6c036dbd8), -2},
    {1, 1, 1, 38, 4, "inf", "p{\"k\":\"recovery\"}", 58, 1, UINT64_C(0xb64549e8f90110c8), -2},
    {1, 1, 1, 39, 4, "inf", "p{\"k\":\"recovery\"}", 59, 1, UINT64_C(0x5e11fc32d5a26abc), -2},
    {1, 1, 1, 40, 4, "inf", "p{\"k\":\"\"}", 58, 1, UINT64_C(0x9c11d723b8ba768f), -2},
    {1, 1, 1, 41, 4, "inf", "p{\"k\":\"\"}", 59, 1, UINT64_C(0xa9a483305377d8b9), -2},
    {1, 1, 1, 42, 4, "inf", "p{\"\":\"\"}", 57, 1, UINT64_C(0x58e686a85e22d0bf), -2},
    {1, 1, 1, 43, 4, "inf", "p{\"\":\"\"}", 59, 1, UINT64_C(0xd8a3bfa99189d665), -2},
    {1, 1, 1, 44, 4, "inf", "p{\"k\":null}", 57, 1, UINT64_C(0xb7cd827aae5b9de3), -2},
    {1, 1, 1, 45, 4, "inf", "p{\"k\":null}", 58, 1, UINT64_C(0xc901aa4562715073), -2},
    {1, 1, 1, 46, 4, "inf", "p{}", 52, 1, UINT64_C(0xb9b076a5e7fcb303), -2},
    {1, 1, 1, 47, 4, "inf", "p{\"k\":\"\"}", 60, 1, UINT64_C(0x7a7b3b316939da83), -2},
    {1, 1, 1, 48, 4, "inf", "p{\"\":\"\"}", 59, 1, UINT64_C(0xd8a3bfa99189d665), -2},
    {1, 1, 1, 49, 4, "inf", "p{\"\":\"\"}", 60, 1, UINT64_C(0x7a7b3b316939da83), -2},
    {1, 1, 1, 50, 4, "inf", "p{\"k\":null}", 59, 1, UINT64_C(0x8242d1842bb69e53), -2},
    {1, 1, 1, 51, 4, "inf", "p{\"k\":null}", 60, 1, UINT64_C(0x7a7b3b316939da83), -2},
    {1, 1, 1, 52, 4, "inf", "p{\"k\":\"\"}", 61, 1, UINT64_C(0x303529626abee9ac), -2},
    {1, 1, 1, 53, 4, "inf", "p\"k\":\"recovery\"}", 62, 1, UINT64_C(0x621da11fedc3cd77), -2},
    {1, 1, 1, 54, 4, "inf", "p\"k\":\"recovery\"}", 63, 1, UINT64_C(0x54123ca5b4a95d58), -2},
    {1, 1, 1, 55, 4, "inf", "p{}", 58, 1, UINT64_C(0x106c88119e6508ba), -2},
    {1, 1, 1, 56, 4, "inf", "p{}", 58, 1, UINT64_C(0x106c88119e6508ba), -2},
    {1, 1, 1, 57, 4, "inf", "p{\"k\":\"recovery\"}", 61, 1, UINT64_C(0x303529626abee9ac), -2},
    {1, 1, 1, 58, 4, "inf", "p{k\":\"recovery\"}", 62, 1, UINT64_C(0x52c21e3b3173e466), -2},
    {1, 1, 1, 59, 4, "inf", "p{\"k\":\"recovery\"}", 61, 1, UINT64_C(0x303529626abee9ac), -2},
    {1, 1, 1, 60, 4, "inf", "p{\"k\":\"recovery\"", 61, 1, UINT64_C(0x303529626abee9ac), -2},
    {1, 1, 1, 61, 4, "inf", "p{\"k\":\"recovery\"}", 61, 0, UINT64_C(0x303529626abee9ac), -2},
};
int main() {
    RecoveryAllocationTrace trace; RecoveryAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize callback-return allocation contracts");
    Value replacement{}; _ZN3sce4Json5ValueC1Ev(&replacement);
    const char document[] = "{\"k\":\"recovery\"}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&replacement, document, sizeof(document) - 1) == 0 && Contents(replacement) == document, "Construct the independently owned callback-return template");
    String lookupKey{}; _ZN3sce4Json6StringC1EPKc(&lookupKey, "k");
    const auto baseline = trace.live;
    const auto baselineIds = trace.ids;
    for (const auto& expected : Cases) {
        Value value{}; _ZN3sce4Json5ValueC1Ed(&value, expected.format ? std::numeric_limits<double>::infinity() : 0.125);
        RecoveryContext callback{&replacement, expected.object};
        Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, RecoverAllocated, &callback) == 0, "Register allocation-backed recovery");
        void* auxiliary = value.auxiliary;
        void* fallback = static_cast<void**>(auxiliary)[2];
        void* record = static_cast<void**>(auxiliary)[4];
        const auto auxiliaryId = trace.ids.at(auxiliary), fallbackId = trace.ids.at(fallback), recordId = trace.ids.at(record);
        String output{}; _ZN3sce4Json6StringC1EPKc(&output, "p");
        const auto requests = trace.requests.size(); const auto notifications = trace.notifications;
        trace.budget = expected.budget; trace.once = expected.once;
        int status = 0;
        if (expected.format) status = _ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output);
        else _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
        trace.budget = -1; trace.once = false;
        const auto end = trace.requests.size();
        const auto errors = trace.notifications - notifications;
        Require(value.type == expected.type && Contents(value) == expected.contents && std::strcmp(_ZNK3sce4Json6String5c_strEv(&output), expected.output) == 0, "Callback-return allocation failure must preserve the reference's exact partial value and output");
        if (expected.object) {
            const auto* found = value.type == 7 ? _ZN3sce4Json5Value10referValueERKNS0_6StringE(&value, &lookupKey) : nullptr;
            const int lookupType = value.type == 7 ? (found ? found->type : -1) : -2;
            Require(lookupType == expected.lookupType, "Callback-return cloning must preserve internal tree-key lookup independently of the public key text");
        }
        std::uint64_t order = UINT64_C(14695981039346656037);
        for (auto i = requests; i < end; ++i) { order ^= trace.requests[i]; order *= UINT64_C(1099511628211); }
        if (end - requests != expected.requests || order != expected.order || errors != expected.notifications) std::fprintf(stderr, "Recovery trace differs: object=%d format=%d once=%d budget=%d requests=%zu/%zu notices=%u/%u\n", expected.object, expected.format, expected.once, expected.budget, end - requests, expected.requests, errors, expected.notifications);
        Require(end - requests == expected.requests && order == expected.order && errors == expected.notifications, "Callback construction, cloning and final assignment must preserve exact request order and failure notifications");
        if (expected.format) Require(status == 0, "Serialization must preserve the reference callback-recovery status");
        Require(callback.calls == 1 && callback.function == (expected.format ? 0 : 1) && callback.errorPointer == expected.format, "Recovery dispatch must retain callback function, invocation count, error-pointer shape and observed serialization status");
        Require(!value.parent && value.auxiliary == auxiliary && static_cast<void**>(auxiliary)[2] == fallback && static_cast<void**>(auxiliary)[4] == record, "Partial callback cloning must preserve destination callback metadata and fallback identity");
        Require(trace.ids.count(auxiliary) && trace.ids.count(fallback) && trace.ids.count(record), "Destination recovery metadata must retain live allocator ownership");
        Require(trace.ids.at(auxiliary) == auxiliaryId && trace.ids.at(fallback) == fallbackId && trace.ids.at(record) == recordId && static_cast<void**>(record)[0] == reinterpret_cast<void*>(&RecoverAllocated) && static_cast<void**>(record)[1] == &callback && static_cast<Value*>(fallback)->parent == &value, "Recovery metadata must retain its allocation generation, handler, context and fallback owner");
        Require(Contents(replacement) == document, "Failure recovery must preserve the independently owned source template");
        _ZN3sce4Json6StringD1Ev(&output); _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live == baseline && trace.ids == baselineIds, "Every callback-return failure state must reclaim temporary ownership and remain destructible");
    }
    _ZN3sce4Json6StringD1Ev(&lookupKey);
    _ZN3sce4Json5ValueD1Ev(&replacement);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty() && trace.ids.empty(), "Callback-return allocation checks must release all allocator-owned resources");
    return 0;
}
