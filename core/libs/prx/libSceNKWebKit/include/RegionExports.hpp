#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
void Deallocate(void* ptr);
}  // namespace NKWebKit

namespace WTF {

enum AdoptRefTag { AdoptRef };

template<typename T>
class Ref {
public:
    Ref(T& object) : m_ptr(&object) {
        m_ptr->ref();
    }

    Ref(T& object, AdoptRefTag) : m_ptr(&object) {}

    Ref(const Ref& other) : m_ptr(other.m_ptr) {
        if (m_ptr)
            m_ptr->ref();
    }

    Ref(Ref&& other) noexcept : m_ptr(other.leakRef()) {}

    template<typename U>
    Ref(Ref<U>&& other) noexcept : m_ptr(other.leakRef()) {}

    template<typename U>
    Ref(const Ref<U>& other) : m_ptr(other.get()) {
        if (m_ptr)
            m_ptr->ref();
    }
    ~Ref() {
        if (m_ptr)
            m_ptr->deref();
    }

    Ref& operator=(const Ref& other) {
        if (other.m_ptr)
            other.m_ptr->ref();
        T* old = m_ptr;
        m_ptr = other.m_ptr;
        if (old)
            old->deref();
        return *this;
    }

    Ref& operator=(Ref&& other) noexcept {
        T* old = m_ptr;
        m_ptr = other.leakRef();
        if (old)
            old->deref();
        return *this;
    }

    T* leakRef() noexcept {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    T* get() const { return m_ptr; }
    T* ptr() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }

private:
    T* m_ptr{nullptr};
};

template<typename T>
inline Ref<T> adoptRef(T& object) {
    return Ref<T>(object, AdoptRef);
}

template<typename T, std::size_t inlineCapacity>
class Vector {
public:
    Vector() : m_buffer(inlineBuffer()), m_capacity(inlineCapacity), m_size(0) {}

    Vector(std::initializer_list<T> list) : m_buffer(inlineBuffer()), m_capacity(inlineCapacity), m_size(0) {
        if (list.size() > inlineCapacity)
            allocateBuffer(list.size());
        for (const auto& item : list)
            m_buffer[m_size++] = item;
    }

    Vector(const Vector& other) : m_buffer(inlineBuffer()), m_capacity(inlineCapacity), m_size(0) {
        if (other.m_size > inlineCapacity)
            allocateBuffer(other.m_size);
        for (std::size_t i = 0; i < other.m_size; ++i)
            m_buffer[i] = other.m_buffer[i];
        m_size = other.m_size;
    }

    Vector(Vector&& other) noexcept : m_buffer(inlineBuffer()), m_capacity(inlineCapacity), m_size(0) {
        if (other.m_buffer == other.inlineBuffer()) {
            for (std::size_t i = 0; i < other.m_size; ++i)
                m_buffer[i] = other.m_buffer[i];
            m_size = other.m_size;
            other.m_size = 0;
        } else {
            m_buffer = other.m_buffer;
            m_capacity = other.m_capacity;
            m_size = other.m_size;
            other.m_buffer = other.inlineBuffer();
            other.m_capacity = inlineCapacity;
            other.m_size = 0;
        }
    }

    ~Vector() {
        if (m_buffer != inlineBuffer())
            deallocateBuffer(m_buffer);
    }

    Vector& operator=(const Vector& other) {
        if (this != &other) {
            clear();
            if (other.m_size > m_capacity)
                allocateBuffer(other.m_size);
            for (std::size_t i = 0; i < other.m_size; ++i)
                m_buffer[i] = other.m_buffer[i];
            m_size = other.m_size;
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            clear();
            if (other.m_buffer == other.inlineBuffer()) {
                for (std::size_t i = 0; i < other.m_size; ++i)
                    m_buffer[i] = other.m_buffer[i];
                m_size = other.m_size;
                other.m_size = 0;
            } else {
                if (m_buffer != inlineBuffer())
                    deallocateBuffer(m_buffer);
                m_buffer = other.m_buffer;
                m_capacity = other.m_capacity;
                m_size = other.m_size;
                other.m_buffer = other.inlineBuffer();
                other.m_capacity = inlineCapacity;
                other.m_size = 0;
            }
        }
        return *this;
    }

    std::size_t size() const { return m_size; }
    bool isEmpty() const { return m_size == 0; }
    std::size_t capacity() const { return m_capacity; }

