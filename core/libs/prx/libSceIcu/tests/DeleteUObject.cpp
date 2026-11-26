#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uprv_deleteUObject_nid_postfix), void (APS5_VABI*)(void*)>);

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

struct GuestObject {
    const void* vtable;
};

struct GuestVtable {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    void (APS5_VABI* complete)(GuestObject*);
    void (APS5_VABI* deleting)(GuestObject*);
};

int completeCalls = 0;
int deletingCalls = 0;
GuestObject* deletedObject = nullptr;

void APS5_VABI CompleteDestructor(GuestObject*) {
    ++completeCalls;
}

void APS5_VABI DeletingDestructor(GuestObject* self) {
    ++deletingCalls;
    deletedObject = self;
}

void RequireRejected(void* object) {
    try {
        uprv_deleteUObject_nid_postfix(object);
        Require(false);
    } catch (const std::invalid_argument&) {
    }
    Require(completeCalls == 0 && deletingCalls == 0 && deletedObject == nullptr);
}

}

int main() {
    uprv_deleteUObject_nid_postfix(nullptr);
    Require(completeCalls == 0 && deletingCalls == 0);

    RequireRejected(reinterpret_cast<void*>(sizeof(void*)));

    GuestObject unreadableVtable{nullptr};
    RequireRejected(&unreadableVtable);

    const GuestVtable missingSlot{0, nullptr, CompleteDestructor, nullptr};
    GuestObject missingDestructor{&missingSlot.complete};
    RequireRejected(&missingDestructor);

    const GuestVtable vtable{0, nullptr, CompleteDestructor, DeletingDestructor};
    GuestObject object{&vtable.complete};
    uprv_deleteUObject_nid_postfix(&object);
    Require(completeCalls == 0);
    Require(deletingCalls == 1);
    Require(deletedObject == &object);
    return 0;
}
