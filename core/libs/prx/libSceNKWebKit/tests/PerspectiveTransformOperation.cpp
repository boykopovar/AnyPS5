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

static WebCore::LegacyLength Legacy(int value, WebCore::LegacyLengthType type, bool quirk = false) {
    WebCore::LegacyLength length{};
    length.m_intValue = value;
    length.m_hasQuirk = quirk;
    length.m_type = type;
    return length;
}

static WebCore::PerspectiveTransformOperation Make(WebCore::LegacyLength p, std::uint32_t type = PerspectiveType) {
    WebCore::PerspectiveTransformOperation op{};
    op.m_base.m_refCount = 1;
    op.m_base.m_type = type;
    op.m_p = p;
    return op;
}

static WebCore::LegacyLength FloatLength(float value, WebCore::LegacyLengthType type) {
    WebCore::LegacyLength length = Legacy(0, type);
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
    const WebCore::PerspectiveTransformOperation zero = Make(Legacy(0, WebCore::LegacyLengthType::Fixed));
    const WebCore::PerspectiveTransformOperation floatZero = Make(FloatLength(0.0f, WebCore::LegacyLengthType::Fixed));
    const WebCore::PerspectiveTransformOperation fixed = Make(Legacy(500, WebCore::LegacyLengthType::Fixed));
    const WebCore::PerspectiveTransformOperation tiny = Make(FloatLength(0.25f, WebCore::LegacyLengthType::Fixed));
    const WebCore::PerspectiveTransformOperation percent = Make(Legacy(50, WebCore::LegacyLengthType::Percent));
    const WebCore::PerspectiveTransformOperation percentZero = Make(Legacy(0, WebCore::LegacyLengthType::Percent));
    const WebCore::PerspectiveTransformOperation automatic = Make(Legacy(0, WebCore::LegacyLengthType::Auto));

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

    const WebCore::PerspectiveTransformOperation calculated = Make(Legacy(7, WebCore::LegacyLengthType::Calculated));
    Require(Throws([&] { IsIdentity(calculated); }));
    Require(Throws([&] { IsAffected(calculated); }));
}

static void TestPerspective() {
    WebCore::PerspectiveTransformOperation op = Make(FloatLength(12.5f, WebCore::LegacyLengthType::Fixed));
    op.m_p.m_hasQuirk = true;
    WebCore::LegacyLength result = Legacy(1234, WebCore::LegacyLengthType::Percent);
    Require(_ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(&result, &op) == &result);
    Require(std::memcmp(&result, &op.m_p, sizeof(WebCore::LegacyLength)) == 0);

    const WebCore::PerspectiveTransformOperation calculated = Make(Legacy(3, WebCore::LegacyLengthType::Calculated));
    WebCore::LegacyLength untouched = Legacy(99, WebCore::LegacyLengthType::Fixed);
    Require(Throws([&] { _ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(&untouched, &calculated); }));
    Require(untouched.m_intValue == 99 && untouched.m_type == WebCore::LegacyLengthType::Fixed);
}

static void TestApply() {
    const WebCore::FloatSize box{300.0f, 200.0f};
    const WebCore::PerspectiveTransformOperation op = Make(Legacy(4, WebCore::LegacyLengthType::Fixed));

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

    const WebCore::PerspectiveTransformOperation identity = Make(Legacy(0, WebCore::LegacyLengthType::Fixed));
    WebCore::TransformationMatrix unchanged;
    const WebCore::TransformationMatrix reference;
    Require(!_ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(&identity, unchanged, box));
    Require(std::memcmp(&unchanged, &reference, sizeof(unchanged)) == 0);

    const WebCore::PerspectiveTransformOperation calculated = Make(Legacy(1, WebCore::LegacyLengthType::Calculated));
    Require(Throws([&] {
        WebCore::TransformationMatrix matrix;
        _ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(&calculated, matrix, box);
    }));
}

static void TestEquality() {
    const WebCore::PerspectiveTransformOperation a = Make(Legacy(5, WebCore::LegacyLengthType::Fixed));
    const WebCore::PerspectiveTransformOperation b = Make(Legacy(5, WebCore::LegacyLengthType::Fixed));
    const WebCore::PerspectiveTransformOperation asFloat = Make(FloatLength(5.0f, WebCore::LegacyLengthType::Fixed));
    const WebCore::PerspectiveTransformOperation other = Make(Legacy(6, WebCore::LegacyLengthType::Fixed));
    const WebCore::PerspectiveTransformOperation percent = Make(Legacy(5, WebCore::LegacyLengthType::Percent));
    const WebCore::PerspectiveTransformOperation quirk = Make(Legacy(5, WebCore::LegacyLengthType::Fixed, true));
    const WebCore::PerspectiveTransformOperation otherType = Make(Legacy(5, WebCore::LegacyLengthType::Fixed), 10);

    Require(Equal(a, a));
    Require(Equal(a, b) && Equal(b, a));
    Require(Equal(a, asFloat) && Equal(asFloat, a));
    Require(!Equal(a, other));
    Require(!Equal(a, percent));
    Require(!Equal(a, quirk));
    Require(!Equal(a, otherType));

    const WebCore::PerspectiveTransformOperation undefinedA = Make(Legacy(1, WebCore::LegacyLengthType::Undefined));
    const WebCore::PerspectiveTransformOperation undefinedB = Make(Legacy(2, WebCore::LegacyLengthType::Undefined));
    Require(Equal(undefinedA, undefinedB));

    const WebCore::PerspectiveTransformOperation calcA = Make(Legacy(1, WebCore::LegacyLengthType::Calculated));
    const WebCore::PerspectiveTransformOperation calcB = Make(Legacy(1, WebCore::LegacyLengthType::Calculated));
    Require(Throws([&] { Equal(calcA, calcB); }));
    Require(!Equal(calcA, a));
}

