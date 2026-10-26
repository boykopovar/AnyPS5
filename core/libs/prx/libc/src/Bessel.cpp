#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
#include <limits>

#include "prx/libc/include/general/VabiMacros.hpp"

namespace {

constexpr double One = 1.0;
constexpr double Huge = 1.0e300;
constexpr double InverseSqrtPi = 5.64189583547756279280e-01;
constexpr double TwoOverPi = 6.36619772367581382433e-01;
constexpr double LogMax = 7.09782712893383973096e+02;

constexpr double PZeroNumerators[4][6] = {
    {0.00000000000000000000e+00, -7.03124999999900357484e-02, -8.08167041275349795626e+00, -2.57063105679704847262e+02, -2.48521641009428822144e+03, -5.25304380490729545272e+03},
    {-1.14125464691894502584e-11, -7.03124940873599280078e-02, -4.15961064470587782438e+00, -6.76747652265167261021e+01, -3.31231299649172967747e+02, -3.46433388365604912451e+02},
    {-2.54704601771951915620e-09, -7.03119616381481654654e-02, -2.40903221549529611423e+00, -2.19659774734883086467e+01, -5.80791704701737572236e+01, -3.14479470594888503854e+01},
    {-8.87534333032526411254e-08, -7.03030995483624743247e-02, -1.45073846780952986357e+00, -7.63569613823527770791e+00, -1.11931668860356747786e+01, -3.23364579351335335033e+00},
};
constexpr double PZeroDenominators[4][5] = {
    {1.16534364619668181717e+02, 3.83374475364121826715e+03, 4.05978572648472545552e+04, 1.16752972564375915681e+05, 4.76277284146730962675e+04},
    {6.07539382692300335975e+01, 1.05125230595704579173e+03, 5.97897094333855784498e+03, 9.62544514357774460223e+03, 2.40605815922939109441e+03},
    {3.58560338055209726349e+01, 3.61513983050303863820e+02, 1.19360783792111533330e+03, 1.12799679856907414432e+03, 1.73580930813335754692e+02},
    {2.22202997532088808441e+01, 1.36206794218215208048e+02, 2.70470278658083486789e+02, 1.53875394208320329881e+02, 1.46576176948256193810e+01},
};
constexpr double QZeroNumerators[4][6] = {
    {0.00000000000000000000e+00, 7.32421874999935051953e-02, 1.17682064682252693899e+01, 5.57673380256401856059e+02, 8.85919720756468632317e+03, 3.70146267776887834771e+04},
    {1.84085963594515531381e-11, 7.32421766612684765896e-02, 5.83563508962056953777e+00, 1.35111577286449829671e+02, 1.02724376596164097464e+03, 1.98997785864605384631e+03},
    {4.37741014089738620906e-09, 7.32411180042911447163e-02, 3.34423137516170720929e+00, 4.26218440745412650017e+01, 1.70808091340565596283e+02, 1.66733948696651168575e+02},
    {1.50444444886983272379e-07, 7.32234265963079278272e-02, 1.99819174093815998816e+00, 1.44956029347885735348e+01, 3.16662317504781540833e+01, 1.62527075710929267416e+01},
};
constexpr double QZeroDenominators[4][6] = {
    {1.63776026895689824414e+02, 8.09834494656449805916e+03, 1.42538291419120476348e+05, 8.03309257119514397345e+05, 8.40501579819060512818e+05, -3.43899293537866615225e+05},
    {8.27766102236537761883e+01, 2.07781416421392987104e+03, 1.88472887785718085070e+04, 5.67511122894947329769e+04, 3.59767538425114471465e+04, -5.35434275601944773371e+03},
    {4.87588729724587182091e+01, 7.09689221056606015736e+02, 3.70414822620111362994e+03, 6.46042516752568917582e+03, 2.51633368920368957333e+03, -1.49247451836156386662e+02},
    {3.03655848355219184498e+01, 2.69348118608049844624e+02, 8.44783757595320139444e+02, 8.82935845112488550512e+02, 2.12666388511798828631e+02, -5.31095493882666946917e+00},
};
constexpr double POneNumerators[4][6] = {
    {0.00000000000000000000e+00, 1.17187499999988647970e-01, 1.32394806593073575129e+01, 4.12051854307378562225e+02, 3.87474538913960532227e+03, 7.91447954031891731574e+03},
    {1.31990519556243522749e-11, 1.17187493190614097638e-01, 6.80275127868432871736e+00, 1.08308182990189109773e+02, 5.17636139533199752805e+02, 5.28715201363337541807e+02},
    {3.02503916137373618024e-09, 1.17186865567253592491e-01, 3.93297750033315640650e+00, 3.51194035591636932736e+01, 9.10550110750781271918e+01, 4.85590685197364919645e+01},
    {1.07710830106873743082e-07, 1.17176219462683348094e-01, 2.36851496667608785174e+00, 1.22426109148261232917e+01, 1.76939711271687727390e+01, 5.07352312588818499250e+00},
};
constexpr double POneDenominators[4][5] = {
    {1.14207370375678408436e+02, 3.65093083420853463394e+03, 3.69562060269033463555e+04, 9.76027935934950801311e+04, 3.08042720627888811578e+04},
    {5.92805987221131331921e+01, 9.91401418733614377743e+02, 5.35326695291487976647e+03, 7.84469031749551231769e+03, 1.50404688810361062679e+03},
    {3.47913095001251519989e+01, 3.36762458747825746741e+02, 1.04687139975775130551e+03, 8.90811346398256432622e+02, 1.03787932439639277504e+02},
    {2.14364859363821409488e+01, 1.25290227168402751090e+02, 2.32276469057162813669e+02, 1.17679373287147100768e+02, 8.36463893371618283368e+00},
};
constexpr double QOneNumerators[4][6] = {
    {0.00000000000000000000e+00, -1.02539062499992714161e-01, -1.62717534544589987888e+01, -7.59601722513950107896e+02, -1.18498066702429587167e+04, -4.84385124285750353010e+04},
    {-2.08979931141764104297e-11, -1.02539050241375426231e-01, -8.05644828123936029840e+00, -1.83669607474888380239e+02, -1.37319376065508163265e+03, -2.61244440453215656817e+03},
    {-5.07831226461766561369e-09, -1.02537829820837089745e-01, -4.61011581139473403113e+00, -5.78472216562783643212e+01, -2.28244540737631695038e+02, -2.19210128478909325622e+02},
    {-1.78381727510958865572e-07, -1.02517042607985553460e-01, -2.75220568278187460720e+00, -1.96636162643703720221e+01, -4.23253133372830490089e+01, -2.13719211703704061733e+01},
};
constexpr double QOneDenominators[4][6] = {
    {1.61395369700722909556e+02, 7.82538599923348465381e+03, 1.33875336287249578163e+05, 7.19657723683240939863e+05, 6.66601232617776375264e+05, -2.94490264303834643215e+05},
    {8.12765501384335777857e+01, 1.99179873460485964642e+03, 1.74684851924908907677e+04, 4.98514270910352279316e+04, 2.79480751638918118260e+04, -4.71918354795128470869e+03},
    {4.76651550323729509273e+01, 6.73865112676699709482e+02, 3.38015286679526343505e+03, 5.54772909720722782367e+03, 1.90311919338810798763e+03, -1.35201191444307340817e+02},
    {2.95333629060523854548e+01, 2.52981549982190529136e+02, 7.57502834868645436472e+02, 7.39393205320467245656e+02, 1.55949003336666123687e+02, -4.95949898822628210127e+00},
};

constexpr double J0Numerator[] = {1.56249999999999947958e-02, -1.89979294238854721751e-04, 1.82954049532700665670e-06, -4.61832688532103189199e-09};
constexpr double J0Denominator[] = {1.56191029464890010492e-02, 1.16926784663337450260e-04, 5.13546550207318111446e-07, 1.16614003333790000205e-09};
constexpr double Y0Numerator[] = {
    -7.38042951086872317523e-02, 1.76666452509181115538e-01, -1.38185671945596898896e-02, 3.47453432093683650238e-04,
    -3.81407053724364161125e-06, 1.95590137035022920206e-08, -3.98205194132103398453e-11,
};
constexpr double Y0Denominator[] = {1.27304834834123699328e-02, 7.60068627350353253702e-05, 2.59150851840457805467e-07, 4.41110311332675467403e-10};
constexpr double J1Numerator[] = {-6.25000000000000000000e-02, 1.40705666955189706048e-03, -1.59955631084035597520e-05, 4.96727999609584448412e-08};
constexpr double J1Denominator[] = {
    1.91537599538363460805e-02, 1.85946785588630915560e-04, 1.17718464042623683263e-06, 5.04636257076217042715e-09, 1.23542274426137913908e-11,
};
constexpr double Y1Numerator[] = {
    -1.96057090646238940668e-01, 5.04438716639811282616e-02, -1.91256895875763547298e-03, 2.35252600561610495928e-05, -9.19099158039878874504e-08,
};
constexpr double Y1Denominator[] = {
    1.99167318236649903973e-02, 2.02552581025135171496e-04, 1.35608801097516229404e-06, 6.22741452364621501295e-09, 1.66559246207992079114e-11,
};

std::uint32_t HighWord(double x) {
    return static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
}

std::uint32_t LowWord(double x) {
    return static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(x));
}

