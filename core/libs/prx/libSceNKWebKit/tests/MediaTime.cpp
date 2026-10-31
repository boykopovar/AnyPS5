#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "MediaTime check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(cond) Check((cond), __LINE__)

void TestMediaTimeBasics() {
    WTF::MediaTime def;
    _ZN3WTF9MediaTimeC1Ev(&def);
    Require(_ZNK3WTF9MediaTime7isValidEv(&def));
    Require(!_ZNK3WTF9MediaTime9isInvalidEv(&def));
    Require(!_ZNK3WTF9MediaTime14hasDoubleValueEv(&def));
    Require(!_ZNK3WTF9MediaTime12isIndefiniteEv(&def));
    Require(!_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&def));
    Require(!_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&def));
    Require(_ZNK3WTF9MediaTime9timeValueEv(&def) == 0);
    Require(_ZNK3WTF9MediaTime9timeScaleEv(&def) == WTF::MediaTime::DefaultTimeScale);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&def) == 0.0);
    Require(_ZNK3WTF9MediaTime7toFloatEv(&def) == 0.0f);
    Require(_ZNK3WTF9MediaTimentEv(&def));
    Require(!_ZNK3WTF9MediaTimecvbEv(&def));

    WTF::MediaTime def2;
    _ZN3WTF9MediaTimeC2Ev(&def2);
    Require(_ZNK3WTF9MediaTimeeqERKS0_(&def, def2));

    WTF::MediaTime copy1;
    _ZN3WTF9MediaTimeC1ERKS0_(&copy1, def);
    Require(_ZNK3WTF9MediaTimeeqERKS0_(&copy1, def));

    WTF::MediaTime copy2;
    _ZN3WTF9MediaTimeC2ERKS0_(&copy2, def);
    Require(_ZNK3WTF9MediaTimeeqERKS0_(&copy2, def));

    WTF::MediaTime assigned;
    _ZN3WTF9MediaTimeC1Ev(&assigned);
    _ZN3WTF9MediaTimeaSERKS0_(&assigned, def);
    Require(_ZNK3WTF9MediaTimeeqERKS0_(&assigned, def));

    const auto& zero = _ZN3WTF9MediaTime8zeroTimeEv();
    Require(_ZNK3WTF9MediaTime7isValidEv(&zero));
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&zero) == 0.0);
    Require(_ZNK3WTF9MediaTimeeqERKS0_(&def, zero));

    const auto& invalid = _ZN3WTF9MediaTime11invalidTimeEv();
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&invalid));
    Require(!_ZNK3WTF9MediaTime7isValidEv(&invalid));
    Require(std::isnan(_ZNK3WTF9MediaTime8toDoubleEv(&invalid)));
    Require(std::isnan(_ZNK3WTF9MediaTime7toFloatEv(&invalid)));
    Require(_ZNK3WTF9MediaTimentEv(&invalid));
    Require(!_ZNK3WTF9MediaTimecvbEv(&invalid));

    const auto& posInf = _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    Require(_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&posInf));
    Require(_ZNK3WTF9MediaTime7isValidEv(&posInf));
    Require(!_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&posInf));
    Require(std::isinf(_ZNK3WTF9MediaTime8toDoubleEv(&posInf)));
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&posInf) > 0);
    Require(std::isinf(_ZNK3WTF9MediaTime7toFloatEv(&posInf)));
    Require(_ZNK3WTF9MediaTime7toFloatEv(&posInf) > 0);

    const auto& negInf = _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    Require(_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&negInf));
    Require(_ZNK3WTF9MediaTime7isValidEv(&negInf));
    Require(!_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&negInf));
    Require(std::isinf(_ZNK3WTF9MediaTime8toDoubleEv(&negInf)));
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&negInf) < 0);
    Require(std::isinf(_ZNK3WTF9MediaTime7toFloatEv(&negInf)));
    Require(_ZNK3WTF9MediaTime7toFloatEv(&negInf) < 0);

    const auto& indef = _ZN3WTF9MediaTime14indefiniteTimeEv();
    Require(_ZNK3WTF9MediaTime12isIndefiniteEv(&indef));
    Require(!_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&indef));
    Require(!_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&indef));
    Require(std::isnan(_ZNK3WTF9MediaTime8toDoubleEv(&indef)));
    Require(std::isnan(_ZNK3WTF9MediaTime7toFloatEv(&indef)));
}

