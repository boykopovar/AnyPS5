/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * This code by P. McIlroy, Oct 1992;
 *
 * The financial support of UUNET Communications Services is greatfully
 * acknowledged.
 */
/* https://github.com/freebsd/freebsd-src/blob/afced35c1d2057e168e91a50768ca4d6535a5974/lib/msun/bsdsrc/b_tgamma.c */
/* https://github.com/freebsd/freebsd-src/blob/afced35c1d2057e168e91a50768ca4d6535a5974/lib/msun/bsdsrc/b_log.c */
/* https://github.com/freebsd/freebsd-src/blob/afced35c1d2057e168e91a50768ca4d6535a5974/lib/msun/bsdsrc/b_exp.c */

/* https://github.com/freebsd/freebsd-src/blob/afced35c1d2057e168e91a50768ca4d6535a5974/lib/msun/src/k_sin.c */
/* https://github.com/freebsd/freebsd-src/blob/afced35c1d2057e168e91a50768ca4d6535a5974/lib/msun/src/k_cos.c */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

struct Parts {
    double high;
    double low;
};

constexpr double Center = .461632144968362356785;
constexpr double Left = -.3955078125;
constexpr double Pi = 0x1.921fb54442d18p+1;
constexpr double MinimumHigh = 0.88560319441088874992;
constexpr double MinimumLow = -.00000000000000004996427036469019695;

constexpr double LogHead[] = {
    0., .007782140442060381246, .015504186535963526694, .023167059281547608406,
    .030771658666765233647, .038318864302141264488, .045809536031242714670, .053244514518837604555,
    .060624621816486978786, .067950661908525944454, .075223421237524235039, .082443669210988446138,
    .089612158689760690322, .096729626458454731618, .103796793681567578460, .110814366340264314203,
    .117783035656430001836, .124703478501032805070, .131576357788617315236, .138402322859292326029,
    .145182009844575077295, .151916042025732167530, .158605030176659056451, .165249572895390883786,
    .171850256926518341060, .178407657472689606947, .184922338493834104156, .191394852999565046047,
    .197825743329758552135, .204215541428766300668, .210564769107350002741, .216873938300523150246,
    .223143551314024080056, .229374101064877322642, .235566071312860003672, .241719936886966024758,
    .247836163904594286577, .253915209980732470285, .259957524436686071567, .265963548496984003577,
    .271933715484010463114, .277868451003087102435, .283768173130738432519, .289633292582948342896,
    .295464212893421063199, .301261330578199704177, .307025035294827830512, .312755710004239517729,
    .318453731118097493890, .324119468654316733591, .329753286372579168528, .335355541920762334484,
    .340926586970454081892, .346466767346100823488, .351976423156884266063, .357455888922231679316,
    .362905493689140712376, .368325561158599157352, .373716409793814818840, .379078352934811846353,
    .384411698910298582632, .389716751140440464951, .394993808240542421117, .400243164127459749579,
    .405465108107819105498, .410659924985338875558, .415827895143593195825, .420969294644237379543,
    .426084395310681429691, .431173464818130014464, .436236766774527495726, .441274560805140936281,
    .446287102628048160113, .451274644139630254358, .456237433481874177232, .461175715122408291790,
    .466089729924533457960, .470979715219073113985, .475845904869856894947, .480688529345570714212,
    .485507815781602403149, .490303988045525329653, .495077266798034543171, .499827869556611403822,
    .504556010751912253908, .509261901790523552335, .513945751101346104405, .518607764208354637958,
    .523248143765158602036, .527867089620485785417, .532464798869114019908, .537041465897345915436,
    .541597282432121573947, .546132437597407260909, .550647117952394182793, .555141507540611200965,
    .559615787935399566777, .564070138285387656651, .568504735352689749561, .572919753562018740922,
    .577315365035246941260, .581691739635061821900, .586049045003164792433, .590387446602107957005,
    .594707107746216934174, .599008189645246602594, .603290851438941899687, .607555250224322662688,
    .611801541106615331955, .616029877215623855590, .620240409751204424537, .624433288012369303032,
    .628608659422752680256, .632766669570628437213, .636907462236194987781, .641031179420679109171,
    .645137961373620782978, .649227946625615004450, .653301272011958644725, .657358072709030238911,
    .661398482245203922502, .665422632544505177065, .669430653942981734871, .673422675212350441142,
    .677398823590920073911, .681359224807238206267, .685304003098281100392, .689233281238557538017,
    .693147180560117703862,
};

