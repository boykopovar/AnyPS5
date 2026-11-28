// Stage 3: source of truth for compiler-generated x86-64 functions.
// All functions accept one unsigned integer in EDI and return EAX.
// Deliberately avoid external calls, globals and undefined signed overflow.
__attribute__((noinline)) unsigned fixture_triangular(unsigned n) {
    unsigned s = 0;
    for (unsigned i = 1; i <= n; ++i) s += i;
    return s;
}
__attribute__((noinline)) unsigned fixture_bitmix(unsigned n) {
    return ((n << 2) | (n >> 3)) & 255u;
}
__attribute__((noinline)) unsigned fixture_popcount(unsigned n) {
    unsigned c = 0;
    while (n) { c += n & 1u; n >>= 1; }
    return c;
}
__attribute__((noinline)) unsigned fixture_factorial(unsigned n) {
    unsigned a = 1;
    for (unsigned i = 2; i <= n; ++i) a *= i;
    return a;
}
__attribute__((noinline)) unsigned fixture_fib(unsigned n) {
    unsigned a = 0, b = 1;
    while (n--) { unsigned t = a + b; a = b; b = t; }
    return a;
}

__attribute__((noinline)) unsigned fixture_gcd(unsigned n) {
    unsigned a = n, b = 84u;
    while (b != 0) {
        unsigned remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}
__attribute__((noinline)) unsigned fixture_digit_sum(unsigned n) {
    unsigned sum = 0;
    do {
        sum += n % 10u;
        n /= 10u;
    } while (n != 0);
    return sum;
}
__attribute__((noinline)) unsigned fixture_reverse_bits(unsigned n) {
    unsigned result = 0;
    for (unsigned i = 0; i < 32; ++i) {
        result = (result << 1) | (n & 1u);
        n >>= 1;
    }
    return result;
}

/* SysV AMD64: RDI points to a guest array, ESI is the element count.
 * Both functions operate only on caller-provided memory, with no relocations. */
__attribute__((noinline)) unsigned fixture_array_sum(const unsigned* a, unsigned n) {
    unsigned sum = 0;
    for (unsigned i = 0; i < n; ++i) sum += a[i];
    return sum;
}
__attribute__((noinline)) unsigned fixture_array_sort(unsigned* a, unsigned n) {
    /* Insertion sort: return the sum after sorting, allowing verification
     * of both the returned value and all guest-memory mutations. */
    for (unsigned i = 1; i < n; ++i) {
        unsigned value = a[i];
        unsigned j = i;
        while (j && a[j - 1] > value) {
            a[j] = a[j - 1];
            --j;
        }
        a[j] = value;
    }
    unsigned checksum = 0;
    for (unsigned i = 0; i < n; ++i) checksum += a[i];
    return checksum;
}

/* Recursive factorial via an explicit callback. No symbol relocations:
 * native code receives a native callback; guest code receives a guest
 * trampoline address and uses an indirect call. */
__attribute__((noinline)) unsigned fixture_recursive_factorial(
    unsigned n, unsigned (*recurse)(unsigned)) {
    if (n <= 1u) return 1u;
    return n * recurse(n - 1u);
}

/* Cross-function execution through an ABI-compatible guest callback pointer.
 * The callback is a separately compiled function placed in the same guest
 * code image by the test runner; neither function contains relocations. */
__attribute__((noinline)) unsigned fixture_callback_twice(
    unsigned n, unsigned (*transform)(unsigned)) {
    return transform(n) + transform(n + 1u);
}
