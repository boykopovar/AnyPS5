#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#ifdef __linux__
#include <csignal>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#endif

struct ExpectedGamma {
    std::uint16_t control;
    std::uint16_t exponent;
    std::uint64_t mantissa;
    int flags;
};

struct GammaCase {
    std::uint16_t exponent;
    std::uint64_t mantissa;
    std::array<ExpectedGamma, 12> expected;
};

static constexpr GammaCase Cases[] = {
    {0x4006, 0x8200000000000000ull, {{
        {0x007f, 0x4007, 0xfaa1f40000000000ull, 32},
        {0x027f, 0x4007, 0xfaa1f50d4ba13800ull, 32},
        {0x037f, 0x4007, 0xfaa1f50d4ba13f4eull, 32},
        {0x087f, 0x4007, 0xfaa1f90000000000ull, 32},
        {0x0a7f, 0x4007, 0xfaa1f50d4ba15800ull, 32},
        {0x0b7f, 0x4007, 0xfaa1f50d4ba13f4full, 32},
        {0x047f, 0x4007, 0xfaa1f30000000000ull, 32},
        {0x067f, 0x4007, 0xfaa1f50d4ba13800ull, 32},
        {0x077f, 0x4007, 0xfaa1f50d4ba13f4bull, 32},
        {0x0c7f, 0x4007, 0xfaa1f30000000000ull, 32},
        {0x0e7f, 0x4007, 0xfaa1f50d4ba13800ull, 32},
        {0x0f7f, 0x4007, 0xfaa1f50d4ba13f4bull, 32},
    }}},
    {0x4006, 0x8200068db8bac711ull, {{
        {0x007f, 0x4007, 0xfaa2060000000000ull, 32},
        {0x027f, 0x4007, 0xfaa204fd3fc34000ull, 32},
        {0x037f, 0x4007, 0xfaa204fd3fc3312full, 32},
        {0x087f, 0x4007, 0xfaa20a0000000000ull, 32},
        {0x0a7f, 0x4007, 0xfaa204fd3fc34000ull, 32},
        {0x0b7f, 0x4007, 0xfaa204fd3fc33130ull, 32},
        {0x047f, 0x4007, 0xfaa2000000000000ull, 32},
        {0x067f, 0x4007, 0xfaa204fd3fc31000ull, 32},
        {0x077f, 0x4007, 0xfaa204fd3fc3312cull, 32},
        {0x0c7f, 0x4007, 0xfaa2000000000000ull, 32},
        {0x0e7f, 0x4007, 0xfaa204fd3fc31000ull, 32},
        {0x0f7f, 0x4007, 0xfaa204fd3fc3312cull, 32},
    }}},
    {0x4006, 0x8280000000000000ull, {{
        {0x007f, 0x4007, 0xfbd95b0000000000ull, 32},
        {0x027f, 0x4007, 0xfbd95b3a8fd01000ull, 32},
        {0x037f, 0x4007, 0xfbd95b3a8fd00aecull, 32},
        {0x087f, 0x4007, 0xfbd95e0000000000ull, 32},
        {0x0a7f, 0x4007, 0xfbd95b3a8fd01800ull, 32},
        {0x0b7f, 0x4007, 0xfbd95b3a8fd00aecull, 32},
        {0x047f, 0x4007, 0xfbd9580000000000ull, 32},
        {0x067f, 0x4007, 0xfbd95b3a8fcff800ull, 32},
        {0x077f, 0x4007, 0xfbd95b3a8fd00ae8ull, 32},
        {0x0c7f, 0x4007, 0xfbd9580000000000ull, 32},
        {0x0e7f, 0x4007, 0xfbd95b3a8fcff800ull, 32},
        {0x0f7f, 0x4007, 0xfbd95b3a8fd00ae8ull, 32},
    }}},
    {0x4006, 0x8300000000000000ull, {{
        {0x007f, 0x4007, 0xfd11010000000000ull, 32},
        {0x027f, 0x4007, 0xfd11006bba0f5000ull, 32},
        {0x037f, 0x4007, 0xfd11006bba0f55b6ull, 32},
        {0x087f, 0x4007, 0xfd11040000000000ull, 32},
        {0x0a7f, 0x4007, 0xfd11006bba0f6800ull, 32},
        {0x0b7f, 0x4007, 0xfd11006bba0f55b6ull, 32},
        {0x047f, 0x4007, 0xfd10fe0000000000ull, 32},
        {0x067f, 0x4007, 0xfd11006bba0f4800ull, 32},
        {0x077f, 0x4007, 0xfd11006bba0f55b2ull, 32},
        {0x0c7f, 0x4007, 0xfd10fe0000000000ull, 32},
        {0x0e7f, 0x4007, 0xfd11006bba0f4800ull, 32},
        {0x0f7f, 0x4007, 0xfd11006bba0f55b2ull, 32},
    }}},
    {0x4006, 0x8c40000000000000ull, {{
        {0x007f, 0x4008, 0x89e0dc0000000000ull, 32},
        {0x027f, 0x4008, 0x89e0db6b5569f800ull, 32},
        {0x037f, 0x4008, 0x89e0db6b5569f7deull, 32},
        {0x087f, 0x4008, 0x89e0de0000000000ull, 32},
        {0x0a7f, 0x4008, 0x89e0db6b556a0800ull, 32},
        {0x0b7f, 0x4008, 0x89e0db6b5569f7dfull, 32},
        {0x047f, 0x4008, 0x89e0d90000000000ull, 32},
        {0x067f, 0x4008, 0x89e0db6b5569f000ull, 32},
        {0x077f, 0x4008, 0x89e0db6b5569f7dcull, 32},
        {0x0c7f, 0x4008, 0x89e0d90000000000ull, 32},
        {0x0e7f, 0x4008, 0x89e0db6b5569f000ull, 32},
        {0x0f7f, 0x4008, 0x89e0db6b5569f7dcull, 32},
    }}},
    {0x4006, 0xab80000000000000ull, {{
        {0x007f, 0x4008, 0xb1492a0000000000ull, 32},
        {0x027f, 0x4008, 0xb149299545f55000ull, 32},
        {0x037f, 0x4008, 0xb149299545f5507aull, 32},
        {0x087f, 0x4008, 0xb1492b0000000000ull, 32},
        {0x0a7f, 0x4008, 0xb149299545f56000ull, 32},
        {0x0b7f, 0x4008, 0xb149299545f5507dull, 32},
        {0x047f, 0x4008, 0xb149270000000000ull, 32},
        {0x067f, 0x4008, 0xb149299545f54800ull, 32},
        {0x077f, 0x4008, 0xb149299545f5507aull, 32},
        {0x0c7f, 0x4008, 0xb149270000000000ull, 32},
        {0x0e7f, 0x4008, 0xb149299545f54800ull, 32},
        {0x0f7f, 0x4008, 0xb149299545f5507aull, 32},
    }}},
    {0x4007, 0x8000000000000000ull, {{
        {0x007f, 0x4009, 0x9136ca0000000000ull, 32},
        {0x027f, 0x4009, 0x9136c98848df3800ull, 32},
        {0x037f, 0x4009, 0x9136c98848df39e9ull, 32},
        {0x087f, 0x4009, 0x9136ca0000000000ull, 32},
        {0x0a7f, 0x4009, 0x9136c98848df4800ull, 32},
        {0x0b7f, 0x4009, 0x9136c98848df39eaull, 32},
        {0x047f, 0x4009, 0x9136c70000000000ull, 32},
        {0x067f, 0x4009, 0x9136c98848df3000ull, 32},
        {0x077f, 0x4009, 0x9136c98848df39e7ull, 32},
        {0x0c7f, 0x4009, 0x9136c70000000000ull, 32},
        {0x0e7f, 0x4009, 0x9136c98848df3000ull, 32},
        {0x0f7f, 0x4009, 0x9136c98848df39e7ull, 32},
    }}},
    {0x4008, 0xf9c0000000000000ull, {{
        {0x007f, 0x400b, 0xb852830000000000ull, 32},
        {0x027f, 0x400b, 0xb852826498177000ull, 32},
        {0x037f, 0x400b, 0xb852826498176740ull, 32},
        {0x087f, 0x400b, 0xb852850000000000ull, 32},
        {0x0a7f, 0x400b, 0xb852826498177000ull, 32},
        {0x0b7f, 0x400b, 0xb852826498176741ull, 32},
        {0x047f, 0x400b, 0xb852810000000000ull, 32},
        {0x067f, 0x400b, 0xb852826498175800ull, 32},
        {0x077f, 0x400b, 0xb85282649817673eull, 32},
        {0x0c7f, 0x400b, 0xb852810000000000ull, 32},
        {0x0e7f, 0x400b, 0xb852826498175800ull, 32},
        {0x0f7f, 0x400b, 0xb85282649817673eull, 32},
    }}},
    {0x4008, 0xfa00000000000000ull, {{
        {0x007f, 0x400b, 0xb889c30000000000ull, 32},
        {0x027f, 0x400b, 0xb889c36d3e55b000ull, 32},
        {0x037f, 0x400b, 0xb889c36d3e55b38dull, 32},
        {0x087f, 0x400b, 0xb889c60000000000ull, 32},
        {0x0a7f, 0x400b, 0xb889c36d3e55c000ull, 32},
        {0x0b7f, 0x400b, 0xb889c36d3e55b38eull, 32},
        {0x047f, 0x400b, 0xb889c20000000000ull, 32},
        {0x067f, 0x400b, 0xb889c36d3e55a800ull, 32},
        {0x077f, 0x400b, 0xb889c36d3e55b38bull, 32},
        {0x0c7f, 0x400b, 0xb889c20000000000ull, 32},
        {0x0e7f, 0x400b, 0xb889c36d3e55a800ull, 32},
        {0x0f7f, 0x400b, 0xb889c36d3e55b38bull, 32},
    }}},
    {0x4008, 0xfa20000000000000ull, {{
        {0x007f, 0x400b, 0xb8a5650000000000ull, 32},
        {0x027f, 0x400b, 0xb8a564b64eae9800ull, 32},
        {0x037f, 0x400b, 0xb8a564b64eae9537ull, 32},
        {0x087f, 0x400b, 0xb8a5670000000000ull, 32},
        {0x0a7f, 0x400b, 0xb8a564b64eaea000ull, 32},
        {0x0b7f, 0x400b, 0xb8a564b64eae9539ull, 32},
        {0x047f, 0x400b, 0xb8a5630000000000ull, 32},
        {0x067f, 0x400b, 0xb8a564b64eae8800ull, 32},
        {0x077f, 0x400b, 0xb8a564b64eae9536ull, 32},
        {0x0c7f, 0x400b, 0xb8a5630000000000ull, 32},
        {0x0e7f, 0x400b, 0xb8a564b64eae8800ull, 32},
        {0x0f7f, 0x400b, 0xb8a564b64eae9536ull, 32},
    }}},
    {0x400b, 0x8000000000000000ull, {{
        {0x007f, 0x400d, 0xea24a80000000000ull, 32},
        {0x027f, 0x400d, 0xea24a91c624a5000ull, 32},
        {0x037f, 0x400d, 0xea24a91c624a5747ull, 32},
        {0x087f, 0x400d, 0xea24ac0000000000ull, 32},
        {0x0a7f, 0x400d, 0xea24a91c624a6800ull, 32},
        {0x0b7f, 0x400d, 0xea24a91c624a5749ull, 32},
        {0x047f, 0x400d, 0xea24a60000000000ull, 32},
        {0x067f, 0x400d, 0xea24a91c624a4800ull, 32},
        {0x077f, 0x400d, 0xea24a91c624a5745ull, 32},
        {0x0c7f, 0x400d, 0xea24a60000000000ull, 32},
        {0x0e7f, 0x400d, 0xea24a91c624a4800ull, 32},
        {0x0f7f, 0x400d, 0xea24a91c624a5745ull, 32},
    }}},
    {0x4020, 0x9502f90000000000ull, {{
        {0x007f, 0x4024, 0xcd21b90000000000ull, 32},
        {0x027f, 0x4024, 0xcd21b887a33e0800ull, 32},
        {0x037f, 0x4024, 0xcd21b887a33e0912ull, 32},
        {0x087f, 0x4024, 0xcd21bb0000000000ull, 32},
        {0x0a7f, 0x4024, 0xcd21b887a33e1000ull, 32},
        {0x0b7f, 0x4024, 0xcd21b887a33e0913ull, 32},
        {0x047f, 0x4024, 0xcd21b50000000000ull, 32},
        {0x067f, 0x4024, 0xcd21b887a33df800ull, 32},
        {0x077f, 0x4024, 0xcd21b887a33e0910ull, 32},
        {0x0c7f, 0x4024, 0xcd21b50000000000ull, 32},
        {0x0e7f, 0x4024, 0xcd21b887a33df800ull, 32},
        {0x0f7f, 0x4024, 0xcd21b887a33e0910ull, 32},
    }}},
    {0x4062, 0xc9f2c9cd04674edfull, {{
        {0x007f, 0x4068, 0xd6d09c0000000000ull, 32},
        {0x027f, 0x4068, 0xd6d09bf456c30000ull, 32},
        {0x037f, 0x4068, 0xd6d09bf456c304a0ull, 32},
        {0x087f, 0x4068, 0xd6d09f0000000000ull, 32},
        {0x0a7f, 0x4068, 0xd6d09bf456c32000ull, 32},
        {0x0b7f, 0x4068, 0xd6d09bf456c304a3ull, 32},
        {0x047f, 0x4068, 0xd6d0990000000000ull, 32},
        {0x067f, 0x4068, 0xd6d09bf456c2f800ull, 32},
        {0x077f, 0x4068, 0xd6d09bf456c3049eull, 32},
        {0x0c7f, 0x4068, 0xd6d0990000000000ull, 32},
        {0x0e7f, 0x4068, 0xd6d09bf456c2f800ull, 32},
        {0x0f7f, 0x4068, 0xd6d09bf456c3049eull, 32},
    }}},
    {0x4063, 0x8000000000000000ull, {{
        {0x007f, 0x4069, 0x88a1230000000000ull, 32},
        {0x027f, 0x4069, 0x88a122b99bea1800ull, 32},
        {0x037f, 0x4069, 0x88a122b99bea170eull, 32},
        {0x087f, 0x4069, 0x88a1250000000000ull, 32},
        {0x0a7f, 0x4069, 0x88a122b99bea2000ull, 32},
        {0x0b7f, 0x4069, 0x88a122b99bea1710ull, 32},
        {0x047f, 0x4069, 0x88a1210000000000ull, 32},
        {0x067f, 0x4069, 0x88a122b99bea0800ull, 32},
        {0x077f, 0x4069, 0x88a122b99bea170dull, 32},
        {0x0c7f, 0x4069, 0x88a1210000000000ull, 32},
        {0x0e7f, 0x4069, 0x88a122b99bea0800ull, 32},
        {0x0f7f, 0x4069, 0x88a122b99bea170dull, 32},
    }}},
    {0x43e7, 0x8000000000000000ull, {{
        {0x007f, 0x43f0, 0xad096b0000000000ull, 32},
        {0x027f, 0x43f0, 0xad096b6802e4a000ull, 32},
        {0x037f, 0x43f0, 0xad096b6802e49cd2ull, 32},
        {0x087f, 0x43f0, 0xad096d0000000000ull, 32},
        {0x0a7f, 0x43f0, 0xad096b6802e4a800ull, 32},
        {0x0b7f, 0x43f0, 0xad096b6802e49cd3ull, 32},
        {0x047f, 0x43f0, 0xad09690000000000ull, 32},
        {0x067f, 0x43f0, 0xad096b6802e49000ull, 32},
        {0x077f, 0x43f0, 0xad096b6802e49cd0ull, 32},
        {0x0c7f, 0x43f0, 0xad09690000000000ull, 32},
        {0x0e7f, 0x43f0, 0xad096b6802e49000ull, 32},
        {0x0f7f, 0x43f0, 0xad096b6802e49cd0ull, 32},
    }}},
    {0x5f3f, 0x8000000000000000ull, {{
        {0x007f, 0x5f4b, 0xad416b0000000000ull, 32},
        {0x027f, 0x5f4b, 0xad416b6802e4a000ull, 32},
        {0x037f, 0x5f4b, 0xad416b6802e49cd2ull, 32},
        {0x087f, 0x5f4b, 0xad416d0000000000ull, 32},
        {0x0a7f, 0x5f4b, 0xad416b6802e4a800ull, 32},
        {0x0b7f, 0x5f4b, 0xad416b6802e49cd3ull, 32},
        {0x047f, 0x5f4b, 0xad41690000000000ull, 32},
        {0x067f, 0x5f4b, 0xad416b6802e49000ull, 32},
        {0x077f, 0x5f4b, 0xad416b6802e49cd0ull, 32},
        {0x0c7f, 0x5f4b, 0xad41690000000000ull, 32},
        {0x0e7f, 0x5f4b, 0xad416b6802e49000ull, 32},
        {0x0f7f, 0x5f4b, 0xad416b6802e49cd0ull, 32},
    }}},
    {0x7e7f, 0x8000000000000000ull, {{
        {0x007f, 0x7e8c, 0xad456b0000000000ull, 48},
        {0x027f, 0x7e8c, 0xad456b6802e4a000ull, 48},
        {0x037f, 0x7e8c, 0xad456b6802e49cd2ull, 48},
        {0x087f, 0x7e8c, 0xad456d0000000000ull, 48},
        {0x0a7f, 0x7e8c, 0xad456b6802e4a800ull, 48},
        {0x0b7f, 0x7e8c, 0xad456b6802e49cd3ull, 48},
        {0x047f, 0x7e8c, 0xad45690000000000ull, 48},
        {0x067f, 0x7e8c, 0xad456b6802e49000ull, 48},
        {0x077f, 0x7e8c, 0xad456b6802e49cd0ull, 48},
        {0x0c7f, 0x7e8c, 0xad45690000000000ull, 48},
        {0x0e7f, 0x7e8c, 0xad456b6802e49000ull, 48},
        {0x0f7f, 0x7e8c, 0xad456b6802e49cd0ull, 48},
    }}},
    {0x7ff1, 0x8000000000000000ull, {{
        {0x007f, 0x7ffe, 0xb147470000000000ull, 48},
        {0x027f, 0x7ffe, 0xb147470293999800ull, 48},
        {0x037f, 0x7ffe, 0xb147470293999449ull, 48},
        {0x087f, 0x7ffe, 0xb147490000000000ull, 48},
        {0x0a7f, 0x7ffe, 0xb14747029399a000ull, 48},
        {0x0b7f, 0x7ffe, 0xb14747029399944bull, 48},
        {0x047f, 0x7ffe, 0xb147450000000000ull, 48},
        {0x067f, 0x7ffe, 0xb147470293998800ull, 48},
        {0x077f, 0x7ffe, 0xb147470293999448ull, 48},
        {0x0c7f, 0x7ffe, 0xb147450000000000ull, 48},
        {0x0e7f, 0x7ffe, 0xb147470293998800ull, 48},
        {0x0f7f, 0x7ffe, 0xb147470293999448ull, 48},
    }}},
    {0x7ffb, 0x8000000000000000ull, {{
        {0x007f, 0x7fff, 0x8000000000000000ull, 56},
        {0x027f, 0x7fff, 0x8000000000000000ull, 56},
        {0x037f, 0x7fff, 0x8000000000000000ull, 56},
        {0x087f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0a7f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0b7f, 0x7fff, 0x8000000000000000ull, 56},
        {0x047f, 0x7fff, 0x8000000000000000ull, 56},
        {0x067f, 0x7fff, 0x8000000000000000ull, 56},
        {0x077f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0c7f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0e7f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0f7f, 0x7fff, 0x8000000000000000ull, 56},
    }}},
    {0x7ffe, 0xffffffffffffffffull, {{
        {0x007f, 0x7fff, 0x8000000000000000ull, 56},
        {0x027f, 0x7fff, 0x8000000000000000ull, 56},
        {0x037f, 0x7fff, 0x8000000000000000ull, 56},
        {0x087f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0a7f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0b7f, 0x7fff, 0x8000000000000000ull, 56},
        {0x047f, 0x7fff, 0x8000000000000000ull, 56},
        {0x067f, 0x7fff, 0x8000000000000000ull, 56},
        {0x077f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0c7f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0e7f, 0x7fff, 0x8000000000000000ull, 56},
        {0x0f7f, 0x7fff, 0x8000000000000000ull, 56},
    }}},
    {0xc006, 0x8200068db8bac711ull, {{
        {0x007f, 0xc007, 0xf876250000000000ull, 32},
        {0x027f, 0xc007, 0xf87623f31171e000ull, 32},
        {0x037f, 0xc007, 0xf87623f31171d422ull, 32},
        {0x087f, 0xc007, 0xf876280000000000ull, 32},
        {0x0a7f, 0xc007, 0xf87623f31171e000ull, 32},
        {0x0b7f, 0xc007, 0xf87623f31171d423ull, 32},
        {0x047f, 0xc007, 0xf8761f0000000000ull, 32},
        {0x067f, 0xc007, 0xf87623f31171b800ull, 32},
        {0x077f, 0xc007, 0xf87623f31171d420ull, 32},
        {0x0c7f, 0xc007, 0xf8761e0000000000ull, 32},
        {0x0e7f, 0xc007, 0xf87623f31171b000ull, 32},
        {0x0f7f, 0xc007, 0xf87623f31171d41full, 32},
    }}},
    {0xc006, 0x8240000000000000ull, {{
        {0x007f, 0xc007, 0xfcee060000000000ull, 32},
        {0x027f, 0xc007, 0xfcee078a2ae92800ull, 32},
        {0x037f, 0xc007, 0xfcee078a2ae930c8ull, 32},
        {0x087f, 0xc007, 0xfcee0b0000000000ull, 32},
        {0x0a7f, 0xc007, 0xfcee078a2ae94800ull, 32},
        {0x0b7f, 0xc007, 0xfcee078a2ae930c9ull, 32},
        {0x047f, 0x7fff, 0xc000000000000000ull, 33},
        {0x067f, 0x7fff, 0xc000000000000000ull, 33},
        {0x077f, 0xc007, 0xfcee078a2ae930c6ull, 32},
        {0x0c7f, 0xc007, 0xfcee050000000000ull, 32},
        {0x0e7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0f7f, 0xc007, 0xfcee078a2ae930c5ull, 32},
    }}},
    {0xc006, 0x8280000000000000ull, {{
        {0x007f, 0xc007, 0xfdb65e0000000000ull, 32},
        {0x027f, 0xc007, 0xfdb65de0b45a2000ull, 32},
        {0x037f, 0xc007, 0xfdb65de0b45a27dcull, 32},
        {0x087f, 0xc007, 0xfdb6600000000000ull, 32},
        {0x0a7f, 0xc007, 0xfdb65de0b45a2000ull, 32},
        {0x0b7f, 0xc007, 0xfdb65de0b45a27dcull, 32},
        {0x047f, 0x7fff, 0xc000000000000000ull, 33},
        {0x067f, 0x7fff, 0xc000000000000000ull, 33},
        {0x077f, 0xc007, 0xfdb65de0b45a1713ull, 32},
        {0x0c7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0e7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0f7f, 0xc007, 0xfdb65de0b45a27d8ull, 32},
    }}},
    {0xc006, 0x82c0000000000000ull, {{
        {0x007f, 0xc007, 0xfe260c0000000000ull, 32},
        {0x027f, 0xc007, 0xfe260acd6fcae800ull, 32},
        {0x037f, 0xc007, 0xfe260acd7056ed36ull, 32},
        {0x087f, 0xc007, 0xfe260d0000000000ull, 32},
        {0x0a7f, 0xc007, 0xfe260acd6fcaf000ull, 32},
        {0x0b7f, 0xc007, 0xfe260acd7056ed37ull, 32},
        {0x047f, 0x7fff, 0xc000000000000000ull, 33},
        {0x067f, 0x7fff, 0xc000000000000000ull, 33},
        {0x077f, 0xc007, 0xfe260acd7056ed34ull, 32},
        {0x0c7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0e7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0f7f, 0xc007, 0xfe260acd7056ed33ull, 32},
    }}},
    {0xc006, 0x8340000000000000ull, {{
        {0x007f, 0xc007, 0xff5e4d0000000000ull, 32},
        {0x027f, 0xc007, 0xff5e4c7af8eb1800ull, 32},
        {0x037f, 0xc007, 0xff5e4c7af8eb1f96ull, 32},
        {0x087f, 0xc007, 0xff5e4e0000000000ull, 32},
        {0x0a7f, 0xc007, 0xff5e4c7af8eb3000ull, 32},
        {0x0b7f, 0xc007, 0xff5e4c7af8eb1f96ull, 32},
        {0x047f, 0x7fff, 0xc000000000000000ull, 33},
        {0x067f, 0x7fff, 0xc000000000000000ull, 33},
        {0x077f, 0xc007, 0xff5e4c7af8eb1f93ull, 32},
        {0x0c7f, 0xc007, 0xff5e480000000000ull, 32},
        {0x0e7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0f7f, 0xc007, 0xff5e4c7af8eb1f92ull, 32},
    }}},
    {0xc006, 0x8c80000000000000ull, {{
        {0x007f, 0xc008, 0x8b23240000000000ull, 32},
        {0x027f, 0xc008, 0x8b2323c2d650b000ull, 32},
        {0x037f, 0xc008, 0x8b2323c2d650ba4dull, 32},
        {0x087f, 0xc008, 0x8b23260000000000ull, 32},
        {0x0a7f, 0xc008, 0x8b2323c2d650c000ull, 32},
        {0x0b7f, 0xc008, 0x8b2323c2d650ba4dull, 32},
        {0x047f, 0x7fff, 0xc000000000000000ull, 33},
        {0x067f, 0x7fff, 0xc000000000000000ull, 33},
        {0x077f, 0xc008, 0x8b2323c2d650b1e9ull, 32},
        {0x0c7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0e7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0f7f, 0xc008, 0x8b2323c2d650ba4aull, 32},
    }}},
    {0xc006, 0xab20000000000000ull, {{
        {0x007f, 0xc008, 0xb190290000000000ull, 32},
        {0x027f, 0xc008, 0xb1902a104fe2d800ull, 32},
        {0x037f, 0xc008, 0xb1902a104fe2cf43ull, 32},
        {0x087f, 0xc008, 0xb1902d0000000000ull, 32},
        {0x0a7f, 0xc008, 0xb1902a104fe2d000ull, 32},
        {0x0b7f, 0xc008, 0xb1902a104fe2cf45ull, 32},
        {0x047f, 0xc008, 0xb190290000000000ull, 32},
        {0x067f, 0xc008, 0xb1902a104fe2c000ull, 32},
        {0x077f, 0xc008, 0xb1902a104fe2cf43ull, 32},
        {0x0c7f, 0xc008, 0xb190280000000000ull, 32},
        {0x0e7f, 0xc008, 0xb1902a104fe2b800ull, 32},
        {0x0f7f, 0xc008, 0xb1902a104fe2cf42ull, 32},
    }}},
    {0xc008, 0xf9d0000000000000ull, {{
        {0x007f, 0xc00b, 0xb88ba60000000000ull, 32},
        {0x027f, 0xc00b, 0xb88ba5d0ded4d000ull, 32},
        {0x037f, 0xc00b, 0xb88ba5d0ded4ccbaull, 32},
        {0x087f, 0xc00b, 0xb88ba70000000000ull, 32},
        {0x0a7f, 0xc00b, 0xb88ba5d0ded4d000ull, 32},
        {0x0b7f, 0xc00b, 0xb88ba5d0ded4ccbbull, 32},
        {0x047f, 0x7fff, 0xc000000000000000ull, 33},
        {0x067f, 0x7fff, 0xc000000000000000ull, 33},
        {0x077f, 0xc00b, 0xb88ba5d0ded4ccb9ull, 32},
        {0x0c7f, 0xc00b, 0xb88ba30000000000ull, 32},
        {0x0e7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0f7f, 0xc00b, 0xb88ba5d0ded4ccb8ull, 32},
    }}},
    {0xc008, 0xfa20000000000000ull, {{
        {0x007f, 0xc00b, 0xb8d3810000000000ull, 32},
        {0x027f, 0xc00b, 0xb8d3806971726800ull, 32},
        {0x037f, 0xc00b, 0xb8d3806971726971ull, 32},
        {0x087f, 0xc00b, 0xb8d3820000000000ull, 32},
        {0x0a7f, 0xc00b, 0xb8d3806971727000ull, 32},
        {0x0b7f, 0xc00b, 0xb8d3806971726972ull, 32},
        {0x047f, 0x7fff, 0xc000000000000000ull, 33},
        {0x067f, 0x7fff, 0xc000000000000000ull, 33},
        {0x077f, 0xc00b, 0xb8d3806971726864ull, 32},
        {0x0c7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0e7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0f7f, 0xc00b, 0xb8d380697172696full, 32},
    }}},
    {0xc00b, 0x8006000000000000ull, {{
        {0x007f, 0xc00d, 0xea3ec90000000000ull, 32},
        {0x027f, 0xc00d, 0xea3eca5d2f4fc800ull, 32},
        {0x037f, 0xc00d, 0xea3eca5d2f5201e2ull, 32},
        {0x087f, 0xc00d, 0xea3ecd0000000000ull, 32},
        {0x0a7f, 0xc00d, 0xea3eca5d2f4fe000ull, 32},
        {0x0b7f, 0xc00d, 0xea3eca5d2f5201e3ull, 32},
        {0x047f, 0x7fff, 0xc000000000000000ull, 33},
        {0x067f, 0x7fff, 0xc000000000000000ull, 33},
        {0x077f, 0xc00d, 0xea3eca5d2f5201e0ull, 32},
        {0x0c7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0e7f, 0x7fff, 0xc000000000000000ull, 33},
        {0x0f7f, 0xc00d, 0xea3eca5d2f5201dfull, 32},
    }}},
};

