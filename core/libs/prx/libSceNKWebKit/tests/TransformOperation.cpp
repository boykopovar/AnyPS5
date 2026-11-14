#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include "../src/TransformOperationAbi.hpp"
#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::TransformAbi::Dispatch;
using NKWebKit::TransformAbi::TransformOperationRef;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;
using OperationType = WebCore::TransformOperation::OperationType;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "TransformOperation check failed at line %u\n", location.line());
        std::abort();
    }
}

bool Near(double first, double second, double epsilon = 1e-9) {
    return std::fabs(first - second) <= epsilon;
}

bool IsIdentityMatrix(const WebCore::TransformationMatrix& matrix) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (matrix.m_matrix[i][j] != (i == j ? 1.0 : 0.0)) return false;
        }
    }
    return true;
}

TextStream NewStream(unsigned flags = 0) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.formattingFlags = flags;
    stream.multiLineMode = false;
    return stream;
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

std::string DumpThroughVtable(const WebCore::TransformOperation* operation, unsigned flags = 0) {
    TextStream stream = NewStream(flags);
    Dispatch(*operation).dump(operation, stream);
    Check(stream.formattingFlags == flags);
    return Release(stream);
}

template<typename T> const T* As(const TransformOperationRef& ref) {
    return static_cast<const T*>(ref.Get());
}

void TestIdentity() {
    TransformOperationRef identity = _ZN7WebCore26IdentityTransformOperation6createEv();
    WebCore::TransformOperation* operation = identity.Get();
    Check(operation->m_refCount == 1);
    Check(operation->m_type == OperationType::Identity);
    Check(static_cast<std::uint32_t>(operation->m_type) == 20);
    Check(Dispatch(*operation).isIdentity(operation));
    Check(!Dispatch(*operation).isAffectedByTransformOrigin(operation));
    Check(Dispatch(*operation).isRepresentableIn2D(operation));

    WebCore::TransformationMatrix matrix;
    matrix.m_matrix[3][0] = 5.0;
    WebCore::FloatSize size{100.0f, 50.0f};
    Check(!Dispatch(*operation).apply(operation, matrix, size));
    Check(matrix.m_matrix[3][0] == 5.0);

    TransformOperationRef blended = Dispatch(*operation).blend(operation, nullptr, 0.5, false);
    Check(blended.Get() == operation);
    Check(operation->m_refCount == 2);

    TransformOperationRef copy = Dispatch(*operation).clone(operation);
    Check(copy.Get() != operation);
    Check(copy.Get()->m_refCount == 1);
    Check(Dispatch(*operation).equals(operation, *copy.Get()));
    Check(DumpThroughVtable(operation) == "identity");

    WebCore::IdentityTransformOperation local{};
    _ZN7WebCore26IdentityTransformOperationC2Ev(&local);
    Check(local.m_vptr == operation->m_vptr);
    Check(local.m_refCount == 1);
    _ZN7WebCore26IdentityTransformOperationD2Ev(&local);
}