    const T* data() const { return m_buffer; }
    T* data() { return m_buffer; }

    const T& operator[](std::size_t index) const { return m_buffer[index]; }
    T& operator[](std::size_t index) { return m_buffer[index]; }

    const T& last() const { return m_buffer[m_size - 1]; }
    T& last() { return m_buffer[m_size - 1]; }

    void append(const T& val) {
        if (m_size == m_capacity)
            expandCapacity(m_size + 1);
        m_buffer[m_size++] = val;
    }

    void append(T&& val) {
        if (m_size == m_capacity)
            expandCapacity(m_size + 1);
        m_buffer[m_size++] = std::move(val);
    }

    void uncheckedAppend(const T& val) {
        m_buffer[m_size++] = val;
    }

    template<typename Iterator>
    void appendRange(Iterator begin, Iterator end) {
        for (Iterator it = begin; it != end; ++it)
            append(*it);
    }

    void clear() {
        m_size = 0;
    }

    void shrink(std::size_t newSize) {
        if (newSize < m_size)
            m_size = newSize;
    }

    void reserveCapacity(std::size_t newCap) {
        if (newCap > m_capacity)
            expandCapacity(newCap);
    }

    const T* begin() const { return m_buffer; }
    const T* end() const { return m_buffer + m_size; }
    T* begin() { return m_buffer; }
    T* end() { return m_buffer + m_size; }

    bool operator==(const Vector& other) const {
        if (m_size != other.m_size)
            return false;
        for (std::size_t i = 0; i < m_size; ++i) {
            if (!(m_buffer[i] == other.m_buffer[i]))
                return false;
        }
        return true;
    }

private:
    T* inlineBuffer() { return reinterpret_cast<T*>(m_inlineBuffer); }
    const T* inlineBuffer() const { return reinterpret_cast<const T*>(m_inlineBuffer); }

    void allocateBuffer(std::size_t newCap) {
        std::size_t allocBytes = newCap * sizeof(T);
        T* newBuf = static_cast<T*>(NKWebKit::Allocate(allocBytes));
        if (m_buffer != inlineBuffer())
            deallocateBuffer(m_buffer);
        m_buffer = newBuf;
        m_capacity = newCap;
    }

    void expandCapacity(std::size_t minCap) {
        std::size_t newCap = std::max(minCap, static_cast<std::size_t>(m_capacity * 2));
        std::size_t allocBytes = newCap * sizeof(T);
        T* newBuf = static_cast<T*>(NKWebKit::Allocate(allocBytes));
        for (std::size_t i = 0; i < m_size; ++i)
            newBuf[i] = std::move(m_buffer[i]);
        if (m_buffer != inlineBuffer())
            deallocateBuffer(m_buffer);
        m_buffer = newBuf;
        m_capacity = newCap;
    }

    void deallocateBuffer(T* ptr) {
        NKWebKit::Deallocate(ptr);
    }

    T* m_buffer{nullptr};
    unsigned m_capacity{0};
    unsigned m_size{0};
    alignas(T) char m_inlineBuffer[inlineCapacity * sizeof(T)];
};

}  // namespace WTF

namespace WebCore {

enum class LengthType : std::uint8_t {
    Auto,
    Relative,
    Percent,
    Fixed,
    Intrinsic,
    MinIntrinsic,
    MinContent,
    MaxContent,
    FillAvailable,
    FitContent,
    Calculated,
    Content,
    Undefined
};

struct Length {
    union {
        int m_intValue{0};
        float m_floatValue;
        unsigned m_calculationValueHandle;
    };
    LengthType m_type{LengthType::Auto};
    bool m_hasQuirk{false};
    bool m_isFloat{false};

    Length() = default;
    Length(LengthType t) : m_intValue(0), m_type(t) {}
    Length(int val, LengthType t, bool quirk = false)
        : m_intValue(val), m_type(t), m_hasQuirk(quirk), m_isFloat(false) {}

    LengthType type() const { return m_type; }
    int value() const { return m_intValue; }
    int intValue() const { return m_intValue; }
    bool isAuto() const { return m_type == LengthType::Auto; }
    bool isFixed() const { return m_type == LengthType::Fixed; }
    bool isPercent() const { return m_type == LengthType::Percent; }
    bool isCalculated() const { return m_type == LengthType::Calculated; }

