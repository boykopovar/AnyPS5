#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ExtendedPrecisionParts.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

using namespace LibcDetail;

namespace {

constexpr int GuestEdom = 33;
constexpr int GuestErange = 34;

}

template<>
struct LibcDetail::Precision<double> {
    using Bits = std::uint64_t;
    static constexpr short PartBits = 26;
    static constexpr std::int64_t FirstExponent = 0x800;
    static constexpr Bits NanBits = 0x7ff8000000000000ull;
    static short Test(double* value) { return _Dtest_nid_postfix(value); }
    static short Unscale(short* exponent, double* value) { return _ZNSt8__sce_v29_DunscaleEPsPd_nid_postfix(exponent, value); }
    static void Truncate(double* value, short bits) { _ZNSt8__sce_v25_DintEPds_nid_postfix(value, bits); }
    static void Scale(double* value, std::int64_t exponent) { _ZNSt8__sce_v27_DscaleEPdl_nid_postfix(value, exponent); }
    static double Ldexp(double value, int exponent) { return ldexp_nid_postfix(value, exponent); }
    static void Raise(int exceptions) { _ZNSt8__sce_v28_FeraiseEi_nid_postfix(exceptions); }
};

template<>
struct LibcDetail::Precision<float> {
    using Bits = std::uint32_t;
    static constexpr short PartBits = 12;
    static constexpr std::int64_t FirstExponent = 0x100;
    static constexpr Bits NanBits = 0x7fc00000u;
    static short Test(float* value) { return _FDtest_nid_postfix(value); }
    static short Unscale(short* exponent, float* value) { return _ZNSt8__sce_v210_FDunscaleEPsPf_nid_postfix(exponent, value); }
    static void Truncate(float* value, short bits) { _ZNSt8__sce_v26_FDintEPfs_nid_postfix(value, bits); }
    static void Scale(float* value, std::int64_t exponent) { _ZNSt8__sce_v28_FDscaleEPfl_nid_postfix(value, exponent); }
    static float Ldexp(float value, int exponent) { return ldexpf_nid_postfix(value, exponent); }
    static void Raise(int exceptions) { _ZNSt8__sce_v28_FeraiseEi_nid_postfix(exceptions); }
};

extern "C" {

void APS5_VABI _ZNSt8__sce_v28_FeraiseEi_nid_postfix(int exceptions) {
    if ((exceptions & 0x05) != 0) *__error_nid_postfix() = GuestEdom;
    else if ((exceptions & 0x18) != 0) *__error_nid_postfix() = GuestErange;
}

double* APS5_VABI _ZNSt8__sce_v28_Xp_setwEPdid_nid_postfix(double* p, int n, double x) { return SetWide(p, n, x); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_setwEPfif_nid_postfix(float* p, int n, float x) { return SetWide(p, n, x); }
double APS5_VABI _ZNSt8__sce_v28_Xp_getwEPKdi_nid_postfix(const double* p, int n) { return GetWide(p, n); }
float APS5_VABI _ZNSt8__sce_v29_FXp_getwEPKfi_nid_postfix(const float* p, int n) { return GetWide(p, n); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_setnEPdil_nid_postfix(double* p, int n, std::int64_t value) { return SetInteger(p, n, value); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_setnEPfil_nid_postfix(float* p, int n, std::int64_t value) { return SetInteger(p, n, value); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_movxEPdiPKd_nid_postfix(double* p, int n, const double* q) { return Move(p, n, q); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_movxEPfiPKf_nid_postfix(float* p, int n, const float* q) { return Move(p, n, q); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_addhEPdid_nid_postfix(double* p, int n, double x) { return AddHalf(p, n, x); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_addhEPfif_nid_postfix(float* p, int n, float x) { return AddHalf(p, n, x); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_addxEPdiPKdi_nid_postfix(double* p, int n, const double* q, int m) { return AddWide(p, n, q, m); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_addxEPfiPKfi_nid_postfix(float* p, int n, const float* q, int m) { return AddWide(p, n, q, m); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_mulhEPdid_nid_postfix(double* p, int n, double x) { return MultiplyHalf(p, n, x); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_mulhEPfif_nid_postfix(float* p, int n, float x) { return MultiplyHalf(p, n, x); }
double* APS5_VABI _ZNSt8__sce_v28_Xp_mulxEPdiPKdiS0__nid_postfix(double* p, int n, const double* q, int m, double* scratch) { return MultiplyWide(p, n, q, m, scratch); }
float* APS5_VABI _ZNSt8__sce_v29_FXp_mulxEPfiPKfiS0__nid_postfix(float* p, int n, const float* q, int m, float* scratch) { return MultiplyWide(p, n, q, m, scratch); }
double* APS5_VABI _ZNSt8__sce_v210_Xp_ldexpxEPdii_nid_postfix(double* p, int n, int exponent) { return ScaleWide(p, n, exponent); }
float* APS5_VABI _ZNSt8__sce_v211_FXp_ldexpxEPfii_nid_postfix(float* p, int n, int exponent) { return ScaleWide(p, n, exponent); }

}
