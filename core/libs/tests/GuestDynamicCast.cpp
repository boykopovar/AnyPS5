#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <typeinfo>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
struct Vtable { std::intptr_t offsetToTop; const TypeRecord* type; const void* first; };
struct Object { const void* vtable; };
struct BaseRecord { const TypeRecord* type; std::ptrdiff_t flags; };
struct MultipleRecord { const void* vtable; const char* name; unsigned flags; unsigned count; BaseRecord bases[2]; };
struct UpcastResult { const void* destination; int path; int sourceDetails; const TypeRecord* baseType; };
struct DynamicCastResult { const void* destination; int wholeToDestination; int wholeToSource; int destinationToSource; int wholeDetails; };

extern "C" {
extern const unsigned char _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix[];
void* APS5_VABI __dynamic_cast_nid_postfix(const void*, const void*, const void*, std::ptrdiff_t);
LibcException::Header* APS5_VABI __cxa_init_primary_exception_nid_postfix(void*, std::type_info*, void (*)(void*));
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

void Destroy(void*) {}

const TypeRecord BaseType{_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix + 16, "4Base", nullptr};
const TypeRecord DerivedType{_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix + 16, "7Derived", &BaseType};
const TypeRecord OtherType{_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix + 16, "5Other", &BaseType};
const MultipleRecord MultipleType{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "8Multiple", 0, 1,
                                  {{&BaseType, (16 << 8) | 2}}};
const MultipleRecord PrivateType{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "7Private", 0, 1,
                                 {{&BaseType, (16 << 8)}}};
const MultipleRecord AmbiguousType{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "10Ambiguous", 1, 2,
                                   {{&BaseType, (8 << 8) | 2}, {&BaseType, (16 << 8) | 2}}};
const MultipleRecord CrossType{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "5Cross", 1, 2,
                               {{&BaseType, (8 << 8) | 2}, {&OtherType, (16 << 8) | 2}}};
const MultipleRecord VirtualType{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "7Virtual", 2, 1,
                                 {{&BaseType, (-3 * static_cast<std::ptrdiff_t>(sizeof(void*)) * 256) | 3}}};
const Vtable BaseVtable{0, &BaseType, nullptr};
const Vtable DerivedVtable{0, &DerivedType, nullptr};

template<class Result>
bool CallUpcast(const TypeRecord* type, const TypeRecord* destination, const void* object, Result& result) {
    using Function = bool (APS5_VABI *)(const TypeRecord*, const TypeRecord*, const void*, Result&);
    Function function;
    std::memcpy(&function, static_cast<const unsigned char*>(type->vtable) + 6 * sizeof(void*), sizeof(function));
    return function(type, destination, object, result);
}

template<class Result>
bool CallDynamicCast(const TypeRecord* type, const TypeRecord* destination, const void* object,
                     const TypeRecord* source, const void* sourceObject, Result& result) {
    using Function = bool (APS5_VABI *)(const TypeRecord*, std::ptrdiff_t, int, const TypeRecord*, const void*,
                                        const TypeRecord*, const void*, Result&);
    Function function;
    std::memcpy(&function, static_cast<const unsigned char*>(type->vtable) + 7 * sizeof(void*), sizeof(function));
    return function(type, 0, 6, destination, object, source, sourceObject, result);
}

} // namespace

int main() {
    const Object derived{&DerivedVtable.first};
    const Object base{&BaseVtable.first};
    Require(__dynamic_cast_nid_postfix(&derived, &BaseType, &DerivedType, 0) == &derived);
    Require(__dynamic_cast_nid_postfix(&derived, &BaseType, &OtherType, 0) == nullptr);
    Require(__dynamic_cast_nid_postfix(&base, &BaseType, &DerivedType, 0) == nullptr);
    Require(__dynamic_cast_nid_postfix(nullptr, &BaseType, &DerivedType, 0) == nullptr);
    alignas(std::intptr_t) unsigned char instance[32]{};
    UpcastResult upcast{};
    Require(CallUpcast(reinterpret_cast<const TypeRecord*>(&MultipleType), &BaseType, instance, upcast));
    Require(upcast.destination == instance + 16 && upcast.path == 6 &&
            reinterpret_cast<std::uintptr_t>(upcast.baseType) == 1);
    Require(CallUpcast(reinterpret_cast<const TypeRecord*>(&PrivateType), &BaseType, instance, upcast));
    Require(upcast.destination == instance + 16 && upcast.path == 4 &&
            reinterpret_cast<std::uintptr_t>(upcast.baseType) == 1);
    Require(CallUpcast(reinterpret_cast<const TypeRecord*>(&AmbiguousType), &BaseType, instance, upcast));
    Require(upcast.destination == nullptr && upcast.path == 2 && upcast.baseType == nullptr);
    std::intptr_t virtualTable[4]{};
    virtualTable[0] = 16;
    *reinterpret_cast<const std::intptr_t**>(instance) = virtualTable + 3;
    Require(CallUpcast(reinterpret_cast<const TypeRecord*>(&VirtualType), &BaseType, instance, upcast));
    Require(upcast.destination == instance + 16 && upcast.path == 7 && upcast.baseType == &BaseType);
    DynamicCastResult dynamic{};
    dynamic.wholeDetails = 0x10;
    Require(!CallDynamicCast(reinterpret_cast<const TypeRecord*>(&DerivedType), &DerivedType, instance, &BaseType,
                             instance, dynamic));
    Require(dynamic.destination == instance && dynamic.destinationToSource == 6);
    dynamic = {nullptr, 0, 0, 0, 0x10};
    Require(!CallDynamicCast(reinterpret_cast<const TypeRecord*>(&CrossType), &OtherType, instance, &BaseType,
                             instance + 8, dynamic));
    Require(dynamic.destination == instance + 16 && dynamic.destinationToSource == 1);
    dynamic = {nullptr, 0, 0, 0, 0x10};
    Require(!CallDynamicCast(reinterpret_cast<const TypeRecord*>(&PrivateType),
                             reinterpret_cast<const TypeRecord*>(&PrivateType), instance, &BaseType,
                             instance + 16, dynamic));
    Require(dynamic.destination == nullptr);
    void* object = __cxa_allocate_exception_nid_postfix(16);
    auto* type = reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(&DerivedType));
    LibcException::Header* header = __cxa_init_primary_exception_nid_postfix(object, type, Destroy);
    Require(header == LibcException::FromObject(object));
    Require(header->type == type && header->destructor == Destroy && header->adjusted == object);
    __cxa_free_exception_nid_postfix(object);
}
