#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <typeinfo>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
struct Vtable { std::intptr_t offsetToTop; const TypeRecord* type; const void* first; };
struct Object { const void* vtable; };

extern "C" {
extern const unsigned char _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix[];

}

namespace {

void Require(bool value) { if (!value) std::abort(); }

void Destroy(void*) {}

const TypeRecord BaseType{_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix + 16, "4Base", nullptr};
const TypeRecord DerivedType{_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix + 16, "7Derived", &BaseType};
const TypeRecord OtherType{_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix + 16, "5Other", &BaseType};
const Vtable BaseVtable{0, &BaseType, nullptr};
const Vtable DerivedVtable{0, &DerivedType, nullptr};

} // namespace

int main() {
    const Object derived{&DerivedVtable.first};
    const Object base{&BaseVtable.first};
    Require(__dynamic_cast_nid_postfix(&derived, reinterpret_cast<const __cxxabiv1::__class_type_info*>(&BaseType), reinterpret_cast<const __cxxabiv1::__class_type_info*>(&DerivedType), 0) == &derived);
    Require(__dynamic_cast_nid_postfix(&derived, reinterpret_cast<const __cxxabiv1::__class_type_info*>(&BaseType), reinterpret_cast<const __cxxabiv1::__class_type_info*>(&OtherType), 0) == nullptr);
    Require(__dynamic_cast_nid_postfix(&base, reinterpret_cast<const __cxxabiv1::__class_type_info*>(&BaseType), reinterpret_cast<const __cxxabiv1::__class_type_info*>(&DerivedType), 0) == nullptr);
    Require(__dynamic_cast_nid_postfix(nullptr, reinterpret_cast<const __cxxabiv1::__class_type_info*>(&BaseType), reinterpret_cast<const __cxxabiv1::__class_type_info*>(&DerivedType), 0) == nullptr);
    void* object = __cxa_allocate_exception_nid_postfix(16);
    auto* type = reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(&DerivedType));
    LibcException::Header* header = __cxa_init_primary_exception_nid_postfix(object, type, Destroy);
    Require(header == LibcException::FromObject(object));
    Require(header->type == type && header->destructor == Destroy && header->adjusted == object);
    __cxa_free_exception_nid_postfix(object);

}
