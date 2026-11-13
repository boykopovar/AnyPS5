/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace WebCore {

inline int RectX(const IntRect& r) { return r.m_location.m_x; }
inline int RectY(const IntRect& r) { return r.m_location.m_y; }
inline int RectWidth(const IntRect& r) { return r.m_size.m_width; }
inline int RectHeight(const IntRect& r) { return r.m_size.m_height; }
inline int RectMaxX(const IntRect& r) { return r.m_location.m_x + r.m_size.m_width; }
inline int RectMaxY(const IntRect& r) { return r.m_location.m_y + r.m_size.m_height; }
inline bool RectIsEmpty(const IntRect& r) { return r.m_size.m_width <= 0 || r.m_size.m_height <= 0; }

inline bool RectContains(const IntRect& r, const IntPoint& p) {
    return p.m_x >= RectX(r) && p.m_x < RectMaxX(r) && p.m_y >= RectY(r) && p.m_y < RectMaxY(r);
}

inline bool RectContains(const IntRect& r, const IntRect& other) {
    return RectX(r) <= RectX(other) && RectMaxX(r) >= RectMaxX(other) &&
           RectY(r) <= RectY(other) && RectMaxY(r) >= RectMaxY(other);
}

inline bool RectIntersects(const IntRect& a, const IntRect& b) {
    return !RectIsEmpty(a) && !RectIsEmpty(b) &&
           RectX(a) < RectMaxX(b) && RectX(b) < RectMaxX(a) &&
           RectY(a) < RectMaxY(b) && RectY(b) < RectMaxY(a);
}

inline IntRect RectIntersection(const IntRect& a, const IntRect& b) {
    int newX = std::max(RectX(a), RectX(b));
    int newY = std::max(RectY(a), RectY(b));
    int newMaxX = std::min(RectMaxX(a), RectMaxX(b));
    int newMaxY = std::min(RectMaxY(a), RectMaxY(b));
    if (newMaxX <= newX || newMaxY <= newY)
        return IntRect{IntPoint{0, 0}, IntSize{0, 0}};
    return IntRect{IntPoint{newX, newY}, IntSize{newMaxX - newX, newMaxY - newY}};
}

Region::Shape::Shape(const IntRect& rect)
    : m_segments({ RectX(rect), RectMaxX(rect) })
    , m_spans({ { RectY(rect), 0 }, { RectMaxY(rect), 2 } })
{
}

Region::Shape::Shape(WTF::Vector<int, 32>&& segments, WTF::Vector<Span, 16>&& spans)
    : m_segments(std::move(segments)), m_spans(std::move(spans))
{
}

void Region::Shape::appendSpan(int y) {
    m_spans.append(Span{ y, m_segments.size() });
}

bool Region::Shape::canCoalesce(SegmentIterator begin, SegmentIterator end) {
    if (m_spans.isEmpty())
        return false;
    SegmentIterator lastSpanBegin = m_segments.data() + m_spans.last().segmentIndex;
    SegmentIterator lastSpanEnd = m_segments.data() + m_segments.size();
    if (lastSpanEnd - lastSpanBegin != end - begin)
        return false;
    if (!std::equal(begin, end, lastSpanBegin))
        return false;
    return true;
}

void Region::Shape::appendSpan(int y, SegmentIterator begin, SegmentIterator end) {
    if (canCoalesce(begin, end))
        return;
    appendSpan(y);
    m_segments.appendRange(begin, end);
}

void Region::Shape::appendSpans(const Shape& shape, SpanIterator begin, SpanIterator end) {
    for (SpanIterator it = begin; it != end; ++it)
        appendSpan(it->y, shape.segments_begin(it), shape.segments_end(it));
}

Region::Shape::SegmentIterator Region::Shape::segments_begin(SpanIterator it) const {
    if (it->segmentIndex == m_segments.size())
        return nullptr;
    return &m_segments[it->segmentIndex];
}

Region::Shape::SegmentIterator Region::Shape::segments_end(SpanIterator it) const {
    if (it->segmentIndex == m_segments.size())
        return nullptr;
    std::size_t segmentIndex = (it + 1)->segmentIndex;
    return m_segments.data() + segmentIndex;
}

IntRect Region::Shape::bounds() const {
    if (isEmpty())
        return IntRect{IntPoint{0, 0}, IntSize{0, 0}};

    SpanIterator span = spans_begin();
    int minY = span->y;
    SpanIterator lastSpan = spans_end() - 1;
    int maxY = lastSpan->y;

    int minX = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();

    while (span != lastSpan) {
        SegmentIterator firstSegment = segments_begin(span);
        SegmentIterator lastSegment = segments_end(span) - 1;

        if (firstSegment && lastSegment) {
            if (*firstSegment < minX)
                minX = *firstSegment;
            if (*lastSegment > maxX)
                maxX = *lastSegment;
        }
        ++span;
    }

    if (minX > maxX || minY > maxY)
        return IntRect{IntPoint{0, 0}, IntSize{0, 0}};

    int width = maxX - minX;
    int height = maxY - minY;
    return IntRect{IntPoint{minX, minY}, IntSize{width, height}};
}


template<typename Operation>
Region::Shape Region::Shape::shapeOperation(const Shape& shape1, const Shape& shape2) {
    Shape result;
    if (Operation::trySimpleOperation(shape1, shape2, result))
        return result;

    SpanIterator spans1 = shape1.spans_begin();
    SpanIterator spans1End = shape1.spans_end();
    SpanIterator spans2 = shape2.spans_begin();
    SpanIterator spans2End = shape2.spans_end();

    SegmentIterator segments1 = nullptr;
    SegmentIterator segments1End = nullptr;
    SegmentIterator segments2 = nullptr;
    SegmentIterator segments2End = nullptr;

    while (spans1 != spans1End && spans2 != spans2End) {
        int y = 0;
        int test = spans1->y - spans2->y;

        if (test <= 0) {
            y = spans1->y;
            segments1 = shape1.segments_begin(spans1);
            segments1End = shape1.segments_end(spans1);
            ++spans1;
        }
        if (test >= 0) {
            y = spans2->y;
            segments2 = shape2.segments_begin(spans2);
            segments2End = shape2.segments_end(spans2);
            ++spans2;
        }

        int flag = 0;
        int oldFlag = 0;

        SegmentIterator s1 = segments1;
        SegmentIterator s2 = segments2;

        WTF::Vector<int, 32> segments;

        while (s1 != segments1End && s2 != segments2End) {
            int segTest = *s1 - *s2;
            int x;

            if (segTest <= 0) {
                x = *s1;
                flag = flag ^ 1;
                ++s1;
            }
            if (segTest >= 0) {
                x = *s2;
                flag = flag ^ 2;
                ++s2;
            }

            if (flag == Operation::opCode || oldFlag == Operation::opCode)
                segments.append(x);

            oldFlag = flag;
        }

        if (Operation::shouldAddRemainingSegmentsFromSpan1 && s1 != segments1End)
            segments.appendRange(s1, segments1End);
        else if (Operation::shouldAddRemainingSegmentsFromSpan2 && s2 != segments2End)
            segments.appendRange(s2, segments2End);

        if (!segments.isEmpty() || !result.isEmpty())
            result.appendSpan(y, segments.data(), segments.data() + segments.size());
    }

    if (Operation::shouldAddRemainingSpansFromShape1 && spans1 != spans1End)
        result.appendSpans(shape1, spans1, spans1End);
    else if (Operation::shouldAddRemainingSpansFromShape2 && spans2 != spans2End)
        result.appendSpans(shape2, spans2, spans2End);

    return result;
}

struct Region::Shape::UnionOperation {
    static bool trySimpleOperation(const Shape& shape1, const Shape& shape2, Shape& result) {
        if (shape1.isEmpty()) {
            result = shape2;
            return true;
        }
        return false;
    }
    static const int opCode = 0;
    static const bool shouldAddRemainingSegmentsFromSpan1 = true;
    static const bool shouldAddRemainingSegmentsFromSpan2 = true;
    static const bool shouldAddRemainingSpansFromShape1 = true;
    static const bool shouldAddRemainingSpansFromShape2 = true;
};

Region::Shape Region::Shape::unionShapes(const Shape& shape1, const Shape& shape2) {
    return shapeOperation<UnionOperation>(shape1, shape2);
}

struct Region::Shape::IntersectOperation {
    static bool trySimpleOperation(const Shape&, const Shape&, Shape&) {
        return false;
    }
    static const int opCode = 3;
    static const bool shouldAddRemainingSegmentsFromSpan1 = false;
    static const bool shouldAddRemainingSegmentsFromSpan2 = false;
    static const bool shouldAddRemainingSpansFromShape1 = false;
    static const bool shouldAddRemainingSpansFromShape2 = false;
};

Region::Shape Region::Shape::intersectShapes(const Shape& shape1, const Shape& shape2) {
    return shapeOperation<IntersectOperation>(shape1, shape2);
}

struct Region::Shape::SubtractOperation {
    static bool trySimpleOperation(const Shape&, const Shape&, Region::Shape&) {
        return false;
    }
    static const int opCode = 1;
    static const bool shouldAddRemainingSegmentsFromSpan1 = true;
    static const bool shouldAddRemainingSegmentsFromSpan2 = false;
    static const bool shouldAddRemainingSpansFromShape1 = true;
    static const bool shouldAddRemainingSpansFromShape2 = false;
};

Region::Shape Region::Shape::subtractShapes(const Shape& shape1, const Shape& shape2) {
    return shapeOperation<SubtractOperation>(shape1, shape2);
}

struct Region::Shape::CompareContainsOperation {
    static constexpr bool defaultResult = true;
    static bool aOutsideB(bool&) { return false; }
    static bool bOutsideA(bool& result) { result = false; return true; }
    static bool aOverlapsB(bool&) { return false; }
};

struct Region::Shape::CompareIntersectsOperation {
    static constexpr bool defaultResult = false;
    static bool aOutsideB(bool&) { return false; }
    static bool bOutsideA(bool&) { return false; }
    static bool aOverlapsB(bool& result) { result = true; return true; }
};

template<typename CompareOperation>
bool Region::Shape::compareShapes(const Shape& aShape, const Shape& bShape) {
    bool result = CompareOperation::defaultResult;

    Shape::SpanIterator aSpan = aShape.spans_begin();
    Shape::SpanIterator aSpanEnd = aShape.spans_end();
    Shape::SpanIterator bSpan = bShape.spans_begin();
    Shape::SpanIterator bSpanEnd = bShape.spans_end();

    bool aHadSegmentInPreviousSpan = false;
    bool bHadSegmentInPreviousSpan = false;

    while (aSpan != aSpanEnd && aSpan + 1 != aSpanEnd && bSpan != bSpanEnd && bSpan + 1 != bSpanEnd) {
        int aY = aSpan->y;
        int aMaxY = (aSpan + 1)->y;
        int bY = bSpan->y;
        int bMaxY = (bSpan + 1)->y;

        Shape::SegmentIterator aSegment = aShape.segments_begin(aSpan);
        Shape::SegmentIterator aSegmentEnd = aShape.segments_end(aSpan);
        Shape::SegmentIterator bSegment = bShape.segments_begin(bSpan);
        Shape::SegmentIterator bSegmentEnd = bShape.segments_end(bSpan);

        bool aHasSegmentInSpan = aSegment != aSegmentEnd;
        bool bHasSegmentInSpan = bSegment != bSegmentEnd;

        if (aY < bY && !bHadSegmentInPreviousSpan && aHasSegmentInSpan && CompareOperation::aOutsideB(result))
            return result;
        if (bY < aY && !aHadSegmentInPreviousSpan && bHasSegmentInSpan && CompareOperation::bOutsideA(result))
            return result;

        aHadSegmentInPreviousSpan = aHasSegmentInSpan;
        bHadSegmentInPreviousSpan = bHasSegmentInSpan;

        bool spansOverlap = bMaxY > aY && bY < aMaxY;
        if (spansOverlap) {
            while (aSegment != aSegmentEnd && bSegment != bSegmentEnd) {
                int aX = *aSegment;
                int aMaxX = *(aSegment + 1);
                int bX = *bSegment;
                int bMaxX = *(bSegment + 1);

                bool segmentsOverlap = bMaxX > aX && bX < aMaxX;
                if (segmentsOverlap && CompareOperation::aOverlapsB(result))
                    return result;
                if (aX < bX && CompareOperation::aOutsideB(result))
                    return result;
                if (bX < aX && CompareOperation::bOutsideA(result))
                    return result;

                if (aMaxX < bMaxX)
                    aSegment += 2;
                else if (bMaxX < aMaxX)
                    bSegment += 2;
                else {
                    aSegment += 2;
                    bSegment += 2;
                }
            }

            if (aSegment != aSegmentEnd && CompareOperation::aOutsideB(result))
                return result;
            if (bSegment != bSegmentEnd && CompareOperation::bOutsideA(result))
                return result;
        }

        if (aMaxY < bMaxY)
            aSpan += 1;
        else if (bMaxY < aMaxY)
            bSpan += 1;
        else {
            aSpan += 1;
            bSpan += 1;
        }
    }

    if (aSpan != aSpanEnd && aSpan + 1 != aSpanEnd && CompareOperation::aOutsideB(result))
        return result;
    if (bSpan != bSpanEnd && bSpan + 1 != bSpanEnd && CompareOperation::bOutsideA(result))
        return result;

    return result;
}

template bool Region::Shape::compareShapes<Region::Shape::CompareContainsOperation>(const Region::Shape&, const Region::Shape&);
template bool Region::Shape::compareShapes<Region::Shape::CompareIntersectsOperation>(const Region::Shape&, const Region::Shape&);

static void SetRegionShape(Region* self, Region::Shape&& shape) {
    self->m_bounds = shape.bounds();
    if (shape.isRect()) {
        self->m_shape = nullptr;
        return;
    }
    if (!self->m_shape)
        self->m_shape = std::make_unique<Region::Shape>(std::move(shape));
    else
        *self->m_shape = std::move(shape);
}

}  // namespace WebCore

