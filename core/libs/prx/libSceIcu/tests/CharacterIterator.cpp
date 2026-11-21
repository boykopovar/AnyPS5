#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

using namespace Icu::Abi;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::printf("ICU character iterator check failed at line %u\n", location.line());
    std::exit(1);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Check(false);
}

std::int32_t APS5_VABI Move(CharacterIterator* self, std::int32_t delta, std::int32_t origin) {
    Check(delta == 0 && origin == 0);
    return _ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(reinterpret_cast<UCharCharacterIterator*>(self), delta, origin);
}

char16_t APS5_VABI Next(ForwardCharacterIterator* self) {
    return _ZN6icu_6822UCharCharacterIterator11nextPostIncEv_nid_postfix(reinterpret_cast<UCharCharacterIterator*>(self));
}

std::int32_t APS5_VABI Next32(ForwardCharacterIterator* self) {
    return _ZN6icu_6822UCharCharacterIterator13next32PostIncEv_nid_postfix(reinterpret_cast<UCharCharacterIterator*>(self));
}

const CharacterIteratorFunctions Functions{.nextPostInc = Next, .next32PostInc = Next32, .move = Move};

void Units() {
    const char16_t text[]{u'a', 0xd83d, 0xde00, 0xd800, u'b', 0xdc00, 0xffff};
    UCharCharacterIterator iterator{{&Functions, 7, 0, 0, 7}, text};
    for (int index = 0; index < 7; ++index) {
        Check(_ZN6icu_6822UCharCharacterIterator7hasNextEv_nid_postfix(&iterator));
        Check(_ZNK6icu_6822UCharCharacterIterator7currentEv_nid_postfix(&iterator) == text[index]);
        Check(_ZN6icu_6822UCharCharacterIterator11nextPostIncEv_nid_postfix(&iterator) == text[index]);
    }
    Check(!_ZN6icu_6822UCharCharacterIterator7hasNextEv_nid_postfix(&iterator));
    Check(_ZN6icu_6822UCharCharacterIterator11nextPostIncEv_nid_postfix(&iterator) == 0xffff && iterator.base.position == 7);
    for (int index = 6; index >= 0; --index) {
        Check(_ZN6icu_6822UCharCharacterIterator11hasPreviousEv_nid_postfix(&iterator));
        Check(_ZN6icu_6822UCharCharacterIterator8previousEv_nid_postfix(&iterator) == text[index]);
    }
    Check(!_ZN6icu_6822UCharCharacterIterator11hasPreviousEv_nid_postfix(&iterator));
    Check(_ZN6icu_6822UCharCharacterIterator8previousEv_nid_postfix(&iterator) == 0xffff && iterator.base.position == 0);
    Check(_ZN6icu_6822UCharCharacterIterator5firstEv_nid_postfix(&iterator) == u'a' && iterator.base.position == 0);
    Check(_ZN6icu_6822UCharCharacterIterator4nextEv_nid_postfix(&iterator) == 0xd83d && iterator.base.position == 1);
    Check(_ZN6icu_6822UCharCharacterIterator4lastEv_nid_postfix(&iterator) == 0xffff && iterator.base.position == 6);
    Check(_ZN6icu_6822UCharCharacterIterator4nextEv_nid_postfix(&iterator) == 0xffff && iterator.base.position == 7);
    Check(_ZN6icu_6822UCharCharacterIterator8setIndexEi_nid_postfix(&iterator, -8) == u'a' && iterator.base.position == 0);
    Check(_ZN6icu_6822UCharCharacterIterator8setIndexEi_nid_postfix(&iterator, 99) == 0xffff && iterator.base.position == 7);
    Check(_ZN6icu_6822UCharCharacterIterator12firstPostIncEv_nid_postfix(&iterator) == u'a' && iterator.base.position == 1);
    Check(_ZN6icu_6817CharacterIterator12firstPostIncEv_nid_postfix(&iterator.base) == u'a' && iterator.base.position == 1);
    Check(_ZN6icu_6817CharacterIterator14first32PostIncEv_nid_postfix(&iterator.base) == u'a' && iterator.base.position == 1);
    Check(_ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, 2, 0) == 2);
    Check(_ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, -1, 1) == 1);
    Check(_ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, -2, 2) == 5);
    Check(_ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, 999, 0) == 7);
    Check(_ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, 0, 3) == 7);
    Invalid([&] { _ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, std::numeric_limits<std::int32_t>::max(), 2); });
}

