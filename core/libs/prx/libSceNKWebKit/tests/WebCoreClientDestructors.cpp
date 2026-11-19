#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceNKWebKit/include/WebCoreClientDestructorsExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

static void Check(bool value, const char* name, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore client destructor check failed for %s at line %d\n", name, line);
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

template<typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestXPathNSResolverFreesThroughFastFree() {
    for (int i = 0; i < 16; ++i) {
        void* object = _ZN3WTF10fastMallocEm(16);
        Require(object != nullptr, "fastMalloc");
        Require(guestAllocations.count(object) == 0, "fastMalloc does not use the guest heap");
        std::memset(object, 0xa5, 16);
        const std::size_t freesBefore = guestFrees;
        _ZN7WebCore15XPathNSResolverD0Ev(object);
        Require(guestFrees == freesBefore, "no guest free");
    }
    Require(guestAllocations.empty(), "no guest allocation left");
}

static void TestContentSecurityPolicyClientCompleteDestructorLeavesObject() {
    unsigned char object[8];
    std::memset(object, 0x3c, sizeof(object));
    const std::size_t freesBefore = guestFrees;
    _ZN7WebCore27ContentSecurityPolicyClientD1Ev(object);
    Require(guestFrees == freesBefore, "no free");
    for (unsigned char byte : object) Require(byte == 0x3c, "object bytes unchanged");
}

static void TestContentSecurityPolicyClientFreesThroughOperatorDelete() {
    for (int i = 0; i < 16; ++i) {
        void* object = _Znwm_nid_postfix(8);
        Require(object != nullptr, "operator new");
        Require(guestAllocations.count(object) == 1, "object is a guest allocation");
        std::memset(object, 0x5a, 8);
        const std::size_t freesBefore = guestFrees;
        _ZN7WebCore27ContentSecurityPolicyClientD0Ev(object);
        Require(guestFrees == freesBefore + 1, "one guest free");
        Require(guestAllocations.count(object) == 0, "the object itself was freed");
    }
    Require(guestAllocations.empty(), "no guest allocation left");
}

static void TestNullObject() {
    const std::size_t freesBefore = guestFrees;
    Require(Throws([] { _ZN7WebCore15XPathNSResolverD0Ev(nullptr); }), "XPathNSResolver null object");
    Require(Throws([] { _ZN7WebCore27ContentSecurityPolicyClientD1Ev(nullptr); }), "ContentSecurityPolicyClient D1 null object");
    Require(Throws([] { _ZN7WebCore27ContentSecurityPolicyClientD0Ev(nullptr); }), "ContentSecurityPolicyClient D0 null object");
    Require(guestFrees == freesBefore, "a null object frees nothing");
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestXPathNSResolverFreesThroughFastFree();
    TestContentSecurityPolicyClientCompleteDestructorLeavesObject();
    TestContentSecurityPolicyClientFreesThroughOperatorDelete();
    TestNullObject();
    std::puts("WebCore client destructor tests passed");
    return 0;
}