void TestRotateBasics() {
    TransformOperationRef rotate = _ZN7WebCore24RotateTransformOperation6createEdNS_18TransformOperation13OperationTypeE(90.0, OperationType::Rotate);
    const auto* op = As<WebCore::RotateTransformOperation>(rotate);
    Check(op->m_type == OperationType::Rotate);
    Check(_ZNK7WebCore24RotateTransformOperation1xEv(op) == 0.0);
    Check(_ZNK7WebCore24RotateTransformOperation1yEv(op) == 0.0);
    Check(_ZNK7WebCore24RotateTransformOperation1zEv(op) == 1.0);
    Check(_ZNK7WebCore24RotateTransformOperation5angleEv(op) == 90.0);
    Check(!Dispatch(*op).isIdentity(op));
    Check(Dispatch(*op).isAffectedByTransformOrigin(op));
    Check(Dispatch(*op).isRepresentableIn2D(op));

    WebCore::TransformationMatrix matrix;
    WebCore::FloatSize size{};
    Check(!Dispatch(*op).apply(op, matrix, size));
    Check(Near(matrix.m_matrix[0][0], 0.0));
    Check(Near(matrix.m_matrix[0][1], 1.0));
    Check(Near(matrix.m_matrix[1][0], -1.0));
    Check(Near(matrix.m_matrix[1][1], 0.0));
    Check(matrix.m_matrix[2][2] == 1.0);

    TransformOperationRef rotateX = _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(1.0, 0.0, 0.0, 90.0, OperationType::RotateX);
    const auto* opX = As<WebCore::RotateTransformOperation>(rotateX);
    WebCore::TransformationMatrix matrixX;
    Check(!_ZNK7WebCore24RotateTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(opX, matrixX, size));
    Check(matrixX.m_matrix[0][0] == 1.0);
    Check(Near(matrixX.m_matrix[1][1], 0.0));
    Check(Near(matrixX.m_matrix[1][2], 1.0));
    Check(Near(matrixX.m_matrix[2][1], -1.0));
    Check(!_ZNK7WebCore24RotateTransformOperation19isRepresentableIn2DEv(opX));

    TransformOperationRef zeroX = _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(1.0, 0.0, 0.0, 0.0, OperationType::RotateX);
    const auto* zeroOp = As<WebCore::RotateTransformOperation>(zeroX);
    Check(_ZNK7WebCore24RotateTransformOperation19isRepresentableIn2DEv(zeroOp));
    Check(_ZNK7WebCore24RotateTransformOperation10isIdentityEv(zeroOp));
    Check(!_ZNK7WebCore24RotateTransformOperation27isAffectedByTransformOriginEv(zeroOp));

    TransformOperationRef same = _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(0.0, 0.0, 1.0, 90.0, OperationType::Rotate);
    TransformOperationRef other3d = _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(0.0, 0.0, 1.0, 90.0, OperationType::Rotate3D);
    TransformOperationRef otherAngle = _ZN7WebCore24RotateTransformOperation6createEdNS_18TransformOperation13OperationTypeE(45.0, OperationType::Rotate);
    Check(Dispatch(*op).equals(op, *same.Get()));
    Check(!Dispatch(*op).equals(op, *other3d.Get()));
    Check(!Dispatch(*op).equals(op, *otherAngle.Get()));

    TransformOperationRef copy = _ZNK7WebCore24RotateTransformOperation5cloneEv(op);
    Check(copy.Get() != op);
    Check(copy.Get()->m_refCount == 1);
    Check(_ZNK7WebCore24RotateTransformOperationeqERKNS_18TransformOperationE(op, *copy.Get()));

    Check(DumpThroughVtable(op) == "rotate(0, 0, 1, 90deg)");
    TransformOperationRef fractional = _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(0.5, 0.0, 1.0, 12.25, OperationType::Rotate3D);
    Check(DumpThroughVtable(fractional.Get()) == "rotate3d(0.50, 0, 1, 12.25deg)");

    WebCore::RotateTransformOperation local{};
    _ZN7WebCore24RotateTransformOperationC1EddddNS_18TransformOperation13OperationTypeE(&local, 0.0, 1.0, 0.0, 30.0, OperationType::RotateY);
    Check(local.m_vptr == op->m_vptr);
    Check(local.m_refCount == 1);
    Check(local.m_type == OperationType::RotateY);
    Check(local.m_y == 1.0 && local.m_angle == 30.0);
    _ZN7WebCore24RotateTransformOperationD1Ev(&local);
}

