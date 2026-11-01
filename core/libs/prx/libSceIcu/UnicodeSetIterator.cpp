// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (c) 2002-2006, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <limits>

namespace {
using namespace Icu::Abi;
using Iterator = UnicodeSetIterator;
template<typename T> T& Require(T* pointer) {
    if (!pointer) throw std::invalid_argument("UnicodeSetIterator: null pointer");
    return *pointer;
}
const GuestFunction* Vtable() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_6818UnicodeSetIteratorE"}, Rtti_nid_no_patch(Class::Object)};
    struct Table { std::ptrdiff_t offset; const TypeInfo* type; std::array<GuestFunction,4> slots; };
    static_assert(sizeof(Table) == 48 && offsetof(Table,slots) == 16);
    static const Table table{0, &type.type, {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6818UnicodeSetIteratorD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818UnicodeSetIteratorD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6818UnicodeSetIterator17getDynamicClassIDEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818UnicodeSetIterator9loadRangeEi_nid_postfix)}}};
    return table.slots.data();
}
void Load(Iterator& iterator) { Virtual_nid_no_patch<void (APS5_VABI *)(Iterator*,std::int32_t)>(&iterator,3)(&iterator,iterator.range); }
std::int32_t After(std::int32_t point) {
    if (point == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("UnicodeSetIterator: increment overflow");
    return point + 1;
}
void Reset(Iterator& iterator) {
    iterator.endRange = iterator.set ? _ZNK6icu_6810UnicodeSet13getRangeCountEv_nid_postfix(iterator.set) - 1 : -1;
    iterator.stringCount = iterator.set ? _ZNK6icu_6810UnicodeSet11stringsSizeEv_nid_postfix(iterator.set) : 0;
    iterator.range = 0;
    iterator.endElement = -1;
    iterator.nextElement = 0;
    if (iterator.endRange >= 0) Load(iterator);
    iterator.nextString = 0;
    iterator.string = nullptr;
}
void Initialize(Iterator& iterator, const UnicodeSet* set) {
    iterator.vtable = Vtable();
    iterator.set = set;
    iterator.cpString = nullptr;
    Reset(iterator);
}
void Destroy(Iterator& iterator) {
    if (iterator.cpString) Virtual_nid_no_patch<void (APS5_VABI *)(UnicodeString*)>(iterator.cpString,1)(iterator.cpString);
    iterator.vtable = Vtable_nid_no_patch(Class::Object);
}
std::int8_t Next(Iterator& iterator, bool range) {
    if (range) iterator.string = nullptr;
    if (iterator.nextElement > iterator.endElement) {
        if (iterator.range >= iterator.endRange) {
            if (iterator.nextString >= iterator.stringCount) return 0;
            iterator.codepoint = -1;
            iterator.string = _ZNK6icu_6810UnicodeSet9getStringEi_nid_postfix(iterator.set,iterator.nextString);
            iterator.nextString = After(iterator.nextString);
            return 1;
        }
        iterator.range = After(iterator.range);
        Load(iterator);
    }
    iterator.codepoint = iterator.nextElement;
    iterator.codepointEnd = range ? iterator.endElement : iterator.nextElement;
    iterator.nextElement = After(iterator.codepointEnd);
    iterator.string = nullptr;
    return 1;
}
void* Id() { static char id; return &id; }
}

extern "C" {
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorC1Ev_nid_postfix(Iterator* self) { Initialize(Require(self),nullptr); }
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorC2Ev_nid_postfix(Iterator* self) { Initialize(Require(self),nullptr); }
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorC1ERKNS_10UnicodeSetE_nid_postfix(Iterator* self, const UnicodeSet* set) { Initialize(Require(self),&Require(set)); }
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorC2ERKNS_10UnicodeSetE_nid_postfix(Iterator* self, const UnicodeSet* set) { Initialize(Require(self),&Require(set)); }
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorD1Ev_nid_postfix(Iterator* self) { Destroy(Require(self)); }
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorD2Ev_nid_postfix(Iterator* self) { Destroy(Require(self)); }
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorD0Ev_nid_postfix(Iterator* self) { Destroy(Require(self)); Free_nid_no_patch(self); }
void APS5_VABI _ZN6icu_6818UnicodeSetIterator5resetEv_nid_postfix(Iterator* self) { Reset(Require(self)); }
void APS5_VABI _ZN6icu_6818UnicodeSetIterator5resetERKNS_10UnicodeSetE_nid_postfix(Iterator* self,const UnicodeSet* set) { Require(self).set=&Require(set); Reset(*self); }
void APS5_VABI _ZN6icu_6818UnicodeSetIterator9loadRangeEi_nid_postfix(Iterator* self,std::int32_t index) {
    auto& iterator=Require(self);
    iterator.nextElement=_ZNK6icu_6810UnicodeSet13getRangeStartEi_nid_postfix(iterator.set,index);
    iterator.endElement=_ZNK6icu_6810UnicodeSet11getRangeEndEi_nid_postfix(iterator.set,index);
}
std::int8_t APS5_VABI _ZN6icu_6818UnicodeSetIterator4nextEv_nid_postfix(Iterator* self) { return Next(Require(self),false); }
std::int8_t APS5_VABI _ZN6icu_6818UnicodeSetIterator9nextRangeEv_nid_postfix(Iterator* self) { return Next(Require(self),true); }
const UnicodeString* APS5_VABI _ZN6icu_6818UnicodeSetIterator9getStringEv_nid_postfix(Iterator* self) {
    auto& iterator=Require(self);
    if (!iterator.string && iterator.codepoint != -1) {
        if (!iterator.cpString) {
            iterator.cpString=static_cast<UnicodeString*>(Allocate_nid_no_patch(sizeof(UnicodeString)));
            if (iterator.cpString) iterator.cpString->Init();
        }
        if (iterator.cpString) {
            if (iterator.cpString->Flags() & UnicodeString::IsBogus) iterator.cpString->Empty();
            _ZN6icu_6813UnicodeString7replaceEiii_nid_postfix(iterator.cpString,0,iterator.cpString->Length(),iterator.codepoint);
        }
        iterator.string=iterator.cpString;
    }
    return &Require(iterator.string);
}
void* APS5_VABI _ZN6icu_6818UnicodeSetIterator16getStaticClassIDEv_nid_postfix() { return Id(); }
void* APS5_VABI _ZNK6icu_6818UnicodeSetIterator17getDynamicClassIDEv_nid_postfix(const Iterator*) { return Id(); }
}
