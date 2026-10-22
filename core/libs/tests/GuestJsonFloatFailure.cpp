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
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
std::int32_t APS5_VABI _ZNK3sce4Json5Value7getTypeEv(const Value*);
const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv(const Value*);
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
    Require(failed, "String conversion must propagate host allocation failure");
    Require(liveAllocations == before && _ZNK3sce4Json5Value7getTypeEv(&value) == 4 &&
                *_ZNK3sce4Json5Value7getRealEv(&value) == 0.125,
            "Failed conversion must retain the original value and ownership");
    allocationBudget = 0;
    _ZN3sce4Json5Value3setEd(&value, 0.5);
    allocationBudget = -1;
    Require(*_ZNK3sce4Json5Value7getRealEv(&value) == 0.5,
            "Finite assignment must not require a conversion allocation");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(liveAllocations == baseline, "Destroy must reclaim the retained value");
    for (int budget : {0, 1}) {
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
        Require(failed && destination.bytes == original && liveAllocations == baseline,
                "Failed construction must leave destination storage unchanged and reclaim partial state");
    }
    _ZN3sce4Json5ValueC1Ed(&value, std::numeric_limits<double>::infinity());
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == 5, "Conversion must remain usable after allocation failure");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(liveAllocations == baseline, "Successful conversion must not leak");
    _ZN3sce4Json11InitializerD1Ev(&initializer);
}
