#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeTypeOt/include/Calc.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType corner: %s\n", message);
    std::exit(1);
}

template <typename TCall> bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

template <typename TCall> bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

bool SameMatrix(const FreeType::GuestMatrix& left, const FreeType::GuestMatrix& right) {
    return left.xx == right.xx && left.xy == right.xy && left.yx == right.yx && left.yy == right.yy;
}

bool SameVector(const FreeType::GuestVector& left, const FreeType::GuestVector& right) {
    return left.x == right.x && left.y == right.y;
}

bool MulDivNoRoundThrows(std::int64_t a, std::int64_t b, std::int64_t c) {
    return Throws([=] { FT_MulDiv_No_Round_nid_postfix(a, b, c); });
}

FreeType::GuestMatrix Multiplied(const FreeType::GuestMatrix& a, FreeType::GuestMatrix b, std::int64_t scaling) {
    FT_Matrix_Multiply_Scaled_nid_postfix(&a, &b, scaling);
    return b;
}

bool MultiplyThrowsUnchanged(const FreeType::GuestMatrix& a, const FreeType::GuestMatrix& b, std::int64_t scaling) {
    FreeType::GuestMatrix target = b;
    return Throws([&] { FT_Matrix_Multiply_Scaled_nid_postfix(&a, &target, scaling); }) && SameMatrix(target, b);
}

FreeType::GuestVector Transformed(FreeType::GuestVector vector, const FreeType::GuestMatrix& matrix, std::int64_t scaling) {
    FT_Vector_Transform_Scaled_nid_postfix(&vector, &matrix, scaling);
    return vector;
}

bool TransformThrowsUnchanged(const FreeType::GuestVector& vector, const FreeType::GuestMatrix& matrix, std::int64_t scaling) {
    FreeType::GuestVector target = vector;
    return Throws([&] { FT_Vector_Transform_Scaled_nid_postfix(&target, &matrix, scaling); }) && SameVector(target, vector);
}

void CheckMulDivNoRound() {
    Require(FT_MulDiv_No_Round_nid_postfix(7, 3, 2) == 10, "FT_MulDiv_No_Round truncates the quotient");
    Require(FT_MulDiv_No_Round_nid_postfix(-7, 3, 2) == -10, "FT_MulDiv_No_Round truncates a negative quotient toward zero");
    Require(FT_MulDiv_No_Round_nid_postfix(7, -3, -2) == 10, "FT_MulDiv_No_Round multiplies the operand signs");
    Require(FT_MulDiv_No_Round_nid_postfix(7, 3, -2) == -10, "FT_MulDiv_No_Round takes the divisor sign");
    Require(FT_MulDiv_No_Round_nid_postfix(0, 3, 5) == 0, "FT_MulDiv_No_Round of a zero factor is 0");
    Require(FT_MulDiv_No_Round_nid_postfix(5, 7, 7) == 5, "FT_MulDiv_No_Round with b equal to c returns a");
    Require(FT_MulDiv_No_Round_nid_postfix(5, 3, 0) == 0x7FFFFFFF, "FT_MulDiv_No_Round saturates a zero divisor");
    Require(FT_MulDiv_No_Round_nid_postfix(-5, 3, 0) == -0x7FFFFFFF, "FT_MulDiv_No_Round keeps the factor sign for a zero divisor");
    Require(FT_MulDiv_No_Round_nid_postfix(-5, -3, 0) == 0x7FFFFFFF, "FT_MulDiv_No_Round multiplies the factor signs for a zero divisor");
    Require(FT_MulDiv_No_Round_nid_postfix(0x7FFFFFFF, 3, 2) == 3221225470, "FT_MulDiv_No_Round accepts the largest operand");
    Require(FT_MulDiv_No_Round_nid_postfix(-0x7FFFFFFF, -0x7FFFFFFF, 0x7FFFFFFF) == 0x7FFFFFFF, "FT_MulDiv_No_Round accepts the largest negative operands");
    Require(FT_MulDiv_No_Round_nid_postfix(0xFFFF, 0x10001, 1) == 0xFFFFFFFF, "FT_MulDiv_No_Round returns the largest quotient");
    Require(MulDivNoRoundThrows(0, 3, 0) && MulDivNoRoundThrows(5, 0, 0), "FT_MulDiv_No_Round throws for a zero divisor with a zero factor");
    Require(MulDivNoRoundThrows(0x80000000LL, 1, 1) && MulDivNoRoundThrows(1, -0x80000000LL, 1) && MulDivNoRoundThrows(1, 1, 0x80000000LL),
        "FT_MulDiv_No_Round throws for an operand outside 0x7FFFFFFF");
    Require(MulDivNoRoundThrows(0x10000, 0x10000, 1) && MulDivNoRoundThrows(-0x7FFFFFFF, 0x7FFFFFFF, 1), "FT_MulDiv_No_Round throws for a quotient above 0xFFFFFFFF");
    Require(MulDivNoRoundThrows(1LL << 40, 1LL << 20, 3), "FT_MulDiv_No_Round throws where the 32-bit build truncates operands");
    Require(MulDivNoRoundThrows(INT64_MAX, 2, 1) && MulDivNoRoundThrows(INT64_MIN, 1, 1) && MulDivNoRoundThrows(1, 1, INT64_MIN), "FT_MulDiv_No_Round throws at the 64-bit limits");
}

