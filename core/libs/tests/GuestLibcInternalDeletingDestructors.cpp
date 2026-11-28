#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libSceLibcInternal/include/PendingExports.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
void APS5_VABI sceLibcInternalUnknown6_112_6yplvTHbxpE_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_119_xFva4yxsVW8_nid_no_patch(GuestLocale::NumpunctFacet* self);
}

namespace {

unsigned releases = 0;
void* lastReleased = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal deleting destructors: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    ++releases;
    lastReleased = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void CheckNumpunctDestructors() {
    using Destructor = void (APS5_VABI*)(GuestLocale::NumpunctFacet*);
    for (const Destructor destroy : {libSceLibcInternalInventory_167_nid_no_patch, sceLibcInternalUnknown6_119_xFva4yxsVW8_nid_no_patch}) {
        GuestLocale::NumpunctFacet facet{};
        NumpunctConstruct_nid_no_patch(&facet, 0);
        const GuestLocale::NumpunctFacet before = facet;
        releases = 0;
        lastReleased = nullptr;
        destroy(&facet);
        Require(releases == 3 && lastReleased == before.truename, "the numpunct<char> destructor releases the grouping and both bool names");
        bool rejected = false;
        try {
            destroy(nullptr);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected, "the numpunct<char> destructor rejects a null facet");
    }
    auto* facet = static_cast<GuestLocale::NumpunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::NumpunctFacet)));
    NumpunctConstruct_nid_no_patch(facet, 0);
    releases = 0;
    lastReleased = nullptr;
    _ZNSt8numpunctIcED0Ev_nid_postfix(facet);
    Require(releases == 4 && lastReleased == facet, "the numpunct<char> deleting destructor tidies the facet and then releases it");
    bool rejected = false;
    try {
        _ZNSt8numpunctIcED0Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "the numpunct<char> deleting destructor rejects a null facet");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    using Destructor = void (APS5_VABI*)(void*);
    const Destructor destructors[] = {
        sceLibcInternalUnknown6_112_6yplvTHbxpE_nid_no_patch,
        _ZNSt7codecvtIwc9_MbstatetED0Ev_nid_postfix,
        _ZNSt8messagesIwED0Ev_nid_postfix,
        _ZNSt24_Iostream_error_categoryD0Ev_nid_postfix,
        _ZNSt8messagesIcED0Ev_nid_postfix,
        _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED0Ev_nid_postfix,
        _ZNSt10moneypunctIwLb1EED0Ev_nid_postfix,
        _ZNSt7_MpunctIcED0Ev_nid_postfix,
        _ZNSt7_MpunctIwED0Ev_nid_postfix,
        _ZNSt10moneypunctIcLb0EED0Ev_nid_postfix,
        _ZNSt10moneypunctIcLb1EED0Ev_nid_postfix,
    };

    for (Destructor destructor : destructors) {
        void* object = _Znwm_nid_postfix(0x40);
        Require(object != nullptr, "the guest heap allocates the object");
        releases = 0;
        lastReleased = nullptr;
        destructor(object);
        Require(releases == 1 && lastReleased == object, "the deleting destructor releases the object");
    }

    CheckNumpunctDestructors();
    std::puts("libc internal deleting destructor checks passed");
    return 0;
}
