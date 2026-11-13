// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>

namespace {
void Check(bool value) { if (!value) std::abort(); }
}
int main() {
    using namespace Icu::Abi;
    UnicodeString text{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&text, u"abc\U0001f600z", 6);
    StringCharacterIterator iterator{};
    _ZN6icu_6823StringCharacterIteratorC1ERKNS_13UnicodeStringEiii_nid_postfix(&iterator, &text, 1, 5, 2);
    Check(iterator.base.base.textLength == 6 && iterator.base.base.begin == 1 && iterator.base.base.end == 5 && iterator.base.base.position == 2);
    Check(iterator.base.text == iterator.text.storage.stack.buffer && iterator.base.text != text.storage.stack.buffer);
    auto* copy = iterator.base.base.vtable->clone(reinterpret_cast<CharacterIterator*>(&iterator));
    Check(copy != nullptr);
    Check(iterator.base.base.vtable->equals(reinterpret_cast<ForwardCharacterIterator*>(&iterator), reinterpret_cast<ForwardCharacterIterator*>(copy)) == 1);
    _ZN6icu_6813UnicodeString5setToEDs_nid_postfix(&text, u'x');
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&iterator.text) == 6 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&iterator.text, 0) == u'a');
    _ZN6icu_6823StringCharacterIterator7setTextERKNS_13UnicodeStringE_nid_postfix(&iterator, &text);
    Check(iterator.base.base.position == 0 && iterator.base.base.begin == 0 && iterator.base.base.end == 1 && iterator.base.text[0] == u'x');
    UnicodeString extracted{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&extracted, nullptr, 0);
    iterator.base.base.vtable->getText(reinterpret_cast<CharacterIterator*>(&iterator), &extracted);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&extracted) == 1 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&extracted, 0) == u'x');
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&extracted);
    Check(iterator.base.base.vtable->getDynamicClassId(reinterpret_cast<ForwardCharacterIterator*>(&iterator)) == _ZN6icu_6823StringCharacterIterator16getStaticClassIDEv_nid_postfix());
    copy->vtable->deleteObject(reinterpret_cast<ForwardCharacterIterator*>(copy));
    _ZN6icu_6823StringCharacterIteratorD1Ev_nid_postfix(&iterator);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&text);
    return 0;
}