constexpr int GuestInexact = 0x20;
constexpr int GuestAllExceptions = 0x3d;

#ifdef __linux__
static int* TrapError;
static unsigned TrapPrecision;

static void FirstTrap(int, siginfo_t* info, void* context) {
    const auto* state = static_cast<ucontext_t*>(context)->uc_mcontext.fpregs;
    const unsigned flags = (state->swd | state->mxcsr) & 0x3fu;
    const bool correct = info->si_code == FPE_FLTRES && flags == GuestInexact && *TrapError == 91 && (state->cwd & 0xf00u) == TrapPrecision;
    _exit(correct ? 0 : 1);
}

static int CheckFirstTrap() {
    for (const unsigned precision : {0u, 0x200u, 0x300u}) {
        const pid_t child = fork();
        if (child < 0) return 1;
        if (child == 0) {
            struct sigaction action{};
            action.sa_sigaction = FirstTrap;
            action.sa_flags = SA_SIGINFO;
            sigemptyset(&action.sa_mask);
            if (sigaction(SIGFPE, &action, nullptr) != 0) _exit(3);
            TrapError = __error_nid_postfix();
            TrapPrecision = precision;
            GuestCpu::X87ClearExceptions();
            GuestCpu::SetMxcsr((GuestCpu::Mxcsr() & ~0x3fu) | 0x1f80u);
            const auto control = static_cast<std::uint16_t>(((GuestCpu::X87Control() & ~0xf00u) | precision | 0x3fu) & ~static_cast<unsigned>(GuestInexact));
            *TrapError = 91;
            GuestCpu::SetX87Control(control);
            const volatile long double result = lgammal_nid_postfix(0x1p16380L);
            asm volatile("fwait");
            (void)result;
            _exit(2);
        }
        int status{};
        if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::fprintf(stderr, "lgammal first-trap regression failed: precision=%x status=%x\n", precision, status);
            return 1;
        }
    }
    std::puts("lgammal: all3 precision modes preserve the reference first inexact trap");
    return 0;
}
#endif