void CheckMatrixMultiplyScaled() {
    const FreeType::GuestMatrix identity{0x10000, 0, 0, 0x10000};
    const FreeType::GuestMatrix matrix{2 << 16, 3 << 16, 4 << 16, 5 << 16};
    Require(SameMatrix(Multiplied(identity, matrix, 1), matrix), "FT_Matrix_Multiply_Scaled by the identity with scaling 1 keeps b");
    Require(SameMatrix(Multiplied({1 << 16, 2 << 16, 3 << 16, 4 << 16}, {5 << 16, 6 << 16, 7 << 16, 8 << 16}, 1), {19 << 16, 22 << 16, 43 << 16, 50 << 16}),
        "FT_Matrix_Multiply_Scaled stores a times b");
    Require(SameMatrix(Multiplied(identity, {3, 5, -3, -5}, 2), {2, 3, -2, -3}), "FT_Matrix_Multiply_Scaled rounds half away from zero");
    Require(SameMatrix(Multiplied(identity, matrix, -1), {-(2 << 16), -(3 << 16), -(4 << 16), -(5 << 16)}), "FT_Matrix_Multiply_Scaled takes the scaling sign");
    Require(SameMatrix(Multiplied(identity, matrix, 32767), {4, 6, 8, 10}), "FT_Matrix_Multiply_Scaled accepts scaling 32767");
    Require(SameMatrix(Multiplied({1 << 16, 1 << 16, 1 << 16, 1 << 16}, {1 << 16, -(1 << 16), 1 << 16, 1 << 16}, 0), {0xFFFFFFFE, 0, 0xFFFFFFFE, 0}),
        "FT_Matrix_Multiply_Scaled adds saturated terms for scaling 0");
    FreeType::GuestMatrix square{1 << 16, 2 << 16, 3 << 16, 4 << 16};
    FT_Matrix_Multiply_Scaled_nid_postfix(&square, &square, 1);
    Require(SameMatrix(square, {7 << 16, 10 << 16, 15 << 16, 22 << 16}), "FT_Matrix_Multiply_Scaled reads both matrices before writing b");
    Require(MultiplyThrowsUnchanged(identity, matrix, 0), "FT_Matrix_Multiply_Scaled throws for scaling 0 with a zero factor");
    Require(MultiplyThrowsUnchanged(identity, matrix, 32768) && MultiplyThrowsUnchanged(identity, matrix, -32768), "FT_Matrix_Multiply_Scaled throws for scaling outside 32767");
    Require(MultiplyThrowsUnchanged(identity, matrix, 1LL << 48), "FT_Matrix_Multiply_Scaled throws where 0x10000 * scaling overflows");
    Require(MultiplyThrowsUnchanged({0x10000, 0, 0, 0x7FFFFFFF}, {1, 2, 3, 0x7FFFFFFF}, 1), "FT_Matrix_Multiply_Scaled checks the last term before writing b");
    Require(MultiplyThrowsUnchanged({0x80000000LL, 0, 0, 0x10000}, {1, 2, 3, 4}, 1), "FT_Matrix_Multiply_Scaled throws for an entry outside 0x7FFFFFFF");
    FreeType::GuestMatrix target = matrix;
    FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, 1);
    FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, 32768);
    FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, (1LL << 47) - 1);
    FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, -(1LL << 47));
    FT_Matrix_Multiply_Scaled_nid_postfix(&identity, nullptr, 1);
    Require(SameMatrix(target, matrix), "FT_Matrix_Multiply_Scaled ignores a null matrix");
    Require(Throws([&] { FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, 1LL << 47); }) &&
            Throws([&] { FT_Matrix_Multiply_Scaled_nid_postfix(&identity, nullptr, -(1LL << 47) - 1); }) && SameMatrix(target, matrix),
        "FT_Matrix_Multiply_Scaled checks 0x10000 * scaling before the null test");
    errno = 71;
    Require(ThrowsInvalidArgument([&] { FT_Matrix_Multiply_Scaled_nid_postfix(&identity, reinterpret_cast<FreeType::GuestMatrix*>(0x10), 1); }) &&
            errno == 71, "FT_Matrix_Multiply_Scaled rejects an inaccessible output without changing errno");
    errno = 71;
    Require(ThrowsInvalidArgument([&] { FT_Matrix_Multiply_Scaled_nid_postfix(reinterpret_cast<const FreeType::GuestMatrix*>(0x10), &target, 1); }) &&
            errno == 71 && SameMatrix(target, matrix), "FT_Matrix_Multiply_Scaled rejects an inaccessible input without changing the result");
}