static WebCore::PerspectiveTransformOperation FromBytes(std::uint32_t value, std::uint8_t quirk, std::uint8_t type, std::uint8_t isFloat) {
    WebCore::PerspectiveTransformOperation op{};
    op.m_base.m_refCount = 1;
    op.m_base.m_type = PerspectiveType;
    const std::uint8_t bytes[8] = {static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value >> 16),
                                   static_cast<std::uint8_t>(value >> 24), quirk, type, isFloat, 0};
    std::memcpy(&op.m_p, bytes, sizeof(bytes));
    return op;
}

static bool AppliesPerspective(const WebCore::PerspectiveTransformOperation& op, double perspective) {
    WebCore::TransformationMatrix applied;
    _ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(&op, applied, WebCore::FloatSize{});
    WebCore::TransformationMatrix expected;
    _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(&expected, perspective);
    return std::memcmp(&applied, &expected, sizeof(applied)) == 0;
}

static void TestLegacyByteLayout() {
    const WebCore::PerspectiveTransformOperation fixed = FromBytes(500, 0, 3, 0);
    Require(!IsIdentity(fixed) && IsAffected(fixed));
    Require(AppliesPerspective(fixed, 500.0));

    const WebCore::PerspectiveTransformOperation fixedZero = FromBytes(0, 0, 3, 0);
    Require(IsIdentity(fixedZero) && !IsAffected(fixedZero));

    const WebCore::PerspectiveTransformOperation quirky = FromBytes(250, 1, 3, 0);
    Require(AppliesPerspective(quirky, 250.0));

    const WebCore::PerspectiveTransformOperation asFloat = FromBytes(0x41480000u, 0, 3, 1);
    Require(AppliesPerspective(asFloat, 12.5));

    const WebCore::PerspectiveTransformOperation percent = FromBytes(50, 0, 2, 0);
    Require(AppliesPerspective(percent, 0.5));

    const WebCore::PerspectiveTransformOperation automatic = FromBytes(777, 0, 0, 0);
    Require(AppliesPerspective(automatic, 1.0));
    const WebCore::PerspectiveTransformOperation fillAvailable = FromBytes(777, 0, 8, 0);
    Require(AppliesPerspective(fillAvailable, 1.0));

    for (const std::uint8_t type : {std::uint8_t{1}, std::uint8_t{4}, std::uint8_t{5}, std::uint8_t{6}, std::uint8_t{7}, std::uint8_t{9}, std::uint8_t{11}, std::uint8_t{12}, std::uint8_t{255}}) {
        const WebCore::PerspectiveTransformOperation other = FromBytes(777, 0, type, 0);
        Require(IsIdentity(other) && !IsAffected(other));
    }

    const WebCore::PerspectiveTransformOperation calculated = FromBytes(7, 0, 10, 0);
    Require(Throws([&] { IsIdentity(calculated); }));

    const WebCore::PerspectiveTransformOperation undefinedA = FromBytes(1, 0, 11, 0);
    const WebCore::PerspectiveTransformOperation undefinedB = FromBytes(2, 0, 11, 0);
    Require(Equal(undefinedA, undefinedB));
    const WebCore::PerspectiveTransformOperation twelveA = FromBytes(1, 0, 12, 0);
    const WebCore::PerspectiveTransformOperation twelveB = FromBytes(2, 0, 12, 0);
    Require(!Equal(twelveA, twelveB));
    const WebCore::PerspectiveTransformOperation calculatedB = FromBytes(7, 0, 10, 0);
    Require(Throws([&] { Equal(calculated, calculatedB); }));
    Require(!Equal(FromBytes(5, 0, 3, 0), FromBytes(5, 1, 3, 0)));
    Require(!Equal(FromBytes(5, 0, 3, 0), FromBytes(5, 0, 2, 0)));
    Require(Equal(FromBytes(5, 0, 3, 0), FromBytes(0x40A00000u, 0, 3, 1)));
}

int main() {
    TestIdentityQueries();
    TestPerspective();
    TestApply();
    TestEquality();
    TestLegacyByteLayout();
    return 0;
}
