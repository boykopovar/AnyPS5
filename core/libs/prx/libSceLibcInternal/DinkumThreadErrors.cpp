#include "prx/libSceLibcInternal/include/DinkumThreadErrors.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include <cstddef>
#include <cstring>
#include <typeinfo>

extern "C" {
extern const void* const _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix[];
extern const unsigned char _ZTISt13runtime_error_nid_postfix[];
extern const void* const _ZTVSt13runtime_error_nid_postfix[];
}

namespace {

struct TypeRecord {
    const void* const* vtable;
    const char* name;
    const void* base;
};

struct ErrorVtable {
    std::ptrdiff_t offset;
    const TypeRecord* type;
    void (APS5_VABI *destroy)(DinkumThreadError*);
    void (APS5_VABI *deleteObject)(DinkumThreadError*);
    const char* (APS5_VABI *what)(const DinkumThreadError*);
    void (APS5_VABI *doraise)(const DinkumThreadError*);
};

static_assert(sizeof(DinkumThreadError) == 0x30);
static_assert(offsetof(DinkumThreadError, message) == 8);
static_assert(sizeof(TypeRecord) == 0x18);
static_assert(sizeof(ErrorVtable) == 0x30);

const TypeRecord LockErrorType {
    _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix + 2,
    "N6Dinkum7threads10lock_errorE", _ZTISt13runtime_error_nid_postfix
};
const TypeRecord ResourceErrorType {
    _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix + 2,
    "N6Dinkum7threads21thread_resource_errorE", _ZTISt13runtime_error_nid_postfix
};

const char* APS5_VABI What_nid_no_patch(const DinkumThreadError* self) {
    return self->message.capacity > GuestLocale::StringInlineCapacity ? self->message.pointer : self->message.buffer;
}

void APS5_VABI Doraise_nid_no_patch(const DinkumThreadError*) {}

const ErrorVtable LockErrorVtable {
    0, &LockErrorType, _ZN6Dinkum7threads10lock_errorD1Ev_nid_postfix,
    _ZN6Dinkum7threads10lock_errorD0Ev_nid_postfix, What_nid_no_patch, Doraise_nid_no_patch
};
const ErrorVtable ResourceErrorVtable {
    0, &ResourceErrorType, _ZN6Dinkum7threads21thread_resource_errorD1Ev_nid_postfix,
    _ZN6Dinkum7threads21thread_resource_errorD0Ev_nid_postfix, What_nid_no_patch, Doraise_nid_no_patch
};

void Destroy(DinkumThreadError* self) {
    self->vtable = _ZTVSt13runtime_error_nid_postfix + 2;
    auto& message = self->message;
    if (message.capacity > GuestLocale::StringInlineCapacity) {
        GuestStringStorage::Deallocate(message.pointer, message.capacity + 1);
    }
    message.capacity = GuestLocale::StringInlineCapacity;
    message.size = 0;
    message.buffer[0] = 0;
}

[[noreturn]] void Throw(const ErrorVtable& table, const char* message) {
    auto* object = static_cast<DinkumThreadError*>(__cxa_allocate_exception_nid_postfix(sizeof(DinkumThreadError)));
    object->vtable = _ZTVSt13runtime_error_nid_postfix + 2;
    object->message = {};
    object->message.capacity = GuestLocale::StringInlineCapacity;
    try {
        _ZNSs6assignEPKcm_nid_postfix(&object->message, message, std::strlen(message));
    } catch (...) {
        __cxa_free_exception_nid_postfix(object);
        throw;
    }
    object->vtable = &table.destroy;
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(table.type)),
        reinterpret_cast<void (*)(void*)>(table.destroy));
}

}

extern "C" {

void APS5_VABI _ZN6Dinkum7threads10lock_errorD0Ev_nid_postfix(DinkumThreadError* self) {
    Destroy(self);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZN6Dinkum7threads10lock_errorD1Ev_nid_postfix(DinkumThreadError* self) {
    Destroy(self);
}

[[noreturn]] void APS5_VABI _ZN6Dinkum7threads17_Throw_lock_errorEv_nid_postfix() {
    Throw(LockErrorVtable, "lock error");
}

[[noreturn]] void APS5_VABI _ZN6Dinkum7threads21_Throw_resource_errorEv_nid_postfix() {
    Throw(ResourceErrorVtable, "thread resource error");
}

void APS5_VABI _ZN6Dinkum7threads21thread_resource_errorD0Ev_nid_postfix(DinkumThreadError* self) {
    Destroy(self);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZN6Dinkum7threads21thread_resource_errorD1Ev_nid_postfix(DinkumThreadError* self) {
    Destroy(self);
}

}
