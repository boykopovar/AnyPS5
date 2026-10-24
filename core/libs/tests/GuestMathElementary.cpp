#include "prx/libc/include/general/VabiMacros.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

extern "C" {
double APS5_VABI cbrt_nid_postfix(double);
float APS5_VABI cbrtf_nid_postfix(float);
double APS5_VABI acos_nid_postfix(double);
double APS5_VABI atan_nid_postfix(double);
float APS5_VABI atanf_nid_postfix(float);
double APS5_VABI tan_nid_postfix(double);
float APS5_VABI tanf_nid_postfix(float);
double APS5_VABI log2_nid_postfix(double);
float APS5_VABI log2f_nid_postfix(float);
void APS5_VABI sincos_nid_postfix(double, double*, double*);
void APS5_VABI sincosf_nid_postfix(float, float*, float*);
}

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "elementary math check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

constexpr double Infinity = std::numeric_limits<double>::infinity();
constexpr float InfinityF = std::numeric_limits<float>::infinity();
constexpr double NotANumber = std::numeric_limits<double>::quiet_NaN();
constexpr float NotANumberF = std::numeric_limits<float>::quiet_NaN();
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr float PiF = 3.14159265358979323846f;

static bool Near(double actual, double expected, double ulps) {
    return std::fabs(actual - expected) <= ulps * std::numeric_limits<double>::epsilon() * std::fabs(expected);
}

static bool NearF(float actual, float expected, float ulps) {
    return std::fabs(actual - expected) <= ulps * std::numeric_limits<float>::epsilon() * std::fabs(expected);
}

