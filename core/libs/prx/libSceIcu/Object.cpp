// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "prx/libc/include/General.hpp"

namespace Icu::Abi {
namespace {
alignas(std::max_align_t) const std::int32_t Zero[6]{};
std::array<char, 10> ClassIds{};
}
void* Allocate_nid_no_patch(std::size_t size) { return size ? std::malloc(size) : const_cast<std::int32_t*>(Zero); }
void Free_nid_no_patch(void* pointer) { if (pointer != Zero) std::free(pointer); }
void* ClassId_nid_no_patch(Class type) { return &ClassIds[static_cast<std::size_t>(type)]; }
void Initialize_nid_no_patch(void* object, Class type) {
    if (!object) throw std::invalid_argument("ICU: null object");
    static_cast<Object*>(object)->vtable = Vtable_nid_no_patch(type);
}
}

extern "C" {
void* APS5_VABI _ZN6icu_687UMemorynwEm_nid_postfix(std::size_t size) {
    return Icu::Abi::Allocate_nid_no_patch(size);
}

void* APS5_VABI _ZN6icu_687UMemorynaEm_nid_postfix(std::size_t size) {
    return Icu::Abi::Allocate_nid_no_patch(size);
}

void APS5_VABI _ZN6icu_687UMemorydlEPv_nid_postfix(void* pointer) {
    Icu::Abi::Free_nid_no_patch(pointer);
}

void APS5_VABI _ZN6icu_687UMemorydaEPv_nid_postfix(void* pointer) {
    Icu::Abi::Free_nid_no_patch(pointer);
}

void APS5_VABI _ZN6icu_687UObjectD1Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Object);
}

void APS5_VABI _ZN6icu_687UObjectD2Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Object);
}

void APS5_VABI _ZN6icu_687UObjectD0Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Object);
    Icu::Abi::Free_nid_no_patch(object);
}

void APS5_VABI _ZN6icu_6811ReplaceableD1Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Object);
}

void APS5_VABI _ZN6icu_6811ReplaceableD2Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Object);
}

void APS5_VABI _ZN6icu_6811ReplaceableD0Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Object);
    Icu::Abi::Free_nid_no_patch(object);
}

void* APS5_VABI _ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix(const void*) {
    return nullptr;
}

std::int8_t APS5_VABI _ZNK6icu_6811Replaceable11hasMetaDataEv_nid_postfix(const void*) {
    return 1;
}

void* APS5_VABI _ZNK6icu_6811Replaceable5cloneEv_nid_postfix(const void*) {
    return nullptr;
}

}

