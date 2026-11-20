#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PerspectiveTransformOperation check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr std::uint32_t PerspectiveType = 19;

static WebCore::PerspectiveTransformOperation Make(WebCore::Length p, std::uint32_t type = PerspectiveType) {
    WebCore::PerspectiveTransformOperation op{};
    op.m_base.m_refCount = 1;
    op.m_base.m_type = type;
    op.m_p = p;
    return op;
}

static WebCore::Length FloatLength(float value, WebCore::LengthType type) {
    WebCore::Length length(type);
    length.m_floatValue = value;
    length.m_isFloat = true;
    return length;
}

template <typename F>
static bool Throws(F function) {
    try {
        function();
    } catch (...) {
        return true;
    }
    return false;
}

static bool IsIdentity(const WebCore::PerspectiveTransformOperation& op) {
    return _ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv(&op);
}

static bool IsAffected(const WebCore::PerspectiveTransformOperation& op) {
    return _ZNK7WebCore29PerspectiveTransformOperation27isAffectedByTransformOriginEv(&op);
}

static bool Equal(const WebCore::PerspectiveTransformOperation& a, const WebCore::PerspectiveTransformOperation& b) {
    return _ZNK7WebCore29PerspectiveTransformOperationeqERKNS_18TransformOperationE(&a, b.m_base);
}

static void TestIdentityQueries() {
    const WebCore::PerspectiveTransformOperation zero = Make(WebCore::Length(0, WebCore::LengthType::Fixed));
    const WebCore::PerspectiveTransformOperation floatZero = Make(FloatLength(0.0f, WebCore::LengthType::Fixed));
    const WebCore::PerspectiveTransformOperation fixed = Make(WebCore::Length(500, WebCore::LengthType::Fixed));
    const WebCore::PerspectiveTransformOperation tiny = Make(FloatLength(0.25f, WebCore::LengthType::Fixed));
    const WebCore::PerspectiveTransformOperation percent = Make(WebCore::Length(50, WebCore::LengthType::Percent));
    const WebCore::PerspectiveTransformOperation percentZero = Make(WebCore::Length(0, WebCore::LengthType::Percent));
    const WebCore::PerspectiveTransformOperation automatic = Make(WebCore::Length(WebCore::LengthType::Auto));

    Require(IsIdentity(zero) && !IsAffected(zero));
    Require(IsIdentity(floatZero) && !IsAffected(floatZero));
    Require(!IsIdentity(fixed) && IsAffected(fixed));
    Require(!IsIdentity(tiny) && IsAffected(tiny));
    Require(!IsIdentity(percent) && IsAffected(percent));
    Require(IsIdentity(percentZero) && !IsAffected(percentZero));
    Require(!IsIdentity(automatic) && IsAffected(automatic));

    for (const auto* op : {&zero, &fixed, &percent}) {
        Require(!_ZNK7WebCore29PerspectiveTransformOperation19isRepresentableIn2DEv(op));
    }

    const WebCore::PerspectiveTransformOperation calculated = Make(WebCore::Length(7, WebCore::LengthType::Calculated));
    Require(Throws([&] { IsIdentity(calculated); }));
    Require(Throws([&] { IsAffected(calculated); }));
}

static void TestPerspective() {
    WebCore::PerspectiveTransformOperation op = Make(FloatLength(12.5f, WebCore::LengthType::Fixed));
    op.m_p.m_hasQuirk = true;
    WebCore::Length result(1234, WebCore::LengthType::Percent);
    Require(_ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(&result, &op) == &result);
    Require(std::memcmp(&result, &op.m_p, sizeof(WebCore::Length)) == 0);

    const WebCore::PerspectiveTransformOperation calculated = Make(WebCore::Length(3, WebCore::LengthType::Calculated));
    WebCore::Length untouched(99, WebCore::LengthType::Fixed);
    Require(Throws([&] { _ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(&untouched, &calculated); }));
    Require(untouched.m_intValue == 99 && untouched.isFixed());
}

static void TestApply() {
    const WebCore::FloatSize box{300.0f, 200.0f};
    const WebCore::PerspectiveTransformOperation op = Make(WebCore::Length(4, WebCore::LengthType::Fixed));

    WebCore::TransformationMatrix applied;
    Require(!_ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(&op, applied, box));
    WebCore::TransformationMatrix expected;
    _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(&expected, 4.0);
    Require(std::memcmp(&applied, &expected, sizeof(applied)) == 0);
    Require(applied.m_matrix[2][3] == -0.25);

    WebCore::TransformationMatrix translated;
    translated.m_matrix[3][0] = 10.0;
    translated.m_matrix[3][1] = -20.0;
    WebCore::TransformationMatrix translatedExpected = translated;
    _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(&translatedExpected, 4.0);
    Require(!_ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(&op, translated, WebCore::FloatSize{}));
    Require(std::memcmp(&translated, &translatedExpected, sizeof(translated)) == 0);

    const WebCore::PerspectiveTransformOperation identity = Make(WebCore::Length(0, WebCore::LengthType::Fixed));
    WebCore::TransformationMatrix unchanged;
    const WebCore::TransformationMatrix reference;
    Require(!_ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(&identity, unchanged, box));
    Require(std::memcmp(&unchanged, &reference, sizeof(unchanged)) == 0);

    const WebCore::PerspectiveTransformOperation calculated = Make(WebCore::Length(1, WebCore::LengthType::Calculated));
    Require(Throws([&] {
        WebCore::TransformationMatrix matrix;
        _ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(&calculated, matrix, box);
    }));
}

static void TestEquality() {
    const WebCore::PerspectiveTransformOperation a = Make(WebCore::Length(5, WebCore::LengthType::Fixed));
    const WebCore::PerspectiveTransformOperation b = Make(WebCore::Length(5, WebCore::LengthType::Fixed));
    const WebCore::PerspectiveTransformOperation asFloat = Make(FloatLength(5.0f, WebCore::LengthType::Fixed));
    const WebCore::PerspectiveTransformOperation other = Make(WebCore::Length(6, WebCore::LengthType::Fixed));
    const WebCore::PerspectiveTransformOperation percent = Make(WebCore::Length(5, WebCore::LengthType::Percent));
    const WebCore::PerspectiveTransformOperation quirk = Make(WebCore::Length(5, WebCore::LengthType::Fixed, true));
    const WebCore::PerspectiveTransformOperation otherType = Make(WebCore::Length(5, WebCore::LengthType::Fixed), 10);

    Require(Equal(a, a));
    Require(Equal(a, b) && Equal(b, a));
    Require(Equal(a, asFloat) && Equal(asFloat, a));
    Require(!Equal(a, other));
    Require(!Equal(a, percent));
    Require(!Equal(a, quirk));
    Require(!Equal(a, otherType));

    const WebCore::PerspectiveTransformOperation undefinedA = Make(WebCore::Length(1, WebCore::LengthType::Undefined));
    const WebCore::PerspectiveTransformOperation undefinedB = Make(WebCore::Length(2, WebCore::LengthType::Undefined));
    Require(Equal(undefinedA, undefinedB));

    const WebCore::PerspectiveTransformOperation calcA = Make(WebCore::Length(1, WebCore::LengthType::Calculated));
    const WebCore::PerspectiveTransformOperation calcB = Make(WebCore::Length(1, WebCore::LengthType::Calculated));
    Require(Throws([&] { Equal(calcA, calcB); }));
    Require(!Equal(calcA, a));
}

int main() {
    TestIdentityQueries();
    TestPerspective();
    TestApply();
    TestEquality();
    return 0;
}
