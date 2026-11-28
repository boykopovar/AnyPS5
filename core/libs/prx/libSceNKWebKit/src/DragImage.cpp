#include "prx/libcairo/include/Surface.hpp"
#include "prx/libSceNKWebKit/include/DragImageExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::DragImage;
using NKWebKit::WebCoreAbi::Optional;
using NKWebKit::WebCoreAbi::Path;
using NKWebKit::WebCoreAbi::TextIndicatorData;

DragImage* Checked(DragImage* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::DragImage::") + operation + ": null this");
    return self;
}

DragImage& Source(DragImage* other, const char* operation) {
    if (!other) throw std::invalid_argument(std::string("WebCore::DragImage::") + operation + ": null source");
    return *other;
}

bool ColorIsOutOfLine(const WebCore::Color& color) {
    constexpr std::uint64_t outOfLineFlag = 1ULL << 51;
    return (color.m_colorAndFlags & outOfLineFlag) != 0;
}

void MoveAssignColor(WebCore::Color& to, WebCore::Color& from) {
    if (to.m_colorAndFlags == from.m_colorAndFlags) {
        return;
    }
    to.m_colorAndFlags = from.m_colorAndFlags;
    from.m_colorAndFlags = 0;
}

bool IndicatorDataDestructible(const TextIndicatorData& data) {
    return !data.contentImageWithHighlight.ptr && !data.contentImageWithoutSelection.ptr && !data.contentImage.ptr
        && !ColorIsOutOfLine(data.estimatedBackgroundColor);
}

bool PathDestructible(const Path& path) {
    return path.index != 2 || path.impl == nullptr;
}

void AdoptIndicatorData(TextIndicatorData& to, TextIndicatorData& from) {
    to.selectionRectInRootViewCoordinates = from.selectionRectInRootViewCoordinates;
    to.textBoundingRectInRootViewCoordinates = from.textBoundingRectInRootViewCoordinates;
    to.contentImageWithoutSelectionRectInRootViewCoordinates = from.contentImageWithoutSelectionRectInRootViewCoordinates;
    to.textRectsInBoundingRectCoordinates.buffer = std::exchange(from.textRectsInBoundingRectCoordinates.buffer, nullptr);
    to.textRectsInBoundingRectCoordinates.capacity = std::exchange(from.textRectsInBoundingRectCoordinates.capacity, 0);
    to.textRectsInBoundingRectCoordinates.size = std::exchange(from.textRectsInBoundingRectCoordinates.size, 0);
    to.contentImageScaleFactor = from.contentImageScaleFactor;
    to.contentImageWithHighlight.ptr = std::exchange(from.contentImageWithHighlight.ptr, nullptr);
    to.contentImageWithoutSelection.ptr = std::exchange(from.contentImageWithoutSelection.ptr, nullptr);
    to.contentImage.ptr = std::exchange(from.contentImage.ptr, nullptr);
    MoveAssignColor(to.estimatedBackgroundColor, from.estimatedBackgroundColor);
    to.presentationTransition = from.presentationTransition;
    to.options = from.options;
}

void AssignIndicatorData(TextIndicatorData& to, TextIndicatorData& from) {
    _ZN3WTF8fastFreeEPv(to.textRectsInBoundingRectCoordinates.buffer);
    AdoptIndicatorData(to, from);
}

void DestroyIndicatorData(TextIndicatorData& data) {
    data.textRectsInBoundingRectCoordinates.clear();
}

void ConstructOptionalIndicator(Optional<TextIndicatorData>& to, Optional<TextIndicatorData>& from) {
    std::memset(&to.engaged, 0, sizeof(Optional<TextIndicatorData>) - offsetof(Optional<TextIndicatorData>, engaged));
    if (from.engaged) {
        AdoptIndicatorData(to.value, from.value);
        to.engaged = true;
    }
}

void AssignOptionalIndicator(Optional<TextIndicatorData>& to, Optional<TextIndicatorData>& from) {
    if (to.engaged == from.engaged) {
        if (to.engaged) AssignIndicatorData(to.value, from.value);
    } else if (from.engaged) {
        ConstructOptionalIndicator(to, from);
    } else {
        to.engaged = false;
        DestroyIndicatorData(to.value);
    }
}

void ConstructOptionalPath(Optional<Path>& to, Optional<Path>& from) {
    std::memset(&to.engaged, 0, sizeof(Optional<Path>) - offsetof(Optional<Path>, engaged));
    if (from.engaged) {
        if (from.value.index == 2) {
            to.value.impl = std::exchange(from.value.impl, nullptr);
        } else if (from.value.index == 1) {
            to.value.segment = from.value.segment;
        }
        to.value.index = from.value.index;
        to.engaged = true;
    }
}

