#include <stdarg.h>

#include "host.h"
#include "libc_nids.h"

int guest_printf(const char* format, ...) __asm__(NID_printf);
int guest_snprintf(char* buffer, unsigned long size, const char* format, ...) __asm__(NID_snprintf);
int guest_vsnprintf(char* buffer, unsigned long size, const char* format, va_list arguments) __asm__(NID_vsnprintf);
int guest_sscanf(const char* input, const char* format, ...) __asm__(NID_sscanf);
int guest_puts(const char* text) __asm__(NID_puts);
long double guest_strtold(const char* text, char** end) __asm__(NID_strtold);
double guest_strtod(const char* text, char** end) __asm__(NID_strtod);
long double guest_modfl(long double value, long double* integral) __asm__(NID_modfl);
long double guest_fabsl(long double value) __asm__(NID_fabsl);
int guest_fprintf(void* stream, const char* format, ...) __asm__(NID_fprintf);
int guest_feclearexcept(int flags) __asm__(NID_feclearexcept);
int guest_fetestexcept(int flags) __asm__(NID_fetestexcept);
long double guest_acosl(long double value) __asm__(NID_acosl);
long guest_lrint(double value) __asm__(NID_lrint);
long double guest_remainderl(long double dividend, long double divisor) __asm__(NID_remainderl);
extern char guest_stdout[] __asm__(NID__Stdout);

typedef struct {
    int quot;
    int rem;
} guest_div_t;
typedef struct {
    long quot;
    long rem;
} guest_ldiv_t;
guest_div_t guest_div(int numerator, int denominator) __asm__(NID_div);
guest_ldiv_t guest_ldiv(long numerator, long denominator) __asm__(NID_ldiv);
guest_ldiv_t guest_lldiv(long long numerator, long long denominator) __asm__(NID_lldiv);
guest_ldiv_t guest_imaxdiv(long numerator, long denominator) __asm__(NID_imaxdiv);

static int format_list(char* buffer, unsigned long size, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int count = guest_vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return count;
}

static volatile long double third = 1.0L;
static volatile long double infinity = __builtin_infl();

static void print_extended(const char* label, long double value, int flags) {
    unsigned long long significand;
    unsigned short exponent;
    __builtin_memcpy(&significand, &value, sizeof(significand));
    __builtin_memcpy(&exponent, (const char*)&value + 8, sizeof(exponent));
    guest_printf("%s %04x %016llx %x\n", label, exponent, significand, flags);
}

int main(void) {
    char buffer[256];
    guest_feclearexcept(0x3f);
    const long double angle = guest_acosl(1.0L);
    const int angleInvalid = guest_fetestexcept(0x01);
    guest_printf("acosl %.1Lf %x\n", angle, angleInvalid);
    guest_printf("integers %d %u %ld %lld %x %llx %o %c|%s|\n", -42, 42u, -1234567890123l, 9223372036854775807ll, 0xbeefu, 0x123456789abcdefull, 8, 'z', "text");
    guest_printf("many %d %d %d %d %d %d %d %d %d %d\n", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    guest_printf("doubles %.3f %e %g %a %.17g %.17g %.17g %.17g %.17g %.17g\n", 3.14159, 6.02e23, 0.0001, 1.5, 0.1, 0.2, 0.3, 1e300, -2.5, 1.0 / 3.0);
    third = third / 3.0L;
    guest_printf("extended %.20Lf %.19Le %Le %d %.3Lf\n", third, third, -third * 1e100L, 7, (long double)2.0005);
    guest_printf("mixed %d %.2f %s %Lg %ld %.1f %c\n", 1, 2.25, "three", (long double)4.5, 5l, 6.75, '7');
    const int count = guest_snprintf(buffer, sizeof(buffer), "%08.3f|%-6d|%+d|%5s|%.2s", -3.5, 12, 34, "ab", "xyz");
    guest_puts(buffer);
    guest_printf("count %d\n", count);
    const int truncated = guest_snprintf(buffer, 8, "%s", "truncated output");
    guest_printf("truncated %d %s\n", truncated, buffer);
    const int listed = format_list(buffer, sizeof(buffer), "list %d %.2f %s %Lf %d %d %d %d %d", 1, 2.5, "c", (long double)0.25, 5, 6, 7, 8, 9);
    guest_printf("%s (%d)\n", buffer, listed);

    int number = 0;
    double real = 0;
    long double extended = 0;
    char word[16] = {0};
    const int scanned = guest_sscanf("  17 2.75 0.1 word", "%d %lf %Lf %15s", &number, &real, &extended, word);
    guest_printf("scanned %d: %d %.2f %.25Lf %s\n", scanned, number, real, extended, word);

    char* end = 0;
    const long double parsed = guest_strtold("3.14159265358979323846264338327950288 rest", &end);
    guest_printf("strtold %.25Lg %.19Le |%s|\n", parsed, parsed, end);
    guest_printf("strtold tiny %.10Lg huge %Lg\n", guest_strtold("1e-4940", 0), guest_strtold("1e4933", 0));
    guest_printf("strtod %.17g\n", guest_strtod("2.718281828459045", 0));
    long double whole = 0;
    const long double fraction = guest_modfl(-12.625L, &whole);
    guest_printf("modfl %Lf %Lf\n", fraction, whole);
    guest_printf("fabsl %.19Le\n", guest_fabsl(-third));
    guest_fprintf(guest_stdout, "fprintf %s %d\n", "_Stdout", 5);
    const guest_div_t small = guest_div(-17, 5);
    const guest_ldiv_t wide = guest_ldiv(-9000000000000l, 7);
    const guest_ldiv_t longer = guest_lldiv(9000000000001ll, -4);
    const guest_ldiv_t widest = guest_imaxdiv(123456789012345l, 1000);
    guest_printf("div %d %d ldiv %ld %ld lldiv %ld %ld imaxdiv %ld %ld\n", small.quot, small.rem, wide.quot, wide.rem, longer.quot, longer.rem, widest.quot,
                 widest.rem);
    guest_feclearexcept(0x3f);
    const long huge = guest_lrint(1e30);
    const int hugeFlags = guest_fetestexcept(0x3f);
    guest_feclearexcept(0x3f);
    const long half = guest_lrint(2.5);
    const int halfFlags = guest_fetestexcept(0x3f);
    guest_printf("lrint %ld %x %ld %x\n", huge, hugeFlags, half, halfFlags);
    guest_feclearexcept(0x3f);
    const long double domain = guest_remainderl(infinity, 1.0L);
    print_extended("remainderl", domain, guest_fetestexcept(0x01));
    guest_feclearexcept(0x3f);
    const long double quiet = guest_remainderl(-__builtin_nanl("0x123"), 1.0L);
    print_extended("remainderl", quiet, guest_fetestexcept(0x01));
    return 0;
}

__asm__(".globl _start\n"
        "_start:\n"
        "    xor %ebp, %ebp\n"
        "    and $-16, %rsp\n"
        "    call main\n"
        "    mov %eax, %edi\n"
        "    call host_exit\n"
        "    ud2\n");