    bool operator==(const Length& other) const {
        return m_type == other.m_type && m_intValue == other.m_intValue &&
               m_isFloat == other.m_isFloat && m_hasQuirk == other.m_hasQuirk;
    }
};

static_assert(sizeof(Length) == 8);

enum class FilterOperationType : std::uint8_t {
    Reference,
    Grayscale,
    Sepia,
    Saturate,
    HueRotate,
    Invert,
    AppleInvertLightness,
    Opacity,
    Brightness,
    Contrast,
    Blur,
    DropShadow,
    Passthrough,
    Default,
    None
};

struct FilterOperation {
    void* m_vptr{nullptr};
    mutable std::atomic<unsigned> m_refCount{1};
    FilterOperationType m_type{FilterOperationType::Default};
    std::uint8_t m_pad[3]{0, 0, 0};

    using Type = FilterOperationType;
    Type type() const { return m_type; }
    bool isSameType(const FilterOperation& other) const { return m_type == other.m_type; }

    void ref() const { ++m_refCount; }
    void deref() const {
        if (!--m_refCount)
            std::free(const_cast<FilterOperation*>(this));
    }
    unsigned refCount() const { return m_refCount; }
};

static_assert(sizeof(FilterOperation) == 16);

struct DefaultFilterOperation : public FilterOperation {
    FilterOperationType m_representedType{FilterOperationType::Default};
    std::uint8_t m_subPad[7]{0, 0, 0, 0, 0, 0, 0};

    FilterOperationType representedType() const { return m_representedType; }
};

static_assert(sizeof(DefaultFilterOperation) == 24);

struct BlurFilterOperation : public FilterOperation {
    Length m_stdDeviation;

    const Length& stdDeviation() const { return m_stdDeviation; }
};

static_assert(sizeof(BlurFilterOperation) == 24);

struct DropShadowFilterOperation : public FilterOperation {
    IntPoint m_location;
    int m_stdDeviation{0};
    Color m_color;

    int x() const { return m_location.m_x; }
    int y() const { return m_location.m_y; }
    IntPoint location() const { return m_location; }
    int stdDeviation() const { return m_stdDeviation; }
    const Color& color() const { return m_color; }
};

static_assert(sizeof(DropShadowFilterOperation) == 40);

struct InvertLightnessFilterOperation : public FilterOperation {
};

static_assert(sizeof(InvertLightnessFilterOperation) == 16);

struct BasicColorMatrixFilterOperation : public FilterOperation {
    double m_amount{0.0};

    double amount() const { return m_amount; }
};

static_assert(sizeof(BasicColorMatrixFilterOperation) == 24);

struct BasicComponentTransferFilterOperation : public FilterOperation {
    double m_amount{0.0};

    double amount() const { return m_amount; }
};

static_assert(sizeof(BasicComponentTransferFilterOperation) == 24);

class Region {
public:
    struct Span {
        int y{0};
        std::size_t segmentIndex{0};

        bool operator==(const Span& other) const {
            return y == other.y && segmentIndex == other.segmentIndex;
        }
    };

    class Shape {
    public:
        Shape() = default;
        Shape(const IntRect& rect);
        Shape(WTF::Vector<int, 32>&& segments, WTF::Vector<Span, 16>&& spans);

        IntRect bounds() const;
        bool isEmpty() const { return m_spans.isEmpty(); }
        bool isRect() const { return m_spans.size() <= 2 && m_segments.size() <= 2; }
        unsigned gridSize() const { return static_cast<unsigned>(m_spans.size() * m_segments.size()); }

        using SpanIterator = const Span*;
        SpanIterator spans_begin() const { return m_spans.data(); }
        SpanIterator spans_end() const { return m_spans.data() + m_spans.size(); }

        using SegmentIterator = const int*;
        SegmentIterator segments_begin(SpanIterator it) const;
        SegmentIterator segments_end(SpanIterator it) const;

        static Shape unionShapes(const Shape& shape1, const Shape& shape2);
        static Shape intersectShapes(const Shape& shape1, const Shape& shape2);
        static Shape subtractShapes(const Shape& shape1, const Shape& shape2);

        void translate(const IntSize& offset);

        struct CompareContainsOperation;
        struct CompareIntersectsOperation;