void AssignOptionalPath(Optional<Path>& to, Optional<Path>& from) {
    if (to.engaged == from.engaged) {
        if (!to.engaged) {
            return;
        }
        if (to.value.index == from.value.index) {
            if (from.value.index == 1) {
                to.value.segment = from.value.segment;
            } else if (from.value.index == 2) {
                to.value.impl = std::exchange(from.value.impl, nullptr);
            }
            return;
        }
        ConstructOptionalPath(to, from);
        return;
    }
    if (from.engaged) {
        ConstructOptionalPath(to, from);
    } else {
        to.engaged = false;
    }
}

void ConstructDefault(DragImage* self) {
    auto* image = Checked(self, "DragImage");
    new (image) DragImage();
}

void MoveConstruct(DragImage* self, DragImage* other) {
    auto* target = Checked(self, "DragImage");
    auto& source = Source(other, "DragImage");
    new (target) DragImage();
    target->m_dragImageRef.ptr = std::exchange(source.m_dragImageRef.ptr, nullptr);
    ConstructOptionalIndicator(target->m_indicatorData, source.m_indicatorData);
    ConstructOptionalPath(target->m_visiblePath, source.m_visiblePath);
}

void Destruct(DragImage& image) {
    if (image.m_visiblePath.engaged) {
        image.m_visiblePath.engaged = false;
    }
    if (image.m_indicatorData.engaged) {
        image.m_indicatorData.engaged = false;
        DestroyIndicatorData(image.m_indicatorData.value);
    }
    if (image.m_dragImageRef.ptr) {
        CairoSurfaceDestroy_nid_no_patch(image.m_dragImageRef.ptr);
        image.m_dragImageRef.ptr = nullptr;
    }
}

DragImage& MoveAssignInto(DragImage& target, DragImage& source) {
    auto* adopted = std::exchange(source.m_dragImageRef.ptr, nullptr);
    if (auto* previous = std::exchange(target.m_dragImageRef.ptr, adopted)) {
        CairoSurfaceDestroy_nid_no_patch(previous);
    }
    AssignOptionalIndicator(target.m_indicatorData, source.m_indicatorData);
    AssignOptionalPath(target.m_visiblePath, source.m_visiblePath);
    return target;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore9DragImageC1EOS0_(DragImage* self, DragImage* other) {
    MoveConstruct(self, other);
}

void APS5_VABI _ZN7WebCore9DragImageC1Ev(DragImage* self) {
    ConstructDefault(self);
}

void APS5_VABI _ZN7WebCore9DragImageC2EOS0_(DragImage* self, DragImage* other) {
    MoveConstruct(self, other);
}

void APS5_VABI _ZN7WebCore9DragImageC2Ev(DragImage* self) {
    ConstructDefault(self);
}

void APS5_VABI _ZN7WebCore9DragImageD1Ev(DragImage* self) {
    auto& image = *Checked(self, "~DragImage");
    if (image.m_indicatorData.engaged && !IndicatorDataDestructible(image.m_indicatorData.value)) {
        NotImplemented_nid_no_patch(__func__);
    }
    if (image.m_visiblePath.engaged && !PathDestructible(image.m_visiblePath.value)) {
        NotImplemented_nid_no_patch(__func__);
    }
    Destruct(image);
}

void APS5_VABI _ZN7WebCore9DragImageD2Ev(DragImage* self) {
    auto& image = *Checked(self, "~DragImage");
    if (image.m_indicatorData.engaged && !IndicatorDataDestructible(image.m_indicatorData.value)) {
        NotImplemented_nid_no_patch(__func__);
    }
    if (image.m_visiblePath.engaged && !PathDestructible(image.m_visiblePath.value)) {
        NotImplemented_nid_no_patch(__func__);
    }
    Destruct(image);
}

DragImage& APS5_VABI _ZN7WebCore9DragImageaSEOS0_(DragImage* self, DragImage* other) {
    auto& target = *Checked(self, "operator=");
    auto& source = Source(other, "operator=");
    if (&target == &source) {
        return target;
    }
    if (target.m_indicatorData.engaged && !IndicatorDataDestructible(target.m_indicatorData.value)) {
        NotImplemented_nid_no_patch(__func__);
    }
    if (target.m_visiblePath.engaged && !PathDestructible(target.m_visiblePath.value)) {
        NotImplemented_nid_no_patch(__func__);
    }
    return MoveAssignInto(target, source);
}

}