extern "C" {

void APS5_VABI _ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE(WebCore::Region::Shape* self, const WebCore::IntSize& offset) {
    for (std::size_t i = 0; i < self->m_segments.size(); ++i)
        self->m_segments[i] += offset.m_width;
    for (std::size_t i = 0; i < self->m_spans.size(); ++i)
        self->m_spans[i].y += offset.m_height;
}

void APS5_VABI _ZN7WebCore6Region5uniteERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    if (other.isEmpty())
        return;
    if (self->isEmpty()) {
        self->m_bounds = other.m_bounds;
        self->m_shape = other.m_shape ? std::make_unique<WebCore::Region::Shape>(*other.m_shape) : nullptr;
        return;
    }
    if (other.isRect() && WebCore::RectContains(other.m_bounds, self->m_bounds)) {
        self->m_bounds = other.m_bounds;
        self->m_shape = nullptr;
        return;
    }
    if (_ZNK7WebCore6Region8containsERKS0_(self, other))
        return;

    SetRegionShape(self, WebCore::Region::Shape::unionShapes(
        self->m_shape ? *self->m_shape : WebCore::Region::Shape(self->m_bounds),
        other.m_shape ? *other.m_shape : WebCore::Region::Shape(other.m_bounds)));
}

void APS5_VABI _ZN7WebCore6Region8subtractERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    if (self->isEmpty() || other.isEmpty())
        return;
    if (!WebCore::RectIntersects(self->m_bounds, other.m_bounds))
        return;

    SetRegionShape(self, WebCore::Region::Shape::subtractShapes(
        self->m_shape ? *self->m_shape : WebCore::Region::Shape(self->m_bounds),
        other.m_shape ? *other.m_shape : WebCore::Region::Shape(other.m_bounds)));
}

