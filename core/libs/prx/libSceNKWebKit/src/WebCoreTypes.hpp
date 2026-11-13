#pragma once

#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WebCore {

struct IntPoint {
    int m_x{0};
    int m_y{0};
};

struct IntSize {
    int m_width{0};
    int m_height{0};
};

struct IntRect {
    IntPoint m_location;
    IntSize m_size;
};

struct FloatPoint {
    float m_x{0.0f};
    float m_y{0.0f};
};

struct FloatSize {
    float m_width{0.0f};
    float m_height{0.0f};

    float aspectRatio() const {
        return m_height != 0.0f ? (m_width / m_height) : 0.0f;
    }
};

struct FloatRect {
    FloatPoint m_location;
    FloatSize m_size;

    float x() const { return m_location.m_x; }
    float y() const { return m_location.m_y; }
    float width() const { return m_size.m_width; }
    float height() const { return m_size.m_height; }
    FloatSize size() const { return m_size; }

    void inflateX(float dx) {
        m_location.m_x -= dx;
        m_size.m_width += dx + dx;
    }

    void inflateY(float dy) {
        m_location.m_y -= dy;
        m_size.m_height += dy + dy;
    }
};

struct FloatQuad {
    FloatPoint m_p1;
    FloatPoint m_p2;
    FloatPoint m_p3;
    FloatPoint m_p4;
};

struct FloatRoundedRect {
    struct Radii {
        FloatSize m_topLeft;
        FloatSize m_topRight;
        FloatSize m_bottomLeft;
        FloatSize m_bottomRight;
    };
    FloatRect m_rect;
    Radii m_radii;
};

struct LayoutUnit {
    int m_value{0};
};

struct LayoutPoint {
    LayoutUnit m_x;
    LayoutUnit m_y;
};

struct LayoutSize {
    LayoutUnit m_width;
    LayoutUnit m_height;
};

struct LayoutRect {
    LayoutPoint m_location;
    LayoutSize m_size;
};

struct RoundedRect {
    struct Radii {
        LayoutSize m_topLeft;
        LayoutSize m_topRight;
        LayoutSize m_bottomLeft;
        LayoutSize m_bottomRight;
    };
    LayoutRect m_rect;
    Radii m_radii;
};

