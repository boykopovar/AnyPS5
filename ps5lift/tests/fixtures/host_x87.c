#include "host.h"

long double host_extended(long a, long b, long c, long d, long e, long f, long g, long double x, long double y) {
    if (a + b + c + d + e + f + g != 28) return 0;
    return x / y;
}
