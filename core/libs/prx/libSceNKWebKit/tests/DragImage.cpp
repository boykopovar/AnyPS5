#include "prx/libcairo/include/Surface.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using NKWebKit::WebCoreAbi::DragImage;
using NKWebKit::WebCoreAbi::Image;
using NKWebKit::WebCoreAbi::Path;
using NKWebKit::WebCoreAbi::PathImpl;
using NKWebKit::WebCoreAbi::PathLineTo;
using NKWebKit::WebCoreAbi::PathRoundedRect;
using NKWebKit::WebCoreAbi::TextIndicatorData;
using NKWebKit::WebCoreAbi::TextIndicatorPresentationTransition;
using WebCore::FloatPoint;
using WebCore::FloatRect;
using WebCore::FloatSize;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "DragImage check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename Action>
static bool ThrowsNotImplemented(Action action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct SurfaceFlag {
    int destroyed{0};
};

static void APS5_VABI DestroySurfaceFlag(void* data) {
    static_cast<SurfaceFlag*>(data)->destroyed += 1;
}

static constexpr int Argb32Format = 0;
static const CairoUserDataKey SurfaceKey{0};

static CairoSurface* TrackedSurface(SurfaceFlag& flag) {
    CairoSurface* surface = cairo_image_surface_create(Argb32Format, 2, 2);
    Require(surface != nullptr);
    Require(cairo_surface_status(surface) == 0);
    Require(cairo_surface_set_user_data(surface, &SurfaceKey, &flag, DestroySurfaceFlag) == 0);
    return surface;
}

static void Construct(DragImage* image) {
    _ZN7WebCore9DragImageC2Ev(image);
}

static void ConstructDefaultStorage(unsigned char* storage) {
    std::memset(storage, 0xAB, sizeof(DragImage));
}

static void FillIndicatorData(TextIndicatorData& data) {
    data.selectionRectInRootViewCoordinates = FloatRect{FloatPoint{1.0f, 2.0f}, FloatSize{3.0f, 4.0f}};
    data.textBoundingRectInRootViewCoordinates = FloatRect{FloatPoint{5.0f, 6.0f}, FloatSize{7.0f, 8.0f}};
    data.contentImageWithoutSelectionRectInRootViewCoordinates = FloatRect{FloatPoint{9.0f, 10.0f}, FloatSize{11.0f, 12.0f}};
    data.textRectsInBoundingRectCoordinates.append(FloatRect{FloatPoint{13.0f, 14.0f}, FloatSize{15.0f, 16.0f}});
    data.textRectsInBoundingRectCoordinates.append(FloatRect{FloatPoint{17.0f, 18.0f}, FloatSize{19.0f, 20.0f}});
    data.contentImageScaleFactor = 2.5f;
    data.estimatedBackgroundColor.m_colorAndFlags = 0x123456789ABCDEF0;
    data.presentationTransition = TextIndicatorPresentationTransition::FadeIn;
    data.options.storage = 0x0007;
}

static void DefaultConstructsEmpty() {
    for (auto construct : {&_ZN7WebCore9DragImageC1Ev, &_ZN7WebCore9DragImageC2Ev}) {
        alignas(DragImage) unsigned char storage[sizeof(DragImage)];
        ConstructDefaultStorage(storage);
        auto* image = reinterpret_cast<DragImage*>(storage);
        construct(image);
        Require(image->m_dragImageRef.ptr == nullptr);
        Require(!image->m_indicatorData.engaged);
        Require(!image->m_visiblePath.engaged);
        _ZN7WebCore9DragImageD2Ev(image);
    }
    alignas(DragImage) unsigned char storage[sizeof(DragImage)];
    ConstructDefaultStorage(storage);
    auto* image = reinterpret_cast<DragImage*>(storage);
    Construct(image);
    _ZN7WebCore9DragImageD1Ev(image);
    Require(ThrowsInvalidArgument([] { _ZN7WebCore9DragImageC1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore9DragImageC2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore9DragImageD1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore9DragImageD2Ev(nullptr); }));
}

static void MovesEmptyState() {
    for (auto moveConstruct : {&_ZN7WebCore9DragImageC1EOS0_, &_ZN7WebCore9DragImageC2EOS0_}) {
        alignas(DragImage) unsigned char storage[sizeof(DragImage)];
        ConstructDefaultStorage(storage);
        auto* target = reinterpret_cast<DragImage*>(storage);
        DragImage source;
        Construct(&source);
        moveConstruct(target, &source);
        Require(target->m_dragImageRef.ptr == nullptr);
        Require(!target->m_indicatorData.engaged && !target->m_visiblePath.engaged);
        Require(!source.m_indicatorData.engaged && !source.m_visiblePath.engaged);
        _ZN7WebCore9DragImageD2Ev(target);
        _ZN7WebCore9DragImageD2Ev(&source);
    }
    DragImage target;
    DragImage source;
    Construct(&target);
    Construct(&source);
    Require(&_ZN7WebCore9DragImageaSEOS0_(&target, &source) == &target);
    Require(target.m_dragImageRef.ptr == nullptr && !target.m_indicatorData.engaged && !target.m_visiblePath.engaged);
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore9DragImageC1EOS0_(nullptr, &source); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore9DragImageC2EOS0_(nullptr, &source); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore9DragImageC1EOS0_(&target, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore9DragImageC2EOS0_(&target, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore9DragImageaSEOS0_(nullptr, &source); }));
    _ZN7WebCore9DragImageD2Ev(&target);
    _ZN7WebCore9DragImageD2Ev(&source);
}