void CheckVectorTransformScaled() {
    const FreeType::GuestMatrix identity{0x10000, 0, 0, 0x10000};
    const FreeType::GuestVector vector{3 << 16, -(7 << 16)};
    Require(SameVector(Transformed(vector, identity, 1), vector), "FT_Vector_Transform_Scaled by the identity with scaling 1 keeps the vector");
    Require(SameVector(Transformed({100, -200}, {0x18000, 0x8000, -0x10000, 0x20000}, 1), {50, -500}), "FT_Vector_Transform_Scaled applies the matrix");
    Require(SameVector(Transformed({3, -5}, identity, 2), {2, -3}), "FT_Vector_Transform_Scaled rounds half away from zero");
    Require(SameVector(Transformed({2 << 16, 3 << 16}, identity, -1), {-(2 << 16), -(3 << 16)}), "FT_Vector_Transform_Scaled takes the scaling sign");
    Require(SameVector(Transformed({1 << 16, 1 << 16}, {1 << 16, -(1 << 16), 1 << 16, 1 << 16}, 0), {0, 0xFFFFFFFE}), "FT_Vector_Transform_Scaled adds saturated terms for scaling 0");
    Require(TransformThrowsUnchanged(vector, identity, 0), "FT_Vector_Transform_Scaled throws for scaling 0 with a zero factor");
    Require(TransformThrowsUnchanged(vector, identity, 32768) && TransformThrowsUnchanged(vector, identity, -32768), "FT_Vector_Transform_Scaled throws for scaling outside 32767");
    Require(TransformThrowsUnchanged({1, 0x7FFFFFFF}, {1, 0, 0, 0x7FFFFFFF}, 1), "FT_Vector_Transform_Scaled checks the second coordinate before writing");
    Require(TransformThrowsUnchanged({0x80000000LL, 1}, identity, 1), "FT_Vector_Transform_Scaled throws for a coordinate outside 0x7FFFFFFF");
    FreeType::GuestVector target = vector;
    FT_Vector_Transform_Scaled_nid_postfix(&target, nullptr, 1);
    FT_Vector_Transform_Scaled_nid_postfix(&target, nullptr, 32768);
    FT_Vector_Transform_Scaled_nid_postfix(nullptr, &identity, 1);
    Require(SameVector(target, vector), "FT_Vector_Transform_Scaled ignores a null vector or matrix");
    Require(Throws([&] { FT_Vector_Transform_Scaled_nid_postfix(&target, nullptr, INT64_MIN); }) &&
            Throws([&] { FT_Vector_Transform_Scaled_nid_postfix(nullptr, &identity, INT64_MAX); }) && SameVector(target, vector),
        "FT_Vector_Transform_Scaled checks 0x10000 * scaling before the null test");
    static const FreeType::GuestVector readOnlyVector{100, -200};
    static const FreeType::GuestMatrix readOnlyMatrix{0x18000, 0x8000, -0x10000, 0x20000};
    errno = 71;
    FT_Vector_Transform_Scaled_nid_postfix(reinterpret_cast<FreeType::GuestVector*>(0x10), &identity, 1);
    Require(errno == 71, "FT_Vector_Transform_Scaled preserves errno on an unreadable vector");
    FT_Vector_Transform_Scaled_nid_postfix(&target, reinterpret_cast<const FreeType::GuestMatrix*>(0x10), 1);
    Require(errno == 71 && SameVector(target, vector), "FT_Vector_Transform_Scaled leaves target unchanged on an unreadable matrix");
    FT_Vector_Transform_Scaled_nid_postfix(const_cast<FreeType::GuestVector*>(&readOnlyVector), &readOnlyMatrix, 1);
    Require(errno == 71 && SameVector(readOnlyVector, {100, -200}), "FT_Vector_Transform_Scaled ignores a read-only vector without changing errno");
}

