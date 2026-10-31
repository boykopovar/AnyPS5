#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <typeinfo>
#include <tuple>
#include <utility>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
struct Vtable { std::intptr_t offsetToTop; const TypeRecord* type; const void* first; };
struct Object { const void* vtable; };
struct BaseRecord { const void* type; std::ptrdiff_t flags; };
struct MultipleRecord { const void* vtable; const char* name; unsigned flags; unsigned count; BaseRecord bases[3]; };
struct UpcastResult { const void* destination; int path; int sourceDetails; const TypeRecord* baseType; };
struct DynamicCastResult { const void* destination; int wholeToDestination; int wholeToSource; int destinationToSource; int wholeDetails; };

extern "C" {
extern const unsigned char _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix[];
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
constexpr std::ptrdiff_t VirtualBaseSlot = -3 * static_cast<std::ptrdiff_t>(sizeof(void*)) * 256;
const MultipleRecord PublicVirtualParent{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "1L", 2, 1, {{&BaseType, VirtualBaseSlot | 3}}};
const MultipleRecord PrivateVirtualParent{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "1R", 2, 1, {{&BaseType, VirtualBaseSlot | 1}}};
const MultipleRecord PublicFirstChild{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "1D", 2, 2, {{&PublicVirtualParent, 2}, {&PrivateVirtualParent, (8 << 8) | 2}}};
const MultipleRecord PrivateFirstChild{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "1E", 2, 2, {{&PrivateVirtualParent, 2}, {&PublicVirtualParent, (8 << 8) | 2}}};
const MultipleRecord PrivateOnlyChild{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "1F", 2, 1, {{&PrivateVirtualParent, 2}}};
const MultipleRecord PublicFirstRoot{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "4RootD", 0, 1, {{&PublicFirstChild, 0}}};
const MultipleRecord PrivateFirstRoot{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "4RootE", 0, 1, {{&PrivateFirstChild, 0}}};
const MultipleRecord PrivateOnlyRoot{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "4RootF", 0, 1, {{&PrivateOnlyChild, 0}}};
const TypeRecord SourceOnlyType{_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix + 16, "6Source", nullptr};
const MultipleRecord PublicTargetFirstWhole{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "1W", 0, 3, {{&DerivedType, 2}, {&DerivedType, 8 << 8}, {&SourceOnlyType, (16 << 8) | 2}}};
const MultipleRecord PrivateTargetFirstWhole{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "1V", 0, 3, {{&DerivedType, 0}, {&DerivedType, (8 << 8) | 2}, {&SourceOnlyType, (16 << 8) | 2}}};
const MultipleRecord PublicTargetOnlyWhole{_ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix + 16, "1U", 0, 2, {{&DerivedType, 2}, {&SourceOnlyType, (16 << 8) | 2}}};
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
                     const TypeRecord* source, const void* sourceObject, Result& result, std::ptrdiff_t hint = 0) {
    using Function = bool (APS5_VABI *)(const TypeRecord*, std::ptrdiff_t, int, const TypeRecord*, const void*,
                                        const TypeRecord*, const void*, Result&);
    Function function;
    std::memcpy(&function, static_cast<const unsigned char*>(type->vtable) + 7 * sizeof(void*), sizeof(function));
    return function(type, hint, 6, destination, object, source, sourceObject, result);
}

} // namespace

int main() {
    const Object derived{&DerivedVtable.first};
    const Object base{&BaseVtable.first};
    Require(__dynamic_cast_nid_postfix(&derived, reinterpret_cast<const __cxxabiv1::__class_type_info*>(&BaseType), reinterpret_cast<const __cxxabiv1::__class_type_info*>(&DerivedType), 0) == &derived);
    Require(__dynamic_cast_nid_postfix(&derived, reinterpret_cast<const __cxxabiv1::__class_type_info*>(&BaseType), reinterpret_cast<const __cxxabiv1::__class_type_info*>(&OtherType), 0) == nullptr);
    Require(__dynamic_cast_nid_postfix(&base, reinterpret_cast<const __cxxabiv1::__class_type_info*>(&BaseType), reinterpret_cast<const __cxxabiv1::__class_type_info*>(&DerivedType), 0) == nullptr);
    Require(__dynamic_cast_nid_postfix(nullptr, reinterpret_cast<const __cxxabiv1::__class_type_info*>(&BaseType), reinterpret_cast<const __cxxabiv1::__class_type_info*>(&DerivedType), 0) == nullptr);
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

    const auto asType = [](const MultipleRecord& record) { return reinterpret_cast<const TypeRecord*>(&record); };
    const auto publicToSource = [&](const MultipleRecord& root, const MultipleRecord& child, std::intptr_t leftOffset,
                                    std::intptr_t rightOffset, std::ptrdiff_t leftSlot, std::ptrdiff_t rightSlot) {
        alignas(std::intptr_t) unsigned char whole[32]{};
        std::intptr_t leftTable[4]{leftOffset};
        std::intptr_t rightTable[4]{rightOffset};
        *reinterpret_cast<const std::intptr_t**>(whole + leftSlot) = leftTable + 3;
        *reinterpret_cast<const std::intptr_t**>(whole + rightSlot) = rightTable + 3;
        DynamicCastResult result{nullptr, 0, 0, 0, 0x10};
        Require(!CallDynamicCast(asType(root), asType(child), whole, &BaseType, whole + 16, result, -1));
        return std::pair<bool, bool>{result.destination == whole, (result.destinationToSource & 2) != 0};
    };
    const auto publicFirst = publicToSource(PublicFirstRoot, PublicFirstChild, 16, 8, 0, 8);
    Require(publicFirst.first && publicFirst.second);
    const auto privateFirst = publicToSource(PrivateFirstRoot, PrivateFirstChild, 8, 16, 8, 0);
    Require(privateFirst.first && privateFirst.second);
    const auto privateOnly = publicToSource(PrivateOnlyRoot, PrivateOnlyChild, 0, 16, 8, 0);
    Require(!privateOnly.first && !privateOnly.second);

    const auto ambiguousDestination = [&](const MultipleRecord& whole) {
        alignas(std::intptr_t) unsigned char complete[32]{};
        DynamicCastResult result{nullptr, 0, 0, 0, 0x10};
        const bool ambiguous = CallDynamicCast(asType(whole), &DerivedType, complete, &SourceOnlyType, complete + 16, result, -1);
        return std::tuple<bool, bool, int>{ambiguous, result.destination == nullptr, result.wholeToDestination};
    };
    const auto publicFirstTargets = ambiguousDestination(PublicTargetFirstWhole);
    Require(std::get<0>(publicFirstTargets) && std::get<1>(publicFirstTargets) && std::get<2>(publicFirstTargets) == 2);
    const auto privateFirstTargets = ambiguousDestination(PrivateTargetFirstWhole);
    Require(std::get<0>(privateFirstTargets) && std::get<1>(privateFirstTargets) && std::get<2>(privateFirstTargets) == 2);
    const auto onlyPublicTarget = ambiguousDestination(PublicTargetOnlyWhole);
    Require(!std::get<0>(onlyPublicTarget) && !std::get<1>(onlyPublicTarget));

    void* object = __cxa_allocate_exception_nid_postfix(16);
    auto* type = reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(&DerivedType));
    LibcException::Header* header = __cxa_init_primary_exception_nid_postfix(object, type, Destroy);
    Require(header == LibcException::FromObject(object));
    Require(header->type == type && header->destructor == Destroy && header->adjusted == object);
    __cxa_free_exception_nid_postfix(object);

}
