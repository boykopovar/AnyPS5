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
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

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

struct RegionFields {
    IntRect bounds;
    Region::Shape* shape;
};

struct VectorHeaderLayout {
    void* buffer;
    unsigned capacity;
    unsigned size;
};

static_assert(sizeof(RegionFields) == sizeof(Region));
static_assert(offsetof(RegionFields, bounds) == offsetof(Region, m_bounds));
static_assert(offsetof(RegionFields, shape) == offsetof(Region, m_shape));
static_assert(offsetof(Region::Shape, m_segments) == 0);
static_assert(offsetof(Region::Shape, m_spans) == 144);
static_assert(sizeof(VectorHeaderLayout) == 16);

static bool IsReadableGuestBytes(const void* pointer, std::size_t size) {
    if (!size) return true;
    if (!pointer) return false;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    return GuestReadable_nid_no_patch(pointer, size);
}

static bool LoadRegion(RegionFields& fields, const Region* region) {
    if (!IsReadableGuestBytes(region, sizeof(fields)))
        return false;
    return GuestCopyIn_nid_no_patch(&fields, region, sizeof(fields));
}

static bool StoreRegion(Region* region, const RegionFields& fields, Region::Shape* previous) {
    if (!region) return false;
    const GuestArena::HostWrite writeGuard(region, sizeof(fields));
    if (!writeGuard.Open() || !GuestWritable_nid_no_patch(region, sizeof(fields)))
        return false;
    if (!GuestCopyOut_nid_no_patch(region, &fields, sizeof(fields)))
        return false;
    if (previous != fields.shape)
        delete previous;
    return true;
}

static bool StoreRegion(Region* region, const IntRect& bounds, std::unique_ptr<Region::Shape> shape, Region::Shape* previous) {
    if (!StoreRegion(region, RegionFields{bounds, shape.get()}, previous))
        return false;
    static_cast<void>(shape.release());
    return true;
}

static bool LoadShapeCopy(const Region::Shape* guestShape, Region::Shape& out) {
    if (!IsReadableGuestBytes(guestShape, sizeof(Region::Shape)))
        return false;
    VectorHeaderLayout segHeader{};
    VectorHeaderLayout spanHeader{};
    const auto* raw = reinterpret_cast<const unsigned char*>(guestShape);
    if (!GuestCopyIn_nid_no_patch(&segHeader, raw + offsetof(Region::Shape, m_segments), sizeof(segHeader)) ||
        !GuestCopyIn_nid_no_patch(&spanHeader, raw + offsetof(Region::Shape, m_spans), sizeof(spanHeader)))
        return false;
    if (segHeader.size > segHeader.capacity || spanHeader.size > spanHeader.capacity)
        return false;
    if ((segHeader.size > 0 && !segHeader.buffer) || (spanHeader.size > 0 && !spanHeader.buffer))
        return false;
    if (segHeader.size > std::numeric_limits<std::size_t>::max() / sizeof(int) ||
        spanHeader.size > std::numeric_limits<std::size_t>::max() / sizeof(Region::Span))
        return false;

    std::vector<int> rawSegments(segHeader.size);
    if (segHeader.size > 0) {
        const std::size_t segBytes = static_cast<std::size_t>(segHeader.size) * sizeof(int);
        if (!IsReadableGuestBytes(segHeader.buffer, segBytes) ||
            !GuestCopyIn_nid_no_patch(rawSegments.data(), segHeader.buffer, segBytes))
            return false;
    }

    std::vector<Region::Span> rawSpans(spanHeader.size);
    if (spanHeader.size > 0) {
        const std::size_t spanBytes = static_cast<std::size_t>(spanHeader.size) * sizeof(Region::Span);
        if (!IsReadableGuestBytes(spanHeader.buffer, spanBytes) ||
            !GuestCopyIn_nid_no_patch(rawSpans.data(), spanHeader.buffer, spanBytes))
            return false;
    }

    for (std::size_t i = 0; i < rawSpans.size(); ++i) {
        if (rawSpans[i].segmentIndex > rawSegments.size())
            return false;
        if (i > 0 && rawSpans[i].segmentIndex < rawSpans[i - 1].segmentIndex)
            return false;
    }
    if (!rawSpans.empty() && rawSpans.back().segmentIndex != rawSegments.size())
        return false;

    WTF::Vector<int, 32> segments;
    if (!rawSegments.empty())
        segments.appendRange(rawSegments.data(), rawSegments.data() + rawSegments.size());
    WTF::Vector<Region::Span, 16> spans;
    if (!rawSpans.empty())
        spans.appendRange(rawSpans.data(), rawSpans.data() + rawSpans.size());
    out = Region::Shape(std::move(segments), std::move(spans));
    return true;
}