static void MovesSurfaceBetweenImages() {
    SurfaceFlag flag;
    CairoSurface* surface = TrackedSurface(flag);
    cairo_surface_reference(surface);
    DragImage source;
    DragImage target;
    Construct(&source);
    Construct(&target);
    source.m_dragImageRef.ptr = surface;
    _ZN7WebCore9DragImageC1EOS0_(&target, &source);
    Require(target.m_dragImageRef.ptr == surface);
    Require(source.m_dragImageRef.ptr == nullptr);
    _ZN7WebCore9DragImageD2Ev(&target);
    Require(flag.destroyed == 0);
    _ZN7WebCore9DragImageD2Ev(&source);
    cairo_surface_destroy(surface);
    Require(flag.destroyed == 1);

    SurfaceFlag assignedFlag;
    CairoSurface* assigned = TrackedSurface(assignedFlag);
    cairo_surface_reference(assigned);
    DragImage empty;
    DragImage holder;
    Construct(&empty);
    Construct(&holder);
    holder.m_dragImageRef.ptr = assigned;
    Require(&_ZN7WebCore9DragImageaSEOS0_(&empty, &holder) == &empty);
    Require(empty.m_dragImageRef.ptr == assigned && holder.m_dragImageRef.ptr == nullptr);
    _ZN7WebCore9DragImageD2Ev(&empty);
    Require(assignedFlag.destroyed == 0);
    _ZN7WebCore9DragImageD2Ev(&holder);
    cairo_surface_destroy(assigned);
    Require(assignedFlag.destroyed == 1);

    SurfaceFlag oldFlag;
    SurfaceFlag replacementFlag;
    CairoSurface* old = TrackedSurface(oldFlag);
    CairoSurface* replacement = TrackedSurface(replacementFlag);
    cairo_surface_reference(replacement);
    DragImage occupied;
    DragImage donor;
    Construct(&occupied);
    Construct(&donor);
    occupied.m_dragImageRef.ptr = old;
    donor.m_dragImageRef.ptr = replacement;
    _ZN7WebCore9DragImageaSEOS0_(&occupied, &donor);
    Require(occupied.m_dragImageRef.ptr == replacement && donor.m_dragImageRef.ptr == nullptr);
    Require(oldFlag.destroyed == 1);
    Require(replacementFlag.destroyed == 0);
    _ZN7WebCore9DragImageD2Ev(&occupied);
    Require(replacementFlag.destroyed == 0);
    _ZN7WebCore9DragImageD2Ev(&donor);
    cairo_surface_destroy(replacement);
    Require(replacementFlag.destroyed == 1);

    SurfaceFlag selfFlag;
    CairoSurface* self = TrackedSurface(selfFlag);
    DragImage selfTarget;
    Construct(&selfTarget);
    selfTarget.m_dragImageRef.ptr = self;
    Require(&_ZN7WebCore9DragImageaSEOS0_(&selfTarget, &selfTarget) == &selfTarget);
    Require(selfTarget.m_dragImageRef.ptr == self);
    Require(selfFlag.destroyed == 0);
    _ZN7WebCore9DragImageD2Ev(&selfTarget);
    cairo_surface_destroy(self);
    Require(selfFlag.destroyed == 1);
}