void CheckSqrtFixed() {
    Require(FT_SqrtFixed_nid_postfix(0) == 0, "FT_SqrtFixed of 0 is 0");
    Require(FT_SqrtFixed_nid_postfix(-1) == 0 && FT_SqrtFixed_nid_postfix(-0x10000) == 0 && FT_SqrtFixed_nid_postfix(INT32_MIN) == 0,
        "FT_SqrtFixed returns 0 for negative inputs");
    Require(FT_SqrtFixed_nid_postfix(1) == 256, "FT_SqrtFixed scales the least positive fixed unit by 256");
    Require(FT_SqrtFixed_nid_postfix(0x10000) == 0x10000 && FT_SqrtFixed_nid_postfix(0x40000) == 0x20000 && FT_SqrtFixed_nid_postfix(0x90000) == 0x30000 &&
            FT_SqrtFixed_nid_postfix(0x24000) == 0x18000,
        "FT_SqrtFixed computes exact 16.16 roots");
    Require(FT_SqrtFixed_nid_postfix(2) == 362 && FT_SqrtFixed_nid_postfix(3) == 443 && FT_SqrtFixed_nid_postfix(12) == 886 &&
            FT_SqrtFixed_nid_postfix(0xFFFE) == 0xFFFE && FT_SqrtFixed_nid_postfix(0xFFFF) == 0xFFFF && FT_SqrtFixed_nid_postfix(0x10001) == 0x10000,
        "FT_SqrtFixed truncates non-square roots");
    Require(FT_SqrtFixed_nid_postfix(INT32_MAX) == 0xB504F3, "FT_SqrtFixed handles INT32_MAX");
}

void CheckSqrt32() {
    Require(FT_Sqrt32_nid_postfix(0) == 0 && FT_Sqrt32_nid_postfix(1) == 1, "FT_Sqrt32 handles 0 and 1");
    Require(FT_Sqrt32_nid_postfix(4) == 2 && FT_Sqrt32_nid_postfix(9) == 3 && FT_Sqrt32_nid_postfix(0x10000) == 256 &&
            FT_Sqrt32_nid_postfix(46340 * 46340) == 46340,
        "FT_Sqrt32 computes exact integer roots");
    Require(FT_Sqrt32_nid_postfix(3) == 1 && FT_Sqrt32_nid_postfix(8) == 2 && FT_Sqrt32_nid_postfix(15) == 3 &&
            FT_Sqrt32_nid_postfix(46340 * 46340 - 1) == 46339 && FT_Sqrt32_nid_postfix(INT32_MAX) == 46340,
        "FT_Sqrt32 truncates non-square positive inputs");
    Require(FT_Sqrt32_nid_postfix(INT32_MIN) == 46340 &&
            FT_Sqrt32_nid_postfix(static_cast<std::int32_t>(46341u * 46341u - 1u)) == 46340 &&
            FT_Sqrt32_nid_postfix(static_cast<std::int32_t>(46341u * 46341u)) == 46341 &&
            FT_Sqrt32_nid_postfix(static_cast<std::int32_t>(65535u * 65535u - 1u)) == 65534 &&
            FT_Sqrt32_nid_postfix(static_cast<std::int32_t>(65535u * 65535u)) == 65535 && FT_Sqrt32_nid_postfix(-1) == 65535,
        "FT_Sqrt32 treats negative inputs as unsigned 32-bit values");
}

