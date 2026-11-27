#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore transform guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

static unsigned char* MapPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(page);
}

static void ProtectReadOnly(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
}

static void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

static bool Near(double value, double expected) {
    return std::fabs(value - expected) < 1.0e-9;
}

static bool Affine(const WebCore::AffineTransform& t, double a, double b, double c, double d, double e, double f) {
    return Near(t.m_transform[0], a) && Near(t.m_transform[1], b) && Near(t.m_transform[2], c) && Near(t.m_transform[3], d) && Near(t.m_transform[4], e) && Near(t.m_transform[5], f);
}

int main() {
    WebCore::AffineTransform affine;
    Require(&_ZN7WebCore15AffineTransform15scaleNonUniformEdd(&affine, 2.0, 3.0) == &affine && Affine(affine, 2.0, 0.0, 0.0, 3.0, 0.0, 0.0));
    Require(&_ZN7WebCore15AffineTransform9translateEdd(&affine, 1.0, 1.0) == &affine && Affine(affine, 2.0, 0.0, 0.0, 3.0, 2.0, 3.0));
    Require(&_ZN7WebCore15AffineTransform9translateERKNS_10FloatPointE(&affine, WebCore::FloatPoint{1.0f, 0.0f}) == &affine && Affine(affine, 2.0, 0.0, 0.0, 3.0, 4.0, 3.0));
    Require(&_ZN7WebCore15AffineTransform9translateERKNS_9FloatSizeE(&affine, WebCore::FloatSize{0.0f, 1.0f}) == &affine && Affine(affine, 2.0, 0.0, 0.0, 3.0, 4.0, 6.0));
    Require(&_ZN7WebCore15AffineTransform5scaleERKNS_9FloatSizeE(&affine, WebCore::FloatSize{0.5f, 1.0f}) == &affine && Affine(affine, 1.0, 0.0, 0.0, 3.0, 4.0, 6.0));
    Require(&_ZN7WebCore15AffineTransform5scaleEd(&affine, 2.0) == &affine && Affine(affine, 2.0, 0.0, 0.0, 6.0, 4.0, 6.0));
    Require(&_ZN7WebCore15AffineTransform5flipXEv(&affine) == &affine && Affine(affine, -2.0, 0.0, 0.0, 6.0, 4.0, 6.0));
    Require(&_ZN7WebCore15AffineTransform5flipYEv(&affine) == &affine && Affine(affine, -2.0, 0.0, 0.0, -6.0, 4.0, 6.0));
    Require(&_ZN7WebCore15AffineTransform5shearEdd(&affine, 1.0, 0.0) == &affine && Affine(affine, -2.0, 0.0, -2.0, -6.0, 4.0, 6.0));
    Require(&_ZN7WebCore15AffineTransform4skewEdd(&affine, 0.0, 0.0) == &affine && Affine(affine, -2.0, 0.0, -2.0, -6.0, 4.0, 6.0));
    WebCore::AffineTransform doubling;
    doubling.m_transform = {2.0, 0.0, 0.0, 2.0, 1.0, 1.0};
    Require(&_ZN7WebCore15AffineTransform8multiplyERKS0_(&affine, doubling) == &affine && Affine(affine, -4.0, 0.0, -4.0, -12.0, 0.0, 0.0));
    Require(&_ZN7WebCore15AffineTransform8multiplyERKS0_(&affine, affine) == &affine && Affine(affine, 16.0, 0.0, 64.0, 144.0, 0.0, 0.0));
    _ZN7WebCore15AffineTransform12makeIdentityEv(&affine);
    Require(_ZNK7WebCore15AffineTransform10isIdentityEv(&affine));
    Require(&_ZN7WebCore15AffineTransform6rotateEd(&affine, 90.0) == &affine && Affine(affine, 0.0, 1.0, -1.0, 0.0, 0.0, 0.0));
    Require(!_ZNK7WebCore15AffineTransform10isIdentityEv(&affine));

    WebCore::AffineTransform scaled;
    scaled.m_transform = {3.0, 4.0, 0.0, 2.0, 5.0, 7.0};
    Require(Near(_ZNK7WebCore15AffineTransform6xScaleEv(&scaled), 5.0) && Near(_ZNK7WebCore15AffineTransform6yScaleEv(&scaled), 2.0));
    const auto mappedPoint = _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(&scaled, WebCore::FloatPoint{1.0f, 1.0f});
    Require(mappedPoint.m_x == 8.0f && mappedPoint.m_y == 13.0f);
    const auto mappedIntPoint = _ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE(&scaled, WebCore::IntPoint{1, 0});
    Require(mappedIntPoint.m_x == 8 && mappedIntPoint.m_y == 11);
    const auto mappedSize = _ZNK7WebCore15AffineTransform7mapSizeERKNS_7IntSizeE(&scaled, WebCore::IntSize{2, 3});
    Require(mappedSize.m_width == 10 && mappedSize.m_height == 6);
    const auto mappedFloatSize = _ZNK7WebCore15AffineTransform7mapSizeERKNS_9FloatSizeE(&scaled, WebCore::FloatSize{1.0f, 1.0f});
    Require(mappedFloatSize.m_width == 5.0f && mappedFloatSize.m_height == 2.0f);
    WebCore::AffineTransform translation;
    translation.m_transform = {1.0, 0.0, 0.0, 1.0, 2.0, 3.0};
    const WebCore::FloatRect unit{WebCore::FloatPoint{0.0f, 0.0f}, WebCore::FloatSize{1.0f, 1.0f}};
    const auto movedRect = _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(&translation, unit);
    Require(movedRect.m_location.m_x == 2.0f && movedRect.m_location.m_y == 3.0f && movedRect.m_size.m_width == 1.0f);
    const auto scaledRect = _ZNK7WebCore15AffineTransform7mapRectERKNS_7IntRectE(&scaled, WebCore::IntRect{WebCore::IntPoint{0, 0}, WebCore::IntSize{1, 1}});
    Require(scaledRect.m_location.m_x == 5 && scaledRect.m_location.m_y == 7 && scaledRect.m_size.m_width == 3 && scaledRect.m_size.m_height == 6);
    const WebCore::FloatQuad quad{WebCore::FloatPoint{0.0f, 0.0f}, WebCore::FloatPoint{1.0f, 0.0f}, WebCore::FloatPoint{1.0f, 1.0f}, WebCore::FloatPoint{0.0f, 1.0f}};
    const auto movedQuad = _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(&translation, quad);
    Require(movedQuad.m_p3.m_x == 3.0f && movedQuad.m_p3.m_y == 4.0f);
    const auto scaledQuad = _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(&scaled, quad);
    Require(scaledQuad.m_p2.m_x == 8.0f && scaledQuad.m_p2.m_y == 11.0f);
    const auto asMatrix = _ZNK7WebCore15AffineTransform22toTransformationMatrixEv(&scaled);
    Require(asMatrix.m_matrix[0][1] == 4.0 && asMatrix.m_matrix[3][1] == 7.0 && asMatrix.m_matrix[2][2] == 1.0);

    WebCore::TransformationMatrix matrix;
    Require(&_ZN7WebCore20TransformationMatrix11translate3dEddd(&matrix, 1.0, 2.0, 3.0) == &matrix && matrix.m_matrix[3][0] == 1.0 && matrix.m_matrix[3][2] == 3.0);
    Require(&_ZN7WebCore20TransformationMatrix9translateEdd(&matrix, 1.0, 0.0) == &matrix && matrix.m_matrix[3][0] == 2.0);
    Require(&_ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(&matrix, 2.0, 3.0) == &matrix && matrix.m_matrix[0][0] == 2.0 && matrix.m_matrix[1][1] == 3.0);
    Require(&_ZN7WebCore20TransformationMatrix5scaleEd(&matrix, 2.0) == &matrix && matrix.m_matrix[0][0] == 4.0 && matrix.m_matrix[1][1] == 6.0);
    Require(&_ZN7WebCore20TransformationMatrix5flipXEv(&matrix) == &matrix && matrix.m_matrix[0][0] == -4.0);
    Require(&_ZN7WebCore20TransformationMatrix5flipYEv(&matrix) == &matrix && matrix.m_matrix[1][1] == -6.0);
    Require(&_ZN7WebCore20TransformationMatrix4skewEdd(&matrix, 0.0, 0.0) == &matrix && matrix.m_matrix[0][0] == -4.0 && matrix.m_matrix[1][0] == 0.0);
    Require(&_ZN7WebCore20TransformationMatrix16applyPerspectiveEd(&matrix, 0.0) == &matrix && matrix.m_matrix[2][3] == 0.0);
    Require(&_ZN7WebCore20TransformationMatrix6rotateEd(&matrix, 0.0) == &matrix && matrix.m_matrix[0][0] == -4.0 && matrix.m_matrix[3][0] == 2.0);
    WebCore::TransformationMatrix doublingMatrix;
    doublingMatrix.m_matrix[0][0] = 2.0;
    doublingMatrix.m_matrix[1][1] = 2.0;
    Require(&_ZN7WebCore20TransformationMatrix8multiplyERKS0_(&matrix, doublingMatrix) == &matrix && matrix.m_matrix[0][0] == -8.0 && matrix.m_matrix[1][1] == -12.0 && matrix.m_matrix[3][0] == 2.0);
    Require(&_ZN7WebCore20TransformationMatrix8multiplyERKS0_(&matrix, matrix) == &matrix && matrix.m_matrix[0][0] == 64.0 && matrix.m_matrix[1][1] == 144.0);
    Require(&_ZN7WebCore20TransformationMatrix8rotate3dEddd(&matrix, 0.0, 0.0, 0.0) == &matrix && matrix.m_matrix[0][0] == 64.0);
    matrix.m_matrix[2][0] = 5.0;
    _ZN7WebCore20TransformationMatrix10makeAffineEv(&matrix);
    Require(matrix.m_matrix[2][0] == 0.0 && matrix.m_matrix[2][2] == 1.0 && matrix.m_matrix[0][0] == 64.0);
    Require(_ZNK7WebCore20TransformationMatrix12isInvertibleEv(&matrix));
    const auto backToAffine = _ZNK7WebCore20TransformationMatrix17toAffineTransformEv(&matrix);
    Require(backToAffine.m_transform[0] == 64.0 && backToAffine.m_transform[3] == 144.0);

    WebCore::TransformationMatrix fromAffine;
    _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(&fromAffine, scaled);
    Require(fromAffine.m_matrix[0][0] == 3.0 && fromAffine.m_matrix[0][1] == 4.0 && fromAffine.m_matrix[3][0] == 5.0 && fromAffine.m_matrix[3][3] == 1.0);
    WebCore::TransformationMatrix fromAffineBase;
    _ZN7WebCore20TransformationMatrixC2ERKNS_15AffineTransformE(&fromAffineBase, translation);
    Require(fromAffineBase.m_matrix[3][0] == 2.0 && fromAffineBase.m_matrix[3][1] == 3.0);
    const auto pointThroughMatrix = _ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(&fromAffine, WebCore::FloatPoint{1.0f, 1.0f});
    Require(pointThroughMatrix.m_x == 8.0f && pointThroughMatrix.m_y == 13.0f);
    const auto rectThroughMatrix = _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(&fromAffineBase, unit);
    Require(rectThroughMatrix.m_location.m_x == 2.0f && rectThroughMatrix.m_location.m_y == 3.0f);
    const auto intRectThroughMatrix = _ZNK7WebCore20TransformationMatrix7mapRectERKNS_7IntRectE(&fromAffine, WebCore::IntRect{WebCore::IntPoint{0, 0}, WebCore::IntSize{1, 1}});
    Require(intRectThroughMatrix.m_location.m_x == 5 && intRectThroughMatrix.m_size.m_height == 6);
    const auto quadThroughMatrix = _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(&fromAffine, quad);
    Require(quadThroughMatrix.m_p2.m_x == 8.0f && quadThroughMatrix.m_p2.m_y == 11.0f);
    const auto rectToRect = _ZN7WebCore20TransformationMatrix10rectToRectERKNS_9FloatRectES3_(unit, WebCore::FloatRect{WebCore::FloatPoint{2.0f, 3.0f}, WebCore::FloatSize{4.0f, 5.0f}});
    Require(rectToRect.m_matrix[0][0] == 4.0 && rectToRect.m_matrix[1][1] == 5.0 && rectToRect.m_matrix[3][0] == 2.0 && rectToRect.m_matrix[3][1] == 3.0);
    WebCore::MatrixTransformOperation operation{};
    operation.m_a = 1.0;
    operation.m_d = 2.0;
    operation.m_e = 3.0;
    operation.m_f = 4.0;
    const auto operationMatrix = _ZNK7WebCore24MatrixTransformOperation6matrixEv(&operation);
    Require(operationMatrix.m_matrix[0][0] == 1.0 && operationMatrix.m_matrix[1][1] == 2.0 && operationMatrix.m_matrix[3][0] == 3.0 && operationMatrix.m_matrix[3][1] == 4.0);

    auto* badAffine = reinterpret_cast<WebCore::AffineTransform*>(std::uintptr_t{0x10});
    auto* badMatrix = reinterpret_cast<WebCore::TransformationMatrix*>(std::uintptr_t{0x10});
    const auto& badPoint = *reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10});
    const auto& badIntPoint = *reinterpret_cast<const WebCore::IntPoint*>(std::uintptr_t{0x10});
    const auto& badSize = *reinterpret_cast<const WebCore::FloatSize*>(std::uintptr_t{0x10});
    const auto& badIntSize = *reinterpret_cast<const WebCore::IntSize*>(std::uintptr_t{0x10});
    const auto& badRect = *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10});
    const auto& badIntRect = *reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10});
    const auto& badQuad = *reinterpret_cast<const WebCore::FloatQuad*>(std::uintptr_t{0x10});
    const auto* badOperation = reinterpret_cast<const WebCore::MatrixTransformOperation*>(std::uintptr_t{0x10});

    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform12makeIdentityEv(badAffine); return 0; }, "_ZN7WebCore15AffineTransform12makeIdentityEv"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform15scaleNonUniformEdd(badAffine, 1.0, 1.0); return 0; }, "_ZN7WebCore15AffineTransform15scaleNonUniformEdd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform4skewEdd(badAffine, 1.0, 1.0); return 0; }, "_ZN7WebCore15AffineTransform4skewEdd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform5flipXEv(badAffine); return 0; }, "_ZN7WebCore15AffineTransform5flipXEv"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform5flipYEv(badAffine); return 0; }, "_ZN7WebCore15AffineTransform5flipYEv"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform5scaleERKNS_9FloatSizeE(&affine, badSize); return 0; }, "_ZN7WebCore15AffineTransform5scaleERKNS_9FloatSizeE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform5shearEdd(badAffine, 1.0, 1.0); return 0; }, "_ZN7WebCore15AffineTransform5shearEdd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform6rotateEd(badAffine, 1.0); return 0; }, "_ZN7WebCore15AffineTransform6rotateEd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform8multiplyERKS0_(badAffine, doubling); return 0; }, "_ZN7WebCore15AffineTransform8multiplyERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform8multiplyERKS0_(&affine, *badAffine); return 0; }, "_ZN7WebCore15AffineTransform8multiplyERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform9translateERKNS_10FloatPointE(&affine, badPoint); return 0; }, "_ZN7WebCore15AffineTransform9translateERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform9translateERKNS_9FloatSizeE(&affine, badSize); return 0; }, "_ZN7WebCore15AffineTransform9translateERKNS_9FloatSizeE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform9translateEdd(badAffine, 1.0, 1.0); return 0; }, "_ZN7WebCore15AffineTransform9translateEdd"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform10isIdentityEv(badAffine); }, "_ZNK7WebCore15AffineTransform10isIdentityEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform22toTransformationMatrixEv(badAffine); }, "_ZNK7WebCore15AffineTransform22toTransformationMatrixEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform6xScaleEv(badAffine); }, "_ZNK7WebCore15AffineTransform6xScaleEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform6yScaleEv(badAffine); }, "_ZNK7WebCore15AffineTransform6yScaleEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(badAffine, quad); }, "_ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(&scaled, badQuad); }, "_ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapRectERKNS_7IntRectE(&scaled, badIntRect); }, "_ZNK7WebCore15AffineTransform7mapRectERKNS_7IntRectE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(badAffine, unit); }, "_ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(&scaled, badRect); }, "_ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapSizeERKNS_7IntSizeE(&scaled, badIntSize); }, "_ZNK7WebCore15AffineTransform7mapSizeERKNS_7IntSizeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapSizeERKNS_9FloatSizeE(badAffine, WebCore::FloatSize{}); }, "_ZNK7WebCore15AffineTransform7mapSizeERKNS_9FloatSizeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(&scaled, badPoint); }, "_ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE(&scaled, badIntPoint); }, "_ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix10makeAffineEv(badMatrix); return 0; }, "_ZN7WebCore20TransformationMatrix10makeAffineEv"));
    Require(FaultsAsNotImplemented([&] { return _ZN7WebCore20TransformationMatrix10rectToRectERKNS_9FloatRectES3_(badRect, unit); }, "_ZN7WebCore20TransformationMatrix10rectToRectERKNS_9FloatRectES3_"));
    Require(FaultsAsNotImplemented([&] { return _ZN7WebCore20TransformationMatrix10rectToRectERKNS_9FloatRectES3_(unit, badRect); }, "_ZN7WebCore20TransformationMatrix10rectToRectERKNS_9FloatRectES3_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix11translate3dEddd(badMatrix, 1.0, 1.0, 1.0); return 0; }, "_ZN7WebCore20TransformationMatrix11translate3dEddd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(badMatrix, 1.0, 1.0); return 0; }, "_ZN7WebCore20TransformationMatrix15scaleNonUniformEdd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix4skewEdd(badMatrix, 1.0, 1.0); return 0; }, "_ZN7WebCore20TransformationMatrix4skewEdd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix8multiplyERKS0_(badMatrix, doublingMatrix); return 0; }, "_ZN7WebCore20TransformationMatrix8multiplyERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix8multiplyERKS0_(&matrix, *badMatrix); return 0; }, "_ZN7WebCore20TransformationMatrix8multiplyERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(badMatrix, 1.0); return 0; }, "_ZN7WebCore20TransformationMatrix16applyPerspectiveEd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix8rotate3dEddd(badMatrix, 1.0, 1.0, 1.0); return 0; }, "_ZN7WebCore20TransformationMatrix8rotate3dEddd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(badMatrix, scaled); return 0; }, "_ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(&fromAffine, *badAffine); return 0; }, "_ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix12isInvertibleEv(badMatrix); }, "_ZNK7WebCore20TransformationMatrix12isInvertibleEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix17toAffineTransformEv(badMatrix); }, "_ZNK7WebCore20TransformationMatrix17toAffineTransformEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(&fromAffine, badQuad); }, "_ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix7mapRectERKNS_7IntRectE(badMatrix, WebCore::IntRect{}); }, "_ZNK7WebCore20TransformationMatrix7mapRectERKNS_7IntRectE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(&fromAffine, badRect); }, "_ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(badMatrix, WebCore::FloatPoint{}); }, "_ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore24MatrixTransformOperation6matrixEv(badOperation); }, "_ZNK7WebCore24MatrixTransformOperation6matrixEv"));

    unsigned char* readOnlyPage = MapPage();
    auto* readOnlyAffine = reinterpret_cast<WebCore::AffineTransform*>(readOnlyPage);
    auto* readOnlyMatrix = reinterpret_cast<WebCore::TransformationMatrix*>(readOnlyPage + 256);
    *readOnlyAffine = scaled;
    *readOnlyMatrix = fromAffine;
    unsigned char before[4096];
    std::memcpy(before, readOnlyPage, sizeof(before));
    ProtectReadOnly(readOnlyPage);
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform15scaleNonUniformEdd(readOnlyAffine, 2.0, 2.0); return 0; }, "_ZN7WebCore15AffineTransform15scaleNonUniformEdd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform8multiplyERKS0_(readOnlyAffine, doubling); return 0; }, "_ZN7WebCore15AffineTransform8multiplyERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform9translateEdd(readOnlyAffine, 1.0, 1.0); return 0; }, "_ZN7WebCore15AffineTransform9translateEdd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix8multiplyERKS0_(readOnlyMatrix, doublingMatrix); return 0; }, "_ZN7WebCore20TransformationMatrix8multiplyERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrix10makeAffineEv(readOnlyMatrix); return 0; }, "_ZN7WebCore20TransformationMatrix10makeAffineEv"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(readOnlyMatrix, translation); return 0; }, "_ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE"));
    Require(std::memcmp(before, readOnlyPage, sizeof(before)) == 0);
    Require(Near(_ZNK7WebCore15AffineTransform6xScaleEv(readOnlyAffine), 5.0) && _ZNK7WebCore20TransformationMatrix12isInvertibleEv(readOnlyMatrix));
    UnmapPage(readOnlyPage);
    return 0;
}