void TestRotateBlend() {
    TransformOperationRef from = _ZN7WebCore24RotateTransformOperation6createEdNS_18TransformOperation13OperationTypeE(10.0, OperationType::Rotate);
    TransformOperationRef to = _ZN7WebCore24RotateTransformOperation6createEdNS_18TransformOperation13OperationTypeE(50.0, OperationType::Rotate);
    WebCore::TransformOperation* toOp = to.Get();

    TransformOperationRef quarter = Dispatch(*toOp).blend(toOp, from.Get(), 0.25, false);
    const auto* quarterOp = As<WebCore::RotateTransformOperation>(quarter);
    Check(quarterOp->m_type == OperationType::Rotate);
    Check(quarterOp->m_z == 1.0);
    Check(Near(quarterOp->m_angle, 20.0));

    TransformOperationRef fromNothing = Dispatch(*toOp).blend(toOp, nullptr, 0.5, false);
    Check(Near(As<WebCore::RotateTransformOperation>(fromNothing)->m_angle, 25.0));

    TransformOperationRef toIdentity = Dispatch(*toOp).blend(toOp, nullptr, 0.2, true);
    Check(Near(As<WebCore::RotateTransformOperation>(toIdentity)->m_angle, 40.0));

    TransformOperationRef scale = _ZN7WebCore23ScaleTransformOperation6createEddNS_18TransformOperation13OperationTypeE(2.0, 2.0, OperationType::Scale);
    TransformOperationRef mismatch = Dispatch(*toOp).blend(toOp, scale.Get(), 0.5, false);
    Check(mismatch.Get() == toOp);
    Check(toOp->m_refCount == 2);

    TransformOperationRef diagonalFrom = _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(1.0, 1.0, 0.0, 30.0, OperationType::Rotate3D);
    TransformOperationRef diagonalTo = _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(1.0, 1.0, 0.0, 90.0, OperationType::Rotate3D);
    auto* diagonalToOp = static_cast<WebCore::RotateTransformOperation*>(diagonalTo.Get());
    TransformOperationRef halfway = _ZN7WebCore24RotateTransformOperation5blendEPKNS_18TransformOperationEdb(diagonalToOp, diagonalFrom.Get(), 0.5, false);
    const auto* halfwayOp = As<WebCore::RotateTransformOperation>(halfway);
    Check(halfwayOp->m_type == OperationType::Rotate3D);
    Check(Near(halfwayOp->m_x, std::sqrt(0.5), 1e-9));
    Check(Near(halfwayOp->m_y, std::sqrt(0.5), 1e-9));
    Check(Near(halfwayOp->m_z, 0.0, 1e-9));
    Check(Near(halfwayOp->m_angle, 60.0, 1e-9));
}

