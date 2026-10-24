#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

struct Value { alignas(8) std::array<std::byte, 32> bytes{}; };
struct String { void* text; };
using Handler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
struct Parameter {
    void* allocator;
    void* userData;
    std::size_t bufferSize;
    std::int32_t mode;
    std::uint32_t reserved[3];
};

extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
void APS5_VABI _ZN3sce4Json11InitializerD1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value*, double);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
std::int32_t APS5_VABI _ZNK3sce4Json5Value7getTypeEv(const Value*);
const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv(const Value*);
const String* APS5_VABI _ZNK3sce4Json5Value9getStringEv(const Value*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, Handler, void*);
}

#ifndef _WIN32
extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* name) {
    std::fprintf(stderr, "Unexpected unsupported Json2 path: %s\n", name);
    std::abort();
}
#endif

static int allocationBudget = -1;
static std::size_t liveAllocations = 0;

void* operator new(std::size_t bytes) {
    if (allocationBudget == 0) throw std::bad_alloc();
    if (allocationBudget > 0) --allocationBudget;
    if (void* pointer = std::malloc(bytes ? bytes : 1)) {
        ++liveAllocations;
        return pointer;
    }
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept {
    if (pointer) {
        --liveAllocations;
        std::free(pointer);
    }
}

void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

struct CallbackFailure {};
struct Context { bool throwAfterConstruction = false; int calls = 0; };
static constexpr const char* Replacement = "A callback replacement long enough to require separately allocated character storage";

static Value* APS5_VABI Recover(Value* result, std::int32_t, double, const Value*, void* context, bool*) {
    auto& trace = *static_cast<Context*>(context);
    ++trace.calls;
    _ZN3sce4Json5ValueC1EPKc(result, Replacement);
    if (trace.throwAfterConstruction) throw CallbackFailure{};
    return result;
}

static void CheckRecoveryFailures() {
    const auto baseline = liveAllocations;
    Context context;
    bool succeeded = false;
    unsigned failures = 0;
    for (int budget = 0; budget < 64 && !succeeded; ++budget) {
        Value value;
        _ZN3sce4Json5ValueC1Ed(&value, 0.125);
        Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, Recover, &context) == 0,
                "Register allocation-failure recovery");
        const auto before = liveAllocations;
        allocationBudget = budget;
        try {
            _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
            succeeded = true;
        } catch (const std::bad_alloc&) {
            ++failures;
        }
        allocationBudget = -1;
        if (succeeded) {
            Require(_ZNK3sce4Json5Value7getTypeEv(&value) == 5 &&
                        std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&value)), Replacement) == 0,
                    "Recovery must remain usable after every earlier allocation failure");
        } else {
            Require(_ZNK3sce4Json5Value7getTypeEv(&value) == 4 &&
                        *_ZNK3sce4Json5Value7getRealEv(&value) == 0.125 && liveAllocations == before,
                    "Failed callback construction or cloning must retain the destination and reclaim temporary ownership");
        }
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(liveAllocations == baseline, "Recovery teardown must reclaim metadata, replacement and temporary allocations");
    }
    Require(succeeded && failures == 0, "Recovery value storage must use the guest allocator without host allocations");
    Value value;
    _ZN3sce4Json5ValueC1Ed(&value, 0.125);
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, Recover, &context) == 0,
            "Register throwing recovery");
    const auto before = liveAllocations;
    context.throwAfterConstruction = true;
    bool threw = false;
    try {
        _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
    } catch (const CallbackFailure&) {
        threw = true;
    }
    Require(threw && liveAllocations == before && _ZNK3sce4Json5Value7getTypeEv(&value) == 4 &&
                *_ZNK3sce4Json5Value7getRealEv(&value) == 0.125,
            "A throwing host callback must release its constructed return object and preserve the destination");
    context.throwAfterConstruction = false;
    const int calls = context.calls;
    _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
    Require(context.calls == calls + 1 && _ZNK3sce4Json5Value7getTypeEv(&value) == 5,
            "Callback exceptions must release the reentry guard before retry");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(liveAllocations == baseline, "A retried callback must not leak");
}

int main() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 0, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0,
            "Initialize string-format conversion");
    const auto baseline = liveAllocations;
    Value value;
    _ZN3sce4Json5ValueC1Ed(&value, 0.125);
    const auto before = liveAllocations;
    allocationBudget = 0;
    bool failed = false;
    try {
        _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    allocationBudget = -1;
    Require(!failed, "String conversion must use the guest allocator without host allocations");
    Require(liveAllocations == before && _ZNK3sce4Json5Value7getTypeEv(&value) == 5 &&
                std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&value)), "inf") == 0,
            "Conversion must own its string through the guest allocator");
    allocationBudget = 0;
    _ZN3sce4Json5Value3setEd(&value, 0.5);
    allocationBudget = -1;
    Require(*_ZNK3sce4Json5Value7getRealEv(&value) == 0.5,
            "Finite assignment must not require a conversion allocation");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(liveAllocations == baseline, "Destroy must reclaim the retained value");
    bool constructed = false;
    unsigned constructorFailures = 0;
    for (int budget = 0; budget < 64 && !constructed; ++budget) {
        Value destination;
        destination.bytes.fill(std::byte{0xa5});
        const auto original = destination.bytes;
        allocationBudget = budget;
        failed = false;
        try {
            _ZN3sce4Json5ValueC1Ed(&destination, std::numeric_limits<double>::infinity());
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        allocationBudget = -1;
        if (failed) {
            ++constructorFailures;
            Require(destination.bytes == original && liveAllocations == baseline,
                    "Failed construction must leave destination storage unchanged and reclaim partial state");
        } else {
            constructed = true;
            Require(_ZNK3sce4Json5Value7getTypeEv(&destination) == 5,
                    "Construction must remain usable after earlier host allocation failures");
            _ZN3sce4Json5ValueD1Ev(&destination);
            Require(liveAllocations == baseline, "Successful construction must release its host ownership");
        }
    }
    Require(constructed && constructorFailures == 0, "Float construction must not depend on host allocation");
    _ZN3sce4Json5ValueC1Ed(&value, std::numeric_limits<double>::infinity());
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == 5, "Conversion must remain usable after allocation failure");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(liveAllocations == baseline, "Successful conversion must not leak");
    CheckRecoveryFailures();
    _ZN3sce4Json11InitializerD1Ev(&initializer);
}