void APS5_VABI _ZN7WebCore6Region9intersectERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    if (self->m_bounds.m_size.m_width <= 0 || self->m_bounds.m_size.m_height <= 0)
        return;
    if (!WebCore::RectIntersects(self->m_bounds, other.m_bounds)) {
        self->m_shape = nullptr;
        self->m_bounds = WebCore::IntRect{WebCore::IntPoint{0, 0}, WebCore::IntSize{0, 0}};
        return;
    }
    if (!self->m_shape && !other.m_shape) {
        self->m_bounds = WebCore::RectIntersection(self->m_bounds, other.m_bounds);
        return;
    }

    SetRegionShape(self, WebCore::Region::Shape::intersectShapes(
        self->m_shape ? *self->m_shape : WebCore::Region::Shape(self->m_bounds),
        other.m_shape ? *other.m_shape : WebCore::Region::Shape(other.m_bounds)));
}

void APS5_VABI _ZN7WebCore6Region9translateERKNS_7IntSizeE(WebCore::Region* self, const WebCore::IntSize& offset) {
    self->m_bounds.m_location.m_x += offset.m_width;
    self->m_bounds.m_location.m_y += offset.m_height;
    if (self->m_shape)
        _ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE(self->m_shape.get(), offset);
}

void APS5_VABI _ZN7WebCore6RegionC1EOS0_(WebCore::Region* self, WebCore::Region&& other) {
    self->m_bounds = std::move(other.m_bounds);
    new (&self->m_shape) std::unique_ptr<WebCore::Region::Shape>(std::move(other.m_shape));
}