static void MovesIndicatorData() {
    DragImage source;
    Construct(&source);
    source.m_indicatorData.engaged = true;
    FillIndicatorData(source.m_indicatorData.value);
    const FloatRect* sourceBuffer = source.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer;
    const unsigned sourceCapacity = source.m_indicatorData.value.textRectsInBoundingRectCoordinates.capacity;
    DragImage target;
    Construct(&target);
    _ZN7WebCore9DragImageC1EOS0_(&target, &source);
    Require(target.m_dragImageRef.ptr == nullptr);
    Require(target.m_indicatorData.engaged && source.m_indicatorData.engaged);
    const auto& movedRects = target.m_indicatorData.value.textRectsInBoundingRectCoordinates;
    Require(movedRects.buffer == sourceBuffer && movedRects.size == 2 && movedRects.capacity == sourceCapacity);
    Require(movedRects.buffer[0].x() == 13.0f && movedRects.buffer[0].y() == 14.0f);
    Require(movedRects.buffer[1].width() == 19.0f && movedRects.buffer[1].height() == 20.0f);
    Require(target.m_indicatorData.value.selectionRectInRootViewCoordinates.x() == 1.0f);
    Require(target.m_indicatorData.value.contentImageScaleFactor == 2.5f);
    Require(target.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == 0x123456789ABCDEF0);
    Require(target.m_indicatorData.value.presentationTransition == TextIndicatorPresentationTransition::FadeIn);
    Require(target.m_indicatorData.value.options.storage == 0x0007);
    const auto& emptiedRects = source.m_indicatorData.value.textRectsInBoundingRectCoordinates;
    Require(emptiedRects.buffer == nullptr && emptiedRects.size == 0 && emptiedRects.capacity == 0);
    Require(source.m_indicatorData.value.contentImageScaleFactor == 2.5f);
    Require(source.m_indicatorData.value.presentationTransition == TextIndicatorPresentationTransition::FadeIn);
    _ZN7WebCore9DragImageD2Ev(&source);
    _ZN7WebCore9DragImageD2Ev(&target);

    DragImage from;
    DragImage onto;
    Construct(&from);
    Construct(&onto);
    from.m_indicatorData.engaged = true;
    FillIndicatorData(from.m_indicatorData.value);
    const FloatRect* fromBuffer = from.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer;
    onto.m_indicatorData.engaged = true;
    onto.m_indicatorData.value.textRectsInBoundingRectCoordinates.append(FloatRect{FloatPoint{1.0f, 1.0f}, FloatSize{1.0f, 1.0f}});
    _ZN7WebCore9DragImageaSEOS0_(&onto, &from);
    Require(onto.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer == fromBuffer);
    Require(onto.m_indicatorData.value.textRectsInBoundingRectCoordinates.size == 2);
    Require(from.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer == nullptr);
    Require(from.m_indicatorData.value.textRectsInBoundingRectCoordinates.size == 0);
    _ZN7WebCore9DragImageD2Ev(&from);
    _ZN7WebCore9DragImageD2Ev(&onto);

    DragImage resetTarget;
    DragImage resetSource;
    Construct(&resetTarget);
    Construct(&resetSource);
    resetTarget.m_indicatorData.engaged = true;
    FillIndicatorData(resetTarget.m_indicatorData.value);
    _ZN7WebCore9DragImageaSEOS0_(&resetTarget, &resetSource);
    Require(!resetTarget.m_indicatorData.engaged);
    Require(resetTarget.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer == nullptr);
    Require(!resetSource.m_indicatorData.engaged);
    _ZN7WebCore9DragImageD2Ev(&resetTarget);
    _ZN7WebCore9DragImageD2Ev(&resetSource);
}