double SecondKindAtZero() {
    return -std::numeric_limits<double>::infinity();
}

double SecondKindOfNegative() {
    return std::bit_cast<double>(0x7ff8000000000000ull);
}

double Signed(std::uint32_t value) {
    return static_cast<double>(static_cast<std::int32_t>(value));
}

double SquareRoot(double x) {
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(x)));
}

const bool HardwareFma = [] {
    __builtin_cpu_init();
    return __builtin_cpu_supports("fma") != 0;
}();

[[gnu::target("fma")]] double HardwareFused(double a, double b, double c) {
    return _mm_cvtsd_f64(_mm_fmadd_sd(_mm_set_sd(a), _mm_set_sd(b), _mm_set_sd(c)));
}

double Fused(double a, double b, double c) {
    return HardwareFma ? HardwareFused(a, b, c) : std::fma(a, b, c);
}

template <std::size_t Count>
double Horner(double z, const double (&coefficients)[Count]) {
    double result = coefficients[Count - 1];
    for (std::size_t index = Count - 1; index-- > 0;) result = Fused(z, result, coefficients[index]);
    return result;
}

std::size_t AsymptoticRange(std::uint32_t magnitude) {
    if (magnitude >= 0x40200000u) return 0;
    if (magnitude >= 0x40122e8bu) return 1;
    if (magnitude >= 0x4006db6du) return 2;
    return 3;
}