void APS5_VABI _ZN7WebCore6RegionC1ERKNS_7IntRectE(WebCore::Region* self, const WebCore::IntRect& rect) {
    self->m_bounds = rect;
    new (&self->m_shape) std::unique_ptr<WebCore::Region::Shape>(nullptr);
}

void APS5_VABI _ZN7WebCore6RegionC1ERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    self->m_bounds = other.m_bounds;
    new (&self->m_shape) std::unique_ptr<WebCore::Region::Shape>(other.m_shape ? std::make_unique<WebCore::Region::Shape>(*other.m_shape) : nullptr);
}

void APS5_VABI _ZN7WebCore6RegionC1Ev(WebCore::Region* self) {
    self->m_bounds = WebCore::IntRect{WebCore::IntPoint{0, 0}, WebCore::IntSize{0, 0}};
    new (&self->m_shape) std::unique_ptr<WebCore::Region::Shape>(nullptr);
}

void APS5_VABI _ZN7WebCore6RegionC2EOS0_(WebCore::Region* self, WebCore::Region&& other) {
    _ZN7WebCore6RegionC1EOS0_(self, std::move(other));
}

void APS5_VABI _ZN7WebCore6RegionC2ERKNS_7IntRectE(WebCore::Region* self, const WebCore::IntRect& rect) {
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(self, rect);
}

