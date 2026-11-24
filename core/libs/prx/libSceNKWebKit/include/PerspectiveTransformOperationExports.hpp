#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/LegacyLengthAbi.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace WebCore {

struct LegacyTransformOperation {
    void* m_vptr;
    std::uint32_t m_refCount;
    std::uint32_t m_type;
};

struct PerspectiveTransformOperation {
    LegacyTransformOperation m_base;
    LegacyLength m_p;
};

static_assert(sizeof(LegacyTransformOperation) == 16);
static_assert(offsetof(LegacyTransformOperation, m_type) == 12);
static_assert(sizeof(PerspectiveTransformOperation) == 24);
static_assert(offsetof(PerspectiveTransformOperation, m_p) == 16);

}

extern "C" {

WebCore::PerspectiveTransformOperation** APS5_VABI _ZN7WebCore29PerspectiveTransformOperation6createERKNS_6LengthE(WebCore::PerspectiveTransformOperation** result, const WebCore::LegacyLength* p);
void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationC1ERKNS_6LengthE(WebCore::PerspectiveTransformOperation* self, const WebCore::LegacyLength* p);
void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationC2ERKNS_6LengthE(WebCore::PerspectiveTransformOperation* self, const WebCore::LegacyLength* p);
void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD0Ev(WebCore::PerspectiveTransformOperation* self);
void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD1Ev(WebCore::PerspectiveTransformOperation* self);
void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD2Ev(WebCore::PerspectiveTransformOperation* self);
WebCore::LegacyTransformOperation** APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation5cloneEv(WebCore::LegacyTransformOperation** result, const WebCore::PerspectiveTransformOperation* self);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv(const WebCore::PerspectiveTransformOperation* self);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation27isAffectedByTransformOriginEv(const WebCore::PerspectiveTransformOperation* self);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation19isRepresentableIn2DEv(const WebCore::PerspectiveTransformOperation* self);
WebCore::LegacyLength* APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(WebCore::LegacyLength* result, const WebCore::PerspectiveTransformOperation* self);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::PerspectiveTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperationeqERKNS_18TransformOperationE(const WebCore::PerspectiveTransformOperation* self, const WebCore::LegacyTransformOperation& other);

}