static void MovesVisiblePath() {
    DragImage source;
    Construct(&source);
    source.m_visiblePath.engaged = true;
    source.m_visiblePath.value.index = 1;
    source.m_visiblePath.value.segment.lineTo = PathLineTo{FloatPoint{3.0f, 4.0f}};
    DragImage target;
    Construct(&target);
    _ZN7WebCore9DragImageC1EOS0_(&target, &source);
    Require(target.m_visiblePath.engaged);
    Require(target.m_visiblePath.value.index == 1);
    Require(target.m_visiblePath.value.segment.lineTo.point.m_x == 3.0f);
    Require(target.m_visiblePath.value.segment.lineTo.point.m_y == 4.0f);
    Require(source.m_visiblePath.value.index == 1);
    Require(source.m_visiblePath.value.segment.lineTo.point.m_x == 3.0f);
    _ZN7WebCore9DragImageD2Ev(&source);
    _ZN7WebCore9DragImageD2Ev(&target);

    DragImage transferred;
    DragImage recipient;
    Construct(&transferred);
    Construct(&recipient);
    transferred.m_visiblePath.engaged = true;
    transferred.m_visiblePath.value.index = 2;
    transferred.m_visiblePath.value.impl = nullptr;
    _ZN7WebCore9DragImageaSEOS0_(&recipient, &transferred);
    Require(recipient.m_visiblePath.engaged && recipient.m_visiblePath.value.index == 2);
    Require(recipient.m_visiblePath.value.impl == nullptr);
    Require(transferred.m_visiblePath.engaged && transferred.m_visiblePath.value.impl == nullptr);
    _ZN7WebCore9DragImageD2Ev(&transferred);
    _ZN7WebCore9DragImageD2Ev(&recipient);
}

static void DestructsFullyPopulatedReproducibleState() {
    for (auto destruct : {&_ZN7WebCore9DragImageD1Ev, &_ZN7WebCore9DragImageD2Ev}) {
        SurfaceFlag flag;
        CairoSurface* surface = TrackedSurface(flag);
        DragImage image;
        Construct(&image);
        image.m_dragImageRef.ptr = surface;
        image.m_indicatorData.engaged = true;
        FillIndicatorData(image.m_indicatorData.value);
        image.m_visiblePath.engaged = true;
        image.m_visiblePath.value.index = 1;
        image.m_visiblePath.value.segment.roundedRect.roundedRect.m_rect = FloatRect{FloatPoint{1.0f, 1.0f}, FloatSize{2.0f, 2.0f}};
        image.m_visiblePath.value.segment.roundedRect.strategy = PathRoundedRect::Strategy::PreferBezier;
        destruct(&image);
        Require(flag.destroyed == 1);
    }
    DragImage nullPath;
    Construct(&nullPath);
    nullPath.m_visiblePath.engaged = true;
    nullPath.m_visiblePath.value.index = 2;
    nullPath.m_visiblePath.value.impl = nullptr;
    _ZN7WebCore9DragImageD1Ev(&nullPath);
}