void TestMediaTimeCreationAndConversions() {
    WTF::MediaTime t1 = _ZN3WTF9MediaTime16createWithDoubleEd(2.5);
    Require(_ZNK3WTF9MediaTime7isValidEv(&t1));
    Require(_ZNK3WTF9MediaTime14hasDoubleValueEv(&t1));
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&t1) == 2.5);
    Require(_ZNK3WTF9MediaTime7toFloatEv(&t1) == 2.5f);
    Require(_ZNK3WTF9MediaTimecvbEv(&t1));
    Require(!_ZNK3WTF9MediaTimentEv(&t1));

    WTF::MediaTime tNan = _ZN3WTF9MediaTime16createWithDoubleEd(std::numeric_limits<double>::quiet_NaN());
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&tNan));

    WTF::MediaTime tPosInf = _ZN3WTF9MediaTime16createWithDoubleEd(std::numeric_limits<double>::infinity());
    Require(_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&tPosInf));

    WTF::MediaTime tNegInf = _ZN3WTF9MediaTime16createWithDoubleEd(-std::numeric_limits<double>::infinity());
    Require(_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&tNegInf));

    WTF::MediaTime t2 = _ZN3WTF9MediaTime16createWithDoubleEdj(2.5, 1000);
    Require(_ZNK3WTF9MediaTime7isValidEv(&t2));
    Require(!_ZNK3WTF9MediaTime14hasDoubleValueEv(&t2));
    Require(_ZNK3WTF9MediaTime9timeValueEv(&t2) == 2500);
    Require(_ZNK3WTF9MediaTime9timeScaleEv(&t2) == 1000);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&t2) == 2.5);
    Require(_ZNK3WTF9MediaTimeeqERKS0_(&t1, t2));

    WTF::MediaTime t2Nan = _ZN3WTF9MediaTime16createWithDoubleEdj(std::numeric_limits<double>::quiet_NaN(), 1000);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&t2Nan));

    WTF::MediaTime t2ZeroScale = _ZN3WTF9MediaTime16createWithDoubleEdj(2.5, 0);
    Require(_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&t2ZeroScale));

    WTF::MediaTime t2ZeroScaleNeg = _ZN3WTF9MediaTime16createWithDoubleEdj(-2.5, 0);
    Require(_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&t2ZeroScaleNeg));

    WTF::MediaTime f1 = _ZN3WTF9MediaTime15createWithFloatEf(1.25f);
    Require(_ZNK3WTF9MediaTime14hasDoubleValueEv(&f1));
    Require(_ZNK3WTF9MediaTime7toFloatEv(&f1) == 1.25f);

    WTF::MediaTime fNan = _ZN3WTF9MediaTime15createWithFloatEf(std::numeric_limits<float>::quiet_NaN());
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&fNan));

    WTF::MediaTime fPosInf = _ZN3WTF9MediaTime15createWithFloatEf(std::numeric_limits<float>::infinity());
    Require(_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&fPosInf));

    WTF::MediaTime fNegInf = _ZN3WTF9MediaTime15createWithFloatEf(-std::numeric_limits<float>::infinity());
    Require(_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&fNegInf));

    WTF::MediaTime f2 = _ZN3WTF9MediaTime15createWithFloatEfj(1.25f, 100);
    Require(!_ZNK3WTF9MediaTime14hasDoubleValueEv(&f2));
    Require(_ZNK3WTF9MediaTime9timeValueEv(&f2) == 125);
    Require(_ZNK3WTF9MediaTime9timeScaleEv(&f2) == 100);

    WTF::MediaTime f2Nan = _ZN3WTF9MediaTime15createWithFloatEfj(std::numeric_limits<float>::quiet_NaN(), 100);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&f2Nan));

    WTF::MediaTime f2ZeroScale = _ZN3WTF9MediaTime15createWithFloatEfj(1.25f, 0);
    Require(_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&f2ZeroScale));

    WTF::MediaTime f2ZeroScaleNeg = _ZN3WTF9MediaTime15createWithFloatEfj(-1.25f, 0);
    Require(_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&f2ZeroScaleNeg));

    WTF::MediaTime custom;
    _ZN3WTF9MediaTimeC1Eljh(&custom, 300, 10, WTF::MediaTime::Valid);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&custom) == 30.0);
    Require(_ZNK3WTF9MediaTime9timeFlagsEv(&custom) == WTF::MediaTime::Valid);

    WTF::MediaTime custom2;
    _ZN3WTF9MediaTimeC2Eljh(&custom2, 300, 10, WTF::MediaTime::Valid);
    Require(_ZNK3WTF9MediaTimeeqERKS0_(&custom, custom2));

    WTF::MediaTime zeroScalePos;
    _ZN3WTF9MediaTimeC1Eljh(&zeroScalePos, 5, 0, WTF::MediaTime::Valid);
    Require(_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&zeroScalePos));

    WTF::MediaTime zeroScaleNeg;
    _ZN3WTF9MediaTimeC1Eljh(&zeroScaleNeg, -5, 0, WTF::MediaTime::Valid);
    Require(_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&zeroScaleNeg));

    WTF::MediaTime zeroScaleInvalid;
    _ZN3WTF9MediaTimeC1Eljh(&zeroScaleInvalid, -5, 0, 0);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&zeroScaleInvalid));
}