constexpr double LogTail[] = {
    0., -.00000000000000543229938420049, .00000000000000172745674997061, -.00000000000001323017818229233,
    -.00000000000001154527628289872, -.00000000000000466529469958300, .00000000000005148849572685810, -.00000000000002532168943117445,
    -.00000000000005213620639136504, -.00000000000001819506003016881, .00000000000006329065958724544, .00000000000008614512936087814,
    -.00000000000007355770219435028, .00000000000009638067658552277, .00000000000007598636597194141, .00000000000002579999128306990,
    -.00000000000004654729747598444, -.00000000000007556920687451336, .00000000000010195735223708472, -.00000000000017319034406422306,
    -.00000000000007718001336828098, .00000000000010980754099855238, -.00000000000002047235780046195, -.00000000000008372091099235912,
    .00000000000014088127937111135, .00000000000012869017157588257, .00000000000017788850778198106, .00000000000006440856150696891,
    .00000000000016132822667240822, -.00000000000007540916511956188, -.00000000000000036507188831790, .00000000000009120937249914984,
    .00000000000018567570959796010, -.00000000000003149265065191483, -.00000000000009309459495196889, .00000000000017914338601329117,
    -.00000000000001302979717330866, .00000000000023097385217586939, .00000000000023999540484211737, .00000000000015393776174455408,
    -.00000000000036870428315837678, .00000000000036920375082080089, -.00000000000009383417223663699, .00000000000009433398189512690,
    .00000000000041481318704258568, -.00000000000003792316480209314, .00000000000008403156304792424, -.00000000000034262934348285429,
    .00000000000043712191957429145, -.00000000000010475750058776541, -.00000000000011118671389559323, .00000000000037549577257259853,
    .00000000000013912841212197565, .00000000000010775743037572640, .00000000000029391859187648000, -.00000000000042790509060060774,
    .00000000000022774076114039555, .00000000000010849569622967912, -.00000000000023073801945705758, .00000000000015761203773969435,
    .00000000000003345710269544082, -.00000000000041525158063436123, .00000000000032655698896907146, -.00000000000044704265010452446,
    .00000000000034527647952039772, -.00000000000007048962392109746, .00000000000011776978751369214, -.00000000000010774341461609578,
    .00000000000021863343293215910, .00000000000024132639491333131, .00000000000039057462209830700, -.00000000000026570679203560751,
    .00000000000037135141919592021, -.00000000000017166921336082431, -.00000000000028658285157914353, -.00000000000023812542263446809,
    .00000000000006576659768580062, -.00000000000028210143846181267, .00000000000010701931762114254, .00000000000018119346366441110,
    .00000000000009840465278232627, -.00000000000033149150282752542, -.00000000000018302857356041668, -.00000000000016207400156744949,
    .00000000000048303314949553201, -.00000000000071560553172382115, .00000000000088821239518571855, -.00000000000030900580513238244,
    -.00000000000061076551972851496, .00000000000035659969663347830, .00000000000035782396591276383, -.00000000000046226087001544578,
    .00000000000062279762917225156, .00000000000072838947272065741, .00000000000026809646615211673, -.00000000000010960825046059278,
    .00000000000002311949383800537, -.00000000000058469058005299247, -.00000000000002103748251144494, -.00000000000023323182945587408,
    -.00000000000042333694288141916, -.00000000000043933937969737844, .00000000000041341647073835565, .00000000000006841763641591466,
    .00000000000047585534004430641, .00000000000083679678674757695, -.00000000000085763734646658640, .00000000000021913281229340092,
    -.00000000000062242842536431148, -.00000000000010983594325438430, .00000000000065310431377633651, -.00000000000047580199021710769,
    -.00000000000037854251265457040, .00000000000040939233218678664, .00000000000087424383914858291, .00000000000025218188456842882,
    -.00000000000003608131360422557, -.00000000000050518555924280902, .00000000000078699403323355317, -.00000000000067020876961949060,
    .00000000000016108575753932458, .00000000000058527188436251509, -.00000000000035246757297904791, -.00000000000018372084495629058,
    .00000000000088606689813494916, .00000000000066486268071468700, .00000000000063831615170646519, .00000000000025144230728376072,
    -.00000000000017239444525614834,
};

constexpr double GammaNumerator[] = {
    6.21389571821820863029017800727e-01, 2.65757198651533466104979197553e-01, 5.53859446429917461063308081748e-03,
    1.38456698304096573887145282811e-03, 2.40659950032711365819348969808e-03
};

constexpr double GammaDenominator[] = {
    1.45019531250000000000000000000e+00, 1.06258521948016171343454061571e+00, -2.07474561943859936441469926649e-01,
    -1.46734131782005422506287573015e-01, 3.07878176156175520361557573779e-02, 5.12449347980666221336054633184e-03,
    -1.76012741431666995019222898833e-03, 9.35021023573788935372153030556e-05, 6.13275507472443958924745652239e-06
};

