#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace WebCore {

struct TransformOperation {
    void* m_vptr;
    std::uint32_t m_refCount;
    std::uint32_t m_type;
};

struct PerspectiveTransformOperation {
    TransformOperation m_base;
    Length m_p;
};

static_assert(sizeof(TransformOperation) == 16);
static_assert(offsetof(TransformOperation, m_type) == 12);
static_assert(sizeof(PerspectiveTransformOperation) == 24);
static_assert(offsetof(PerspectiveTransformOperation, m_p) == 16);

}

extern "C" {

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv(const WebCore::PerspectiveTransformOperation* self);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation27isAffectedByTransformOriginEv(const WebCore::PerspectiveTransformOperation* self);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation19isRepresentableIn2DEv(const WebCore::PerspectiveTransformOperation* self);
WebCore::Length* APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(WebCore::Length* result, const WebCore::PerspectiveTransformOperation* self);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::PerspectiveTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperationeqERKNS_18TransformOperationE(const WebCore::PerspectiveTransformOperation* self, const WebCore::TransformOperation& other);

}