void APS5_VABI _ZN7WebCore6RegionC2ERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    _ZN7WebCore6RegionC1ERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore6RegionC2Ev(WebCore::Region* self) {
    _ZN7WebCore6RegionC1Ev(self);
}

void APS5_VABI _ZN7WebCore6RegionD1Ev(WebCore::Region* self) {
    self->m_shape.~unique_ptr();
}

void APS5_VABI _ZN7WebCore6RegionD2Ev(WebCore::Region* self) {
    self->m_shape.~unique_ptr();
}

WebCore::Region& APS5_VABI _ZN7WebCore6RegionaSEOS0_(WebCore::Region* self, WebCore::Region&& other) {
    self->m_bounds = std::move(other.m_bounds);
    self->m_shape = std::move(other.m_shape);
    return *self;
}

WebCore::Region& APS5_VABI _ZN7WebCore6RegionaSERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    self->m_bounds = other.m_bounds;
    self->m_shape = other.m_shape ? std::make_unique<WebCore::Region::Shape>(*other.m_shape) : nullptr;
    return *self;
}

bool APS5_VABI _ZNK7WebCore6Region10intersectsERKS0_(const WebCore::Region* self, const WebCore::Region& other) {
    if (!WebCore::RectIntersects(self->m_bounds, other.m_bounds))
        return false;
    if (!self->m_shape && !other.m_shape)
        return true;
    return WebCore::Region::Shape::compareShapes<WebCore::Region::Shape::CompareIntersectsOperation>(
        self->m_shape ? *self->m_shape : WebCore::Region::Shape(self->m_bounds),
        other.m_shape ? *other.m_shape : WebCore::Region::Shape(other.m_bounds));
}