void CheckHighPow2() {
    Require(ft_highpow2_nid_postfix(0) == 0, "ft_highpow2 of 0 is 0");
    Require(ft_highpow2_nid_postfix(1) == 1 && ft_highpow2_nid_postfix(2) == 2 && ft_highpow2_nid_postfix(0x10) == 0x10, "ft_highpow2 keeps a power of two");
    Require(ft_highpow2_nid_postfix(3) == 2 && ft_highpow2_nid_postfix(6) == 4, "ft_highpow2 clears the lower bits");
    Require(ft_highpow2_nid_postfix(0x12345678) == 0x10000000, "ft_highpow2 keeps the highest set bit");
    Require(ft_highpow2_nid_postfix(0x80000001u) == 0x80000000u && ft_highpow2_nid_postfix(0xFFFFFFFFu) == 0x80000000u, "ft_highpow2 keeps bit 31");
}

void CheckSynthesizeVerticalMetrics() {
    FreeType::GuestGlyphMetrics metrics{100, 200, 10, 50, 80, 0, 0, 0};
    ft_synthesize_vertical_metrics_nid_postfix(&metrics, 0);
    Require(metrics.width == 100 && metrics.height == 200 && metrics.horiBearingX == 10 && metrics.horiBearingY == 50 && metrics.horiAdvance == 80,
        "ft_synthesize_vertical_metrics keeps the horizontal metrics");
    Require(metrics.vertBearingX == -30 && metrics.vertBearingY == 15 && metrics.vertAdvance == 180,
        "ft_synthesize_vertical_metrics synthesizes vertical metrics from a zero advance");
    FreeType::GuestGlyphMetrics negative{0, 200, 0, -30, 0, 0, 0, 0};
    ft_synthesize_vertical_metrics_nid_postfix(&negative, 0);
    Require(negative.vertBearingX == 0 && negative.vertBearingY == 20 && negative.vertAdvance == 240,
        "ft_synthesize_vertical_metrics keeps the height for a negative horiBearingY");
    FreeType::GuestGlyphMetrics clamped{0, -100, 0, -30, 0, 0, 0, 0};
    ft_synthesize_vertical_metrics_nid_postfix(&clamped, 0);
    Require(clamped.vertBearingX == 0 && clamped.vertBearingY == -3 && clamped.vertAdvance == -36,
        "ft_synthesize_vertical_metrics clamps the height to a negative horiBearingY");
    FreeType::GuestGlyphMetrics sized{0, 200, 4, 0, 81, 0, 0, 0};
    ft_synthesize_vertical_metrics_nid_postfix(&sized, 0);
    Require(sized.vertBearingX == -36 && sized.vertBearingY == 20 && sized.vertAdvance == 240,
        "ft_synthesize_vertical_metrics halves an odd horiAdvance toward zero with a zero horiBearingY");
    FreeType::GuestGlyphMetrics explicitAdvance{100, 200, 10, 50, 80, 0, 0, 0};
    ft_synthesize_vertical_metrics_nid_postfix(&explicitAdvance, 300);
    Require(explicitAdvance.vertBearingX == -30 && explicitAdvance.vertBearingY == 75 && explicitAdvance.vertAdvance == 300,
        "ft_synthesize_vertical_metrics keeps a non-zero advance");
    ft_synthesize_vertical_metrics_nid_postfix(nullptr, 0);
    ft_synthesize_vertical_metrics_nid_postfix(nullptr, 300);
    static const FreeType::GuestGlyphMetrics readOnlyMetrics{100, 200, 10, 50, 80, 0, 0, 0};
    errno = 71;
    ft_synthesize_vertical_metrics_nid_postfix(reinterpret_cast<FreeType::GuestGlyphMetrics*>(0x10), 0);
    Require(errno == 71, "ft_synthesize_vertical_metrics preserves errno on an unreadable metrics pointer");
    ft_synthesize_vertical_metrics_nid_postfix(const_cast<FreeType::GuestGlyphMetrics*>(&readOnlyMetrics), 0);
    Require(errno == 71 && readOnlyMetrics.vertBearingX == 0 && readOnlyMetrics.vertBearingY == 0 && readOnlyMetrics.vertAdvance == 0,
        "ft_synthesize_vertical_metrics ignores a read-only metrics pointer without changing errno");
}

