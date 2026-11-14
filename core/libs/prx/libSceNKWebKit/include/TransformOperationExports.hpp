#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/TransformOperationAbi.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

extern "C" {

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore22SkewTransformOperation5blendEPKNS_18TransformOperationEdb(WebCore::SkewTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(double angleX, double angleY, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore22SkewTransformOperationC1EddNS_18TransformOperation13OperationTypeE(WebCore::SkewTransformOperation* self, double angleX, double angleY, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore22SkewTransformOperationC2EddNS_18TransformOperation13OperationTypeE(WebCore::SkewTransformOperation* self, double angleX, double angleY, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore22SkewTransformOperationD0Ev(WebCore::SkewTransformOperation* self);
void APS5_VABI _ZN7WebCore22SkewTransformOperationD1Ev(WebCore::SkewTransformOperation* self);
void APS5_VABI _ZN7WebCore22SkewTransformOperationD2Ev(WebCore::SkewTransformOperation* self);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore23ScaleTransformOperation5blendEPKNS_18TransformOperationEdb(WebCore::ScaleTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore23ScaleTransformOperation6createEddNS_18TransformOperation13OperationTypeE(double sx, double sy, WebCore::TransformOperation::OperationType type);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(double sx, double sy, double sz, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore23ScaleTransformOperationC1EdddNS_18TransformOperation13OperationTypeE(WebCore::ScaleTransformOperation* self, double sx, double sy, double sz, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore23ScaleTransformOperationC2EdddNS_18TransformOperation13OperationTypeE(WebCore::ScaleTransformOperation* self, double sx, double sy, double sz, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore23ScaleTransformOperationD0Ev(WebCore::ScaleTransformOperation* self);
void APS5_VABI _ZN7WebCore23ScaleTransformOperationD1Ev(WebCore::ScaleTransformOperation* self);
void APS5_VABI _ZN7WebCore23ScaleTransformOperationD2Ev(WebCore::ScaleTransformOperation* self);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24MatrixTransformOperation5blendEPKNS_18TransformOperationEdb(WebCore::MatrixTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24MatrixTransformOperation6createERKNS_20TransformationMatrixE(const WebCore::TransformationMatrix& matrix);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24MatrixTransformOperation6createEdddddd(double a, double b, double c, double d, double e, double f);
void APS5_VABI _ZN7WebCore24MatrixTransformOperationC1ERKNS_20TransformationMatrixE(WebCore::MatrixTransformOperation* self, const WebCore::TransformationMatrix& matrix);
void APS5_VABI _ZN7WebCore24MatrixTransformOperationC1Edddddd(WebCore::MatrixTransformOperation* self, double a, double b, double c, double d, double e, double f);
void APS5_VABI _ZN7WebCore24MatrixTransformOperationC2ERKNS_20TransformationMatrixE(WebCore::MatrixTransformOperation* self, const WebCore::TransformationMatrix& matrix);
void APS5_VABI _ZN7WebCore24MatrixTransformOperationC2Edddddd(WebCore::MatrixTransformOperation* self, double a, double b, double c, double d, double e, double f);
void APS5_VABI _ZN7WebCore24MatrixTransformOperationD0Ev(WebCore::MatrixTransformOperation* self);
void APS5_VABI _ZN7WebCore24MatrixTransformOperationD1Ev(WebCore::MatrixTransformOperation* self);
void APS5_VABI _ZN7WebCore24MatrixTransformOperationD2Ev(WebCore::MatrixTransformOperation* self);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24RotateTransformOperation5blendEPKNS_18TransformOperationEdb(WebCore::RotateTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24RotateTransformOperation6createEdNS_18TransformOperation13OperationTypeE(double angle, WebCore::TransformOperation::OperationType type);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(double x, double y, double z, double angle, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore24RotateTransformOperationC1EddddNS_18TransformOperation13OperationTypeE(WebCore::RotateTransformOperation* self, double x, double y, double z, double angle, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore24RotateTransformOperationC2EddddNS_18TransformOperation13OperationTypeE(WebCore::RotateTransformOperation* self, double x, double y, double z, double angle, WebCore::TransformOperation::OperationType type);
void APS5_VABI _ZN7WebCore24RotateTransformOperationD0Ev(WebCore::RotateTransformOperation* self);
void APS5_VABI _ZN7WebCore24RotateTransformOperationD1Ev(WebCore::RotateTransformOperation* self);
void APS5_VABI _ZN7WebCore24RotateTransformOperationD2Ev(WebCore::RotateTransformOperation* self);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore26IdentityTransformOperation5blendEPKNS_18TransformOperationEdb(WebCore::IdentityTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore26IdentityTransformOperation6createEv();
void APS5_VABI _ZN7WebCore26IdentityTransformOperationC1Ev(WebCore::IdentityTransformOperation* self);
void APS5_VABI _ZN7WebCore26IdentityTransformOperationC2Ev(WebCore::IdentityTransformOperation* self);
void APS5_VABI _ZN7WebCore26IdentityTransformOperationD0Ev(WebCore::IdentityTransformOperation* self);
void APS5_VABI _ZN7WebCore26IdentityTransformOperationD1Ev(WebCore::IdentityTransformOperation* self);
void APS5_VABI _ZN7WebCore26IdentityTransformOperationD2Ev(WebCore::IdentityTransformOperation* self);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore26Matrix3DTransformOperation5blendEPKNS_18TransformOperationEdb(WebCore::Matrix3DTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE(const WebCore::TransformationMatrix& matrix);
void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationC1ERKNS_20TransformationMatrixE(WebCore::Matrix3DTransformOperation* self, const WebCore::TransformationMatrix& matrix);
void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationC2ERKNS_20TransformationMatrixE(WebCore::Matrix3DTransformOperation* self, const WebCore::TransformationMatrix& matrix);
void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationD0Ev(WebCore::Matrix3DTransformOperation* self);
void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationD1Ev(WebCore::Matrix3DTransformOperation* self);
void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationD2Ev(WebCore::Matrix3DTransformOperation* self);
bool APS5_VABI _ZNK7WebCore22SkewTransformOperation10isIdentityEv(const WebCore::SkewTransformOperation* self);
bool APS5_VABI _ZNK7WebCore22SkewTransformOperation27isAffectedByTransformOriginEv(const WebCore::SkewTransformOperation* self);
void APS5_VABI _ZNK7WebCore22SkewTransformOperation4dumpERN3WTF10TextStreamE(const WebCore::SkewTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream);
bool APS5_VABI _ZNK7WebCore22SkewTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::SkewTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore22SkewTransformOperation5cloneEv(const WebCore::SkewTransformOperation* self);
double APS5_VABI _ZNK7WebCore22SkewTransformOperation6angleXEv(const WebCore::SkewTransformOperation* self);
double APS5_VABI _ZNK7WebCore22SkewTransformOperation6angleYEv(const WebCore::SkewTransformOperation* self);
bool APS5_VABI _ZNK7WebCore22SkewTransformOperationeqERKNS_18TransformOperationE(const WebCore::SkewTransformOperation* self, const WebCore::TransformOperation& other);
bool APS5_VABI _ZNK7WebCore23ScaleTransformOperation10isIdentityEv(const WebCore::ScaleTransformOperation* self);
bool APS5_VABI _ZNK7WebCore23ScaleTransformOperation19isRepresentableIn2DEv(const WebCore::ScaleTransformOperation* self);
double APS5_VABI _ZNK7WebCore23ScaleTransformOperation1xEv(const WebCore::ScaleTransformOperation* self);
double APS5_VABI _ZNK7WebCore23ScaleTransformOperation1yEv(const WebCore::ScaleTransformOperation* self);
double APS5_VABI _ZNK7WebCore23ScaleTransformOperation1zEv(const WebCore::ScaleTransformOperation* self);
bool APS5_VABI _ZNK7WebCore23ScaleTransformOperation27isAffectedByTransformOriginEv(const WebCore::ScaleTransformOperation* self);
void APS5_VABI _ZNK7WebCore23ScaleTransformOperation4dumpERN3WTF10TextStreamE(const WebCore::ScaleTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream);
bool APS5_VABI _ZNK7WebCore23ScaleTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::ScaleTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore23ScaleTransformOperation5cloneEv(const WebCore::ScaleTransformOperation* self);
bool APS5_VABI _ZNK7WebCore23ScaleTransformOperationeqERKNS_18TransformOperationE(const WebCore::ScaleTransformOperation* self, const WebCore::TransformOperation& other);
bool APS5_VABI _ZNK7WebCore24MatrixTransformOperation10isIdentityEv(const WebCore::MatrixTransformOperation* self);
bool APS5_VABI _ZNK7WebCore24MatrixTransformOperation27isAffectedByTransformOriginEv(const WebCore::MatrixTransformOperation* self);
void APS5_VABI _ZNK7WebCore24MatrixTransformOperation4dumpERN3WTF10TextStreamE(const WebCore::MatrixTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream);
bool APS5_VABI _ZNK7WebCore24MatrixTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::MatrixTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore24MatrixTransformOperation5cloneEv(const WebCore::MatrixTransformOperation* self);
bool APS5_VABI _ZNK7WebCore24MatrixTransformOperationeqERKNS_18TransformOperationE(const WebCore::MatrixTransformOperation* self, const WebCore::TransformOperation& other);
bool APS5_VABI _ZNK7WebCore24RotateTransformOperation10isIdentityEv(const WebCore::RotateTransformOperation* self);
bool APS5_VABI _ZNK7WebCore24RotateTransformOperation19isRepresentableIn2DEv(const WebCore::RotateTransformOperation* self);
double APS5_VABI _ZNK7WebCore24RotateTransformOperation1xEv(const WebCore::RotateTransformOperation* self);
double APS5_VABI _ZNK7WebCore24RotateTransformOperation1yEv(const WebCore::RotateTransformOperation* self);
double APS5_VABI _ZNK7WebCore24RotateTransformOperation1zEv(const WebCore::RotateTransformOperation* self);
bool APS5_VABI _ZNK7WebCore24RotateTransformOperation27isAffectedByTransformOriginEv(const WebCore::RotateTransformOperation* self);
void APS5_VABI _ZNK7WebCore24RotateTransformOperation4dumpERN3WTF10TextStreamE(const WebCore::RotateTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream);
double APS5_VABI _ZNK7WebCore24RotateTransformOperation5angleEv(const WebCore::RotateTransformOperation* self);
bool APS5_VABI _ZNK7WebCore24RotateTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::RotateTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore24RotateTransformOperation5cloneEv(const WebCore::RotateTransformOperation* self);
bool APS5_VABI _ZNK7WebCore24RotateTransformOperationeqERKNS_18TransformOperationE(const WebCore::RotateTransformOperation* self, const WebCore::TransformOperation& other);
bool APS5_VABI _ZNK7WebCore26IdentityTransformOperation10isIdentityEv(const WebCore::IdentityTransformOperation* self);
void APS5_VABI _ZNK7WebCore26IdentityTransformOperation4dumpERN3WTF10TextStreamE(const WebCore::IdentityTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream);
bool APS5_VABI _ZNK7WebCore26IdentityTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::IdentityTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore26IdentityTransformOperation5cloneEv(const WebCore::IdentityTransformOperation* self);
bool APS5_VABI _ZNK7WebCore26IdentityTransformOperationeqERKNS_18TransformOperationE(const WebCore::IdentityTransformOperation* self, const WebCore::TransformOperation& other);
bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation10isIdentityEv(const WebCore::Matrix3DTransformOperation* self);
bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation19isRepresentableIn2DEv(const WebCore::Matrix3DTransformOperation* self);
bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation27isAffectedByTransformOriginEv(const WebCore::Matrix3DTransformOperation* self);
void APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation4dumpERN3WTF10TextStreamE(const WebCore::Matrix3DTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream);
bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::Matrix3DTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize);
NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation5cloneEv(const WebCore::Matrix3DTransformOperation* self);
WebCore::TransformationMatrix APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation6matrixEv(const WebCore::Matrix3DTransformOperation* self);
bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperationeqERKNS_18TransformOperationE(const WebCore::Matrix3DTransformOperation* self, const WebCore::TransformOperation& other);

}