static void DtorThrowsForUnmodeledIndicatorImage() {
    for (auto destruct : {&_ZN7WebCore9DragImageD1Ev, &_ZN7WebCore9DragImageD2Ev}) {
        DragImage image;
        Construct(&image);
        image.m_indicatorData.engaged = true;
        image.m_indicatorData.value.contentImage.ptr = reinterpret_cast<Image*>(1);
        Require(ThrowsNotImplemented([&] { destruct(&image); }));
        _ZN3WTF8fastFreeEPv(image.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer);
    }
    DragImage highlight;
    Construct(&highlight);
    highlight.m_indicatorData.engaged = true;
    highlight.m_indicatorData.value.contentImageWithHighlight.ptr = reinterpret_cast<Image*>(1);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore9DragImageD2Ev(&highlight); }));
    _ZN3WTF8fastFreeEPv(highlight.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer);
    DragImage withoutSelection;
    Construct(&withoutSelection);
    withoutSelection.m_indicatorData.engaged = true;
    withoutSelection.m_indicatorData.value.contentImageWithoutSelection.ptr = reinterpret_cast<Image*>(1);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore9DragImageD2Ev(&withoutSelection); }));
    _ZN3WTF8fastFreeEPv(withoutSelection.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer);
}

static void MoveConstructAdoptsUnmodeledIndicatorImage() {
    DragImage source;
    Construct(&source);
    source.m_indicatorData.engaged = true;
    FillIndicatorData(source.m_indicatorData.value);
    source.m_indicatorData.value.contentImage.ptr = reinterpret_cast<Image*>(1);
    DragImage target;
    Construct(&target);
    _ZN7WebCore9DragImageC1EOS0_(&target, &source);
    Require(target.m_indicatorData.engaged);
    Require(target.m_indicatorData.value.contentImage.ptr == reinterpret_cast<Image*>(1));
    Require(source.m_indicatorData.engaged);
    Require(source.m_indicatorData.value.contentImage.ptr == nullptr);
    _ZN3WTF8fastFreeEPv(target.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer);
    target.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer = nullptr;
    _ZN7WebCore9DragImageD2Ev(&source);
}

static void MoveAssignThrowsForUnmodeledPathImpl() {
    DragImage target;
    DragImage source;
    Construct(&target);
    Construct(&source);
    target.m_visiblePath.engaged = true;
    target.m_visiblePath.value.index = 2;
    target.m_visiblePath.value.impl = reinterpret_cast<PathImpl*>(1);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore9DragImageaSEOS0_(&target, &source); }));
    Require(target.m_visiblePath.engaged && target.m_visiblePath.value.impl == reinterpret_cast<PathImpl*>(1));
    Require(!source.m_visiblePath.engaged);
    _ZN7WebCore9DragImageD2Ev(&source);
}

static void DtorThrowsForUnmodeledPathImpl() {
    DragImage image;
    Construct(&image);
    image.m_visiblePath.engaged = true;
    image.m_visiblePath.value.index = 2;
    image.m_visiblePath.value.impl = reinterpret_cast<PathImpl*>(1);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore9DragImageD2Ev(&image); }));
}

static void MoveAssignReplacesVisiblePathIndex() {
    DragImage segmentHolder;
    DragImage implHolder;
    Construct(&segmentHolder);
    Construct(&implHolder);
    segmentHolder.m_visiblePath.engaged = true;
    segmentHolder.m_visiblePath.value.index = 1;
    segmentHolder.m_visiblePath.value.segment.lineTo = PathLineTo{FloatPoint{5.0f, 6.0f}};
    implHolder.m_visiblePath.engaged = true;
    implHolder.m_visiblePath.value.index = 2;
    implHolder.m_visiblePath.value.impl = nullptr;
    Require(&_ZN7WebCore9DragImageaSEOS0_(&segmentHolder, &implHolder) == &segmentHolder);
    Require(segmentHolder.m_visiblePath.engaged && segmentHolder.m_visiblePath.value.index == 2);
    Require(segmentHolder.m_visiblePath.value.impl == nullptr);
    Require(implHolder.m_visiblePath.engaged && implHolder.m_visiblePath.value.impl == nullptr);
    DragImage replacement;
    Construct(&replacement);
    replacement.m_visiblePath.engaged = true;
    replacement.m_visiblePath.value.index = 1;
    replacement.m_visiblePath.value.segment.lineTo = PathLineTo{FloatPoint{7.0f, 8.0f}};
    _ZN7WebCore9DragImageaSEOS0_(&implHolder, &replacement);
    Require(implHolder.m_visiblePath.value.index == 1);
    Require(implHolder.m_visiblePath.value.segment.lineTo.point.m_x == 7.0f);
    Require(implHolder.m_visiblePath.value.segment.lineTo.point.m_y == 8.0f);
    _ZN7WebCore9DragImageaSEOS0_(&segmentHolder, &replacement);
    Require(segmentHolder.m_visiblePath.value.index == 1);
    Require(segmentHolder.m_visiblePath.value.segment.lineTo.point.m_x == 7.0f);
    Require(segmentHolder.m_visiblePath.value.segment.lineTo.point.m_y == 8.0f);
    Require(replacement.m_visiblePath.engaged && replacement.m_visiblePath.value.index == 1);
    _ZN7WebCore9DragImageD2Ev(&segmentHolder);
    _ZN7WebCore9DragImageD2Ev(&implHolder);
    _ZN7WebCore9DragImageD2Ev(&replacement);
}