void CheckServiceListLookup() {
    int dataA = 1;
    int dataB = 2;
    const GuestServiceDesc descriptors[]{{"a", &dataA}, {"b", &dataB}, {nullptr, nullptr}};
    Require(ft_service_list_lookup_nid_postfix(descriptors, "a") == &dataA, "ft_service_list_lookup returns the first descriptor data");
    Require(ft_service_list_lookup_nid_postfix(descriptors, "b") == &dataB, "ft_service_list_lookup returns the matching descriptor data");
    Require(ft_service_list_lookup_nid_postfix(descriptors, "c") == nullptr, "ft_service_list_lookup returns null for a missing id");
    Require(ft_service_list_lookup_nid_postfix(descriptors, "") == nullptr, "ft_service_list_lookup returns null for an empty id");
    Require(ft_service_list_lookup_nid_postfix(nullptr, "b") == nullptr, "ft_service_list_lookup returns null for a null array");
    Require(ft_service_list_lookup_nid_postfix(descriptors, nullptr) == nullptr, "ft_service_list_lookup returns null for a null id");
    Require(ft_service_list_lookup_nid_postfix(nullptr, nullptr) == nullptr, "ft_service_list_lookup returns null for null inputs");
}

struct GuardedRegion {
    char* data = nullptr;
    std::size_t pageSize = 0;

    GuardedRegion() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize != 0 ? static_cast<std::size_t>(info.dwPageSize) : static_cast<std::size_t>(4096);
        data = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(data != nullptr, "allocate guarded pages");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "query page size");
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "allocate guarded pages");
        data = static_cast<char*>(mapped);
#endif
    }

    ~GuardedRegion() {
        if (!data) return;
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, pageSize * 2);
#endif
    }

    void ProtectNoAccess(std::size_t pageIndex) {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(data + pageIndex * pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "protect no-access page");
#else
        Require(mprotect(data + pageIndex * pageSize, pageSize, PROT_NONE) == 0, "protect no-access page");
#endif
    }
};

void CheckExportGuestPointers() {
    GuardedRegion region;
    char* rwPage = region.data;
    char* noAccessPage = region.data + region.pageSize;

    region.ProtectNoAccess(1);

    int dataA = 1;
    const GuestServiceDesc validDescriptors[]{{"svc", &dataA}, {nullptr, nullptr}};
    errno = 71;
    Require(ThrowsInvalidArgument([&] { ft_service_list_lookup_nid_postfix(reinterpret_cast<const GuestServiceDesc*>(0x10), "svc"); }) &&
            errno == 71, "ft_service_list_lookup rejects an inaccessible descriptor array without changing errno");
    errno = 71;
    Require(ThrowsInvalidArgument([&] { ft_service_list_lookup_nid_postfix(validDescriptors, reinterpret_cast<const char*>(0x10)); }) &&
            errno == 71, "ft_service_list_lookup rejects an inaccessible service id without changing errno");

    char* boundaryId = noAccessPage - 4;
    std::memcpy(boundaryId, "svc", 4);
    const GuestServiceDesc boundaryDescriptors[]{{boundaryId, &dataA}, {nullptr, nullptr}};
    Require(ft_service_list_lookup_nid_postfix(boundaryDescriptors, boundaryId) == &dataA,
        "ft_service_list_lookup accepts NUL-terminated strings ending at the page boundary");

    char* unterminatedId = noAccessPage - 3;
    std::memcpy(unterminatedId, "svc", 3);
    errno = 71;
    Require(ThrowsInvalidArgument([&] { ft_service_list_lookup_nid_postfix(validDescriptors, unterminatedId); }) &&
            errno == 71, "ft_service_list_lookup rejects an unterminated service id crossing into a guard page");
    const GuestServiceDesc badEntryDescriptors[]{{unterminatedId, &dataA}, {nullptr, nullptr}};
    errno = 71;
    Require(ThrowsInvalidArgument([&] { ft_service_list_lookup_nid_postfix(badEntryDescriptors, "svc"); }) &&
            errno == 71, "ft_service_list_lookup rejects an unterminated descriptor id crossing into a guard page");

    auto* boundaryArray = reinterpret_cast<GuestServiceDesc*>(noAccessPage - sizeof(GuestServiceDesc));
    boundaryArray[0] = {"other", &dataA};
    errno = 71;
    Require(ThrowsInvalidArgument([&] { ft_service_list_lookup_nid_postfix(boundaryArray, "missing"); }) &&
            errno == 71, "ft_service_list_lookup rejects a descriptor array that runs into a guard page");

    std::memcpy(rwPage, "svc", 4);
    boundaryArray[0] = {rwPage, &dataA};
    Require(ft_service_list_lookup_nid_postfix(boundaryArray, "svc") == &dataA,
        "ft_service_list_lookup stops on a match at the end of a readable page");
}

