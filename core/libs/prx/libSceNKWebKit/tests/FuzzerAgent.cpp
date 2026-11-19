#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceNKWebKit/include/FuzzerAgentExports.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

static void Check(bool value, const char* name, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC::FuzzerAgent check failed for %s at line %d\n", name, line);
        std::abort();
    }
}
#define Require(value, name) Check((value), (name), __LINE__)

static std::unordered_set<void*> guestAllocations;
static std::size_t guestFrees = 0;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1, "guest free of a guest allocation");
    ++guestFrees;
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

static void TestDeletingDestructorFreesThroughOperatorDelete() {
    for (int i = 0; i < 16; ++i) {
        void* object = _Znwm_nid_postfix(8);
        Require(object != nullptr, "operator new");
        Require(guestAllocations.count(object) == 1, "object is a guest allocation");
        std::memset(object, 0x5a, 8);
        const std::size_t freesBefore = guestFrees;
        _ZN3JSC11FuzzerAgentD0Ev(object);
        Require(guestFrees == freesBefore + 1, "one guest free");
        Require(guestAllocations.count(object) == 0, "the object itself was freed");
    }
    Require(guestAllocations.empty(), "no guest allocation left");
}

static void TestNullObject() {
    const std::size_t freesBefore = guestFrees;
    bool threw = false;
    try {
        _ZN3JSC11FuzzerAgentD0Ev(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "null object throws");
    Require(guestFrees == freesBefore, "a null object frees nothing");
}

static void TestGetPredictionReturnsOriginal() {
    const std::uint64_t values[] = {0, 1, 0x20, 0x8000000000000000ull, 0x0000ffffffffffffull, ~0ull};
    unsigned char codeBlock[16] = {};
    unsigned char codeOrigin[8] = {};
    unsigned char agent[8] = {};
    for (std::uint64_t value : values) {
        Require(_ZN3JSC11FuzzerAgent13getPredictionEPNS_9CodeBlockERKNS_10CodeOriginEm(agent, codeBlock, codeOrigin, value) == value,
            "prediction with objects");
        Require(_ZN3JSC11FuzzerAgent13getPredictionEPNS_9CodeBlockERKNS_10CodeOriginEm(agent, nullptr, codeOrigin, value) == value,
            "prediction without a code block");
    }
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestDeletingDestructorFreesThroughOperatorDelete();
    TestNullObject();
    TestGetPredictionReturnsOriginal();
    std::puts("JSC::FuzzerAgent tests passed");
    return 0;
}
