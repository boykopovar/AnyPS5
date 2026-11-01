#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

static_assert(sizeof(IntPoint) == 8);
static_assert(sizeof(IntSize) == 8);
static_assert(sizeof(IntRect) == 16);
static_assert(sizeof(FloatPoint) == 8);
static_assert(sizeof(FloatSize) == 8);
static_assert(sizeof(FloatRect) == 16);
static_assert(sizeof(FloatQuad) == 32);
static_assert(sizeof(FloatRoundedRect) == 48);
static_assert(sizeof(LayoutUnit) == 4);
static_assert(sizeof(LayoutPoint) == 8);
static_assert(sizeof(LayoutSize) == 8);
static_assert(sizeof(LayoutRect) == 16);
static_assert(sizeof(RoundedRect) == 48);
static_assert(sizeof(AffineTransform) == 48);
static_assert(sizeof(TransformationMatrix) == 128);
static_assert(sizeof(MatrixTransformOperation) == 64);
static_assert(offsetof(MatrixTransformOperation, m_a) == 16);
static_assert(sizeof(Color) == 8);

}  // namespace WebCore