constexpr double Stirling[] = {
    8.33333333333333148296162562474e-02, -2.77777777774548123579378966497e-03, 7.93650778754435631476282786423e-04,
    -5.95235082566672847950717262222e-04, 8.41428560346653702135821806252e-04, -1.89773526463879200348872089421e-03,
    5.69394463439411649408050664078e-03, -1.44705562421428915453880392761e-02
};

constexpr double LogPolynomial[] = {
    .08333333333333178827, .01250000000377174923, .002232139987919447809,
    .0004348877777076145742
};

constexpr double ExpPolynomial[] = {
    0x1.555555555553ep-3, -0x1.6c16c16bebd93p-9, 0x1.1566aaf25de2cp-14,
    -0x1.bbd41c5d26bf1p-20, 0x1.6376972bea4d0p-25
};

double HighPart(double x) {
    return std::bit_cast<double>(std::bit_cast<std::uint64_t>(x) & ~std::uint64_t{0x7ffffff});
}

template<std::size_t N>
double Polynomial(double x, const double (&coefficients)[N]) {
    double value = coefficients[N - 1];
    for (std::size_t i = N - 1; i != 0; --i) {
        value = std::fma(x, value, coefficients[i - 1]);
    }
    return value;
}

Parts LogParts(double x) {
    int exponent = 0;
    const double g = std::frexp(x, &exponent) * 2.0;
    --exponent;
    const int index = static_cast<int>(std::fma(128.0, g - 1.0, 0.5));
    const double center = 1.0 + index / 128.0;
    const double residual = g - center;
    const double reciprocal = 1.0 / (2.0 * center + residual);
    const double u = 2.0 * residual * reciprocal;
    const double v = u * u;
    const double series = Polynomial(v, LogPolynomial);
    double high = exponent || index ? (u + 513.0) - 513.0 : HighPart(u);
    const double numerator = std::fma(2.0, std::fma(-center, high, residual), -(high * residual));
    double low = std::fma(numerator, reciprocal, LogTail[index]);
    high += std::fma(static_cast<double>(exponent), LogHead[128], LogHead[index]);
    low = std::fma(u * v, series, low);
    low = std::fma(LogTail[128], static_cast<double>(exponent), low);
    const double rounded = HighPart(high + low);
    return {rounded, (high - rounded) + low};
}

double ExpParts(Parts argument) {
    const double x = argument.high;
    const int exponent = static_cast<int>(std::fma(0x1.71547652b82fep0, x, std::copysign(0.5, x)));
    const double high = std::fma(-static_cast<double>(exponent), 0x1.62e42fee00000p-1, x);
    const double low = std::fma(static_cast<double>(exponent), 0x1.a39ef35793c76p-33, -argument.low);
    const double reduced = high - low;
    const double square = reduced * reduced;
    const double correction = std::fma(-square, Polynomial(square, ExpPolynomial), reduced);
    const double tail = reduced * correction / (2.0 - correction);
    double result = 1.0 + (high - (low - tail));
    _ZNSt8__sce_v27_DscaleEPdl_nid_postfix(&result, exponent);
    return result;
}

Parts StirlingLog(double x) {
    const double invSquare = 1.0 / (x * x);
    const double correction = Polynomial(invSquare, Stirling) / x;
    auto logarithm = LogParts(x);
    logarithm.high -= 1.0;
    const double shifted = x - 0.5;
    const double high = HighPart(shifted);
    const double product = high * logarithm.high;
    double low = std::fma(shifted - high, logarithm.high, shifted * logarithm.low);
    low += -.000006779295327258219670263595;
    low += correction;
    constexpr double constant = .4189453125;
    const double sum = (constant + low) + product;
    return {sum, ((product - sum) + constant) + low};
}

Parts GammaAtMinimum(double z, double correction) {
    const double ratio = Polynomial(z, GammaNumerator) / Polynomial(z, GammaDenominator);
    const double high = HighPart(z);
    double low = ((z - high) + correction) * (high + z);
    const double square = high * high;
    const double squareHigh = HighPart(square);
    low += square - squareHigh;
    const double ratioHigh = HighPart(ratio);
    low = std::fma(low, ratio, squareHigh * (ratio - ratioHigh)) + MinimumLow;
    const double product = squareHigh * ratioHigh;
    const double rounded = HighPart(product + MinimumHigh);
    return {rounded, ((MinimumHigh - rounded) + product) + low};
}

double ReducedGamma(double x) {
    double y = x - 1.0;
    if (y <= 1.0 + (Left + Center)) {
        const auto result = GammaAtMinimum(y - Center, 0.0);
        return result.high + result.low;
    }
    double high = HighPart(y);
    double factor = high - 1.0;
    y -= 1.0;
    const double residual = y - factor;
    double low = residual;
    for (double next = y - 1.0; next > Left + Center; next -= 1.0) {
        const double product = high * factor;
        low = std::fma(high, residual, y * low);
        high = HighPart(product);
        low += product - high;
        y = next;
        factor -= 1.0;
    }
    const auto result = GammaAtMinimum(y - Center, 0.0);
    const double productLow = std::fma(low, result.high + result.low, high * result.low);
    return std::fma(result.high, high, productLow);
}