namespace Icu::Abi {
namespace {
void APS5_VABI DestroyType_nid_no_patch(void*) {}
void APS5_VABI DeleteType_nid_no_patch(void*) { throw std::invalid_argument("ICU: static RTTI deletion"); }
bool APS5_VABI FalseType_nid_no_patch(const void*) { return false; }
bool SameType(const TypeInfo* first, const TypeInfo* second) { return first == second || (first && second && std::strcmp(first->name, second->name) == 0); }
const TypeInfo* BaseType(const TypeInfo* type) {
    if (!type || !type->vtable) return nullptr;
    const auto* category = reinterpret_cast<const TypeInfo* const*>(type->vtable)[-1];
    if (!category || std::strcmp(category->name, "N10__cxxabiv120__si_class_type_infoE")) return nullptr;
    return reinterpret_cast<const DerivedTypeInfo*>(type)->base;
}
bool APS5_VABI CatchType_nid_no_patch(const TypeInfo* caught, const TypeInfo* thrown, void**, unsigned) {
    for (auto* type = thrown; type; type = BaseType(type)) if (SameType(caught, type)) return true;
    return false;
}
bool APS5_VABI UpcastType_nid_no_patch(const TypeInfo* self, const TypeInfo* base, void**) {
    for (auto* type = self; type; type = BaseType(type)) if (SameType(base, type)) return true;
    return false;
}
void APS5_VABI UnsupportedType_nid_no_patch() { NotImplemented_nid_no_patch(__func__); }
struct TypeTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    GuestFunction slots[9];
};
static_assert(sizeof(TypeTable) == 88 && alignof(TypeTable) == 8);
static_assert(offsetof(TypeTable, slots) == 16);
const TypeInfo ClassCategory{nullptr, "N10__cxxabiv117__class_type_infoE"};
const TypeInfo SingleCategory{nullptr, "N10__cxxabiv120__si_class_type_infoE"};
TypeTable MakeTypeTable(const TypeInfo* category) {
    return {0, category, {reinterpret_cast<GuestFunction>(DestroyType_nid_no_patch), reinterpret_cast<GuestFunction>(DeleteType_nid_no_patch), reinterpret_cast<GuestFunction>(FalseType_nid_no_patch), reinterpret_cast<GuestFunction>(FalseType_nid_no_patch), reinterpret_cast<GuestFunction>(CatchType_nid_no_patch), reinterpret_cast<GuestFunction>(UpcastType_nid_no_patch), UnsupportedType_nid_no_patch, UnsupportedType_nid_no_patch, UnsupportedType_nid_no_patch}};
}
const TypeTable ClassTable = MakeTypeTable(&ClassCategory);
const TypeTable SingleTable = MakeTypeTable(&SingleCategory);
const TypeInfo MemoryType{ClassTable.slots, "N6icu_687UMemoryE"};
const DerivedTypeInfo ObjectType{{SingleTable.slots, "N6icu_687UObjectE"}, &MemoryType};
const DerivedTypeInfo ReplaceableType{{SingleTable.slots, "N6icu_6811ReplaceableE"}, &ObjectType.type};
const DerivedTypeInfo UnicodeStringType{{SingleTable.slots, "N6icu_6813UnicodeStringE"}, &ReplaceableType.type};
const DerivedTypeInfo AppendableType{{SingleTable.slots, "N6icu_6810AppendableE"}, &ObjectType.type};
const DerivedTypeInfo UnicodeStringAppendableType{{SingleTable.slots, "N6icu_6823UnicodeStringAppendableE"}, &AppendableType.type};
const DerivedTypeInfo ByteSinkType{{SingleTable.slots, "N6icu_688ByteSinkE"}, &MemoryType};
const DerivedTypeInfo CheckedArrayByteSinkType{{SingleTable.slots, "N6icu_6820CheckedArrayByteSinkE"}, &ByteSinkType.type};
const DerivedTypeInfo ForwardType{{SingleTable.slots, "N6icu_6824ForwardCharacterIteratorE"}, &ObjectType.type};
const DerivedTypeInfo CharacterType{{SingleTable.slots, "N6icu_6817CharacterIteratorE"}, &ForwardType.type};
const DerivedTypeInfo UCharType{{SingleTable.slots, "N6icu_6822UCharCharacterIteratorE"}, &CharacterType.type};
const DerivedTypeInfo StringCharacterIteratorType{{SingleTable.slots, "N6icu_6823StringCharacterIteratorE"}, &UCharType.type};
const DerivedTypeInfo StringEnumerationType{{SingleTable.slots, "N6icu_6817StringEnumerationE"}, &ObjectType.type};
const DerivedTypeInfo UStringEnumerationType{{SingleTable.slots, "N6icu_6818UStringEnumerationE"}, &StringEnumerationType.type};
template<std::size_t TCount> struct VirtualTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, TCount> slots;
};
static_assert(sizeof(VirtualTable<3>) == 40 && alignof(VirtualTable<3>) == 8);
static_assert(offsetof(VirtualTable<3>, slots) == 16);
}
const TypeInfo* Rtti_nid_no_patch(Class type) {
    switch (type) {
    case Class::Object: return &ObjectType.type;
    case Class::Replaceable: return &ReplaceableType.type;
    case Class::UnicodeString: return &UnicodeStringType.type;
    case Class::Appendable: return &AppendableType.type;
    case Class::UnicodeStringAppendable: return &UnicodeStringAppendableType.type;
    case Class::ByteSink: return &ByteSinkType.type;
    case Class::CheckedArrayByteSink: return &CheckedArrayByteSinkType.type;
    case Class::StringCharacterIterator: return &StringCharacterIteratorType.type;
    case Class::StringEnumeration: return &StringEnumerationType.type;
    case Class::UStringEnumeration: return &UStringEnumerationType.type;
    }
    throw std::invalid_argument("ICU: invalid class");
}
const GuestFunction* Vtable_nid_no_patch(Class type) {
    static const VirtualTable<3> ObjectTable{0, Rtti_nid_no_patch(Class::Object), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_687UObjectD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_687UObjectD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix),
    }}};
    static const VirtualTable<11> ReplaceableTable{0, Rtti_nid_no_patch(Class::Replaceable), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6811ReplaceableD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6811ReplaceableD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix),
        UnsupportedType_nid_no_patch,
        UnsupportedType_nid_no_patch,
        UnsupportedType_nid_no_patch,
        reinterpret_cast<GuestFunction>(_ZNK6icu_6811Replaceable11hasMetaDataEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6811Replaceable5cloneEv_nid_postfix),
        UnsupportedType_nid_no_patch,
        UnsupportedType_nid_no_patch,
        UnsupportedType_nid_no_patch,
    }}};
    static const VirtualTable<11> UnicodeStringTable{0, Rtti_nid_no_patch(Class::UnicodeString), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6813UnicodeStringD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6813UnicodeStringD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813UnicodeString17getDynamicClassIDEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813UnicodeString14extractBetweenEiiRS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6813UnicodeString20handleReplaceBetweenEiiRKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6813UnicodeString4copyEiii_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813UnicodeString11hasMetaDataEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813UnicodeString5cloneEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813UnicodeString11getChar32AtEi_nid_postfix),
    }}};
    static const VirtualTable<8> AppendableTable{0, Rtti_nid_no_patch(Class::Appendable), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6810AppendableD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810AppendableD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix),
        UnsupportedType_nid_no_patch,
        reinterpret_cast<GuestFunction>(_ZN6icu_6810Appendable15appendCodePointEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810Appendable12appendStringEPKDsi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810Appendable21reserveAppendCapacityEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix),
    }}};
    static const VirtualTable<8> UnicodeStringAppendableTable{0, Rtti_nid_no_patch(Class::UnicodeStringAppendable), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6823UnicodeStringAppendableD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6823UnicodeStringAppendableD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6823UnicodeStringAppendable14appendCodeUnitEDs_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6823UnicodeStringAppendable15appendCodePointEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6823UnicodeStringAppendable12appendStringEPKDsi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6823UnicodeStringAppendable21reserveAppendCapacityEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6823UnicodeStringAppendable15getAppendBufferEiiPDsiPi_nid_postfix),
    }}};
    static const VirtualTable<5> ByteSinkTable{0, Rtti_nid_no_patch(Class::ByteSink), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_688ByteSinkD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_688ByteSinkD0Ev_nid_postfix),
        UnsupportedType_nid_no_patch,
        reinterpret_cast<GuestFunction>(_ZN6icu_688ByteSink15GetAppendBufferEiiPciPi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_688ByteSink5FlushEv_nid_postfix),
    }}};
    static const VirtualTable<6> CheckedArrayByteSinkTable{0, Rtti_nid_no_patch(Class::CheckedArrayByteSink), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6820CheckedArrayByteSinkD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6820CheckedArrayByteSinkD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6820CheckedArrayByteSink15GetAppendBufferEiiPciPi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_688ByteSink5FlushEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6820CheckedArrayByteSink5ResetEv_nid_postfix),
    }}};
    static const VirtualTable<27> StringCharacterIteratorTable{0, Rtti_nid_no_patch(Class::StringCharacterIterator), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6823StringCharacterIteratorD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6823StringCharacterIteratorD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6823StringCharacterIterator17getDynamicClassIDEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6823StringCharacterIteratoreqERKNS_24ForwardCharacterIteratorE_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIterator8hashCodeEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator11nextPostIncEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator13next32PostIncEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator7hasNextEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6823StringCharacterIterator5cloneEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator5firstEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator12firstPostIncEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator7first32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator14first32PostIncEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator4lastEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator6last32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator8setIndexEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator10setIndex32Ei_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIterator7currentEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIterator9current32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator4nextEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator6next32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator8previousEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator10previous32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator11hasPreviousEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6823StringCharacterIterator7getTextERNS_13UnicodeStringE_nid_postfix),
    }}};
    static const VirtualTable<11> StringEnumerationTable{0, Rtti_nid_no_patch(Class::StringEnumeration), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6817StringEnumerationD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6817StringEnumerationD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6817StringEnumeration5cloneEv_nid_postfix),
        UnsupportedType_nid_no_patch,
        reinterpret_cast<GuestFunction>(_ZN6icu_6817StringEnumeration4nextEPiR10UErrorCode_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6817StringEnumeration5unextEPiR10UErrorCode_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6817StringEnumeration5snextER10UErrorCode_nid_postfix),
        UnsupportedType_nid_no_patch,
        reinterpret_cast<GuestFunction>(_ZNK6icu_6817StringEnumerationeqERKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6817StringEnumerationneERKS0__nid_postfix),
    }}};
    static const VirtualTable<11> UStringEnumerationTable{0, Rtti_nid_no_patch(Class::UStringEnumeration), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6818UStringEnumerationD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818UStringEnumerationD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6818UStringEnumeration17getDynamicClassIDEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6817StringEnumeration5cloneEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6818UStringEnumeration5countER10UErrorCode_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818UStringEnumeration4nextEPiR10UErrorCode_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6817StringEnumeration5unextEPiR10UErrorCode_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818UStringEnumeration5snextER10UErrorCode_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818UStringEnumeration5resetER10UErrorCode_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6817StringEnumerationeqERKS0__nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6817StringEnumerationneERKS0__nid_postfix),
    }}};
    switch (type) {
    case Class::Object: return ObjectTable.slots.data();
    case Class::Replaceable: return ReplaceableTable.slots.data();
    case Class::UnicodeString: return UnicodeStringTable.slots.data();
    case Class::Appendable: return AppendableTable.slots.data();
    case Class::UnicodeStringAppendable: return UnicodeStringAppendableTable.slots.data();
    case Class::ByteSink: return ByteSinkTable.slots.data();
    case Class::CheckedArrayByteSink: return CheckedArrayByteSinkTable.slots.data();
    case Class::StringCharacterIterator: return StringCharacterIteratorTable.slots.data();
    case Class::StringEnumeration: return StringEnumerationTable.slots.data();
    case Class::UStringEnumeration: return UStringEnumerationTable.slots.data();
    }
    throw std::invalid_argument("ICU: invalid class");
}
}
