#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

using NKWebKit::WtfAbi::AtomString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "TrackPrivateBase check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

namespace {

using AtomGetter = AtomString (APS5_VABI *)(const void*);

StringImpl* EmptyImpl() {
    String empty = _ZN3WTF10StringImpl6createEPKhj(nullptr, 0);
    return empty.impl;
}

void TestEmptyAtom(AtomGetter getter) {
    StringImpl* empty = EmptyImpl();
    Require(empty != nullptr);
    Require(empty->length == 0);
    Require((empty->refCount & 1) == 1);
    Require((empty->hashAndFlags & 16) != 0);
    Require((empty->hashAndFlags & 4) != 0);
    const unsigned before = empty->refCount;

    std::array<std::uint8_t, 64> object;
    object.fill(0x3c);
    const auto objectBefore = object;

    alignas(AtomString) unsigned char raw[sizeof(AtomString)];
    std::memset(raw, 0xa5, sizeof(raw));
    auto* first = ::new (raw) AtomString(getter(object.data()));
    Require(first->string.impl == empty);
    Require(empty->refCount == before + 2);
    Require(object == objectBefore);

    alignas(AtomString) unsigned char rawNull[sizeof(AtomString)];
    std::memset(rawNull, 0x5a, sizeof(rawNull));
    auto* second = ::new (rawNull) AtomString(getter(nullptr));
    Require(second->string.impl == empty);
    Require(empty->refCount == before + 4);

    first->~AtomString();
    second->~AtomString();
    Require(empty->refCount == before);
}

void TestStartTimeVariance() {
    const WTF::MediaTime& zero = _ZN3WTF9MediaTime8zeroTimeEv();
    std::array<std::uint8_t, 64> object;
    object.fill(0x3c);
    const auto objectBefore = object;

    alignas(WTF::MediaTime) unsigned char raw[sizeof(WTF::MediaTime)];
    std::memset(raw, 0xa5, sizeof(raw));
    auto* time = ::new (raw) WTF::MediaTime(_ZNK7WebCore16TrackPrivateBase17startTimeVarianceEv(object.data()));
    Require(time != &zero);
    std::int64_t value;
    std::uint32_t scale;
    std::memcpy(&value, raw, sizeof(value));
    std::memcpy(&scale, raw + 8, sizeof(scale));
    Require(value == 0);
    Require(scale == 1);
    Require(raw[12] == WTF::MediaTime::Valid);
    Require(time->m_timeValue == zero.m_timeValue);
    Require(time->m_timeScale == zero.m_timeScale);
    Require(time->m_timeFlags == zero.m_timeFlags);
    Require(object == objectBefore);

    WTF::MediaTime fromNull = _ZNK7WebCore16TrackPrivateBase17startTimeVarianceEv(nullptr);
    Require(fromNull.m_timeValue == 0 && fromNull.m_timeScale == 1 && fromNull.m_timeFlags == WTF::MediaTime::Valid);
}

}

int main() {
    TestEmptyAtom(_ZNK7WebCore16TrackPrivateBase2idEv);
    TestEmptyAtom(_ZNK7WebCore16TrackPrivateBase5labelEv);
    TestEmptyAtom(_ZNK7WebCore16TrackPrivateBase8languageEv);
    TestStartTimeVariance();
    return 0;
}