double SmallGamma(double x) {
    double high = HighPart(x);
    double low = x - high;
    double shifted = x - Center;
    double correction = (-Center - shifted) + x;
    if (x < Center + Left) {
        const double residual = (high + x) * low;
        const double square = high * high;
        high = HighPart(square + x);
        low = ((x - high) + square) + residual;
        shifted = (1.0 - Center) + x;
        correction = ((1.0 - Center) - shifted) + x;
        x = high + low;
    }
    auto result = GammaAtMinimum(shifted, correction);
    const double quotient = HighPart(result.high / x);
    result.high = std::fma(-quotient, high, result.high);
    result.high = std::fma(-quotient, low, result.high);
    return quotient + (result.high + result.low) / x;
}

double PositiveGamma(double x) {
    if (x >= 6.0) return ExpParts(StirlingLog(x));
    if (x >= 1.0 + Left + Center) return ReducedGamma(x);
    return x > 1.e-17 ? SmallGamma(x) : 1.0 / x;
}

double ReflectionFactor(double distance) {
    const bool useSine = distance < 0.25;
    const double x = Pi * (useSine ? distance : 0.5 - distance);
    if (x < (useSine ? 0x1p-26 : 0x1.6a09ep-27)) {
        volatile double angle = x;
        [[maybe_unused]] volatile int integral = static_cast<int>(angle);
        return useSine ? x : 1.0;
    }
    const double square = x * x;
    const double fourth = square * square;
    if (useSine) {
        constexpr double s1 = -1.66666666666666324348e-01, s2 = 8.33333333332248946124e-03;
        constexpr double s3 = -1.98412698298579493134e-04, s4 = 2.75573137070700676789e-06;
        constexpr double s5 = -2.50507602534068634195e-08, s6 = 1.58969099521155010221e-10;
        const double first = std::fma(square, std::fma(square, s4, s3), s2);
        const double rest = std::fma(square * fourth, std::fma(square, s6, s5), first);
        return std::fma(square * x, std::fma(square, rest, s1), x);
    }
    constexpr double c1 = 4.16666666666666019037e-02, c2 = -1.38888888888741095749e-03;
    constexpr double c3 = 2.48015872894767294178e-05, c4 = -2.75573143513906633035e-07;
    constexpr double c5 = 2.08757232129817482790e-09, c6 = -1.13596475577881948265e-11;
    const double first = std::fma(square, std::fma(square, c3, c2), c1);
    const double tail = fourth * fourth * std::fma(square, std::fma(square, c6, c5), c4);
    const double rest = std::fma(square, first, tail);
    const double half = 0.5 * square;
    const double high = 1.0 - half;
    return high + (((1.0 - high) - half) + square * rest);
}

double NegativeGamma(double x) {
    double integer;
    std::modf(x, &integer);
    if (integer == x) {
        errno = EDOM;
        volatile double zero = 0.0;
        return (x - x) / zero;
    }
    double distance = integer - x;
    if (distance > 0.5) distance = 1.0 - distance;
    const bool negative = std::fmod(integer, 2.0) == 0.0;
    const double reflection = ReflectionFactor(distance);
    if (x < -170.0) {
        if (x < -190.0) {
            [[maybe_unused]] volatile double evaluated = reflection;
            return negative ? -0.0 : 0.0;
        }
        const auto gamma = StirlingLog(1.0 - x);
        const auto sine = LogParts(Pi / reflection);
        const double high = gamma.high - sine.high;
        const double low = gamma.low - sine.low;
        const double exponent = -(high + low);
        const double result = ExpParts({exponent, (exponent + high) + low});
        return negative ? -result : result;
    }
    const double complement = 1.0 - x;
    double magnitude = 1.0 - complement == x ? PositiveGamma(complement) : -x * PositiveGamma(-x);
    if (negative) magnitude = -magnitude;
    return Pi / (magnitude * reflection);
}

}

extern "C" double APS5_VABI tgamma_nid_postfix(double x) {
    const auto bits = std::bit_cast<std::uint64_t>(x);
    if ((bits << 1) >= 0xffe0000000000000ull) {
        if (bits == 0x7ff0000000000000ull) return x;
        return x - x;
    }
    if (x > 171.63) {
        static volatile double zero = 0.0;
        return x / zero;
    }
    if (x > 1.e-17) return PositiveGamma(x);
    if (x > -1.e-17) return 1.0 / x;
    return NegativeGamma(x);
}
