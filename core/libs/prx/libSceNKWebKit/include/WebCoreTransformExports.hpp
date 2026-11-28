#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

WebCore::TransformOperationRef APS5_VABI _ZN7WebCore27TranslateTransformOperation6createERKNS_6LengthES3_NS_18TransformOperation13OperationTypeE(const WebCore::Length& tx, const WebCore::Length& ty, WebCore::TransformOperation::Type type);
WebCore::TransformOperationRef APS5_VABI _ZN7WebCore27TranslateTransformOperation6createERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(const WebCore::Length& tx, const WebCore::Length& ty, const WebCore::Length& tz, WebCore::TransformOperation::Type type);
void APS5_VABI _ZN7WebCore27TranslateTransformOperationC1ERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(WebCore::TranslateTransformOperation* self, const WebCore::Length& tx, const WebCore::Length& ty, const WebCore::Length& tz, WebCore::TransformOperation::Type type);
void APS5_VABI _ZN7WebCore27TranslateTransformOperationC2ERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(WebCore::TranslateTransformOperation* self, const WebCore::Length& tx, const WebCore::Length& ty, const WebCore::Length& tz, WebCore::TransformOperation::Type type);
void APS5_VABI _ZN7WebCore27TranslateTransformOperationD0Ev(WebCore::TranslateTransformOperation* self);
void APS5_VABI _ZN7WebCore27TranslateTransformOperationD1Ev(WebCore::TranslateTransformOperation* self);
void APS5_VABI _ZN7WebCore27TranslateTransformOperationD2Ev(WebCore::TranslateTransformOperation* self);
bool APS5_VABI _ZNK7WebCore27TranslateTransformOperation10isIdentityEv(const WebCore::TranslateTransformOperation* self);
bool APS5_VABI _ZNK7WebCore27TranslateTransformOperation19isRepresentableIn2DEv(const WebCore::TranslateTransformOperation* self);
float APS5_VABI _ZNK7WebCore27TranslateTransformOperation1xERKNS_9FloatSizeE(const WebCore::TranslateTransformOperation* self, const WebCore::FloatSize& borderBoxSize);
WebCore::Length APS5_VABI _ZNK7WebCore27TranslateTransformOperation1xEv(const WebCore::TranslateTransformOperation* self);
float APS5_VABI _ZNK7WebCore27TranslateTransformOperation1yERKNS_9FloatSizeE(const WebCore::TranslateTransformOperation* self, const WebCore::FloatSize& borderBoxSize);
WebCore::Length APS5_VABI _ZNK7WebCore27TranslateTransformOperation1yEv(const WebCore::TranslateTransformOperation* self);
float APS5_VABI _ZNK7WebCore27TranslateTransformOperation1zERKNS_9FloatSizeE(const WebCore::TranslateTransformOperation* self, const WebCore::FloatSize& borderBoxSize);
WebCore::Length APS5_VABI _ZNK7WebCore27TranslateTransformOperation1zEv(const WebCore::TranslateTransformOperation* self);
void APS5_VABI _ZNK7WebCore27TranslateTransformOperation4dumpERN3WTF10TextStreamE(const WebCore::TranslateTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream);
bool APS5_VABI _ZNK7WebCore27TranslateTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::TranslateTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
WebCore::TransformOperationRef APS5_VABI _ZNK7WebCore27TranslateTransformOperation5cloneEv(const WebCore::TranslateTransformOperation* self);
bool APS5_VABI _ZNK7WebCore27TranslateTransformOperationeqERKNS_18TransformOperationE(const WebCore::TranslateTransformOperation* self, const WebCore::TransformOperation& other);
WebCore::TransformOperationRef APS5_VABI _ZN7WebCore27TranslateTransformOperation5blendEPKNS_18TransformOperationEdb(WebCore::TranslateTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity);

}