static void SelfMoveAssignKeepsPopulatedState() {
    SurfaceFlag flag;
    CairoSurface* surface = TrackedSurface(flag);
    DragImage image;
    Construct(&image);
    image.m_dragImageRef.ptr = surface;
    image.m_indicatorData.engaged = true;
    FillIndicatorData(image.m_indicatorData.value);
    const FloatRect* buffer = image.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer;
    image.m_visiblePath.engaged = true;
    image.m_visiblePath.value.index = 1;
    image.m_visiblePath.value.segment.lineTo = PathLineTo{FloatPoint{7.0f, 8.0f}};
    Require(&_ZN7WebCore9DragImageaSEOS0_(&image, &image) == &image);
    Require(image.m_dragImageRef.ptr == surface);
    Require(image.m_indicatorData.engaged);
    Require(image.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer == buffer);
    Require(image.m_indicatorData.value.textRectsInBoundingRectCoordinates.size == 2);
    Require(image.m_visiblePath.engaged && image.m_visiblePath.value.index == 1);
    Require(image.m_visiblePath.value.segment.lineTo.point.m_x == 7.0f);
    _ZN7WebCore9DragImageD2Ev(&image);
    Require(flag.destroyed == 1);
}

static void MoveAssignThrowsAtomically() {
    SurfaceFlag flag;
    CairoSurface* surface = TrackedSurface(flag);
    DragImage target;
    DragImage source;
    Construct(&target);
    Construct(&source);
    target.m_dragImageRef.ptr = surface;
    target.m_indicatorData.engaged = true;
    FillIndicatorData(target.m_indicatorData.value);
    target.m_indicatorData.value.contentImage.ptr = reinterpret_cast<Image*>(1);
    const FloatRect* bufferBefore = target.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer;
    Require(ThrowsNotImplemented([&] { _ZN7WebCore9DragImageaSEOS0_(&target, &source); }));
    Require(flag.destroyed == 0);
    Require(target.m_dragImageRef.ptr == surface);
    Require(target.m_indicatorData.engaged);
    Require(target.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer == bufferBefore);
    Require(target.m_indicatorData.value.contentImage.ptr == reinterpret_cast<Image*>(1));
    _ZN3WTF8fastFreeEPv(target.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer);
    target.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer = nullptr;
    target.m_indicatorData.engaged = false;
    _ZN7WebCore9DragImageD2Ev(&target);
    _ZN7WebCore9DragImageD2Ev(&source);
    Require(flag.destroyed == 1);
}

