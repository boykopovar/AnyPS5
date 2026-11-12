#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI _Atan_nid_postfix();
int APS5_VABI _Dtento_nid_postfix();
int APS5_VABI _Erfc_nid_postfix();
int APS5_VABI _FAtan_nid_postfix();
int APS5_VABI _FDtento_nid_postfix();
int APS5_VABI _FErfc_nid_postfix();
int APS5_VABI _FHypot_nid_postfix();
int APS5_VABI _FLogpoly_nid_postfix();
int APS5_VABI _FPow_nid_postfix();
int APS5_VABI _FQuad_nid_postfix();
int APS5_VABI _FQuadph_nid_postfix();
int APS5_VABI _FRint_nid_postfix();
int APS5_VABI _FSincos_nid_postfix();
int APS5_VABI _FTan_nid_postfix();
int APS5_VABI _FTgamma_nid_postfix();
int APS5_VABI _FXp_addh_nid_postfix();
int APS5_VABI _FXp_addx_nid_postfix();
float APS5_VABI _FXp_getw_nid_postfix(const float* parts, int count);
int APS5_VABI _FXp_invx_nid_postfix();
int APS5_VABI _FXp_ldexpx_nid_postfix();
int APS5_VABI _FXp_movx_nid_postfix();
int APS5_VABI _FXp_mulh_nid_postfix();
int APS5_VABI _FXp_mulx_nid_postfix();
int APS5_VABI _FXp_setn_nid_postfix();
int APS5_VABI _FXp_setw_nid_postfix();
int APS5_VABI _FXp_sqrtx_nid_postfix();
int APS5_VABI _FXp_subx_nid_postfix();
int APS5_VABI _Hypot_nid_postfix();
int APS5_VABI _LAtan_nid_postfix();
int APS5_VABI _LDtento_nid_postfix();
int APS5_VABI _LErfc_nid_postfix();
int APS5_VABI _LHypot_nid_postfix();
int APS5_VABI _LLogpoly_nid_postfix();
int APS5_VABI _LPow_nid_postfix();
int APS5_VABI _LQuad_nid_postfix();
int APS5_VABI _LQuadph_nid_postfix();
int APS5_VABI _LRint_nid_postfix();
int APS5_VABI _LSincos_nid_postfix();
int APS5_VABI _LTan_nid_postfix();
int APS5_VABI _LTgamma_nid_postfix();
int APS5_VABI _LXp_addh_nid_postfix();
int APS5_VABI _LXp_addx_nid_postfix();
int APS5_VABI _LXp_getw_nid_postfix();
int APS5_VABI _LXp_invx_nid_postfix();
int APS5_VABI _LXp_ldexpx_nid_postfix();
int APS5_VABI _LXp_movx_nid_postfix();
int APS5_VABI _LXp_mulh_nid_postfix();
int APS5_VABI _LXp_mulx_nid_postfix();
int APS5_VABI _LXp_setn_nid_postfix();
int APS5_VABI _LXp_setw_nid_postfix();
int APS5_VABI _LXp_sqrtx_nid_postfix();
int APS5_VABI _LXp_subx_nid_postfix();
int APS5_VABI _Logpoly_nid_postfix();
int APS5_VABI _Nnl_nid_postfix();
int APS5_VABI _Pow_nid_postfix();
int APS5_VABI _Quad_nid_postfix();
int APS5_VABI _Quadph_nid_postfix();
double APS5_VABI _Xp_getw_nid_postfix(const double* p, int n);
double* APS5_VABI _Xp_setn_nid_postfix(double* p, int n, std::int64_t x);
double* APS5_VABI _Xp_setw_nid_postfix(double* p, int n, double x);
double* APS5_VABI _Xp_addh_nid_postfix(double* p, int n, double x0);
double* APS5_VABI _Xp_mulh_nid_postfix(double* p, int n, double x0);
double* APS5_VABI _Xp_movx_nid_postfix(double* p, int n, const double* q);
double* APS5_VABI _Xp_addx_nid_postfix(double* p, int n, const double* q, int m);
double* APS5_VABI _Xp_subx_nid_postfix(double* p, int n, const double* q, int m);
double* APS5_VABI _Xp_ldexpx_nid_postfix(double* p, int n, int m);
double* APS5_VABI _Xp_mulx_nid_postfix(double* p, int n, const double* q, int m, double* ptemp2);
double* APS5_VABI _Xp_invx_nid_postfix(double* p, int n, double* ptemp4);
double* APS5_VABI _Xp_sqrtx_nid_postfix(double* p, int n, double* ptemp4);

}