int main(int argc, char** argv) {
#ifdef __linux__
    if (argc == 2 && std::strcmp(argv[1], "trap-inexact") == 0) return CheckFirstTrap();
#endif
    if (argc != 1) return 1;

    static_assert(std::numeric_limits<long double>::digits == 64);
    const auto savedControl = GuestCpu::X87Control();
    const auto savedSse = GuestCpu::Mxcsr();
    unsigned checked = 0;
    for (const auto& item : Cases) {
        long double input{};
        std::memcpy(&input, &item.mantissa, sizeof(item.mantissa));
        std::memcpy(reinterpret_cast<unsigned char*>(&input) + 8, &item.exponent, sizeof(item.exponent));
        for (const auto& expected : item.expected) {
            const auto control = static_cast<std::uint16_t>((savedControl & ~0xf00u) | (expected.control & 0xf00u) | 0x3fu);
            if (fesetround_nid_postfix(expected.control & 0xc00u) != 0) std::abort();
            GuestCpu::SetX87Control(control);
            GuestCpu::SetMxcsr(GuestCpu::Mxcsr() | 0x1f80u);
            feclearexcept_nid_postfix(0x3f);
            const long double result = lgammal_nid_postfix(input);
            const int flags = fetestexcept_nid_postfix(GuestAllExceptions);
            std::uint64_t mantissa{};
            std::uint16_t exponent{};
            std::memcpy(&mantissa, &result, sizeof(mantissa));
            std::memcpy(&exponent, reinterpret_cast<const unsigned char*>(&result) + 8, sizeof(exponent));
            if (mantissa != expected.mantissa || exponent != expected.exponent || flags != expected.flags || GuestCpu::X87Control() != control) {
                std::fprintf(stderr, "lgammal large case %u control=%04x input=%04x:%016llx result=%04x:%016llx flags=%x expected=%04x:%016llx flags=%x\n",
                             checked, control, item.exponent, static_cast<unsigned long long>(item.mantissa), exponent, static_cast<unsigned long long>(mantissa), flags,
                             expected.exponent, static_cast<unsigned long long>(expected.mantissa), expected.flags);
                std::abort();
            }
            ++checked;
        }
    }
    feclearexcept_nid_postfix(0x3f);
    GuestCpu::SetX87Control(savedControl);
    GuestCpu::SetMxcsr(savedSse);
    std::printf("lgammal: %u measured large-argument result/flag/control cases passed\n", checked);
    return 0;
}