WTF::Vector<WebCore::IntRect, 1> APS5_VABI _ZNK7WebCore6Region5rectsEv(const WebCore::Region* self) {
    WTF::Vector<WebCore::IntRect, 1> rectList;
    if (!self->m_shape) {
        if (self->m_bounds.m_size.m_width > 0 && self->m_bounds.m_size.m_height > 0)
            rectList.uncheckedAppend(self->m_bounds);
        return rectList;
    }

    for (WebCore::Region::Shape::SpanIterator span = self->m_shape->spans_begin(), end = self->m_shape->spans_end(); span != end && span + 1 != end; ++span) {
        int y = span->y;
        int height = (span + 1)->y - y;

        for (WebCore::Region::Shape::SegmentIterator segment = self->m_shape->segments_begin(span), segEnd = self->m_shape->segments_end(span); segment != segEnd && segment + 1 != segEnd; segment += 2) {
            int x = *segment;
            int width = *(segment + 1) - x;
            rectList.append(WebCore::IntRect{WebCore::IntPoint{x, y}, WebCore::IntSize{width, height}});
        }
    }
    return rectList;
}

bool APS5_VABI _ZNK7WebCore6Region8containsERKNS_8IntPointE(const WebCore::Region* self, const WebCore::IntPoint& point) {
    if (!WebCore::RectContains(self->m_bounds, point))
        return false;
    if (!self->m_shape)
        return true;

    for (WebCore::Region::Shape::SpanIterator span = self->m_shape->spans_begin(), end = self->m_shape->spans_end(); span != end && span + 1 != end; ++span) {
        int y = span->y;
        int maxY = (span + 1)->y;
        if (y > point.m_y)
            break;
        if (maxY <= point.m_y)
            continue;

        for (WebCore::Region::Shape::SegmentIterator segment = self->m_shape->segments_begin(span), segEnd = self->m_shape->segments_end(span); segment != segEnd && segment + 1 != segEnd; segment += 2) {
            int x = *segment;
            int maxX = *(segment + 1);
            if (x > point.m_x)
                break;
            if (maxX > point.m_x)
                return true;
        }
    }
    return false;
}

bool APS5_VABI _ZNK7WebCore6Region8containsERKS0_(const WebCore::Region* self, const WebCore::Region& other) {
    if (!WebCore::RectContains(self->m_bounds, other.m_bounds))
        return false;
    if (!self->m_shape)
        return true;
    return WebCore::Region::Shape::compareShapes<WebCore::Region::Shape::CompareContainsOperation>(
        *self->m_shape, other.m_shape ? *other.m_shape : WebCore::Region::Shape(other.m_bounds));
}

std::uint64_t APS5_VABI _ZNK7WebCore6Region9totalAreaEv(const WebCore::Region* self) {
    std::uint64_t total = 0;
    auto rectList = _ZNK7WebCore6Region5rectsEv(self);
    for (std::size_t i = 0; i < rectList.size(); ++i) {
        total += static_cast<std::uint64_t>(rectList[i].m_size.m_width) * static_cast<std::uint64_t>(rectList[i].m_size.m_height);
    }
    return total;
}

}
