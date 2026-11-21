#include "prx/libc/include/Api.hpp"
#include <cmath>
#include <limits>

namespace {

constexpr long double RootCoefficients[] = {
    0xc1f42ed57dd6ac19p-40L,
    0x9da03d51c3de89dap-51L,
    -0xff0a2c5a1b799102p-179L,
    0xc1a4d12a82116472p-40L,
    -0x9d5fbd2e7548db05p-51L,
    -0xec41ef13ed2c2078p-182L,
    0xfe711a4267e88305p-46L,
    0xb4ef24f1d79550cbp-54L,
    -0xf0106ca7a8b0f1bfp-182L,
    0xfbcee5bca7937795p-46L,
    -0xb30fb521d2f09219p-54L,
    0x9ef69a45e2cf7e11p-187L,
    0xe7661976117cd9b7p-51L,
    0xf6b970414d700f01p-57L,
    -0xa26309681516ff4dp-184L,
    0xc168515ed3984afep+29L,
    -0xe16c86c5cc874544p+22L,
    0x8426f221374433e2p+16L,
    -0x9c075fb0c20a6d76p+9L,
    0xb9c12b87844e9ecdp+2L,
    -0xdf6096ebc50a5624p-5L,
    0x87fd2baf53ff491dp-11L,
    -0xa83459f6b4a04938p-18L,
    0xd462e29a2c6529fep-25L,
    -0x89eae8b1de9fbb02p-31L,
    0xba9537ad61392fbap-38L,
    -0x869fbff732e99c33p-44L,
    0xda99e33c51caaec8p-51L,
    -0xe91251f7cf20fb38p-57L,
    0x93796c43b0695ef0p-187L,
    0x94a8864d824d33a6p+35L,
    0xc429d298c8e62c90p+30L,
    0x81b7fca59c2edeffp+26L,
    0xabfd8e76abe0e4f8p+21L,
    0xe4abbce93f41e26dp+16L,
    0x987c65b32ba7fbe2p+12L,
    0xcc1282928ec0ede3p+7L,
    0x89173ba56dfc6045p+3L,
    0xb902bab0bb98e085p-2L,
    0xfaf619a963463b4cp-7L,
    0xab394ee3af8060cap-11L,
    0xeb46bca6577e6c19p-16L,
    0xa2fdf3336ca00c05p-20L,
    0xe41cce3ac4667cfdp-25L,
    0xa19ee71416d36ed4p-29L,
    0xe8a7f24e2721b459p-34L,
    0xaaf1edfccf59e43ap-38L,
    0x8130f5ab99720272p-42L,
    0xcb68c710d75ed78fp-47L,
    0xaacd88d954e3e118p-51L,
    0xa220ae6c09fc706cp-55L,
    0xd652e7a490b211a4p-59L,
    -0xd6dd0fddc5046191p-189L,
    0xe998eb47ef76cd94p+48L,
    -0xadc616cd85ce2ad6p+44L,
    0x817a5faf0977cbc4p+40L,
    -0xc14850950696bb88p+35L,
    0x90899c86dae94182p+31L,
    -0xd89e8857aa461a46p+26L,
    0xa2b175d143545a4ap+22L,
    -0xf4fef3dbfbac4a5dp+17L,
    0xb8faae84cf70c2c2p+13L,
    -0x8c18e4826f73bd26p+9L,
    0xd4f26ed0fd3abe58p+4L,
    -0xa27949ad86aac508p+0L,
    0xf908d5a988a000d1p-5L,
    -0xbfd5b13291d9ae73p-9L,
    0x94a70deedb08115dp-13L,
    -0xe7fe1547afeb9d93p-18L,
    0xb689570c9b52b4e8p-22L,
    -0x9112ff27c249e672p-26L,
    0xe97bb6f3de8137a8p-31L,
    -0xbeddf0317fecf244p-35L,
    0x9f479d5cfe0fa3c7p-39L,
    -0x88b7bc03693698d1p-43L,
    0xf47c14f8a0d528a6p-48L,
    -0xe929acea5979beefp-52L,
    0xfbb6f57021b5ed4ap-56L,
    -0xa5ccecb362b234c7p-59L,
    -0xd51f5b10a3322eb9p-188L,
    0xc4412a1f2c601250p-1L,
    0xeb3a78a45f14851fp-4L,
    0x8d3ced3b2777aad8p-6L,
    0xa9f53df6c25fd218p-9L,
    0xccfb9a6e4cec540dp-12L,
    0xf7d85e4630791f3bp-15L,
    0x964054add7c81712p-17L,
    0xb6bce14a7d69a12ep-20L,
    0xdf05248626f85dabp-23L,
    0x889feeae34a930cfp-25L,
    0xa824a587d83e94d1p-28L,
    0xcffe6cc255eeaa25p-31L,
    0x816910cb23d7b4c7p-33L,
    0xa229061d25dc1e1dp-36L,
    0xcce432871d1c56cfp-39L,
    0x82bfb2e32590daf4p-41L,
    0xa8f519a2fa9a4ac7p-44L,
    0xddcbd505b8f227c0p-47L,
    0x949e1fbc69ded87bp-49L,
    0xccd319bed1cee94ap-52L,
    0x933901ebbb586cabp-54L,
    0xe089b8926ae2d9d7p-57L,
    0xcea694bb8a877b41p-59L,
    0xf90532f97d62a6e3p-61L,
    0xa1d2da615d4ea554p-188L,
    -0x92922b7875e1fd64p+9L,
    0x97d28e82426b4603p+7L,
    -0x9d5f02e81246fbf5p+5L,
    0xa33de7bb46ff54ddp+3L,
    -0xa9765689dfa65557p+1L,
    0xb0103ecb58012900p-1L,
    -0xb714864a0dd6c623p-3L,
    0xbe8d2fa069bc0096p-5L,
    -0xc685882918873072p-7L,
    0xcf0a5f1f50d30874p-9L,
    -0xd82a4826e0b6eae8p-11L,
    0xe1f5ec15f636fbf8p-13L,
    -0xec806bb2daeaedbdp-15L,
    0xf7dfd93b063a7f3ep-17L,
    -0x8216e78a672de11ap-18L,
    0x88c41616d2ea60f3p-20L,
    -0x900900bdd71f0c15p-22L,
    0x97fa60235f869481p-24L,
    -0xa0b0df65d525584dp-26L,
    0xaa4a18c15cba93d8p-28L,
    -0xb4e9e1e837514ff5p-30L,
    0xc0bc075e137871afp-32L,
    -0xcdf6a393b79fe965p-34L,
    0xdcdd58fcdb419e74p-36L,
    -0xedc5cf354629bfd9p-38L,
    0x808f1a22a636176cp-39L,
    -0x8bbb39306d03cec4p-41L,
    0x98c77b927bc071aep-43L,
    -0xa83632fd6b0bf276p-45L,
    0xbac0b9cf7304372ap-47L,
    -0xd1692826bf4c37d1p-49L,
    0xedd32f13a10e2137p-51L,
    -0x8911cdeeb600975dp-52L,
    0xa1b9fbe6384d96dcp-54L,
    -0xc2c422c103f55d69p-56L,
    0xfa8256664f8cd615p-58L,
    -0xa0c2d618645f8e0fp-59L,
    0x9933f9e132d28dc7p-60L,
    -0xf5096d48258c6244p-63L,
    0xf45ca59fc4c7fbdfp-191L,
    -0x85dd951c5287cbc3p-24L,
    0xc21ae597521f5cddp-26L,
    -0x8ce7e840eff0a826p-27L,
    0xccdb03094aa2a719p-29L,
    -0x952280607294169dp-30L,
    0xd97d02f4a3f11566p-32L,
    -0x9edc628320c776bap-33L,
    0xe883d69d6078d8c4p-35L,
    -0xaa82a9b7ea8365fdp-36L,
    0xfaa6e396ed4f18f5p-38L,
    -0xb8b04c65ee5ac62bp-39L,
    0x8877aa5af6b43b39p-40L,
    -0xca48d80907f821b7p-42L,
    0x9675efe279e8e3e7p-43L,
    -0xe0a7e978dff6329fp-45L,
    0xa8895dae33fc723ap-46L,
    -0xfdf9edb464b82f35p-48L,
    0xc0c6586e223dfe28p-49L,
    -0x92f5cbcfed4f72e1p-50L,
    0xe3087aa814004043p-52L,
    -0xaf8cd807fa243ce6p-53L,
    0x8bc1e4126f0baf84p-54L,
    -0xdb22eeec8df68b85p-56L,
    0xb9d32ee9143c7c71p-57L,
    -0x918b20217f977da0p-58L,
    0x917cb7f615f2bf13p-59L,
    -0xd4caaa7400153387p-61L,
    0xc1df5cc64dd30df3p-61L,
    -0xe5fc432fc333fc72p-65L,
    -0xe1385cdd7387dc7ap-67L,
    0xda3065faadbf254ap-25L,
    0xcd397a345b1dc0a0p-26L,
    0xc00d626cea38e728p-27L,
    0xb5183249957982cep-28L,
    0xa981cf0ed10eac4ep-29L,
    0xa055cc6a2f1afc71p-30L,
    0x960e1bb82656de7cp-31L,
    0x8e7f338812888ebap-32L,
    0x85453f8221e17f1ap-33L,
    0xfe66d12c6302d6a4p-35L,
    0xed940fe9cc22fc81p-36L,
    0xe442d0fbbf6633f0p-37L,
    0xd4999e67248be6f8p-38L,
    0xce0d3fa7bc5acac6p-39L,
    0xbf1313a805eb5822p-40L,
    0xbb5bf3a16dde4fedp-41L,
    0xac8c3ec99890fb88p-42L,
    0xabdf5cbf3d1ec8e1p-43L,
    0x9ca2053e6e218bd9p-44L,
    0x9f643678995b1d13p-45L,
    0x8effffe404b0c69fp-46L,
    0x95d8849c6f153224p-47L,
    0x835e75019bc92253p-48L,
    0x8f565765fcc580bbp-49L,
    0xf30143cb296fad21p-51L,
    0x8c39bf6462b3a322p-51L,
    0xe26692ca7380a88ap-53L,
    0x8d4ea6c63153fb7cp-53L,
    0xd497052ef25fbc30p-55L,
    0x9437dc65d75d9bfap-55L,
    0xc94bd6d96f7acddap-57L,
    0xa475540b2b9b0948p-57L,
    0xc04f82773707cd3ep-59L,
    0xc6997b415505e66fp-59L,
    0xb99cff02593b1d6fp-61L,
    0x8b8c6be504f2db06p-60L,
    0xb4a5302c53c2bee2p-63L,
    0x9b775d8017aae4e5p-61L,
    0xc1ff4b357a9af689p-63L,
    -0x8b6bd120d83db5b1p-195L,
};

constexpr long double MinimumNumerator[] = {
    0xb1ef739a1c10e1ecp-83L,
    -0x874c869c1b8e9e52p-81L,
    0x9a2a49152411b7e3p-80L,
    -0x9f55d8eef65a2ff7p-79L,
    0x9d0262a848060b6cp-78L,
    -0x97298161c2990ee7p-77L,
    0x906da97919160ed2p-76L,
    -0x8aa3e6add353205bp-75L,
    0x874bd88cd2a3ef5fp-74L,
    -0x880d7ff979a0539ep-73L,
    0x8f7f7557e4744c33p-72L,
    -0xa2f3974516816b1fp-71L,
    0xd0295416cd611bdbp-70L,
    -0xa34dc963faf38d85p-68L,
    0xc8869182b9d69312p-66L,
    0xeaf0c3baac5936ffp-65L,
    -0xc73ece361b3935c1p-66L,
    -0xec036b6a52d4bcbbp-67L,
};

constexpr long double MinimumDenominator[] = {
    0xb3273091a8aff950p-64L,
    0x8000000000000000p-63L,
};

constexpr long double GammaNumerator[] = {
    0xa4727e63b2e743c5p-83L,
    0x9f4d4d546ee42ebfp-79L,
    0x9c1cf144ba412bc9p-76L,
    0x8a28fc820501e7f0p-72L,
    0x86bcd12277f69e7bp-70L,
    0xe0c09f902c637146p-69L,
    0xd7b7b05a4d21e061p-66L,
    0x9edf8c7ba7fe6220p-64L,
    0xdcbc86ad7e246035p-65L,
};

constexpr long double GammaDenominator[] = {
    0xd2f38f358b49084ap-78L,
    -0x9c781c9a93580e19p-74L,
    0xdca309bb163f009cp-74L,
    0x80535611b8d22851p-69L,
    -0xc960ebdfb5cfb222p-68L,
    -0xa49c80223ac43e4ep-66L,
    0xf2f854c91287abbbp-65L,
    0x828682a33bb9e44bp-63L,
};

struct RootInterval {
    long double lower;
    long double upper;
    long double centerHigh;
    long double centerLow;
    int count;
    int offset;
};

constexpr RootInterval RootIntervals[] = {
    {-0xe001f75104d551d7p-61L, -0xe000d1b71758e219p-61L, -0xe0019fef6ff0f5bfp-61L, 0xedf48c919e1f5536p-126L, 3, 0},
    {-0xdfff04577d955715p-61L, -0xdffe5c91d14e3bcdp-61L, -0xdffe5fbb5c377fe8p-61L, 0xa765268f700b7830p-126L, 3, 3},
    {-0xc00c49ba5e353f7dp-61L, -0xc00902de00d1b717p-61L, -0xc00b592c4be4676dp-61L, 0xf07a4d25d38f468fp-125L, 3, 6},
    {-0xbff7ced916872b02p-61L, -0xbff141205bc01a37p-61L, -0xbff497ac8fa06afcp-61L, 0xac4a74d0b38481c7p-126L, 3, 9},
    {-0xa049ba5e353f7ceep-61L, -0xa0395810624dd2f2p-61L, -0xa04352bf85b6c866p-61L, 0xb675234d58d64267p-125L, 3, 12},
    {-0x9fbe76c8b4395810p-61L, -0x9fb645a1cac08312p-61L, -0x9fbabbd37757e6a3p-61L, 0xe5eab7f12cef5fc5p-125L, 15, 15},
    {-0x8178d4fdf3b645a2p-61L, -0x810624dd2f1a9fbep-61L, -0x814273c2ccac0619p-61L, 0x8c1e9e43cfddaa7dp-125L, 23, 30},
    {-0xfdc28f5c28f5c28fp-62L, -0xfc8b4395810624ddp-62L, -0xfd238aa3e17f285dp-62L, 0xcaea7b5bb724cf38p-126L, 27, 53},
    {-0xcb33333333333333p-62L, -0xc75c28f5c28f5c29p-62L, -0xc9306de4f2cd7befp-62L, 0xd065994b099166afp-126L, 25, 80},
    {-0xb5c28f5c28f5c28fp-62L, -0xaccccccccccccccdp-62L, -0xafda0850dec8065fp-62L, 0x9a96d2e6f854502dp-126L, 40, 105},
    {-0xaccccccccccccccdp-62L, -0xa666666666666666p-62L, -0xa99999999999999ap-62L, 0xcccccccccccccccdp-127L, 30, 145},
    {-0xa666666666666666p-62L, -0x95c28f5c28f5c28fp-62L, -0x9d3fe4b007c360abp-62L, 0x9b0675072fc769e6p-131L, 40, 175},
};

long double ModerateLogGamma(long double x) {
    for (const auto& interval : RootIntervals) {
        if (x > interval.lower && x < interval.upper) {
            const long double argument = (x - interval.centerHigh) - interval.centerLow;
            return _LPoly_nid_postfix(argument, RootCoefficients + interval.offset, interval.count - 1);
        }
    }
    if (x > 0x999999999999999ap-63L && x < 1.5L) {
        const long double argument = x + -0xaccccccccccccccdp-63L;
        const long double numerator = _LPoly_nid_postfix(argument, MinimumNumerator, 17);
        const long double denominator = _LPoly_nid_postfix(argument, MinimumDenominator, 1);
        return numerator / denominator;
    }
    long double argument = x;
    long double product;
    short exponent = 0;
    bool scale = true;
    if (argument < -0.5L) {
        product = argument;
        do {
            argument = argument + 1.0L;
            product = product * argument;
        } while (argument <= 0.5L);
        argument = argument - 1.0L;
    } else if (argument > -0x8000000000000000p-129L && argument < 0x8000000000000000p-129L) {
        product = argument;
    } else if (argument < 0.5L) {
        product = (1.0L + argument) * argument;
    } else if (argument < 1.5L) {
        product = argument;
        argument = argument - 1.0L;
        scale = false;
    } else if (argument < 2.5L) {
        product = 1.0L;
        argument = argument - 2.0L;
        scale = false;
    } else {
        product = 1.0L;
        while (argument > 2.5L) {
            argument = argument - 1.0L;
            product = product / argument;
        }
        argument = argument - 2.0L;
    }
    if (scale) _LDunscale_nid_postfix(&exponent, &product);
    const long double numerator = _LPoly_nid_postfix(argument, GammaNumerator, 8);
    const long double denominator = _LPoly_nid_postfix(argument, GammaDenominator, 7);
    const long double correction = (numerator / denominator) * argument;
    const long double logarithm = _LLog_nid_postfix(std::fabs(product), 0);
    const long double negativeExponent = -exponent;
    const long double low = negativeExponent * 0xfbe8e7bcd5e4f1dap-87L;
    const long double remainder = low - logarithm;
    const long double high = negativeExponent * 0.69314706325531005859375L;
    const volatile long double combined = remainder + high;
    return log1pl_nid_postfix(correction) + combined;
}

long double LargeLogGamma(long double x) {
    const volatile long double inverse = 1.0L / x;
    const volatile long double logarithm = _LLog_nid_postfix(x, 0) - 1.0L;
    const volatile long double argument = x - 0.5L;
    const volatile long double square = inverse * inverse;
    long double product = argument;
    short exponent = 0;
    _LDunscale_nid_postfix(&exponent, &product);
    product = product * logarithm;
    if (_LDscale_nid_postfix(&product, exponent) >= 0) {
        _Feraise_nid_postfix(0x08);
        return product;
    }
    long double correction = 0xdca8f158c7f91ab8p-74L * square;
    correction = correction - 0x9c09c09c09c09c0ap-74L;
    correction = correction * square + 0xd00d00d00d00d00dp-74L;
    correction = correction * square - 0xb60b60b60b60b60bp-72L;
    correction = correction * square + 0xaaaaaaaaaaaaaaabp-67L;
    correction = correction * inverse + 0xd67f1c864beb4a69p-65L;
    return correction + product;
}

}

extern "C" {

long double APS5_VABI lgammal_nid_postfix(long double x) {
    long double integral = x;
    const short kind = _LDint_nid_postfix(&integral, 0);
    if (kind == 2) return x;
    if (kind == 1) return std::numeric_limits<long double>::infinity();
    if (kind == 0 && x <= 0.0L) {
        _Feraise_nid_postfix(0x04);
        return std::numeric_limits<long double>::infinity();
    }
    if (kind <= 0) {
        if (x >= 130.0L) return LargeLogGamma(x);
        if (x < -130.0L) {
            constexpr long double GammaPi = 0xc90fdaa22168c235p-62L;
            const long double sine = _LSin_nid_postfix((x - integral) * GammaPi, 0);
            const long double reflected = _LLog_nid_postfix(GammaPi / (x * sine), 0);
            return reflected - LargeLogGamma(-x);
        }
    }
    return ModerateLogGamma(x);
}
}