static bool LoadRegionShape(const RegionFields& region, Region::Shape& out) {
    if (!region.shape) {
        out = Region::Shape(region.bounds);
        return true;
    }
    return LoadShapeCopy(region.shape, out);
}

static bool RegionContains(const RegionFields& region, const RegionFields& other, bool& result) {
    if (!RectContains(region.bounds, other.bounds)) {
        result = false;
        return true;
    }
    if (!region.shape) {
        result = true;
        return true;
    }
    Region::Shape aShape;
    Region::Shape bShape;
    if (!LoadShapeCopy(region.shape, aShape) || !LoadRegionShape(other, bShape))
        return false;
    result = Region::Shape::compareShapes<Region::Shape::CompareContainsOperation>(aShape, bShape);
    return true;
}

static bool SetRegionShape(Region* self, const RegionFields& region, Region::Shape&& shape) {
    const IntRect bounds = shape.bounds();
    if (shape.isRect())
        return StoreRegion(self, RegionFields{bounds, nullptr}, region.shape);
    return StoreRegion(self, bounds, std::make_unique<Region::Shape>(std::move(shape)), region.shape);
}

static bool TranslateShapeGuest(Region::Shape* self, const IntSize& delta) {
    if (!IsReadableGuestBytes(self, sizeof(Region::Shape)))
        return false;
    const GuestArena::HostWrite shapeWrite(self, sizeof(Region::Shape));
    if (!shapeWrite.Open() || !GuestWritable_nid_no_patch(self, sizeof(Region::Shape)))
        return false;
    VectorHeaderLayout segHeader{};
    VectorHeaderLayout spanHeader{};
    const auto* raw = reinterpret_cast<const unsigned char*>(self);
    if (!GuestCopyIn_nid_no_patch(&segHeader, raw + offsetof(Region::Shape, m_segments), sizeof(segHeader)) ||
        !GuestCopyIn_nid_no_patch(&spanHeader, raw + offsetof(Region::Shape, m_spans), sizeof(spanHeader)))
        return false;
    if (segHeader.size > segHeader.capacity || spanHeader.size > spanHeader.capacity)
        return false;
    if ((segHeader.size > 0 && !segHeader.buffer) || (spanHeader.size > 0 && !spanHeader.buffer))
        return false;
    if (segHeader.size > std::numeric_limits<std::size_t>::max() / sizeof(int) ||
        spanHeader.size > std::numeric_limits<std::size_t>::max() / sizeof(Region::Span))
        return false;

    const std::size_t segBytes = static_cast<std::size_t>(segHeader.size) * sizeof(int);
    const std::size_t spanBytes = static_cast<std::size_t>(spanHeader.size) * sizeof(Region::Span);
    std::vector<int> origSegments(segHeader.size);
    if (segBytes > 0) {
        if (!IsReadableGuestBytes(segHeader.buffer, segBytes) ||
            !GuestCopyIn_nid_no_patch(origSegments.data(), segHeader.buffer, segBytes))
            return false;
    }
    std::vector<Region::Span> spans(spanHeader.size);
    if (spanBytes > 0) {
        if (!IsReadableGuestBytes(spanHeader.buffer, spanBytes) ||
            !GuestCopyIn_nid_no_patch(spans.data(), spanHeader.buffer, spanBytes))
            return false;
    }

    std::deque<GuestArena::HostWrite> bufWrites;
    if (segBytes > 0) {
        auto& guard = bufWrites.emplace_back(segHeader.buffer, segBytes);
        if (!guard.Open() || !GuestWritable_nid_no_patch(segHeader.buffer, segBytes))
            return false;
    }
    if (spanBytes > 0) {
        auto& guard = bufWrites.emplace_back(spanHeader.buffer, spanBytes);
        if (!guard.Open() || !GuestWritable_nid_no_patch(spanHeader.buffer, spanBytes))
            return false;
    }

    std::vector<int> updatedSegments = origSegments;
    for (int& value : updatedSegments) value += delta.m_width;
    for (Region::Span& span : spans) span.y += delta.m_height;

    if (segBytes > 0 && !GuestCopyOut_nid_no_patch(segHeader.buffer, updatedSegments.data(), segBytes))
        return false;
    if (spanBytes > 0 && !GuestCopyOut_nid_no_patch(spanHeader.buffer, spans.data(), spanBytes)) {
        if (segBytes > 0)
            GuestCopyOut_nid_no_patch(segHeader.buffer, origSegments.data(), segBytes);
        return false;
    }
    return true;
}