struct AffineTransform {
    std::array<double, 6> m_transform{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
};

struct OptionalAffineTransform {
    bool m_engaged;
    std::array<std::uint8_t, 7> m_padding;
    AffineTransform m_value;
};

struct alignas(16) TransformationMatrix {
    double m_matrix[4][4]{
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };
};

struct MatrixTransformOperation {
    void* m_vptr;
    std::uint32_t m_refCount;
    std::uint8_t m_type;
    std::uint8_t m_pad[3];
    double m_a;
    double m_b;
    double m_c;
    double m_d;
    double m_e;
    double m_f;
};

struct Color {
    std::uint64_t m_colorAndFlags{0};
};

template<typename T> struct SRGBA {
    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> struct LinearSRGBA {
    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> struct DisplayP3 {
    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> struct LinearDisplayP3 {
    T red;
    T green;
    T blue;
    T alpha;
};

template<typename T> struct HSLA {
    T hue;
    T saturation;
    T lightness;
    T alpha;
};

struct RedirectChainCacheStatus {
    enum class Status : std::uint8_t {
        NoRedirection,
        NotCachedRedirection,
        CachedRedirection,
    };

    WTF::WallTime endOfValidity;
    Status status{Status::NoRedirection};
};

enum class ViewportFit : std::uint8_t {
    Auto,
    Contain,
    Cover
};

struct ViewportAttributes {
    FloatSize layoutSize;
    float initialScale;
    float minimumScale;
    float maximumScale;
    float userScalable;
    float orientation;
    float shrinkToFit;
    ViewportFit viewportFit;
};

struct ViewportArguments {
    enum Type : int {
        Implicit,
        ViewportMeta,
        CSSDeviceAdaptation
    };

    static constexpr int ValueAuto = -1;
    static constexpr int ValueDeviceWidth = -2;
    static constexpr int ValueDeviceHeight = -3;

    Type type{Implicit};
    float width{ValueAuto};
    float minWidth{ValueAuto};
    float maxWidth{ValueAuto};
    float height{ValueAuto};
    float minHeight{ValueAuto};
    float maxHeight{ValueAuto};
    float zoom{ValueAuto};
    float minZoom{ValueAuto};
    float maxZoom{ValueAuto};
    float userZoom{ValueAuto};
    float orientation{ValueAuto};
    float shrinkToFit{ValueAuto};
    ViewportFit viewportFit{ViewportFit::Auto};
    bool widthWasExplicit{false};
};

enum class AutofillFieldName {
    None,
    Name,
    HonorificPrefix,
    GivenName,
    AdditionalName,
    FamilyName,
    HonorificSuffix,
    Nickname,
    Username,
    NewPassword,
    CurrentPassword,
    OrganizationTitle,
    Organization,
    StreetAddress,
    AddressLine1,
    AddressLine2,
    AddressLine3,
    AddressLevel4,
    AddressLevel3,
    AddressLevel2,
    AddressLevel1,
    Country,
    CountryName,
    PostalCode,
    CcName,
    CcGivenName,
    CcAdditionalName,
    CcFamilyName,
    CcNumber,
    CcExp,
    CcExpMonth,
    CcExpYear,
    CcCsc,
    CcType,
    TransactionCurrency,
    TransactionAmount,
    Language,
    Bday,
    BdayDay,
    BdayMonth,
    BdayYear,
    Sex,
    URL,
    Photo,
    Tel,
    TelCountryCode,
    TelNational,
    TelAreaCode,
    TelLocal,
    TelLocalPrefix,
    TelLocalSuffix,
    TelExtension,
    Email,
    Impp,
    WebAuthn
};

struct TextureMapper {
    enum ExposedEdges {
        NoEdges = 0,
        LeftEdge = 1 << 0,
        RightEdge = 1 << 1,
        TopEdge = 1 << 2,
        BottomEdge = 1 << 3,
        AllEdges = LeftEdge | RightEdge | TopEdge | BottomEdge,
    };
};

static_assert(sizeof(IntPoint) == 8);
static_assert(sizeof(IntSize) == 8);
static_assert(sizeof(IntRect) == 16);
static_assert(sizeof(FloatPoint) == 8);
static_assert(sizeof(FloatSize) == 8);
static_assert(sizeof(FloatRect) == 16);
static_assert(sizeof(AutofillFieldName) == 4);
static_assert(static_cast<int>(AutofillFieldName::Email) == 52);
static_assert(static_cast<int>(AutofillFieldName::WebAuthn) == 54);
static_assert(sizeof(FloatQuad) == 32);
static_assert(sizeof(FloatRoundedRect) == 48);
static_assert(sizeof(LayoutUnit) == 4);
static_assert(sizeof(LayoutPoint) == 8);
static_assert(sizeof(LayoutSize) == 8);
static_assert(sizeof(LayoutRect) == 16);
static_assert(sizeof(RoundedRect) == 48);
static_assert(sizeof(AffineTransform) == 48);
static_assert(sizeof(OptionalAffineTransform) == 56);
static_assert(offsetof(OptionalAffineTransform, m_padding) == 1);
static_assert(offsetof(OptionalAffineTransform, m_value) == 8);
static_assert(sizeof(TransformationMatrix) == 128);
static_assert(sizeof(MatrixTransformOperation) == 64);
static_assert(offsetof(MatrixTransformOperation, m_a) == 16);
static_assert(sizeof(Color) == 8);
static_assert(sizeof(SRGBA<std::uint8_t>) == 4);
static_assert(offsetof(SRGBA<std::uint8_t>, alpha) == 3);
static_assert(sizeof(SRGBA<float>) == 16);
static_assert(offsetof(SRGBA<float>, green) == 4);
static_assert(offsetof(SRGBA<float>, blue) == 8);
static_assert(offsetof(SRGBA<float>, alpha) == 12);
static_assert(sizeof(LinearSRGBA<float>) == 16);
static_assert(offsetof(LinearSRGBA<float>, alpha) == 12);
static_assert(sizeof(DisplayP3<float>) == 16);
static_assert(offsetof(DisplayP3<float>, alpha) == 12);
static_assert(sizeof(LinearDisplayP3<float>) == 16);
static_assert(offsetof(LinearDisplayP3<float>, alpha) == 12);
static_assert(sizeof(HSLA<float>) == 16);
static_assert(offsetof(HSLA<float>, saturation) == 4);
static_assert(offsetof(HSLA<float>, lightness) == 8);
static_assert(offsetof(HSLA<float>, alpha) == 12);
static_assert(std::is_trivially_copyable_v<SRGBA<std::uint8_t>>);
static_assert(std::is_trivially_copyable_v<SRGBA<float>>);
static_assert(std::is_trivially_copyable_v<HSLA<float>>);
static_assert(sizeof(RedirectChainCacheStatus) == 16);
static_assert(offsetof(RedirectChainCacheStatus, endOfValidity) == 0);
static_assert(offsetof(RedirectChainCacheStatus, status) == 8);
static_assert(std::is_trivially_copyable_v<RedirectChainCacheStatus>);
static_assert(sizeof(ViewportAttributes) == 36);
static_assert(offsetof(ViewportAttributes, viewportFit) == 32);
static_assert(sizeof(ViewportArguments) == 56);
static_assert(offsetof(ViewportArguments, width) == 4);
static_assert(offsetof(ViewportArguments, viewportFit) == 52);
static_assert(offsetof(ViewportArguments, widthWasExplicit) == 53);

}  // namespace WebCore
