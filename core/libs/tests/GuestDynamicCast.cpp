#include "prx/libc/include/exceptions/Runtime.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <typeinfo>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
struct Vtable { std::intptr_t offsetToTop; const TypeRecord* type; const void* first; };
struct Object { const void* vtable; };
struct Source { virtual ~Source() = default; };
struct Side { virtual ~Side() = default; };
struct Native : Side, Source {};
struct PrivateNative : private Source { virtual ~PrivateNative() = default; };
struct VirtualNative : virtual Source { virtual ~VirtualNative() = default; };
struct BaseRecord { const TypeRecord* type; std::ptrdiff_t flags; };
struct MultipleRecord { const void* vtable; const char* name; unsigned flags; unsigned count; BaseRecord bases[1]; };

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
const Vtable BaseVtable{0, &BaseType, nullptr};
const Vtable DerivedVtable{0, &DerivedType, nullptr};
const MultipleRecord MultipleType{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "8Multiple", 0, 1,
                                  {{&BaseType, (16 << 8) | 2}}};
const MultipleRecord PrivateType{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "7Private", 0, 1,
                                 {{&BaseType, (16 << 8) | 1}}};
const MultipleRecord VirtualType{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "7Virtual", 0, 1,
                                 {{&BaseType, (-3 * static_cast<std::ptrdiff_t>(sizeof(void*)) * 256) | 3}}};

int FindPublic(const TypeRecord* type, std::ptrdiff_t hint, const void* object, const TypeRecord* sourceType,
               const void* source) {
    using Function = int (APS5_VABI *)(const void*, std::ptrdiff_t, const void*, const void*, const void*);
    const auto* table = static_cast<const unsigned char*>(type->vtable);
    Function function;
    std::memcpy(&function, table + 8 * sizeof(void*), sizeof(function));
    return function(type, hint, object, sourceType, source);
}

} // namespace

int main() {
    const Object derived{&DerivedVtable.first};
    const Object base{&BaseVtable.first};
    Require(__dynamic_cast_nid_postfix(&derived, &BaseType, &DerivedType, 0) == &derived);
    Require(__dynamic_cast_nid_postfix(&derived, &BaseType, &OtherType, 0) == nullptr);
    Require(__dynamic_cast_nid_postfix(&base, &BaseType, &DerivedType, 0) == nullptr);
    Require(__dynamic_cast_nid_postfix(nullptr, &BaseType, &DerivedType, 0) == nullptr);
    std::array<unsigned char, 32> multipleObject{};
    Require(FindPublic(reinterpret_cast<const TypeRecord*>(&MultipleType), -1, multipleObject.data(), &BaseType,
                       multipleObject.data() + 16) == 6);
    Require(FindPublic(reinterpret_cast<const TypeRecord*>(&MultipleType), -1, multipleObject.data(), &BaseType,
                       multipleObject.data()) == 1);
    Require(FindPublic(reinterpret_cast<const TypeRecord*>(&PrivateType), -1, multipleObject.data(), &BaseType,
                       multipleObject.data() + 16) == 1);
    std::array<std::intptr_t, 4> virtualTable{};
    virtualTable[0] = 16;
    const auto* virtualAddressPoint = virtualTable.data() + 3;
    alignas(std::intptr_t) std::array<unsigned char, 32> virtualObject{};
    *reinterpret_cast<const std::intptr_t**>(virtualObject.data()) = virtualAddressPoint;
    Require(FindPublic(reinterpret_cast<const TypeRecord*>(&VirtualType), -1, virtualObject.data(), &BaseType,
                       virtualObject.data() + 16) == 7);
    Require(FindPublic(reinterpret_cast<const TypeRecord*>(&MultipleType), -3, multipleObject.data(), &BaseType,
                       multipleObject.data() + 16) == 6);
    void* object = __cxa_allocate_exception_nid_postfix(16);
    auto* type = reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(&DerivedType));
    LibcException::Header* header = __cxa_init_primary_exception_nid_postfix(object, type, Destroy);
    Require(header == LibcException::FromObject(object));
    Require(header->type == type && header->destructor == Destroy && header->adjusted == object);
    __cxa_free_exception_nid_postfix(object);
}
