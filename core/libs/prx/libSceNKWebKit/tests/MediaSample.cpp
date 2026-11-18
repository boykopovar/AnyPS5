#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

using WebCore::FloatSize;
using WebCore::MediaSample;
using WTF::MediaTime;

struct Sample {
    const void* const* vtable;
    MediaTime presentationTime;
    MediaTime decodeTime;
    MediaTime duration;
    FloatSize size;
    unsigned flags;
    mutable unsigned flagsCalls = 0;
};

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "MediaSample check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(condition) Check((condition), __LINE__)

template<typename TFunction>
bool ThrowsInvalidArgument(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

const Sample& AsSample(const MediaSample* self) {
    return *reinterpret_cast<const Sample*>(self);
}

MediaTime APS5_VABI PresentationTime(const MediaSample* self) {
    return AsSample(self).presentationTime;
}

MediaTime APS5_VABI DecodeTime(const MediaSample* self) {
    return AsSample(self).decodeTime;
}

MediaTime APS5_VABI Duration(const MediaSample* self) {
    return AsSample(self).duration;
}

FloatSize APS5_VABI PresentationSize(const MediaSample* self) {
    return AsSample(self).size;
}

unsigned APS5_VABI Flags(const MediaSample* self) {
    const auto& sample = AsSample(self);
    ++sample.flagsCalls;
    return sample.flags;
}

void CheckJson(NKWebKit::WtfAbi::String json, std::string_view expected, int line) {
    Check(json.impl && json.impl->data, line);
    Check(std::string_view(static_cast<const char*>(json.impl->data), json.impl->length) == expected, line);
}

#define RequireJson(json, expected) CheckJson((json), (expected), __LINE__)

void TestMediaTimeJson() {
    const MediaTime rational(-7, 2, MediaTime::Valid | MediaTime::HasBeenRounded);
    RequireJson(_ZNK3WTF9MediaTime12toJSONStringEv(&rational),
        R"({"value":-3.5,"numerator":-7,"denominator":2,"flags":3})");
    const MediaTime precise = _ZN3WTF9MediaTime16createWithDoubleEd(1.0000000000000002);
    RequireJson(_ZNK3WTF9MediaTime12toJSONStringEv(&precise), R"({"value":1.0000000000000002})");
    const MediaTime exponent = _ZN3WTF9MediaTime16createWithDoubleEd(1e-7);
    RequireJson(_ZNK3WTF9MediaTime12toJSONStringEv(&exponent), R"({"value":1e-7})");
    const MediaTime invalid(5, 3, 0);
    RequireJson(_ZNK3WTF9MediaTime12toJSONStringEv(&invalid),
        R"({"invalid":true,"numerator":5,"denominator":3,"flags":0})");
    const MediaTime indefinite(5, 3, MediaTime::Valid | MediaTime::Indefinite);
    RequireJson(_ZNK3WTF9MediaTime12toJSONStringEv(&indefinite),
        R"({"value":"NaN","numerator":5,"denominator":3,"flags":17})");
    const MediaTime positive(0, 1, MediaTime::Valid | MediaTime::PositiveInfinite);
    RequireJson(_ZNK3WTF9MediaTime12toJSONStringEv(&positive),
        R"({"value":"POSITIVE_INFINITY","numerator":0,"denominator":1,"flags":5})");
    const MediaTime negative(0, 1, MediaTime::Valid | MediaTime::NegativeInfinite);
    RequireJson(_ZNK3WTF9MediaTime12toJSONStringEv(&negative),
        R"({"value":"NEGATIVE_INFINITY","numerator":0,"denominator":1,"flags":9})");
    const MediaTime nonfinite(0, 0, MediaTime::Valid | MediaTime::DoubleValue | MediaTime::PositiveInfinite);
    RequireJson(_ZNK3WTF9MediaTime12toJSONStringEv(&nonfinite), R"({"value":null})");
    Require(ThrowsInvalidArgument([] { _ZNK3WTF9MediaTime12toJSONStringEv(nullptr); }));
}

void TestMediaSample() {
    std::array<const void*, 14> vtable{};
    vtable[2] = reinterpret_cast<const void*>(&PresentationTime);
    vtable[3] = reinterpret_cast<const void*>(&DecodeTime);
    vtable[4] = reinterpret_cast<const void*>(&Duration);
    vtable[7] = reinterpret_cast<const void*>(&PresentationSize);
    vtable[13] = reinterpret_cast<const void*>(&Flags);

    Sample sample{vtable.data(), MediaTime(3, 2, MediaTime::Valid), MediaTime(0, 1, 0),
        _ZN3WTF9MediaTime16createWithDoubleEd(0.125),
        {0.1f, std::numeric_limits<float>::infinity()}, 0};

    const auto* guest = reinterpret_cast<const MediaSample*>(&sample);
    for (unsigned flags = 0; flags < 32; ++flags) {
        sample.flags = flags;
        Require(_ZNK7WebCore11MediaSample6isSyncEv(guest) == ((flags & 1) != 0));
        Require(_ZNK7WebCore11MediaSample15isNonDisplayingEv(guest) == ((flags & 2) != 0));
        Require(_ZNK7WebCore11MediaSample8hasAlphaEv(guest) == ((flags & 4) != 0));
    }
    Require(sample.flagsCalls == 96);

    sample.flags = 21;
    RequireJson(_ZNK7WebCore11MediaSample12toJSONStringEv(guest),
        R"({"pts":{"value":1.5,"numerator":3,"denominator":2,"flags":1},)"
        R"("dts":{"invalid":true,"numerator":0,"denominator":1,"flags":0},)"
        R"("duration":{"value":0.125},"flags":21,"presentationSize":{"width":0.10000000149011612,"height":null}})");
    Require(sample.flagsCalls == 97);

    vtable[13] = nullptr;
    Require(ThrowsInvalidArgument([&] { _ZNK7WebCore11MediaSample6isSyncEv(guest); }));
    Require(ThrowsInvalidArgument([&] { _ZNK7WebCore11MediaSample12toJSONStringEv(guest); }));
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore11MediaSample8hasAlphaEv(nullptr); }));
}

} // namespace

int main() {
    TestMediaTimeJson();
    TestMediaSample();
    return 0;
}