static bool ComputeRegionRects(const Region* self, WTF::Vector<IntRect, 1>& rectList) {
    RegionFields region;
    if (!LoadRegion(region, self))
        return false;
    if (!region.shape) {
        if (region.bounds.m_size.m_width > 0 && region.bounds.m_size.m_height > 0)
            rectList.uncheckedAppend(region.bounds);
        return true;
    }
    Region::Shape shape;
    if (!LoadShapeCopy(region.shape, shape))
        return false;
    for (Region::Shape::SpanIterator span = shape.spans_begin(), end = shape.spans_end(); span != end && span + 1 != end; ++span) {
        const int y = span->y;
        const int height = (span + 1)->y - y;
        for (Region::Shape::SegmentIterator segment = shape.segments_begin(span), segEnd = shape.segments_end(span); segment != segEnd && segment + 1 != segEnd; segment += 2) {
            const int x = *segment;
            const int width = *(segment + 1) - x;
            rectList.append(IntRect{IntPoint{x, y}, IntSize{width, height}});
        }
    }
    return true;
}

static Region& MoveAssignRegion(Region* self, Region* other, const char* function) {
    RegionFields region;
    RegionFields source;
    if (!LoadRegion(region, self) || !LoadRegion(source, other))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (self == other) {
        const GuestArena::HostWrite selfWrite(self, sizeof(region));
        if (!selfWrite.Open() || !GuestWritable_nid_no_patch(self, sizeof(region)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        return *self;
    }
    const GuestArena::HostWrite selfWrite(self, sizeof(region));
    const GuestArena::HostWrite otherWrite(other, sizeof(source));
    if (!selfWrite.Open() || !otherWrite.Open() ||
        !GuestWritable_nid_no_patch(self, sizeof(region)) ||
        !GuestWritable_nid_no_patch(other, sizeof(source)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const RegionFields clearedOther{source.bounds, nullptr};
    if (!GuestCopyOut_nid_no_patch(other, &clearedOther, sizeof(clearedOther)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!GuestCopyOut_nid_no_patch(self, &source, sizeof(source))) {
        GuestCopyOut_nid_no_patch(other, &source, sizeof(source));
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    if (region.shape != source.shape)
        delete region.shape;
    return *self;
}

static Region& CopyAssignRegion(Region* self, const Region* other, const char* function) {
    RegionFields region;
    RegionFields source;
    if (!LoadRegion(region, self) || !LoadRegion(source, other))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::unique_ptr<Region::Shape> copiedShape;
    if (source.shape) {
        Region::Shape shape;
        if (!LoadShapeCopy(source.shape, shape))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        copiedShape = std::make_unique<Region::Shape>(std::move(shape));
    }
    if (!StoreRegion(self, source.bounds, std::move(copiedShape), region.shape))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return *self;
}

static NKWebKit::WtfAbi::TextStream& DumpRegionToStream(NKWebKit::WtfAbi::TextStream* stream, const Region* region, const char* function) {
    if (!IsReadableGuestBytes(stream, sizeof(NKWebKit::WtfAbi::TextStream)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const GuestArena::HostWrite streamWrite(stream, sizeof(NKWebKit::WtfAbi::TextStream));
    if (!streamWrite.Open() || !GuestWritable_nid_no_patch(stream, sizeof(NKWebKit::WtfAbi::TextStream)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);

    WTF::Vector<IntRect, 1> rects;
    if (!ComputeRegionRects(region, rects))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);

    int indent = 0;
    unsigned char* const indentPtr = reinterpret_cast<unsigned char*>(stream) + offsetof(NKWebKit::WtfAbi::TextStream, indent);
    if (!GuestCopyIn_nid_no_patch(&indent, indentPtr, sizeof(indent)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);

    _ZN3WTF10TextStreamlsEPKc(stream, "\n");
    const int incremented = indent + 1;
    if (!GuestCopyOut_nid_no_patch(indentPtr, &incremented, sizeof(incremented)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    struct IndentScope {
        unsigned char* indentAddress;
        int originalIndent;
        ~IndentScope() {
            GuestCopyOut_nid_no_patch(indentAddress, &originalIndent, sizeof(originalIndent));
        }
    } indentScope{indentPtr, indent};

    for (std::size_t i = 0; i < rects.size(); ++i) {
        _ZN3WTF10TextStream11writeIndentEv(stream);
        _ZN3WTF10TextStreamlsEPKc(stream, "(rect ");
        _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(*stream, rects.data()[i]);
        _ZN3WTF10TextStreamlsEPKc(stream, ")\n");
    }
    return *stream;
}

}  // namespace WebCore

extern "C" {

void APS5_VABI _ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE(WebCore::Region::Shape* self, const WebCore::IntSize& offset) {
    WebCore::IntSize delta;
    if (!WebCore::IsReadableGuestBytes(&offset, sizeof(delta)) ||
        !GuestCopyIn_nid_no_patch(&delta, &offset, sizeof(delta)) ||
        !WebCore::TranslateShapeGuest(self, delta))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore6Region5uniteERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    WebCore::RegionFields region;
    WebCore::RegionFields source;
    if (!WebCore::LoadRegion(region, self) || !WebCore::LoadRegion(source, &other))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (WebCore::RectIsEmpty(source.bounds))
        return;
    if (WebCore::RectIsEmpty(region.bounds)) {
        std::unique_ptr<WebCore::Region::Shape> copiedShape;
        if (source.shape) {
            WebCore::Region::Shape shape;
            if (!WebCore::LoadShapeCopy(source.shape, shape))
                NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
            copiedShape = std::make_unique<WebCore::Region::Shape>(std::move(shape));
        }
        if (!WebCore::StoreRegion(self, source.bounds, std::move(copiedShape), region.shape))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return;
    }
    if (!source.shape && WebCore::RectContains(source.bounds, region.bounds)) {
        if (!WebCore::StoreRegion(self, WebCore::RegionFields{source.bounds, nullptr}, region.shape))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return;
    }
    bool contains = false;
    if (!WebCore::RegionContains(region, source, contains))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (contains)
        return;

    WebCore::Region::Shape aShape;
    WebCore::Region::Shape bShape;
    if (!WebCore::LoadRegionShape(region, aShape) || !WebCore::LoadRegionShape(source, bShape) ||
        !WebCore::SetRegionShape(self, region, WebCore::Region::Shape::unionShapes(aShape, bShape)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore6Region8subtractERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    WebCore::RegionFields region;
    WebCore::RegionFields source;
    if (!WebCore::LoadRegion(region, self) || !WebCore::LoadRegion(source, &other))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (WebCore::RectIsEmpty(region.bounds) || WebCore::RectIsEmpty(source.bounds))
        return;
    if (!WebCore::RectIntersects(region.bounds, source.bounds))
        return;

    WebCore::Region::Shape aShape;
    WebCore::Region::Shape bShape;
    if (!WebCore::LoadRegionShape(region, aShape) || !WebCore::LoadRegionShape(source, bShape) ||
        !WebCore::SetRegionShape(self, region, WebCore::Region::Shape::subtractShapes(aShape, bShape)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore6Region9intersectERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    WebCore::RegionFields region;
    WebCore::RegionFields source;
    if (!WebCore::LoadRegion(region, self) || !WebCore::LoadRegion(source, &other))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (WebCore::RectIsEmpty(region.bounds))
        return;
    if (!WebCore::RectIntersects(region.bounds, source.bounds)) {
        if (!WebCore::StoreRegion(self, WebCore::RegionFields{WebCore::IntRect{WebCore::IntPoint{0, 0}, WebCore::IntSize{0, 0}}, nullptr}, region.shape))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return;
    }
    if (!region.shape && !source.shape) {
        if (!WebCore::StoreRegion(self, WebCore::RegionFields{WebCore::RectIntersection(region.bounds, source.bounds), nullptr}, nullptr))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return;
    }

    WebCore::Region::Shape aShape;
    WebCore::Region::Shape bShape;
    if (!WebCore::LoadRegionShape(region, aShape) || !WebCore::LoadRegionShape(source, bShape) ||
        !WebCore::SetRegionShape(self, region, WebCore::Region::Shape::intersectShapes(aShape, bShape)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore6Region9translateERKNS_7IntSizeE(WebCore::Region* self, const WebCore::IntSize& offset) {
    WebCore::RegionFields region;
    WebCore::IntSize delta;
    if (!WebCore::LoadRegion(region, self) ||
        !WebCore::IsReadableGuestBytes(&offset, sizeof(delta)) ||
        !GuestCopyIn_nid_no_patch(&delta, &offset, sizeof(delta)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const GuestArena::HostWrite selfWrite(self, sizeof(region));
    if (!selfWrite.Open() || !GuestWritable_nid_no_patch(self, sizeof(region)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (region.shape && !WebCore::TranslateShapeGuest(region.shape, delta))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    WebCore::RegionFields updated = region;
    updated.bounds.m_location.m_x += delta.m_width;
    updated.bounds.m_location.m_y += delta.m_height;
    if (!GuestCopyOut_nid_no_patch(self, &updated, sizeof(updated))) {
        if (region.shape) {
            const WebCore::IntSize inverse{-delta.m_width, -delta.m_height};
            WebCore::TranslateShapeGuest(region.shape, inverse);
        }
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
}

void APS5_VABI _ZN7WebCore6RegionC1EOS0_(WebCore::Region* self, WebCore::Region&& other) {
    WebCore::RegionFields source;
    if (!WebCore::LoadRegion(source, &other))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const GuestArena::HostWrite selfWrite(self, sizeof(source));
    const GuestArena::HostWrite otherWrite(&other, sizeof(source));
    if (!selfWrite.Open() || !otherWrite.Open() ||
        !GuestWritable_nid_no_patch(self, sizeof(source)) ||
        !GuestWritable_nid_no_patch(&other, sizeof(source)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!GuestCopyOut_nid_no_patch(self, &source, sizeof(source)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const WebCore::RegionFields clearedOther{source.bounds, nullptr};
    if (!GuestCopyOut_nid_no_patch(&other, &clearedOther, sizeof(clearedOther)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore6RegionC1ERKNS_7IntRectE(WebCore::Region* self, const WebCore::IntRect& rect) {
    WebCore::IntRect bounds;
    if (!WebCore::IsReadableGuestBytes(&rect, sizeof(bounds)) ||
        !GuestCopyIn_nid_no_patch(&bounds, &rect, sizeof(bounds)) ||
        !WebCore::StoreRegion(self, WebCore::RegionFields{bounds, nullptr}, nullptr))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore6RegionC1ERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    WebCore::RegionFields source;
    if (!WebCore::LoadRegion(source, &other))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    std::unique_ptr<WebCore::Region::Shape> copiedShape;
    if (source.shape) {
        WebCore::Region::Shape shape;
        if (!WebCore::LoadShapeCopy(source.shape, shape))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        copiedShape = std::make_unique<WebCore::Region::Shape>(std::move(shape));
    }
    if (!WebCore::StoreRegion(self, source.bounds, std::move(copiedShape), nullptr))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore6RegionC1Ev(WebCore::Region* self) {
    if (!WebCore::StoreRegion(self, WebCore::RegionFields{WebCore::IntRect{WebCore::IntPoint{0, 0}, WebCore::IntSize{0, 0}}, nullptr}, nullptr))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
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
    WebCore::RegionFields region;
    if (!WebCore::LoadRegion(region, self) || !WebCore::StoreRegion(self, WebCore::RegionFields{region.bounds, nullptr}, region.shape))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore6RegionD2Ev(WebCore::Region* self) {
    _ZN7WebCore6RegionD1Ev(self);
}

WebCore::Region& APS5_VABI _ZN7WebCore6RegionaSEOS0_(WebCore::Region* self, WebCore::Region&& other) {
    return WebCore::MoveAssignRegion(self, &other, __func__);
}

WebCore::Region& APS5_VABI _ZN7WebCore6RegionaSERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    return WebCore::CopyAssignRegion(self, &other, __func__);
}

bool APS5_VABI _ZNK7WebCore6Region10intersectsERKS0_(const WebCore::Region* self, const WebCore::Region& other) {
    WebCore::RegionFields region;
    WebCore::RegionFields source;
    if (!WebCore::LoadRegion(region, self) || !WebCore::LoadRegion(source, &other))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!WebCore::RectIntersects(region.bounds, source.bounds))
        return false;
    if (!region.shape && !source.shape)
        return true;
    WebCore::Region::Shape aShape;
    WebCore::Region::Shape bShape;
    if (!WebCore::LoadRegionShape(region, aShape) || !WebCore::LoadRegionShape(source, bShape))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return WebCore::Region::Shape::compareShapes<WebCore::Region::Shape::CompareIntersectsOperation>(aShape, bShape);
}

WTF::Vector<WebCore::IntRect, 1> APS5_VABI _ZNK7WebCore6Region5rectsEv(const WebCore::Region* self) {
    WTF::Vector<WebCore::IntRect, 1> rectList;
    if (!WebCore::ComputeRegionRects(self, rectList))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return rectList;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::Region& region) {
    return WebCore::DumpRegionToStream(&stream, &region, __func__);
}

bool APS5_VABI _ZNK7WebCore6Region8containsERKNS_8IntPointE(const WebCore::Region* self, const WebCore::IntPoint& point) {
    WebCore::RegionFields region;
    WebCore::IntPoint location;
    if (!WebCore::LoadRegion(region, self) ||
        !WebCore::IsReadableGuestBytes(&point, sizeof(location)) ||
        !GuestCopyIn_nid_no_patch(&location, &point, sizeof(location)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!WebCore::RectContains(region.bounds, location))
        return false;
    if (!region.shape)
        return true;

    WebCore::Region::Shape shape;
    if (!WebCore::LoadShapeCopy(region.shape, shape))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    for (WebCore::Region::Shape::SpanIterator span = shape.spans_begin(), end = shape.spans_end(); span != end && span + 1 != end; ++span) {
        const int y = span->y;
        const int maxY = (span + 1)->y;
        if (y > location.m_y)
            break;
        if (maxY <= location.m_y)
            continue;

        for (WebCore::Region::Shape::SegmentIterator segment = shape.segments_begin(span), segEnd = shape.segments_end(span); segment != segEnd && segment + 1 != segEnd; segment += 2) {
            const int x = *segment;
            const int maxX = *(segment + 1);
            if (x > location.m_x)
                break;
            if (maxX > location.m_x)
                return true;
        }
    }
    return false;
}

bool APS5_VABI _ZNK7WebCore6Region8containsERKS0_(const WebCore::Region* self, const WebCore::Region& other) {
    WebCore::RegionFields region;
    WebCore::RegionFields source;
    bool result = false;
    if (!WebCore::LoadRegion(region, self) || !WebCore::LoadRegion(source, &other) ||
        !WebCore::RegionContains(region, source, result))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return result;
}

std::uint64_t APS5_VABI _ZNK7WebCore6Region9totalAreaEv(const WebCore::Region* self) {
    std::uint64_t total = 0;
    WTF::Vector<WebCore::IntRect, 1> rectList;
    if (!WebCore::ComputeRegionRects(self, rectList))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    for (std::size_t i = 0; i < rectList.size(); ++i) {
        total += static_cast<std::uint64_t>(rectList[i].m_size.m_width) * static_cast<std::uint64_t>(rectList[i].m_size.m_height);
    }
    return total;
}

}