struct CornerVector {
    std::int64_t inX;
    std::int64_t inY;
    std::int64_t outX;
    std::int64_t outY;
    int expected;
};

void CheckCornerOrientationMeasured() {
    const CornerVector vectors[] = {
        {5, 0, 3, 7, 7},
        {-5, 0, 3, 7, -7},
        {5, 0, 3, -7, -7},
        {-5, 0, 3, -7, 7},
        {3, 5, 7, 3, -1},
        {3, -5, 7, 3, 1},
        {3, 5, -7, 3, 1},
        {3, -5, -7, 3, -1},
        {3, 5, 7, 0, 5},
        {3, 5, -7, 0, -5},
        {3, -5, 7, 0, -5},
        {3, -5, -7, 0, 5},
        {5, 3, 0, 7, -5},
        {-5, 3, 0, 7, 5},
        {5, 3, 0, -7, 5},
        {-5, 3, 0, -7, -5},
        {5, 0, 0, 5, 5},
        {100, 0, 0, 100, 100},
        {2147483647LL, 0, 0, 2147483647LL, 2147483647},
        {1099511627776LL, 0, 0, 1099511627776LL, 0},
        {-2147483647LL, 0, 0, 2147483647LL, -2147483647},
        {2147483647LL, 0, 0, -2147483647LL, -2147483647},
        {0, 5, 7, 3, -7},
        {0, -5, 7, 3, 7},
        {0, 5, -7, 3, 7},
        {0, -5, -7, 3, -7},
        {0, 5, 3, 7, -3},
        {0, -5, 3, -7, 3},
        {0, 100, 100, 100, -100},
        {0, 2147483647LL, 5, 5, -5},
        {0, 1099511627776LL, 5, 5, -5},
        {0, 5, 7, 0, -7},
        {0, 5, 0, 7, 0},
    };
    for (const CornerVector& vector : vectors) {
        Require(ft_corner_orientation_nid_postfix(vector.inX, vector.inY, vector.outX, vector.outY) == vector.expected,
            "ft_corner_orientation returns the value measured on a PS5");
    }
}

}  // namespace

int main() {
    Require(ft_corner_orientation_nid_postfix(1, 0, 0, 1) == 1, "counter-clockwise 90-degree turn is +1");
    Require(ft_corner_orientation_nid_postfix(0, 1, 1, 0) == -1, "clockwise 90-degree turn is -1");
    Require(ft_corner_orientation_nid_postfix(1, 2, 2, 4) == 0, "collinear parallel vectors are 0");
    Require(ft_corner_orientation_nid_postfix(1, 0, -1, 0) == 0, "collinear opposite vectors are 0");
    CheckCornerOrientationMeasured();

    Require(ft_corner_is_flat_nid_postfix(100, 0, 100, 0) == 1, "parallel vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(1000, 1, 1000, -1) == 1, "nearly parallel vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(100, 0, 0, 100) == 0, "perpendicular vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(100, 0, -100, 0) == 0, "reversing vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(100000, 0, 1, 10) == 1, "dominant in vector is flat");
    Require(ft_corner_is_flat_nid_postfix(1, 10, 100000, 0) == 1, "dominant out vector is flat");
    Require(ft_corner_is_flat_nid_postfix(0, 0, 0, 0) == 0, "zero vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(-100, -50, -200, -100) == 1, "negative coordinates flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 0, 1, 0) == 1, "INT64_MAX boundary does not overflow");
    Require(ft_corner_is_flat_nid_postfix(INT64_MIN, 0, -1, 0) == 1, "INT64_MIN boundary does not overflow");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, INT64_MAX, 1, 1) == 1, "large positive diagonal boundary");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, INT64_MAX, INT64_MAX, INT64_MAX) == 1, "identical full-range vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 0, 0, INT64_MAX) == 0, "full-range perpendicular vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 1, 1, INT64_MAX) == 0, "full-range near-perpendicular vectors are not flat");

    CheckMulDivNoRound();
    CheckMatrixMultiplyScaled();
    CheckVectorTransformScaled();
    CheckSqrtFixed();
    CheckSqrt32();
    CheckHighPow2();
    CheckSynthesizeVerticalMetrics();
    CheckServiceListLookup();
    CheckExportGuestPointers();

    std::puts("Guest FreeType corner tests passed");
    return 0;
}