void TestMediaTimeArithmeticAndComparison() {
    WTF::MediaTime a = _ZN3WTF9MediaTime16createWithDoubleEdj(1.0, 1000);
    WTF::MediaTime b = _ZN3WTF9MediaTime16createWithDoubleEdj(2.0, 1000);

    Require(_ZNK3WTF9MediaTimeltERKS0_(&a, b));
    Require(_ZNK3WTF9MediaTimeleERKS0_(&a, b));
    Require(!_ZNK3WTF9MediaTimegtERKS0_(&a, b));
    Require(!_ZNK3WTF9MediaTimegeERKS0_(&a, b));
    Require(!_ZNK3WTF9MediaTimeeqERKS0_(&a, b));
    Require(_ZNK3WTF9MediaTime7compareERKS0_(&a, b) == WTF::MediaTime::LessThan);

    Require(_ZNK3WTF9MediaTimegtERKS0_(&b, a));
    Require(_ZNK3WTF9MediaTimegeERKS0_(&b, a));
    Require(!_ZNK3WTF9MediaTimeltERKS0_(&b, a));
    Require(!_ZNK3WTF9MediaTimeleERKS0_(&b, a));
    Require(_ZNK3WTF9MediaTime7compareERKS0_(&b, a) == WTF::MediaTime::GreaterThan);

    WTF::MediaTime mid = _ZN3WTF9MediaTime16createWithDoubleEdj(1.5, 1000);
    Require(_ZNK3WTF9MediaTime9isBetweenERKS0_S2_(&mid, a, b));
    Require(_ZNK3WTF9MediaTime9isBetweenERKS0_S2_(&mid, b, a));

    WTF::MediaTime outside = _ZN3WTF9MediaTime16createWithDoubleEdj(5.0, 1000);
    Require(!_ZNK3WTF9MediaTime9isBetweenERKS0_S2_(&outside, a, b));

    WTF::MediaTime sum = _ZNK3WTF9MediaTimeplERKS0_(&a, b);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&sum) == 3.0);

    WTF::MediaTime sumInPlace = a;
    _ZN3WTF9MediaTimepLERKS0_(&sumInPlace, b);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&sumInPlace) == 3.0);

    const auto& posInf = _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    const auto& negInf = _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    const auto& invalid = _ZN3WTF9MediaTime11invalidTimeEv();

    WTF::MediaTime infSum = _ZNK3WTF9MediaTimeplERKS0_(&posInf, negInf);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&infSum));

    WTF::MediaTime invSum = _ZNK3WTF9MediaTimeplERKS0_(&a, invalid);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&invSum));

    WTF::MediaTime diff = _ZNK3WTF9MediaTimemiERKS0_(&b, a);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&diff) == 1.0);

    WTF::MediaTime diffInPlace = b;
    _ZN3WTF9MediaTimemIERKS0_(&diffInPlace, a);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&diffInPlace) == 1.0);

    WTF::MediaTime infDiff = _ZNK3WTF9MediaTimemiERKS0_(&posInf, posInf);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&infDiff));

    WTF::MediaTime invDiff = _ZNK3WTF9MediaTimemiERKS0_(&a, invalid);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&invDiff));

    WTF::MediaTime neg = _ZNK3WTF9MediaTimengEv(&a);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&neg) == -1.0);

    WTF::MediaTime negInv = _ZNK3WTF9MediaTimengEv(&invalid);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&negInv));

    WTF::MediaTime negPosInf = _ZNK3WTF9MediaTimengEv(&posInf);
    Require(_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&negPosInf));

    WTF::MediaTime mul = _ZNK3WTF9MediaTimemlEi(&a, 5);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&mul) == 5.0);

    WTF::MediaTime mulZero = _ZNK3WTF9MediaTimemlEi(&a, 0);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&mulZero) == 0.0);

    WTF::MediaTime mulInv = _ZNK3WTF9MediaTimemlEi(&invalid, 5);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&mulInv));

    WTF::MediaTime absVal = _ZN3WTF3absERKNS_9MediaTimeE(neg);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&absVal) == 1.0);

    WTF::MediaTime absInv = _ZN3WTF3absERKNS_9MediaTimeE(invalid);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&absInv));

    WTF::MediaTime absNegInf = _ZN3WTF3absERKNS_9MediaTimeE(negInf);
    Require(_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&absNegInf));

    WTF::MediaTime scaled = _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(&a, 100, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    Require(_ZNK3WTF9MediaTime9timeScaleEv(&scaled) == 100);
    Require(_ZNK3WTF9MediaTime9timeValueEv(&scaled) == 100);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&scaled) == 1.0);

    WTF::MediaTime frac;
    _ZN3WTF9MediaTimeC1Eljh(&frac, 1, 3, WTF::MediaTime::Valid);

    WTF::MediaTime roundHalf = _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(&frac, 10, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    Require(_ZNK3WTF9MediaTime14hasBeenRoundedEv(&roundHalf));
    Require(_ZNK3WTF9MediaTime9timeValueEv(&roundHalf) == 3);

    WTF::MediaTime roundTowardZero = _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(&frac, 10, WTF::MediaTime::RoundingFlags::TowardZero);
    Require(_ZNK3WTF9MediaTime9timeValueEv(&roundTowardZero) == 3);

    WTF::MediaTime roundAwayFromZero = _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(&frac, 10, WTF::MediaTime::RoundingFlags::AwayFromZero);
    Require(_ZNK3WTF9MediaTime9timeValueEv(&roundAwayFromZero) == 4);

    WTF::MediaTime roundTowardPosInf = _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(&frac, 10, WTF::MediaTime::RoundingFlags::TowardPositiveInfinity);
    Require(_ZNK3WTF9MediaTime9timeValueEv(&roundTowardPosInf) == 4);

    WTF::MediaTime roundTowardNegInf = _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(&frac, 10, WTF::MediaTime::RoundingFlags::TowardNegativeInfinity);
    Require(_ZNK3WTF9MediaTime9timeValueEv(&roundTowardNegInf) == 3);

    WTF::MediaTime negFrac;
    _ZN3WTF9MediaTimeC1Eljh(&negFrac, -1, 3, WTF::MediaTime::Valid);
    WTF::MediaTime negRoundAway = _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(&negFrac, 10, WTF::MediaTime::RoundingFlags::AwayFromZero);
    Require(_ZNK3WTF9MediaTime9timeValueEv(&negRoundAway) == -4);

    WTF::MediaTime zeroScaleTest = a;
    _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&zeroScaleTest, 0, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    Require(_ZNK3WTF9MediaTime18isPositiveInfiniteEv(&zeroScaleTest));

    WTF::MediaTime zeroScaleNegTest = neg;
    _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&zeroScaleNegTest, 0, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    Require(_ZNK3WTF9MediaTime18isNegativeInfiniteEv(&zeroScaleNegTest));
}