void TestScale() {
    TransformOperationRef scale = _ZN7WebCore23ScaleTransformOperation6createEddNS_18TransformOperation13OperationTypeE(2.0, 3.0, OperationType::Scale);
    const auto* op = As<WebCore::ScaleTransformOperation>(scale);
    Check(_ZNK7WebCore23ScaleTransformOperation1xEv(op) == 2.0);
    Check(_ZNK7WebCore23ScaleTransformOperation1yEv(op) == 3.0);
    Check(_ZNK7WebCore23ScaleTransformOperation1zEv(op) == 1.0);
    Check(Dispatch(*op).isRepresentableIn2D(op));
    Check(!Dispatch(*op).isIdentity(op));
    Check(Dispatch(*op).isAffectedByTransformOrigin(op));

    WebCore::TransformationMatrix matrix;
    matrix.m_matrix[3][0] = 7.0;
    WebCore::FloatSize size{};
    Check(!Dispatch(*op).apply(op, matrix, size));
    Check(matrix.m_matrix[0][0] == 2.0);
    Check(matrix.m_matrix[1][1] == 3.0);
    Check(matrix.m_matrix[2][2] == 1.0);
    Check(matrix.m_matrix[3][0] == 7.0);

    TransformOperationRef scale3d = _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(1.0, 1.0, 4.0, OperationType::Scale3D);
    const auto* op3d = As<WebCore::ScaleTransformOperation>(scale3d);
    Check(!_ZNK7WebCore23ScaleTransformOperation19isRepresentableIn2DEv(op3d));
    TransformOperationRef unit = _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(1.0, 1.0, 1.0, OperationType::Scale3D);
    Check(_ZNK7WebCore23ScaleTransformOperation10isIdentityEv(As<WebCore::ScaleTransformOperation>(unit)));
    Check(!_ZNK7WebCore23ScaleTransformOperation27isAffectedByTransformOriginEv(As<WebCore::ScaleTransformOperation>(unit)));

    TransformOperationRef from = _ZN7WebCore23ScaleTransformOperation6createEddNS_18TransformOperation13OperationTypeE(1.0, 1.0, OperationType::Scale);
    auto* mutableOp = static_cast<WebCore::ScaleTransformOperation*>(scale.Get());
    TransformOperationRef half = _ZN7WebCore23ScaleTransformOperation5blendEPKNS_18TransformOperationEdb(mutableOp, from.Get(), 0.5, false);
    Check(As<WebCore::ScaleTransformOperation>(half)->m_x == 1.5);
    Check(As<WebCore::ScaleTransformOperation>(half)->m_y == 2.0);
    Check(As<WebCore::ScaleTransformOperation>(half)->m_z == 1.0);
    Check(half.Get()->m_type == OperationType::Scale);

    TransformOperationRef toIdentity = Dispatch(*mutableOp).blend(mutableOp, nullptr, 0.25, true);
    Check(As<WebCore::ScaleTransformOperation>(toIdentity)->m_x == 1.75);
    Check(As<WebCore::ScaleTransformOperation>(toIdentity)->m_y == 2.5);

    TransformOperationRef scaleX = _ZN7WebCore23ScaleTransformOperation6createEddNS_18TransformOperation13OperationTypeE(2.0, 3.0, OperationType::ScaleX);
    Check(!_ZNK7WebCore23ScaleTransformOperationeqERKNS_18TransformOperationE(op, *scaleX.Get()));
    TransformOperationRef mismatch = Dispatch(*mutableOp).blend(mutableOp, scaleX.Get(), 0.5, false);
    Check(mismatch.Get() == mutableOp);

    TransformOperationRef copy = Dispatch(*op).clone(op);
    Check(_ZNK7WebCore23ScaleTransformOperationeqERKNS_18TransformOperationE(op, *copy.Get()));
    Check(DumpThroughVtable(op) == "scale(2, 3, 1)");
    TransformOperationRef scaleZ = _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(1.0, 1.0, 0.5, OperationType::ScaleZ);
    Check(DumpThroughVtable(scaleZ.Get()) == "scaleX(1, 1, 0.50)");

    WebCore::ScaleTransformOperation local{};
    _ZN7WebCore23ScaleTransformOperationC2EdddNS_18TransformOperation13OperationTypeE(&local, 5.0, 6.0, 7.0, OperationType::Scale3D);
    Check(local.m_vptr == op->m_vptr && local.m_z == 7.0 && local.m_type == OperationType::Scale3D);
}

void TestSkew() {
    TransformOperationRef skew = _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(45.0, 0.0, OperationType::SkewX);
    const auto* op = As<WebCore::SkewTransformOperation>(skew);
    Check(_ZNK7WebCore22SkewTransformOperation6angleXEv(op) == 45.0);
    Check(_ZNK7WebCore22SkewTransformOperation6angleYEv(op) == 0.0);
    Check(!Dispatch(*op).isIdentity(op));
    Check(Dispatch(*op).isAffectedByTransformOrigin(op));
    Check(Dispatch(*op).isRepresentableIn2D(op));

    WebCore::TransformationMatrix matrix;
    WebCore::FloatSize size{};
    Check(!Dispatch(*op).apply(op, matrix, size));
    Check(Near(matrix.m_matrix[1][0], 1.0));
    Check(matrix.m_matrix[0][1] == 0.0);
    Check(matrix.m_matrix[0][0] == 1.0);

    auto* mutableOp = static_cast<WebCore::SkewTransformOperation*>(skew.Get());
    TransformOperationRef fromNothing = _ZN7WebCore22SkewTransformOperation5blendEPKNS_18TransformOperationEdb(mutableOp, nullptr, 0.5, false);
    Check(As<WebCore::SkewTransformOperation>(fromNothing)->m_angleX == 22.5);
    TransformOperationRef from = _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(5.0, 10.0, OperationType::SkewX);
    TransformOperationRef blended = Dispatch(*mutableOp).blend(mutableOp, from.Get(), 0.25, false);
    Check(As<WebCore::SkewTransformOperation>(blended)->m_angleX == 15.0);
    Check(As<WebCore::SkewTransformOperation>(blended)->m_angleY == 7.5);
    TransformOperationRef toIdentity = Dispatch(*mutableOp).blend(mutableOp, nullptr, 0.4, true);
    Check(Near(As<WebCore::SkewTransformOperation>(toIdentity)->m_angleX, 27.0));

    TransformOperationRef zero = _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(0.0, 0.0, OperationType::Skew);
    Check(_ZNK7WebCore22SkewTransformOperation10isIdentityEv(As<WebCore::SkewTransformOperation>(zero)));
    Check(!_ZNK7WebCore22SkewTransformOperation27isAffectedByTransformOriginEv(As<WebCore::SkewTransformOperation>(zero)));
    Check(!_ZNK7WebCore22SkewTransformOperationeqERKNS_18TransformOperationE(op, *from.Get()));
    TransformOperationRef copy = _ZNK7WebCore22SkewTransformOperation5cloneEv(op);
    Check(Dispatch(*op).equals(op, *copy.Get()));
    Check(DumpThroughVtable(op) == "skewX(45deg, 0deg)");
}