template <std::size_t NumeratorCount, std::size_t DenominatorCount>
double Asymptotic(double x, const double (&numerator)[NumeratorCount], const double (&denominator)[DenominatorCount]) {
    const double z = One / (x * x);
    return Horner(z, numerator) / Fused(z, Horner(z, denominator), One);
}

double PZero(double x, std::uint32_t magnitude) {
    const std::size_t range = AsymptoticRange(magnitude);
    return Asymptotic(x, PZeroNumerators[range], PZeroDenominators[range]) + One;
}

double QZero(double x, std::uint32_t magnitude) {
    const std::size_t range = AsymptoticRange(magnitude);
    return (Asymptotic(x, QZeroNumerators[range], QZeroDenominators[range]) + -0.125) / x;
}

double POne(double x, std::uint32_t magnitude) {
    const std::size_t range = AsymptoticRange(magnitude);
    return Asymptotic(x, POneNumerators[range], POneDenominators[range]) + One;
}

double QOne(double x, std::uint32_t magnitude) {
    const std::size_t range = AsymptoticRange(magnitude);
    return (Asymptotic(x, QOneNumerators[range], QOneDenominators[range]) + 0.375) / x;
}

}

extern "C" {

double APS5_VABI j0_nid_postfix(double x) {
    const std::uint32_t magnitude = HighWord(x) & 0x7fffffffu;
    if (magnitude >= 0x7ff00000u) return One / (x * x);
    const double absolute = std::fabs(x);
    if (magnitude >= 0x40000000u) {
        const double s = std::sin(absolute);
        const double c = std::cos(x);
        double ss = s - c;
        double cc = c + s;
        if (magnitude < 0x7fe00000u) {
            const double z = -std::cos(absolute + absolute);
            if (s * c < 0.0) cc = z / ss;
            else ss = z / cc;
        }
        if (magnitude > 0x48000000u) return cc * InverseSqrtPi / SquareRoot(absolute);
        const double u = PZero(absolute, magnitude);
        const double v = QZero(absolute, magnitude);
        return Fused(u, cc, -(ss * v)) * InverseSqrtPi / SquareRoot(absolute);
    }
    if (Huge + absolute > One && magnitude < 0x3f200000u) {
        if (magnitude < 0x3e400000u) return One;
        return Fused(x * x, -0.25, One);
    }
    const double z = x * x;
    const double r = z * Horner(z, J0Numerator);
    const double s = Fused(z, Horner(z, J0Denominator), One);
    if (magnitude < 0x3ff00000u) return Fused(z, r / s + -0.25, One);
    const double u = absolute * 0.5;
    return Fused(One + u, One - u, z * (r / s));
}

double APS5_VABI y0_nid_postfix(double x) {
    const std::uint32_t high = HighWord(x);
    const std::uint32_t magnitude = high & 0x7fffffffu;
    if (magnitude >= 0x7ff00000u) return One / Fused(x, x, x);
    if ((magnitude | LowWord(x)) == 0) return SecondKindAtZero();
    if ((high & 0x80000000u) != 0) return SecondKindOfNegative();
    if (magnitude >= 0x40000000u) {
        const double s = std::sin(x);
        const double c = std::cos(x);
        double ss = s - c;
        double cc = s + c;
        if (magnitude < 0x7fe00000u) {
            const double z = -std::cos(x + x);
            if (s * c < 0.0) cc = z / ss;
            else ss = z / cc;
        }
        if (magnitude > 0x48000000u) return ss * InverseSqrtPi / SquareRoot(x);
        const double u = PZero(x, magnitude);
        const double v = QZero(x, magnitude);
        return Fused(u, ss, cc * v) * InverseSqrtPi / SquareRoot(x);
    }
    if (magnitude <= 0x3e400000u) return Fused(TwoOverPi, std::log(x), Y0Numerator[0]);
    const double z = x * x;
    const double u = Horner(z, Y0Numerator);
    const double v = Fused(z, Horner(z, Y0Denominator), One);
    return Fused(std::log(x) * j0_nid_postfix(x), TwoOverPi, u / v);
}

double APS5_VABI j1_nid_postfix(double x) {
    const std::uint32_t high = HighWord(x);
    const std::uint32_t magnitude = high & 0x7fffffffu;
    if (magnitude >= 0x7ff00000u) return One / x;
    if (magnitude >= 0x40000000u) {
        const double y = std::fabs(x);
        const double s = std::sin(y);
        const double c = std::cos(x);
        double ss = -s - c;
        double cc = s - c;
        if (magnitude < 0x7fe00000u) {
            const double z = std::cos(y + y);
            if (s * c > 0.0) cc = z / ss;
            else ss = z / cc;
        }
        double result;
        if (magnitude > 0x48000000u) {
            result = cc * InverseSqrtPi / SquareRoot(y);
        } else {
            const double u = POne(y, magnitude);
            const double v = QOne(y, magnitude);
            result = Fused(u, cc, -(ss * v)) * InverseSqrtPi / SquareRoot(y);
        }
        return (high & 0x80000000u) != 0 ? -result : result;
    }
    if (Huge + x > One && magnitude < 0x3e400000u) return x * 0.5;
    const double z = x * x;
    const double r = z * Horner(z, J1Numerator);
    const double s = Fused(z, Horner(z, J1Denominator), One);
    return Fused(x, 0.5, r * x / s);
}

double APS5_VABI y1_nid_postfix(double x) {
    const std::uint32_t high = HighWord(x);
    const std::uint32_t magnitude = high & 0x7fffffffu;
    if (magnitude >= 0x7ff00000u) return One / Fused(x, x, x);
    if ((magnitude | LowWord(x)) == 0) return SecondKindAtZero();
    if ((high & 0x80000000u) != 0) return SecondKindOfNegative();
    if (magnitude >= 0x40000000u) {
        const double s = std::sin(x);
        const double c = std::cos(x);
        double ss = -s - c;
        double cc = s - c;
        if (magnitude < 0x7fe00000u) {
            const double z = std::cos(x + x);
            if (s * c > 0.0) cc = z / ss;
            else ss = z / cc;
        }
        if (magnitude > 0x48000000u) return ss * InverseSqrtPi / SquareRoot(x);
        const double u = POne(x, magnitude);
        const double v = QOne(x, magnitude);
        return Fused(u, ss, cc * v) * InverseSqrtPi / SquareRoot(x);
    }
    if (magnitude <= 0x3c900000u) return -TwoOverPi / x;
    const double z = x * x;
    const double u = Horner(z, Y1Numerator);
    const double v = Fused(z, Horner(z, Y1Denominator), One);
    return Fused(x, u / v, Fused(std::log(x), j1_nid_postfix(x), -One / x) * TwoOverPi);
}

double APS5_VABI jn_nid_postfix(int order, double x) {
    const std::uint32_t high = HighWord(x);
    const std::uint32_t low = LowWord(x);
    const std::uint32_t magnitude = high & 0x7fffffffu;
    if ((magnitude | (low != 0 ? 1u : 0u)) > 0x7ff00000u) return x + x;
    const double argument = order < 0 ? -x : x;
    const std::uint32_t n = order < 0 ? 0u - static_cast<std::uint32_t>(order) : static_cast<std::uint32_t>(order);
    if (n == 1) return j1_nid_postfix(argument);
    if (n == 0) return j0_nid_postfix(argument);
    const bool negative = ((high ^ static_cast<std::uint32_t>(order)) & 0x80000000u) != 0;
    double result = 0.0;
    if (magnitude < 0x7ff00000u && (magnitude | low) != 0) {
        const double absolute = std::fabs(x);
        const double orderValue = Signed(n);
        if (absolute >= orderValue) {
            if (magnitude >= 0x52d00000u) {
                double sum;
                switch (n & 3u) {
                case 0: sum = std::sin(absolute) + std::cos(argument); break;
                case 1: sum = std::sin(absolute) - std::cos(argument); break;
                case 2: sum = -std::cos(argument) - std::sin(absolute); break;
                default: sum = std::cos(argument) - std::sin(absolute); break;
                }
                result = sum * InverseSqrtPi / SquareRoot(absolute);
            } else {
                double previous = j0_nid_postfix(absolute);
                result = j1_nid_postfix(absolute);
                std::uint32_t twice = 2;
                for (std::uint32_t remaining = n - 1; remaining != 0; --remaining) {
                    const double next = Fused(result, Signed(twice) / absolute, -previous);
                    previous = result;
                    result = next;
                    twice += 2;
                }
            }
        } else if (magnitude < 0x3e100000u) {
            if (n <= 33u) {
                const double half = absolute * 0.5;
                double power = half;
                double factorial = One;
                for (std::uint32_t index = 2; index <= n; ++index) {
                    power = half * power;
                    factorial *= Signed(index);
                }
                result = power / factorial;
            }
        } else {
            const double h = 2.0 / absolute;
            const double w = Signed(2u * n) / absolute;
            double q0 = w;
            double z = h + w;
            double q1 = Fused(w, z, -One);
            std::uint32_t k = 1;
            while (q1 < 1.0e9) {
                z = h + z;
                ++k;
                const double next = Fused(z, q1, -q0);
                q0 = q1;
                q1 = next;
            }
            const std::int32_t lowest = static_cast<std::int32_t>(2u * n);
            std::uint32_t index = 2u * (k + n);
            double t = 0.0;
            do {
                t = One / (Signed(index) / absolute - t);
                index -= 2u;
            } while (static_cast<std::int32_t>(index) >= lowest);
            const double bound = std::log(std::fabs(h * orderValue)) * orderValue;
            double a = t;
            double b = One;
            double di = Signed(2u * n - 2u);
            std::int32_t remaining = static_cast<std::int32_t>(n);
            const bool rescale = !(bound < LogMax);
            do {
                const double next = b * di / absolute - a;
                di += -2.0;
                a = b;
                b = next;
                if (rescale && b > 1.0e100) {
                    t /= b;
                    a /= b;
                    b = One;
                }
                --remaining;
            } while (remaining > 1);
            const double z0 = j0_nid_postfix(absolute);
            const double z1 = j1_nid_postfix(absolute);
            result = std::fabs(z0) >= std::fabs(z1) ? z0 * t / b : t * z1 / a;
        }
    }
    const double flipped = negative ? -result : result;
    return (n & 1u) != 0 ? flipped : result;
}

double APS5_VABI yn_nid_postfix(int order, double x) {
    const std::uint32_t high = HighWord(x);
    const std::uint32_t low = LowWord(x);
    const std::uint32_t magnitude = high & 0x7fffffffu;
    if ((magnitude | (low != 0 ? 1u : 0u)) > 0x7ff00000u) return x + x;
    if ((magnitude | low) == 0) return SecondKindAtZero();
    if ((high & 0x80000000u) != 0) return SecondKindOfNegative();
    std::uint32_t n = static_cast<std::uint32_t>(order);
    std::int32_t sign = 1;
    if (order < 0) {
        n = 0u - n;
        sign = 1 - static_cast<std::int32_t>((n << 1) & 2u);
    }
    if (n == 1) return static_cast<double>(sign) * y1_nid_postfix(x);
    if (n == 0) return y0_nid_postfix(x);
    if (magnitude == 0x7ff00000u) return 0.0;
    double result;
    if (magnitude >= 0x52d00000u) {
        const double s = std::sin(x);
        const double c = std::cos(x);
        double sum;
        switch (n & 3u) {
        case 0: sum = s - c; break;
        case 1: sum = -s - c; break;
        case 2: sum = c - s; break;
        default: sum = s + c; break;
        }
        result = sum * InverseSqrtPi / SquareRoot(x);
    } else {
        double previous = y0_nid_postfix(x);
        result = y1_nid_postfix(x);
        std::uint32_t twice = 2;
        std::int32_t index = 2;
        while (HighWord(result) != 0xfff00000u) {
            const double next = Fused(result, Signed(twice) / x, -previous);
            previous = result;
            result = next;
            if (index >= static_cast<std::int32_t>(n)) break;
            ++index;
            twice += 2;
        }
    }
    return sign > 0 ? result : -result;
}

}