int main() {
    Check(Near(cbrt_nid_postfix(27.0), 3.0, 1.0));
    Check(Near(cbrt_nid_postfix(-8.0), -2.0, 1.0));
    Check(cbrt_nid_postfix(1.0) == 1.0);
    Check(cbrt_nid_postfix(0.0) == 0.0 && !std::signbit(cbrt_nid_postfix(0.0)));
    Check(cbrt_nid_postfix(-0.0) == 0.0 && std::signbit(cbrt_nid_postfix(-0.0)));
    Check(cbrt_nid_postfix(Infinity) == Infinity && cbrt_nid_postfix(-Infinity) == -Infinity);
    Check(std::isnan(cbrt_nid_postfix(NotANumber)));
    Check(NearF(cbrtf_nid_postfix(27.0f), 3.0f, 1.0f) && NearF(cbrtf_nid_postfix(-8.0f), -2.0f, 1.0f));
    Check(std::signbit(cbrtf_nid_postfix(-0.0f)) && cbrtf_nid_postfix(-0.0f) == 0.0f);
    Check(cbrtf_nid_postfix(InfinityF) == InfinityF && std::isnan(cbrtf_nid_postfix(NotANumberF)));

    Check(acos_nid_postfix(1.0) == 0.0 && !std::signbit(acos_nid_postfix(1.0)));
    Check(Near(acos_nid_postfix(0.0), Pi / 2, 1.0));
    Check(Near(acos_nid_postfix(-1.0), Pi, 1.0));
    Check(Near(acos_nid_postfix(0.5), Pi / 3, 1.0));
    Check(std::isnan(acos_nid_postfix(1.0000001)) && std::isnan(acos_nid_postfix(-2.0)));
    Check(std::isnan(acos_nid_postfix(Infinity)) && std::isnan(acos_nid_postfix(NotANumber)));

    Check(atan_nid_postfix(0.0) == 0.0 && !std::signbit(atan_nid_postfix(0.0)));
    Check(atan_nid_postfix(-0.0) == 0.0 && std::signbit(atan_nid_postfix(-0.0)));
    Check(Near(atan_nid_postfix(1.0), Pi / 4, 1.0) && Near(atan_nid_postfix(-1.0), -Pi / 4, 1.0));
    Check(Near(atan_nid_postfix(Infinity), Pi / 2, 1.0) && Near(atan_nid_postfix(-Infinity), -Pi / 2, 1.0));
    Check(std::isnan(atan_nid_postfix(NotANumber)));
    Check(atanf_nid_postfix(0.0f) == 0.0f && std::signbit(atanf_nid_postfix(-0.0f)));
    Check(NearF(atanf_nid_postfix(1.0f), PiF / 4, 1.0f) && NearF(atanf_nid_postfix(-InfinityF), -PiF / 2, 1.0f));
    Check(std::isnan(atanf_nid_postfix(NotANumberF)));

    Check(tan_nid_postfix(0.0) == 0.0 && !std::signbit(tan_nid_postfix(0.0)));
    Check(tan_nid_postfix(-0.0) == 0.0 && std::signbit(tan_nid_postfix(-0.0)));
    Check(Near(tan_nid_postfix(Pi / 4), 1.0, 2.0));
    Check(Near(tan_nid_postfix(-Pi / 4), -1.0, 2.0));
    Check(std::isnan(tan_nid_postfix(Infinity)) && std::isnan(tan_nid_postfix(-Infinity)) && std::isnan(tan_nid_postfix(NotANumber)));
    Check(tanf_nid_postfix(0.0f) == 0.0f && std::signbit(tanf_nid_postfix(-0.0f)));
    Check(NearF(tanf_nid_postfix(PiF / 4), 1.0f, 2.0f));
    Check(std::isnan(tanf_nid_postfix(InfinityF)));

    Check(log2_nid_postfix(1.0) == 0.0 && !std::signbit(log2_nid_postfix(1.0)));
    Check(log2_nid_postfix(2.0) == 1.0 && log2_nid_postfix(8.0) == 3.0 && log2_nid_postfix(1024.0) == 10.0);
    Check(log2_nid_postfix(0.5) == -1.0 && log2_nid_postfix(0.125) == -3.0);
    Check(log2_nid_postfix(0.0) == -Infinity && log2_nid_postfix(-0.0) == -Infinity);
    Check(log2_nid_postfix(Infinity) == Infinity);
    Check(std::isnan(log2_nid_postfix(-1.0)) && std::isnan(log2_nid_postfix(-Infinity)) && std::isnan(log2_nid_postfix(NotANumber)));
    Check(log2f_nid_postfix(1.0f) == 0.0f && log2f_nid_postfix(8.0f) == 3.0f && log2f_nid_postfix(0.25f) == -2.0f);
    Check(log2f_nid_postfix(0.0f) == -InfinityF && log2f_nid_postfix(InfinityF) == InfinityF);
    Check(std::isnan(log2f_nid_postfix(-1.0f)) && std::isnan(log2f_nid_postfix(NotANumberF)));

    {
        double sine = 9.0;
        double cosine = 9.0;
        sincos_nid_postfix(0.0, &sine, &cosine);
        Check(sine == 0.0 && !std::signbit(sine) && cosine == 1.0);
        sincos_nid_postfix(-0.0, &sine, &cosine);
        Check(sine == 0.0 && std::signbit(sine) && cosine == 1.0);
        sincos_nid_postfix(Pi / 2, &sine, &cosine);
        Check(sine == 1.0 && std::fabs(cosine) < 1.0e-15);
        sincos_nid_postfix(Pi, &sine, &cosine);
        Check(std::fabs(sine) < 1.0e-15 && cosine == -1.0);
        sincos_nid_postfix(Pi / 6, &sine, &cosine);
        Check(Near(sine, 0.5, 2.0) && Near(cosine, 0.8660254037844386, 2.0));
        sincos_nid_postfix(Infinity, &sine, &cosine);
        Check(std::isnan(sine) && std::isnan(cosine));
        sincos_nid_postfix(NotANumber, &sine, &cosine);
        Check(std::isnan(sine) && std::isnan(cosine));
    }
    {
        float sine = 9.0f;
        float cosine = 9.0f;
        sincosf_nid_postfix(0.0f, &sine, &cosine);
        Check(sine == 0.0f && !std::signbit(sine) && cosine == 1.0f);
        sincosf_nid_postfix(-0.0f, &sine, &cosine);
        Check(sine == 0.0f && std::signbit(sine) && cosine == 1.0f);
        sincosf_nid_postfix(PiF / 2, &sine, &cosine);
        Check(sine == 1.0f && std::fabs(cosine) < 1.0e-6f);
        sincosf_nid_postfix(PiF / 6, &sine, &cosine);
        Check(NearF(sine, 0.5f, 2.0f) && NearF(cosine, 0.8660254f, 2.0f));
        sincosf_nid_postfix(InfinityF, &sine, &cosine);
        Check(std::isnan(sine) && std::isnan(cosine));
        sincosf_nid_postfix(NotANumberF, &sine, &cosine);
        Check(std::isnan(sine) && std::isnan(cosine));
    }
    return 0;
}