void TestMatrix() {
    TransformOperationRef translate = _ZN7WebCore24MatrixTransformOperation6createEdddddd(1.0, 0.0, 0.0, 1.0, 10.0, 20.0);
    const auto* op = As<WebCore::MatrixTransformOperation>(translate);
    Check(op->m_type == OperationType::Matrix);
    Check(!Dispatch(*op).isIdentity(op));
    Check(Dispatch(*op).isAffectedByTransformOrigin(op));
    Check(Dispatch(*op).isRepresentableIn2D(op));

    WebCore::TransformationMatrix matrix;
    matrix.m_matrix[0][0] = 2.0;
    matrix.m_matrix[1][1] = 2.0;
    WebCore::FloatSize size{};
    Check(!Dispatch(*op).apply(op, matrix, size));
    Check(matrix.m_matrix[3][0] == 20.0);
    Check(matrix.m_matrix[3][1] == 40.0);
    Check(matrix.m_matrix[0][0] == 2.0);

    WebCore::TransformationMatrix source;
    source.m_matrix[0][0] = 2.0;
    source.m_matrix[0][1] = 0.5;
    source.m_matrix[0][2] = 9.0;
    source.m_matrix[1][0] = -0.5;
    source.m_matrix[1][1] = 3.0;
    source.m_matrix[3][0] = 4.0;
    source.m_matrix[3][1] = 6.0;
    source.m_matrix[3][2] = 8.0;
    TransformOperationRef fromMatrix = _ZN7WebCore24MatrixTransformOperation6createERKNS_20TransformationMatrixE(source);
    const auto* fromMatrixOp = As<WebCore::MatrixTransformOperation>(fromMatrix);
    Check(fromMatrixOp->m_a == 2.0 && fromMatrixOp->m_b == 0.5 && fromMatrixOp->m_c == -0.5);
    Check(fromMatrixOp->m_d == 3.0 && fromMatrixOp->m_e == 4.0 && fromMatrixOp->m_f == 6.0);
    WebCore::TransformationMatrix flattened = _ZNK7WebCore24MatrixTransformOperation6matrixEv(fromMatrixOp);
    Check(flattened.m_matrix[0][2] == 0.0 && flattened.m_matrix[3][2] == 0.0);

    TransformOperationRef identity = _ZN7WebCore24MatrixTransformOperation6createEdddddd(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    Check(_ZNK7WebCore24MatrixTransformOperation10isIdentityEv(As<WebCore::MatrixTransformOperation>(identity)));
    Check(!_ZNK7WebCore24MatrixTransformOperation27isAffectedByTransformOriginEv(As<WebCore::MatrixTransformOperation>(identity)));

    auto* mutableOp = static_cast<WebCore::MatrixTransformOperation*>(translate.Get());
    TransformOperationRef half = _ZN7WebCore24MatrixTransformOperation5blendEPKNS_18TransformOperationEdb(mutableOp, identity.Get(), 0.5, false);
    const auto* halfOp = As<WebCore::MatrixTransformOperation>(half);
    Check(Near(halfOp->m_e, 5.0) && Near(halfOp->m_f, 10.0));
    Check(Near(halfOp->m_a, 1.0) && Near(halfOp->m_b, 0.0) && Near(halfOp->m_c, 0.0) && Near(halfOp->m_d, 1.0));

    TransformOperationRef toIdentity = Dispatch(*mutableOp).blend(mutableOp, nullptr, 0.25, true);
    const auto* toIdentityOp = As<WebCore::MatrixTransformOperation>(toIdentity);
    Check(Near(toIdentityOp->m_e, 7.5) && Near(toIdentityOp->m_f, 15.0));

    TransformOperationRef skew = _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(1.0, 0.0, OperationType::SkewX);
    TransformOperationRef mismatch = Dispatch(*mutableOp).blend(mutableOp, skew.Get(), 0.5, false);
    Check(mismatch.Get() == mutableOp);

    TransformOperationRef copy = _ZNK7WebCore24MatrixTransformOperation5cloneEv(op);
    Check(copy.Get() != op);
    Check(_ZNK7WebCore24MatrixTransformOperationeqERKNS_18TransformOperationE(op, *copy.Get()));
    Check(!_ZNK7WebCore24MatrixTransformOperationeqERKNS_18TransformOperationE(op, *identity.Get()));
    Check(!_ZNK7WebCore24MatrixTransformOperationeqERKNS_18TransformOperationE(op, *skew.Get()));
    Check(DumpThroughVtable(fromMatrix.Get()) == "(2, 0.50, -0.50, 3, 4, 6)");

    WebCore::MatrixTransformOperation local{};
    _ZN7WebCore24MatrixTransformOperationC2ERKNS_20TransformationMatrixE(&local, source);
    Check(local.m_vptr == op->m_vptr && local.m_refCount == 1 && local.m_f == 6.0);
    WebCore::MatrixTransformOperation local2{};
    _ZN7WebCore24MatrixTransformOperationC1Edddddd(&local2, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    Check(local2.m_type == OperationType::Matrix && local2.m_c == 3.0);
    _ZN7WebCore24MatrixTransformOperationD2Ev(&local2);
}

void TestMatrix3D() {
    WebCore::TransformationMatrix perspective;
    perspective.m_matrix[2][3] = -0.01;
    TransformOperationRef matrix3d = _ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE(perspective);
    const auto* op = As<WebCore::Matrix3DTransformOperation>(matrix3d);
    Check(op->m_type == OperationType::Matrix3D);
    Check(reinterpret_cast<std::uintptr_t>(op) % 16 == 0);
    Check(!Dispatch(*op).isIdentity(op));
    Check(Dispatch(*op).isAffectedByTransformOrigin(op));
    Check(!Dispatch(*op).isRepresentableIn2D(op));
    WebCore::TransformationMatrix stored = _ZNK7WebCore26Matrix3DTransformOperation6matrixEv(op);
    Check(stored.m_matrix[2][3] == -0.01);

    WebCore::TransformationMatrix target;
    target.m_matrix[3][0] = 3.0;
    WebCore::FloatSize size{};
    Check(!Dispatch(*op).apply(op, target, size));
    Check(target.m_matrix[2][3] == -0.01);
    Check(target.m_matrix[3][0] == 3.0);
    Check(target.m_matrix[3][3] == 1.0);

    WebCore::TransformationMatrix affine;
    affine.m_matrix[3][0] = 10.0;
    TransformOperationRef affineOp = _ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE(affine);
    const auto* affineConst = As<WebCore::Matrix3DTransformOperation>(affineOp);
    Check(_ZNK7WebCore26Matrix3DTransformOperation19isRepresentableIn2DEv(affineConst));
    TransformOperationRef identity = _ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE(WebCore::TransformationMatrix{});
    Check(_ZNK7WebCore26Matrix3DTransformOperation10isIdentityEv(As<WebCore::Matrix3DTransformOperation>(identity)));
    Check(!_ZNK7WebCore26Matrix3DTransformOperation27isAffectedByTransformOriginEv(As<WebCore::Matrix3DTransformOperation>(identity)));

    auto* affineMutable = static_cast<WebCore::Matrix3DTransformOperation*>(affineOp.Get());
    TransformOperationRef half = _ZN7WebCore26Matrix3DTransformOperation5blendEPKNS_18TransformOperationEdb(affineMutable, identity.Get(), 0.5, false);
    const auto* halfOp = As<WebCore::Matrix3DTransformOperation>(half);
    Check(Near(halfOp->m_matrix.m_matrix[3][0], 5.0));
    Check(Near(halfOp->m_matrix.m_matrix[0][0], 1.0));
    TransformOperationRef toIdentity = Dispatch(*affineMutable).blend(affineMutable, nullptr, 0.25, true);
    Check(Near(As<WebCore::Matrix3DTransformOperation>(toIdentity)->m_matrix.m_matrix[3][0], 7.5));
    TransformOperationRef fromNothing = Dispatch(*affineMutable).blend(affineMutable, nullptr, 0.25, false);
    Check(Near(As<WebCore::Matrix3DTransformOperation>(fromNothing)->m_matrix.m_matrix[3][0], 2.5));

    TransformOperationRef twoD = _ZN7WebCore24MatrixTransformOperation6createEdddddd(1.0, 0.0, 0.0, 1.0, 10.0, 0.0);
    TransformOperationRef mismatch = Dispatch(*affineMutable).blend(affineMutable, twoD.Get(), 0.5, false);
    Check(mismatch.Get() == affineMutable);
    Check(!Dispatch(*affineConst).equals(affineConst, *twoD.Get()));

    TransformOperationRef copy = _ZNK7WebCore26Matrix3DTransformOperation5cloneEv(op);
    Check(copy.Get() != op);
    Check(_ZNK7WebCore26Matrix3DTransformOperationeqERKNS_18TransformOperationE(op, *copy.Get()));
    Check(!_ZNK7WebCore26Matrix3DTransformOperationeqERKNS_18TransformOperationE(op, *affineOp.Get()));

    Check(DumpThroughVtable(identity.Get()) ==
        "matrix3d(\n[1.00 0.00 0.00 0.00]\n[0.00 1.00 0.00 0.00]\n[0.00 0.00 1.00 0.00]\n[0.00 0.00 0.00 1.00])");

    alignas(16) WebCore::Matrix3DTransformOperation local{};
    _ZN7WebCore26Matrix3DTransformOperationC2ERKNS_20TransformationMatrixE(&local, affine);
    Check(local.m_vptr == op->m_vptr && local.m_matrix.m_matrix[3][0] == 10.0);
    _ZN7WebCore26Matrix3DTransformOperationD2Ev(&local);
    Check(IsIdentityMatrix(As<WebCore::Matrix3DTransformOperation>(identity)->m_matrix));
}

void TestRefCounting() {
    TransformOperationRef scale = _ZN7WebCore23ScaleTransformOperation6createEddNS_18TransformOperation13OperationTypeE(2.0, 2.0, OperationType::Scale);
    WebCore::TransformOperation* operation = scale.Get();
    {
        TransformOperationRef self = Dispatch(*operation).blend(operation, operation, 0.5, false);
        Check(self.Get() != operation);
        Check(operation->m_refCount == 1);
    }
    NKWebKit::TransformAbi::RefOperation(operation);
    Check(operation->m_refCount == 2);
    NKWebKit::TransformAbi::DerefOperation(operation);
    Check(operation->m_refCount == 1);
}

}  // namespace

int main() {
    TestIdentity();
    TestRotateBasics();
    TestRotateBlend();
    TestScale();
    TestSkew();
    TestMatrix();
    TestMatrix3D();
    TestRefCounting();
    return 0;
}
