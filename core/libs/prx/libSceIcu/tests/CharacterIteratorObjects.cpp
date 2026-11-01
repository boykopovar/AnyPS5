// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>
#include <stdexcept>

namespace {
void Check(bool value) { if (!value) std::abort(); }
}
int main() {
    using namespace Icu::Abi;
    ConstChar16Ptr text{u"abc\U0001f600z"};
    UCharCharacterIterator iterator{};
    _ZN6icu_6822UCharCharacterIteratorC1ENS_14ConstChar16PtrEiiii_nid_postfix(&iterator, &text, -1, -2, 999, 999);
    Check(iterator.base.textLength == 6 && iterator.base.begin == 0 && iterator.base.end == 6 && iterator.base.position == 6);
    auto* base = reinterpret_cast<CharacterIterator*>(&iterator);
    Check(iterator.base.vtable->first(base) == u'a');
    Check(iterator.base.vtable->setIndex32(base, 4) == 0x1f600 && iterator.base.position == 3);
    auto* copy = _ZNK6icu_6822UCharCharacterIterator5cloneEv_nid_postfix(&iterator);
    Check(copy && copy != &iterator && copy->text == iterator.text);
    Check(iterator.base.vtable->equals(reinterpret_cast<ForwardCharacterIterator*>(&iterator), reinterpret_cast<ForwardCharacterIterator*>(copy)) == 1);
    copy->base.position = 0;
    Check(iterator.base.vtable->equals(reinterpret_cast<ForwardCharacterIterator*>(&iterator), reinterpret_cast<ForwardCharacterIterator*>(copy)) == 0);
    UnicodeString extracted{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&extracted, u"before", 6);
    iterator.base.vtable->getText(base, &extracted);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&extracted) == 6 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&extracted, 4) == 0xde00);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&extracted);
    copy->base.vtable->deleteObject(reinterpret_cast<ForwardCharacterIterator*>(copy));
    _ZN6icu_6822UCharCharacterIteratorD1Ev_nid_postfix(&iterator);
    CharacterIterator character{};
    _ZN6icu_6817CharacterIteratorC2Eiiii_nid_postfix(&character, -5, 10, -10, 20);
    Check(character.vtable->destroy != nullptr && character.vtable->deleteObject != nullptr);
    Check(character.textLength == 0 && character.position == 0 && character.begin == 0 && character.end == 0);
    _ZN6icu_6817CharacterIteratorD1Ev_nid_postfix(&character);
    ConstChar16Ptr empty{nullptr};
    _ZN6icu_6822UCharCharacterIteratorC2ENS_14ConstChar16PtrEii_nid_postfix(&iterator, &empty, 100, 50);
    Check(iterator.base.textLength == 0 && iterator.base.position == 0 && iterator.text == nullptr);
    _ZN6icu_6822UCharCharacterIteratorD2Ev_nid_postfix(&iterator);
    try { _ZN6icu_6817CharacterIteratorC2Ev_nid_postfix(nullptr); } catch (const std::invalid_argument&) { return 0; }
    return 1;
}