void Points() {
    const char16_t text[]{u'a', 0xd83d, 0xde00, 0xd800, u'b', 0xdc00, 0xffff};
    UCharCharacterIterator iterator{{&Functions, 7, 0, 0, 7}, text};
    const std::array<std::int32_t, 6> points{u'a', 0x1f600, 0xd800, u'b', 0xdc00, 0xffff};
    const std::array<int, 6> ends{1, 3, 4, 5, 6, 7};
    for (int index = 0; index < 6; ++index) {
        Check(_ZN6icu_6822UCharCharacterIterator13next32PostIncEv_nid_postfix(&iterator) == points[index]);
        Check(iterator.base.position == ends[index]);
    }
    Check(_ZN6icu_6822UCharCharacterIterator13next32PostIncEv_nid_postfix(&iterator) == 0xffff);
    for (int index = 5; index >= 0; --index) Check(_ZN6icu_6822UCharCharacterIterator10previous32Ev_nid_postfix(&iterator) == points[index]);
    Check(_ZN6icu_6822UCharCharacterIterator7first32Ev_nid_postfix(&iterator) == u'a' && iterator.base.position == 0);
    Check(_ZN6icu_6822UCharCharacterIterator6next32Ev_nid_postfix(&iterator) == 0x1f600 && iterator.base.position == 1);
    Check(_ZN6icu_6822UCharCharacterIterator6next32Ev_nid_postfix(&iterator) == 0xd800 && iterator.base.position == 3);
    Check(_ZN6icu_6822UCharCharacterIterator8setIndexEi_nid_postfix(&iterator, 2) == 0xde00);
    Check(_ZNK6icu_6822UCharCharacterIterator9current32Ev_nid_postfix(&iterator) == 0x1f600 && iterator.base.position == 2);
    Check(_ZN6icu_6822UCharCharacterIterator10setIndex32Ei_nid_postfix(&iterator, 2) == 0x1f600 && iterator.base.position == 1);
    Check(_ZN6icu_6822UCharCharacterIterator6last32Ev_nid_postfix(&iterator) == 0xffff && iterator.base.position == 6);
    Check(_ZN6icu_6822UCharCharacterIterator6next32Ev_nid_postfix(&iterator) == 0xffff && iterator.base.position == 7);
    Check(_ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, 2, 0) == 3);
    Check(_ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, -1, 1) == 1);
    Check(_ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, -5, 2) == 1);
    Check(_ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, 99, 1) == 7);
    Check(_ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, 1, 2) == 7);
    Check(_ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, -1, 0) == 0);
    Invalid([&] { _ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(&iterator, std::numeric_limits<std::int32_t>::min(), 1); });
    iterator.base.begin = 1;
    iterator.base.end = 3;
    Check(_ZN6icu_6822UCharCharacterIterator14first32PostIncEv_nid_postfix(&iterator) == 0x1f600 && iterator.base.position == 3);
    iterator.base.begin = 2;
    Check(_ZN6icu_6822UCharCharacterIterator7first32Ev_nid_postfix(&iterator) == 0xde00);
    iterator.base.begin = 1;
    iterator.base.end = 2;
    Check(_ZN6icu_6822UCharCharacterIterator7first32Ev_nid_postfix(&iterator) == 0xd83d);
}

void Assignment() {
    UCharCharacterIterator iterator{};
    ConstChar16Ptr text{u"abc"};
    _ZN6icu_6822UCharCharacterIterator7setTextENS_14ConstChar16PtrEi_nid_postfix(&iterator, &text, 3);
    Check(iterator.text == text.pointer && iterator.base.textLength == 3 && iterator.base.begin == 0 && iterator.base.end == 3);
    Check(_ZNK6icu_6822UCharCharacterIterator8hashCodeEv_nid_postfix(&iterator) == (((97 * 37 + 98) * 37 + 99) ^ 3));
    UCharCharacterIterator other{};
    other.base.vtable = &Functions;
    Check(_ZN6icu_6822UCharCharacterIteratoraSERKS0__nid_postfix(&other, &iterator) == &other);
    Check(other.text == text.pointer && other.base.vtable == &Functions && other.base.end == 3);
    iterator.base.position = 2;
    Check(_ZN6icu_6817CharacterIteratoraSERKS0__nid_postfix(&other.base, &iterator.base) == &other.base);
    Check(other.base.position == 2 && other.base.vtable == &Functions);
    _ZN6icu_6822UCharCharacterIterator7setTextENS_14ConstChar16PtrEi_nid_postfix(&iterator, &text, -1);
    Check(iterator.base.end == 0 && iterator.text == text.pointer);
    Check(_ZN6icu_6822UCharCharacterIterator5firstEv_nid_postfix(&iterator) == 0xffff);
    ConstChar16Ptr empty{nullptr};
    _ZN6icu_6822UCharCharacterIterator7setTextENS_14ConstChar16PtrEi_nid_postfix(&iterator, &empty, 99);
    Check(iterator.base.end == 0 && iterator.text == nullptr && _ZNK6icu_6822UCharCharacterIterator8hashCodeEv_nid_postfix(&iterator) == 0);
    Check(_ZNK6icu_6822UCharCharacterIterator17getDynamicClassIDEv_nid_postfix(&iterator) == _ZN6icu_6822UCharCharacterIterator16getStaticClassIDEv_nid_postfix());
    Invalid([&] { _ZN6icu_6817CharacterIterator12firstPostIncEv_nid_postfix(&iterator.base); });
    Invalid([] { _ZN6icu_6822UCharCharacterIterator7hasNextEv_nid_postfix(nullptr); });
    iterator.base.end = 1;
    Invalid([&] { _ZN6icu_6822UCharCharacterIterator5firstEv_nid_postfix(&iterator); });
}

}

int main() {
    Units();
    Points();
    Assignment();
    std::puts("ICU character iterator tests passed");
}