        template<typename CompareOperation>
        static bool compareShapes(const Shape& shape1, const Shape& shape2);

        struct UnionOperation;
        struct IntersectOperation;
        struct SubtractOperation;

        template<typename Operation>
        static Shape shapeOperation(const Shape& shape1, const Shape& shape2);

        void appendSegment(int x) { m_segments.append(x); }
        void appendSpan(int y);
        void appendSpan(int y, SegmentIterator begin, SegmentIterator end);
        void appendSpans(const Shape& shape, SpanIterator begin, SpanIterator end);
        bool canCoalesce(SegmentIterator begin, SegmentIterator end);

        WTF::Vector<int, 32> m_segments;
        WTF::Vector<Span, 16> m_spans;

        bool operator==(const Shape& other) const {
            return m_spans == other.m_spans && m_segments == other.m_segments;
        }
    };

    IntRect bounds() const { return m_bounds; }
    bool isEmpty() const { return m_bounds.m_size.m_width <= 0 || m_bounds.m_size.m_height <= 0; }
    bool isRect() const { return !m_shape; }

    IntRect m_bounds;
    std::unique_ptr<Shape> m_shape;
};

static_assert(sizeof(Region) == 24);
static_assert(offsetof(Region, m_bounds) == 0);
static_assert(offsetof(Region, m_shape) == 16);
static_assert(sizeof(Region::Span) == 16);
static_assert(sizeof(Region::Shape) == 416);

}  // namespace WebCore

extern "C" {







void APS5_VABI _ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE(WebCore::Region::Shape* self, const WebCore::IntSize& offset);
void APS5_VABI _ZN7WebCore6Region5uniteERKS0_(WebCore::Region* self, const WebCore::Region& other);
void APS5_VABI _ZN7WebCore6Region8subtractERKS0_(WebCore::Region* self, const WebCore::Region& other);
void APS5_VABI _ZN7WebCore6Region9intersectERKS0_(WebCore::Region* self, const WebCore::Region& other);
void APS5_VABI _ZN7WebCore6Region9translateERKNS_7IntSizeE(WebCore::Region* self, const WebCore::IntSize& offset);
void APS5_VABI _ZN7WebCore6RegionC1EOS0_(WebCore::Region* self, WebCore::Region&& other);
void APS5_VABI _ZN7WebCore6RegionC1ERKNS_7IntRectE(WebCore::Region* self, const WebCore::IntRect& rect);
void APS5_VABI _ZN7WebCore6RegionC1ERKS0_(WebCore::Region* self, const WebCore::Region& other);
void APS5_VABI _ZN7WebCore6RegionC1Ev(WebCore::Region* self);
void APS5_VABI _ZN7WebCore6RegionC2EOS0_(WebCore::Region* self, WebCore::Region&& other);
void APS5_VABI _ZN7WebCore6RegionC2ERKNS_7IntRectE(WebCore::Region* self, const WebCore::IntRect& rect);
void APS5_VABI _ZN7WebCore6RegionC2ERKS0_(WebCore::Region* self, const WebCore::Region& other);
void APS5_VABI _ZN7WebCore6RegionC2Ev(WebCore::Region* self);
void APS5_VABI _ZN7WebCore6RegionD1Ev(WebCore::Region* self);
void APS5_VABI _ZN7WebCore6RegionD2Ev(WebCore::Region* self);
WebCore::Region& APS5_VABI _ZN7WebCore6RegionaSEOS0_(WebCore::Region* self, WebCore::Region&& other);
WebCore::Region& APS5_VABI _ZN7WebCore6RegionaSERKS0_(WebCore::Region* self, const WebCore::Region& other);
bool APS5_VABI _ZNK7WebCore6Region10intersectsERKS0_(const WebCore::Region* self, const WebCore::Region& other);
WTF::Vector<WebCore::IntRect, 1> APS5_VABI _ZNK7WebCore6Region5rectsEv(const WebCore::Region* self);
bool APS5_VABI _ZNK7WebCore6Region8containsERKNS_8IntPointE(const WebCore::Region* self, const WebCore::IntPoint& point);
bool APS5_VABI _ZNK7WebCore6Region8containsERKS0_(const WebCore::Region* self, const WebCore::Region& other);
std::uint64_t APS5_VABI _ZNK7WebCore6Region9totalAreaEv(const WebCore::Region* self);

}
