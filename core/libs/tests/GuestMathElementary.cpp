#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

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

static void CheckSincosBits() {
    constexpr struct {
        std::uint64_t input;
        std::uint64_t sine;
        std::uint64_t cosine;
    } cases[] = {
        {0xbff38db77b728354ull, 0xbfee12fe51d52ed4ull, 0x3fd5ddfc9f502f39ull},
        {0x3ff3cdd6b716682full, 0x3fee3dddabdaf80cull, 0x3fd4ec422e520ddcull},
        {0x4034bec5d69ceef8ull, 0x3fee538958769860ull, 0xbfd46d49e445c5f6ull},
        {0x404b99990533005cull, 0xbfef36c2aa77f6a3ull, 0x3fcc326f2739013cull},
        {0x4058e07b3a9d54aeull, 0xbfeb53311385a20full, 0x3fe0a759b6ce663cull},
        {0xc0586d0afcbf8becull, 0x3fd3cb0c9c2455b3ull, 0xbfee6e62dd34860bull},
        {0xc041959add647634ull, 0x3fe25d733160450bull, 0xbfea34980a0bb288ull},
        {0x4046fe3c411f9390ull, 0x3fed0b8e2203c7c6ull, 0xbfdadc72937888deull},
        {0x0000000000000000ull, 0x0000000000000000ull, 0x3ff0000000000000ull},
        {0x0000000000000001ull, 0x0000000000000001ull, 0x3ff0000000000000ull},
        {0x8000000000000000ull, 0x8000000000000000ull, 0x3ff0000000000000ull},
        {0x8000000000000001ull, 0x8000000000000001ull, 0x3ff0000000000000ull},
        {0x3e3fffffffffffffull, 0x3e3fffffffffffffull, 0x3ff0000000000000ull},
        {0xbe3fffffffffffffull, 0xbe3fffffffffffffull, 0x3ff0000000000000ull},
        {0x3e40000000000000ull, 0x3e40000000000000ull, 0x3ff0000000000000ull},
        {0xbe40000000000000ull, 0xbe40000000000000ull, 0x3ff0000000000000ull},
        {0x3e40000000000001ull, 0x3e40000000000001ull, 0x3ff0000000000000ull},
        {0xbe40000000000001ull, 0xbe40000000000001ull, 0x3ff0000000000000ull},
        {0x3e46a09dffffffffull, 0x3e46a09dffffffffull, 0x3ff0000000000000ull},
        {0xbe46a09dffffffffull, 0xbe46a09dffffffffull, 0x3ff0000000000000ull},
        {0x3e46a09e00000000ull, 0x3e46a09e00000000ull, 0x3ff0000000000000ull},
        {0xbe46a09e00000000ull, 0xbe46a09e00000000ull, 0x3ff0000000000000ull},
        {0x3e46a09e00000001ull, 0x3e46a09e00000001ull, 0x3ff0000000000000ull},
        {0xbe46a09e00000001ull, 0xbe46a09e00000001ull, 0x3ff0000000000000ull},
        {0x3e4fffffffffffffull, 0x3e4fffffffffffffull, 0x3fefffffffffffffull},
        {0xbe4fffffffffffffull, 0xbe4fffffffffffffull, 0x3fefffffffffffffull},
        {0x3e50000000000000ull, 0x3e50000000000000ull, 0x3fefffffffffffffull},
        {0xbe50000000000000ull, 0xbe50000000000000ull, 0x3fefffffffffffffull},
        {0x3e50000000000001ull, 0x3e50000000000001ull, 0x3fefffffffffffffull},
        {0xbe50000000000001ull, 0xbe50000000000001ull, 0x3fefffffffffffffull},
        {0x3fe921faffffffffull, 0x3fe6a09e2ae9667dull, 0x3fe6a09ea2151080ull},
        {0xbfe921faffffffffull, 0xbfe6a09e2ae9667dull, 0x3fe6a09ea2151080ull},
        {0x3fe921fb00000000ull, 0x3fe6a09e2ae9667eull, 0x3fe6a09ea215107full},
        {0xbfe921fb00000000ull, 0xbfe6a09e2ae9667eull, 0x3fe6a09ea215107full},
        {0x3fe921fb00000001ull, 0x3fe6a09e2ae9667eull, 0x3fe6a09ea215107eull},
        {0xbfe921fb00000001ull, 0xbfe6a09e2ae9667eull, 0x3fe6a09ea215107eull},
        {0x3fe921fbffffffffull, 0x3fe6a09edfee58baull, 0x3fe6a09ded101c54ull},
        {0xbfe921fbffffffffull, 0xbfe6a09edfee58baull, 0x3fe6a09ded101c54ull},
        {0x3fe921fc00000000ull, 0x3fe6a09edfee58baull, 0x3fe6a09ded101c54ull},
        {0xbfe921fc00000000ull, 0xbfe6a09edfee58baull, 0x3fe6a09ded101c54ull},
        {0x3fe921fc00000001ull, 0x3fe6a09edfee58bbull, 0x3fe6a09ded101c53ull},
        {0xbfe921fc00000001ull, 0xbfe6a09edfee58bbull, 0x3fe6a09ded101c53ull},
        {0x3ff921fb54442d17ull, 0x3ff0000000000000ull, 0x3cb469898cc51702ull},
        {0xbff921fb54442d17ull, 0xbff0000000000000ull, 0x3cb469898cc51702ull},
        {0x3ff921fb54442d18ull, 0x3ff0000000000000ull, 0x3c91a62633145c07ull},
        {0xbff921fb54442d18ull, 0xbff0000000000000ull, 0x3c91a62633145c07ull},
        {0x3ff921fb54442d19ull, 0x3ff0000000000000ull, 0xbca72cece675d1fdull},
        {0xbff921fb54442d19ull, 0xbff0000000000000ull, 0xbca72cece675d1fdull},
        {0x400921fb54442d17ull, 0x3cc469898cc51702ull, 0xbff0000000000000ull},
        {0xc00921fb54442d17ull, 0xbcc469898cc51702ull, 0xbff0000000000000ull},
        {0x400921fb54442d18ull, 0x3ca1a62633145c07ull, 0xbff0000000000000ull},
        {0xc00921fb54442d18ull, 0xbca1a62633145c07ull, 0xbff0000000000000ull},
        {0x400921fb54442d19ull, 0xbcb72cece675d1fdull, 0xbff0000000000000ull},
        {0xc00921fb54442d19ull, 0x3cb72cece675d1fdull, 0xbff0000000000000ull},
        {0x4012d97c7f3321d1ull, 0xbff0000000000000ull, 0xbcd34f272993d141ull},
        {0xc012d97c7f3321d1ull, 0x3ff0000000000000ull, 0xbcd34f272993d141ull},
        {0x4012d97c7f3321d2ull, 0xbff0000000000000ull, 0xbcaa79394c9e8a0aull},
        {0xc012d97c7f3321d2ull, 0x3ff0000000000000ull, 0xbcaa79394c9e8a0aull},
        {0x4012d97c7f3321d3ull, 0xbff0000000000000ull, 0x3cc961b1acd85d7dull},
        {0xc012d97c7f3321d3ull, 0x3ff0000000000000ull, 0x3cc961b1acd85d7dull},
        {0x401921fb54442d17ull, 0xbcd469898cc51702ull, 0x3ff0000000000000ull},
        {0xc01921fb54442d17ull, 0x3cd469898cc51702ull, 0x3ff0000000000000ull},
        {0x401921fb54442d18ull, 0xbcb1a62633145c07ull, 0x3ff0000000000000ull},
        {0xc01921fb54442d18ull, 0x3cb1a62633145c07ull, 0x3ff0000000000000ull},
        {0x401921fb54442d19ull, 0x3cc72cece675d1fdull, 0x3ff0000000000000ull},
        {0xc01921fb54442d19ull, 0xbcc72cece675d1fdull, 0x3ff0000000000000ull},
        {0x413921fb54442d17ull, 0xbdf469898cc51702ull, 0x3ff0000000000000ull},
        {0xc13921fb54442d17ull, 0x3df469898cc51702ull, 0x3ff0000000000000ull},
        {0x413921fb54442d18ull, 0xbdd1a62633145c07ull, 0x3ff0000000000000ull},
        {0xc13921fb54442d18ull, 0x3dd1a62633145c07ull, 0x3ff0000000000000ull},
        {0x413921fb54442d19ull, 0x3de72cece675d1fdull, 0x3ff0000000000000ull},
        {0xc13921fb54442d19ull, 0xbde72cece675d1fdull, 0x3ff0000000000000ull},
        {0x4415af1d78b58c3full, 0x3feecbe4600cd531ull, 0xbfd1631dd2571774ull},
        {0xc415af1d78b58c3full, 0xbfeecbe4600cd531ull, 0xbfd1631dd2571774ull},
        {0x4415af1d78b58c40ull, 0xbfe4a5e605fd6450ull, 0x3fe872720fc60d3dull},
        {0xc415af1d78b58c40ull, 0x3fe4a5e605fd6450ull, 0x3fe872720fc60d3dull},
        {0x4415af1d78b58c41ull, 0x3fbb59a056464aa2ull, 0xbfefd11d33a55413ull},
        {0xc415af1d78b58c41ull, 0xbfbb59a056464aa2ull, 0xbfefd11d33a55413ull},
        {0x7feffffffffffffeull, 0x3fedaa3677c6ee8aull, 0x3fd7ffdfb4c53090ull},
        {0xffeffffffffffffeull, 0xbfedaa3677c6ee8aull, 0x3fd7ffdfb4c53090ull},
        {0x7fefffffffffffffull, 0x3f7452fc98b34e97ull, 0xbfefffe62ecfab75ull},
        {0xffefffffffffffffull, 0xbf7452fc98b34e97ull, 0xbfefffe62ecfab75ull},
        {0x7ff0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull},
        {0xfff0000000000000ull, 0xfff8000000000000ull, 0xfff8000000000000ull},
        {0x7ff8000000000123ull, 0x7ff8000000000123ull, 0x7ff8000000000123ull},
        {0xfff8000000000123ull, 0xfff8000000000123ull, 0xfff8000000000123ull},
        {0x7ff0000000000123ull, 0x7ff8000000000123ull, 0x7ff8000000000123ull},
        {0xfff0000000000123ull, 0xfff8000000000123ull, 0xfff8000000000123ull},
    };
    for (const auto& item : cases) {
        const double x = std::bit_cast<double>(item.input);
        double sine, cosine;
        errno = 1234;
        sincos_nid_postfix(x, &sine, &cosine);
        if (std::bit_cast<std::uint64_t>(sine) != item.sine || std::bit_cast<std::uint64_t>(cosine) != item.cosine) {
            std::fprintf(stderr, "sincos(%016llx): got %016llx %016llx, expected %016llx %016llx\n",
                         static_cast<unsigned long long>(item.input), static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(sine)),
                         static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(cosine)), static_cast<unsigned long long>(item.sine),
                         static_cast<unsigned long long>(item.cosine));
        }
        Check(std::bit_cast<std::uint64_t>(sine) == item.sine);
        Check(std::bit_cast<std::uint64_t>(cosine) == item.cosine);
        Check(errno == ((item.input >> 32 & 0x7fffffffu) == 0x7ff00000u ? EDOM : 1234));
        Check(std::bit_cast<std::uint64_t>(sine) == std::bit_cast<std::uint64_t>(sin_nid_postfix(x)));
        Check(std::bit_cast<std::uint64_t>(cosine) == std::bit_cast<std::uint64_t>(cos_nid_postfix(x)));
    }
    std::uint64_t state = 0x917583ab6542ull;
    for (int i = 0; i < 10000; ++i) {
        state = state * 6364136223846793005ull + 1;
        if ((state & 0x7ff0000000000000ull) == 0x7ff0000000000000ull) continue;
        const double x = std::bit_cast<double>(state);
        double sine, cosine;
        sincos_nid_postfix(x, &sine, &cosine);
        Check(std::bit_cast<std::uint64_t>(sine) == std::bit_cast<std::uint64_t>(sin_nid_postfix(x)));
        Check(std::bit_cast<std::uint64_t>(cosine) == std::bit_cast<std::uint64_t>(cos_nid_postfix(x)));
    }
}

int main() {
    CheckSincosBits();
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