void TestMediaTimeMemory() {
    void* mem = std::malloc(sizeof(WTF::MediaTime));
    Require(mem != nullptr);
    WTF::NotNullTag tag{};
    void* placedTag = _ZN3WTF9MediaTimenwEm10NotNullTagPv(sizeof(WTF::MediaTime), tag, mem);
    Require(placedTag == mem);
    void* placed = _ZN3WTF9MediaTimenwEmPv(sizeof(WTF::MediaTime), mem);
    Require(placed == mem);
    auto* t = static_cast<WTF::MediaTime*>(placed);
    _ZN3WTF9MediaTimeC1Eljh(t, 42, 1, WTF::MediaTime::Valid);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(t) == 42.0);
    std::free(mem);

    void* arrMem = std::malloc(sizeof(WTF::MediaTime) * 2);
    Require(arrMem != nullptr);
    void* arrPlaced = _ZN3WTF9MediaTimenaEmPv(sizeof(WTF::MediaTime) * 2, arrMem);
    Require(arrPlaced == arrMem);
    std::free(arrMem);
}

void TestPlatformTimeRanges() {
    WebCore::PlatformTimeRanges emptyRanges;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&emptyRanges);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&emptyRanges) == 0);
    WTF::MediaTime emptyMax = _ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv(&emptyRanges);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&emptyMax));
    WTF::MediaTime emptyZero = _ZN3WTF9MediaTime8zeroTimeEv();
    WTF::MediaTime emptyNear = _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(&emptyRanges, emptyZero);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&emptyNear));
    bool emptyValid = true;
    WTF::MediaTime emptyStart = _ZNK7WebCore18PlatformTimeRanges5startEjRb(&emptyRanges, 0, emptyValid);
    Require(!emptyValid && _ZNK3WTF9MediaTime8toDoubleEv(&emptyStart) == 0.0);
    WTF::MediaTime emptyEnd = _ZNK7WebCore18PlatformTimeRanges3endEjRb(&emptyRanges, 0, emptyValid);
    Require(!emptyValid && _ZNK3WTF9MediaTime8toDoubleEv(&emptyEnd) == 0.0);
    WTF::MediaTime emptyDur = _ZNK7WebCore18PlatformTimeRanges8durationEj(&emptyRanges, 0);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&emptyDur));
    WTF::MediaTime emptyTot = _ZNK7WebCore18PlatformTimeRanges13totalDurationEv(&emptyRanges);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&emptyTot) == 0.0);

    WebCore::PlatformTimeRanges invalidAddRanges;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&invalidAddRanges);
    WTF::MediaTime fiveTime = _ZN3WTF9MediaTime16createWithDoubleEd(5.0);
    WTF::MediaTime zeroTime = _ZN3WTF9MediaTime8zeroTimeEv();
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&invalidAddRanges, fiveTime, zeroTime);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&invalidAddRanges) == 0);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&invalidAddRanges);

    WebCore::PlatformTimeRanges ranges;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&ranges);

    WebCore::PlatformTimeRanges ranges2;
    _ZN7WebCore18PlatformTimeRangesC2Ev(&ranges2);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges2) == 0);
    _ZN7WebCore18PlatformTimeRangesD2Ev(&ranges2);

    WTF::MediaTime zero = _ZN3WTF9MediaTime8zeroTimeEv();
    WTF::MediaTime five = _ZN3WTF9MediaTime16createWithDoubleEd(5.0);
    WTF::MediaTime ten = _ZN3WTF9MediaTime16createWithDoubleEd(10.0);
    WTF::MediaTime fifteen = _ZN3WTF9MediaTime16createWithDoubleEd(15.0);

    WebCore::PlatformTimeRanges ranges3;
    _ZN7WebCore18PlatformTimeRangesC2ERKN3WTF9MediaTimeES4_(&ranges3, zero, five);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges3) == 1);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&ranges3);

    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&ranges, zero, five);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges) == 1);
    bool valid = false;
    WTF::MediaTime s0 = _ZNK7WebCore18PlatformTimeRanges5startEjRb(&ranges, 0, valid);
    Require(valid && _ZNK3WTF9MediaTime8toDoubleEv(&s0) == 0.0);
    WTF::MediaTime s0NoVal = _ZNK7WebCore18PlatformTimeRanges5startEj(&ranges, 0);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&s0NoVal) == 0.0);

    WTF::MediaTime e0 = _ZNK7WebCore18PlatformTimeRanges3endEjRb(&ranges, 0, valid);
    Require(valid && _ZNK3WTF9MediaTime8toDoubleEv(&e0) == 5.0);
    WTF::MediaTime e0NoVal = _ZNK7WebCore18PlatformTimeRanges3endEj(&ranges, 0);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&e0NoVal) == 5.0);

    bool outValid = true;
    WTF::MediaTime sOut = _ZNK7WebCore18PlatformTimeRanges5startEjRb(&ranges, 99, outValid);
    Require(!outValid && _ZNK3WTF9MediaTime8toDoubleEv(&sOut) == 0.0);
    WTF::MediaTime eOut = _ZNK7WebCore18PlatformTimeRanges3endEjRb(&ranges, 99, outValid);
    Require(!outValid && _ZNK3WTF9MediaTime8toDoubleEv(&eOut) == 0.0);
    WTF::MediaTime durOut = _ZNK7WebCore18PlatformTimeRanges8durationEj(&ranges, 99);
    Require(_ZNK3WTF9MediaTime9isInvalidEv(&durOut));

    WTF::MediaTime dur0 = _ZNK7WebCore18PlatformTimeRanges8durationEj(&ranges, 0);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&dur0) == 5.0);
    WTF::MediaTime totDur = _ZNK7WebCore18PlatformTimeRanges13totalDurationEv(&ranges);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&totDur) == 5.0);
    WTF::MediaTime maxBuf = _ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv(&ranges);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&maxBuf) == 5.0);

    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&ranges, ten, fifteen);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges) == 2);
    WTF::MediaTime totDur2 = _ZNK7WebCore18PlatformTimeRanges13totalDurationEv(&ranges);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&totDur2) == 10.0);
    WTF::MediaTime maxBuf2 = _ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv(&ranges);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&maxBuf2) == 15.0);

    WTF::MediaTime t3 = _ZN3WTF9MediaTime16createWithDoubleEd(3.0);
    WTF::MediaTime t8 = _ZN3WTF9MediaTime16createWithDoubleEd(8.0);
    Require(_ZNK7WebCore18PlatformTimeRanges7containERKN3WTF9MediaTimeE(&ranges, t3));
    Require(!_ZNK7WebCore18PlatformTimeRanges7containERKN3WTF9MediaTimeE(&ranges, t8));
    Require(_ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(&ranges, t3) == 0);
    Require(_ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(&ranges, t8) == static_cast<std::size_t>(-1));

    WTF::MediaTime nearT = _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(&ranges, t8);
    double nearTd = _ZNK3WTF9MediaTime8toDoubleEv(&nearT);
    Require(nearTd == 5.0 || nearTd == 10.0);

    WebCore::PlatformTimeRanges copyRanges1;
    _ZN7WebCore18PlatformTimeRangesC1ERKS0_(&copyRanges1, ranges);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&copyRanges1) == 2);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&copyRanges1);

    WebCore::PlatformTimeRanges copyRanges2;
    _ZN7WebCore18PlatformTimeRangesC2ERKS0_(&copyRanges2, ranges);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&copyRanges2) == 2);

    WebCore::PlatformTimeRanges assignedRanges;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&assignedRanges);
    _ZN7WebCore18PlatformTimeRangesaSERKS0_(&assignedRanges, copyRanges2);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&assignedRanges) == 2);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&assignedRanges);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&copyRanges2);

    WebCore::PlatformTimeRanges inverted;
    _ZN7WebCore18PlatformTimeRangesC1ERKS0_(&inverted, ranges);
    _ZN7WebCore18PlatformTimeRanges6invertEv(&inverted);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&inverted) == 3);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&inverted);

    WebCore::PlatformTimeRanges unioned;
    _ZN7WebCore18PlatformTimeRangesC1ERKS0_(&unioned, ranges);
    WebCore::PlatformTimeRanges otherRanges;
    WTF::MediaTime twenty = _ZN3WTF9MediaTime16createWithDoubleEd(20.0);
    WTF::MediaTime twentyFive = _ZN3WTF9MediaTime16createWithDoubleEd(25.0);
    _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(&otherRanges, twenty, twentyFive);
    _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(&unioned, otherRanges);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&unioned) == 3);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&unioned);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&otherRanges);

    WTF::MediaTime bridgeStart = _ZN3WTF9MediaTime16createWithDoubleEd(4.0);
    WTF::MediaTime bridgeEnd = _ZN3WTF9MediaTime16createWithDoubleEd(11.0);
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&ranges, bridgeStart, bridgeEnd);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges) == 1);
    WTF::MediaTime durBridge = _ZNK7WebCore18PlatformTimeRanges8durationEj(&ranges, 0);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&durBridge) == 15.0);

    WebCore::PlatformTimeRanges other;
    _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(&other, zero, five);
    _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(&ranges, other);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges) == 1);
    WTF::MediaTime durOther = _ZNK7WebCore18PlatformTimeRanges8durationEj(&ranges, 0);
    Require(_ZNK3WTF9MediaTime8toDoubleEv(&durOther) == 5.0);

    _ZN7WebCore18PlatformTimeRanges5clearEv(&ranges);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges) == 0);

    WebCore::PlatformTimeRanges::VectorRange vecRange1{nullptr, 0, 0};
    WebCore::PlatformTimeRanges fromVec1;
    _ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&fromVec1, &vecRange1);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&fromVec1) == 0);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&fromVec1);

    WebCore::PlatformTimeRanges::VectorRange vecRange2{nullptr, 0, 0};
    WebCore::PlatformTimeRanges fromVec2;
    _ZN7WebCore18PlatformTimeRangesC2EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&fromVec2, &vecRange2);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&fromVec2) == 0);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&fromVec2);

    void* ptrMem = std::malloc(sizeof(WebCore::PlatformTimeRanges));
    Require(ptrMem != nullptr);
    WTF::NotNullTag ptrTag{};
    void* ptrPlacedTag = _ZN7WebCore18PlatformTimeRangesnwEm10NotNullTagPv(sizeof(WebCore::PlatformTimeRanges), ptrTag, ptrMem);
    Require(ptrPlacedTag == ptrMem);
    void* ptrPlaced = _ZN7WebCore18PlatformTimeRangesnwEmPv(sizeof(WebCore::PlatformTimeRanges), ptrMem);
    Require(ptrPlaced == ptrMem);
    std::free(ptrMem);

    void* arrPtrMem = std::malloc(sizeof(WebCore::PlatformTimeRanges) * 2);
    Require(arrPtrMem != nullptr);
    void* arrPtrPlaced = _ZN7WebCore18PlatformTimeRangesnaEmPv(sizeof(WebCore::PlatformTimeRanges) * 2, arrPtrMem);
    Require(arrPtrPlaced == arrPtrMem);
    std::free(arrPtrMem);

    _ZN7WebCore18PlatformTimeRangesD1Ev(&ranges);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&other);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&emptyRanges);
}

