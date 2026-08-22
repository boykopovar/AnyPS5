#include <cmath>

extern "C" {

float sinf_nid_postfix(float x) { return std::sin(x); }
float cosf_nid_postfix(float x) { return std::cos(x); }

void sincosf_nid_postfix(float x, float* sinp, float* cosp) {
    *sinp = std::sin(x);
    *cosp = std::cos(x);
}

double sin_nid_postfix(double x) { return std::sin(x); }
double cos_nid_postfix(double x) { return std::cos(x); }

void sincos_nid_postfix(double x, double* sinp, double* cosp) {
    *sinp = std::sin(x);
    *cosp = std::cos(x);
}

float atanf_nid_postfix(float x) { return std::atan(x); }
double atan2_nid_postfix(double y, double x) { return std::atan2(y, x); }
float powf_nid_postfix(float base, float exp) { return std::pow(base, exp); }
double pow_nid_postfix(double base, double exp) { return std::pow(base, exp); }
float expf_nid_postfix(float x) { return std::exp(x); }
float exp2f_nid_postfix(float x) { return std::exp2(x); }
float logf_nid_postfix(float x) { return std::log(x); }
float log2f_nid_postfix(float x) { return std::log2(x); }
double log10_nid_postfix(double x) { return std::log10(x); }
float ldexpf_nid_postfix(float x, int exp) { return std::ldexp(x, exp); }
double fmod_nid_postfix(double x, double y) { return std::fmod(x, y); }
float roundf_nid_postfix(float x) { return std::round(x); }
double round_nid_postfix(double x) { return std::round(x); }

int __isfinite_nid_postfix(double x) { return std::isfinite(x) ? 1 : 0; }
int __isnan_nid_postfix(double x) { return std::isnan(x) ? 1 : 0; }
int __signbit_nid_postfix(double x) { return std::signbit(x) ? 1 : 0; }

}