static void MovesEstimatedBackgroundColor() {
    DragImage source;
    DragImage target;
    Construct(&source);
    Construct(&target);
    source.m_indicatorData.engaged = true;
    FillIndicatorData(source.m_indicatorData.value);
    _ZN7WebCore9DragImageC1EOS0_(&target, &source);
    Require(target.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == 0x123456789ABCDEF0);
    Require(source.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == 0);
    _ZN3WTF8fastFreeEPv(target.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer);
    target.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer = nullptr;
    _ZN7WebCore9DragImageD2Ev(&source);
    _ZN7WebCore9DragImageD2Ev(&target);

    DragImage equalTarget;
    DragImage equalSource;
    Construct(&equalTarget);
    Construct(&equalSource);
    equalTarget.m_indicatorData.engaged = true;
    FillIndicatorData(equalTarget.m_indicatorData.value);
    equalSource.m_indicatorData.engaged = true;
    FillIndicatorData(equalSource.m_indicatorData.value);
    _ZN7WebCore9DragImageaSEOS0_(&equalTarget, &equalSource);
    Require(equalTarget.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == 0x123456789ABCDEF0);
    Require(equalSource.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == 0x123456789ABCDEF0);
    _ZN3WTF8fastFreeEPv(equalTarget.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer);
    equalTarget.m_indicatorData.value.textRectsInBoundingRectCoordinates.buffer = nullptr;
    _ZN7WebCore9DragImageD2Ev(&equalTarget);
    _ZN7WebCore9DragImageD2Ev(&equalSource);

    DragImage movedTarget;
    DragImage movedSource;
    Construct(&movedTarget);
    Construct(&movedSource);
    movedTarget.m_indicatorData.engaged = true;
    movedTarget.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags = 0x1;
    movedSource.m_indicatorData.engaged = true;
    movedSource.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags = 0x2;
    _ZN7WebCore9DragImageaSEOS0_(&movedTarget, &movedSource);
    Require(movedTarget.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == 0x2);
    Require(movedSource.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == 0);
    _ZN7WebCore9DragImageD2Ev(&movedTarget);
    _ZN7WebCore9DragImageD2Ev(&movedSource);
}

static void ThrowsForOutOfLineColor() {
    constexpr std::uint64_t outOfLineColor = 1ULL << 51;
    for (auto destruct : {&_ZN7WebCore9DragImageD1Ev, &_ZN7WebCore9DragImageD2Ev}) {
        DragImage image;
        Construct(&image);
        image.m_indicatorData.engaged = true;
        image.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags = outOfLineColor;
        Require(ThrowsNotImplemented([&] { destruct(&image); }));
        image.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags = 0;
        _ZN7WebCore9DragImageD2Ev(&image);
    }
    DragImage target;
    DragImage source;
    Construct(&target);
    Construct(&source);
    target.m_indicatorData.engaged = true;
    target.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags = outOfLineColor;
    Require(ThrowsNotImplemented([&] { _ZN7WebCore9DragImageaSEOS0_(&target, &source); }));
    Require(target.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == outOfLineColor);
    target.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags = 0;
    _ZN7WebCore9DragImageD2Ev(&target);
    _ZN7WebCore9DragImageD2Ev(&source);

    DragImage movedFrom;
    DragImage movedOnto;
    Construct(&movedFrom);
    Construct(&movedOnto);
    movedFrom.m_indicatorData.engaged = true;
    movedFrom.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags = outOfLineColor;
    _ZN7WebCore9DragImageC1EOS0_(&movedOnto, &movedFrom);
    Require(movedOnto.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == outOfLineColor);
    Require(movedFrom.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags == 0);
    movedOnto.m_indicatorData.value.estimatedBackgroundColor.m_colorAndFlags = 0;
    _ZN7WebCore9DragImageD2Ev(&movedFrom);
    _ZN7WebCore9DragImageD2Ev(&movedOnto);
}

int main() {
    DefaultConstructsEmpty();
    MovesEmptyState();
    MovesSurfaceBetweenImages();
    MovesIndicatorData();
    MovesEstimatedBackgroundColor();
    ThrowsForOutOfLineColor();
    MovesVisiblePath();
    DestructsFullyPopulatedReproducibleState();
    DtorThrowsForUnmodeledIndicatorImage();
    MoveConstructAdoptsUnmodeledIndicatorImage();
    DtorThrowsForUnmodeledPathImpl();
    MoveAssignThrowsForUnmodeledPathImpl();
    MoveAssignReplacesVisiblePathIndex();
    SelfMoveAssignKeepsPopulatedState();
    MoveAssignThrowsAtomically();
    std::puts("DragImage passed");
    return 0;
}