void TestTimeRanges() {
    WebCore::TimeRanges trObj1;
    _ZN7WebCore10TimeRangesC1Ev(&trObj1);
    Require(_ZNK7WebCore10TimeRanges6lengthEv(&trObj1) == 0);

    WebCore::TimeRanges trObj2;
    _ZN7WebCore10TimeRangesC2Ev(&trObj2);
    Require(_ZNK7WebCore10TimeRanges6lengthEv(&trObj2) == 0);

    WebCore::TimeRanges trObj3;
    _ZN7WebCore10TimeRangesC1Edd(&trObj3, 10.0, 20.0);
    Require(_ZNK7WebCore10TimeRanges6lengthEv(&trObj3) == 1);

    WebCore::TimeRanges trObj4;
    _ZN7WebCore10TimeRangesC2Edd(&trObj4, 10.0, 20.0);
    Require(_ZNK7WebCore10TimeRanges6lengthEv(&trObj4) == 1);

    auto tr = _ZN7WebCore10TimeRanges6createEv();
    Require(_ZNK7WebCore10TimeRanges6lengthEv(tr.get()) == 0);

    _ZN7WebCore10TimeRanges3addEdd(tr.get(), 10.0, 5.0);
    Require(_ZNK7WebCore10TimeRanges6lengthEv(tr.get()) == 0);

    _ZN7WebCore10TimeRanges3addEdd(tr.get(), 1.0, 4.0);
    Require(_ZNK7WebCore10TimeRanges6lengthEv(tr.get()) == 1);

    auto start0 = _ZNK7WebCore10TimeRanges5startEj(tr.get(), 0);
    Require(!start0.hasException());
    Require(start0.returnValue() == 1.0);

    auto end0 = _ZNK7WebCore10TimeRanges3endEj(tr.get(), 0);
    Require(!end0.hasException());
    Require(end0.returnValue() == 4.0);

    auto start1 = _ZNK7WebCore10TimeRanges5startEj(tr.get(), 1);
    Require(start1.hasException());

    auto end1 = _ZNK7WebCore10TimeRanges3endEj(tr.get(), 1);
    Require(end1.hasException());

    Require(_ZNK7WebCore10TimeRanges7nearestEd(tr.get(), 0.0) == 1.0);
    Require(_ZNK7WebCore10TimeRanges7nearestEd(tr.get(), 2.0) == 2.0);
    Require(_ZNK7WebCore10TimeRanges7nearestEd(tr.get(), 10.0) == 4.0);

    auto copy = _ZNK7WebCore10TimeRanges4copyEv(tr.get());
    Require(_ZNK7WebCore10TimeRanges6lengthEv(copy.get()) == 1);

    auto tr2 = _ZN7WebCore10TimeRanges6createEdd(3.0, 8.0);
    Require(_ZNK7WebCore10TimeRanges6lengthEv(tr2.get()) == 1);
    _ZN7WebCore10TimeRanges13intersectWithERKS0_(tr.get(), *tr2.get());
    Require(_ZNK7WebCore10TimeRanges6lengthEv(tr.get()) == 1);
    Require(_ZNK7WebCore10TimeRanges5startEj(tr.get(), 0).returnValue() == 3.0);
    Require(_ZNK7WebCore10TimeRanges3endEj(tr.get(), 0).returnValue() == 4.0);
}

} // namespace

int main() {
    TestMediaTimeBasics();
    TestMediaTimeCreationAndConversions();
    TestMediaTimeArithmeticAndComparison();
    TestMediaTimeMemory();
    TestPlatformTimeRanges();
    TestTimeRanges();
    std::printf("MediaTime and TimeRanges tests passed successfully!\n");
    return 0;
}
