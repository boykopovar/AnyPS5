#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <vector>

using anyps5::experimental::GuestCpu;

static void put32(std::vector<std::uint8_t>& code, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        code.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

static void put64(std::vector<std::uint8_t>& code, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        code.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

static bool check(const char* name, const std::vector<std::uint8_t>& code,
                  std::uint64_t rax, std::uint64_t rdx,
                  int carry = -1, int overflow = -1) {
    try {
        GuestCpu cpu(code, 256);
        const auto returned = cpu.run(256);
        bool ok = true;
        if (returned != rax) {
            std::fprintf(stderr, "%s rax expected=%llu actual=%llu\n", name,
                static_cast<unsigned long long>(rax),
                static_cast<unsigned long long>(returned));
            ok = false;
        }
        if (cpu.registerValue(2) != rdx) {
            std::fprintf(stderr, "%s rdx expected=%llu actual=%llu\n", name,
                static_cast<unsigned long long>(rdx),
                static_cast<unsigned long long>(cpu.registerValue(2)));
            ok = false;
        }
        if (carry >= 0 && (cpu.registerValue(1) & 0xFF) !=
                static_cast<std::uint64_t>(carry)) {
            std::fprintf(stderr, "%s carry expected=%d actual=%llu\n", name,
                carry, static_cast<unsigned long long>(cpu.registerValue(1) & 0xFF));
            ok = false;
        }
        if (overflow >= 0 && (cpu.registerValue(3) & 0xFF) !=
                static_cast<std::uint64_t>(overflow)) {
            std::fprintf(stderr, "%s overflow expected=%d actual=%llu\n", name,
                overflow, static_cast<unsigned long long>(cpu.registerValue(3) & 0xFF));
            ok = false;
        }
        return ok;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s exception=%s\n", name, e.what());
        return false;
    }
}

static bool expectThrow(const char* name, const std::vector<std::uint8_t>& code) {
    try {
        (void)GuestCpu(code, 256).run(256);
    } catch (const std::runtime_error&) {
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s wrong exception=%s\n", name, e.what());
        return false;
    }
    std::fprintf(stderr, "%s did not throw\n", name);
    return false;
}

static std::vector<std::uint8_t> withRax(std::uint64_t value) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, value);
    return code;
}

static std::vector<std::uint8_t> idiv32(std::uint32_t low, std::uint32_t high,
                                        std::uint32_t divisor) {
    std::vector<std::uint8_t> code{0xB8};
    put32(code, low);
    code.push_back(0xBA);
    put32(code, high);
    code.push_back(0xB9);
    put32(code, divisor);
    code.insert(code.end(), {0xF7, 0xF9, 0xC3});
    return code;
}

static std::vector<std::uint8_t> idiv64(std::uint64_t low, std::uint64_t high,
                                        std::uint64_t divisor) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, low);
    code.insert(code.end(), {0x48, 0xBA});
    put64(code, high);
    code.insert(code.end(), {0x48, 0xB9});
    put64(code, divisor);
    code.insert(code.end(), {0x48, 0xF7, 0xF9, 0xC3});
    return code;
}

static std::vector<std::uint8_t> multiply32(std::uint32_t left,
                                            std::uint32_t right,
                                            std::uint8_t group) {
    std::vector<std::uint8_t> code{0xB8};
    put32(code, left);
    code.push_back(0xB9);
    put32(code, right);
    code.insert(code.end(), {0xF7,
        static_cast<std::uint8_t>(0xC1 | (group << 3)),
        0x0F, 0x92, 0xC1, 0x0F, 0x90, 0xC3, 0xC3});
    return code;
}

static std::vector<std::uint8_t> multiply64(std::uint64_t left,
                                            std::uint64_t right,
                                            std::uint8_t group) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, left);
    code.insert(code.end(), {0x48, 0xB9});
    put64(code, right);
    code.insert(code.end(), {0x48, 0xF7,
        static_cast<std::uint8_t>(0xC1 | (group << 3)),
        0x0F, 0x92, 0xC1, 0x0F, 0x90, 0xC3, 0xC3});
    return code;
}

static void referenceMultiply(std::uint64_t left, std::uint64_t right,
                              std::uint64_t& low, std::uint64_t& high) {
    const std::array<std::uint64_t, 4> a{
        left & 0xFFFF,
        (left >> 16) & 0xFFFF,
        (left >> 32) & 0xFFFF,
        left >> 48};
    const std::array<std::uint64_t, 4> b{
        right & 0xFFFF,
        (right >> 16) & 0xFFFF,
        (right >> 32) & 0xFFFF,
        right >> 48};
    std::array<std::uint64_t, 8> product{};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            product[i + j] += a[i] * b[j];
    for (unsigned i = 0; i < 7; ++i) {
        product[i + 1] += product[i] >> 16;
        product[i] &= 0xFFFF;
    }
    low = product[0] | (product[1] << 16) | (product[2] << 32) |
          (product[3] << 48);
    high = product[4] | (product[5] << 16) | (product[6] << 32) |
           (product[7] << 48);
}

static void referenceSignedMultiply(std::uint64_t left, std::uint64_t right,
                                    std::uint64_t& low, std::uint64_t& high) {
    const bool leftNegative = (left >> 63) != 0;
    const bool rightNegative = (right >> 63) != 0;
    const auto leftMagnitude = leftNegative
        ? std::uint64_t{0} - left : left;
    const auto rightMagnitude = rightNegative
        ? std::uint64_t{0} - right : right;
    referenceMultiply(leftMagnitude, rightMagnitude, low, high);
    if (leftNegative != rightNegative) {
        const auto negatedLow = ~low + 1;
        high = ~high + (negatedLow == 0 ? 1 : 0);
        low = negatedLow;
    }
}

static bool fitsSigned64(std::uint64_t low, std::uint64_t high) {
    if (high == 0) return (low >> 63) == 0;
    return high == UINT64_MAX && (low >> 63) != 0;
}

int main() {
    bool ok = true;

    {
        auto code = withRax(0x1122334455667780ULL);
        code.insert(code.end(), {0x66, 0x98, 0xC3});
        ok &= check("cbw negative", code, 0x112233445566FF80ULL, 0);
    }
    {
        auto code = withRax(0x112233445566777FULL);
        code.insert(code.end(), {0x66, 0x98, 0xC3});
        ok &= check("cbw positive", code, 0x112233445566007FULL, 0);
    }
    {
        auto code = withRax(0x0000000080000000ULL);
        code.insert(code.end(), {0x99, 0xC3});
        ok &= check("cdq negative", code, 0x0000000080000000ULL, 0xFFFFFFFFULL);
    }
    {
        auto code = withRax(0x123456787FFFFFFFULL);
        code.insert(code.end(), {0x99, 0xC3});
        ok &= check("cdq positive", code, 0x123456787FFFFFFFULL, 0);
    }
    {
        auto code = withRax(0x8000000000000000ULL);
        code.insert(code.end(), {0x48, 0x99, 0xC3});
        ok &= check("cqo negative", code, 0x8000000000000000ULL, UINT64_MAX);
    }
    {
        auto code = withRax(0x7FFFFFFFFFFFFFFFULL);
        code.insert(code.end(), {0x48, 0x99, 0xC3});
        ok &= check("cqo positive", code, 0x7FFFFFFFFFFFFFFFULL, 0);
    }
    {
        std::vector<std::uint8_t> code{0x48, 0xBA};
        put64(code, 0x1122334455660000ULL);
        code.insert(code.end(), {0x48, 0xB8});
        put64(code, 0xAABBCCDDEEFF8000ULL);
        code.insert(code.end(), {0x66, 0x99, 0xC3});
        ok &= check("cwd negative", code, 0xAABBCCDDEEFF8000ULL,
            0x112233445566FFFFULL);
    }
    {
        std::vector<std::uint8_t> code{0x48, 0xBA};
        put64(code, 0x1122334455660000ULL);
        code.insert(code.end(), {0x48, 0xB8});
        put64(code, 0xAABBCCDDEEFF7FFFULL);
        code.insert(code.end(), {0x66, 0x99, 0xC3});
        ok &= check("cwd positive", code, 0xAABBCCDDEEFF7FFFULL,
            0x1122334455660000ULL);
    }
    {
        std::vector<std::uint8_t> code{0x48, 0xBA};
        put64(code, 0x1122334455667788ULL);
        code.insert(code.end(), {0x48, 0xB8});
        put64(code, 0x8000000000000000ULL);
        code.insert(code.end(), {0x66, 0x48, 0x99, 0xC3});
        ok &= check("rexw overrides operand size", code,
            0x8000000000000000ULL, UINT64_MAX);
    }

    ok &= check("idiv32 positive", idiv32(100, 0, 7), 14, 2);
    ok &= check("idiv32 negative dividend", idiv32(0xFFFFFF9Cu, 0xFFFFFFFFu, 7),
        0xFFFFFFF2ULL, 0xFFFFFFFEULL);
    ok &= check("idiv32 negative divisor", idiv32(100, 0, 0xFFFFFFF9u),
        0xFFFFFFF2ULL, 2);
    ok &= check("idiv32 both negative", idiv32(0xFFFFFF9Cu, 0xFFFFFFFFu,
        0xFFFFFFF9u), 14, 0xFFFFFFFEULL);
    ok &= check("idiv32 int32 min", idiv32(0x80000000u, 0xFFFFFFFFu, 1),
        0x80000000ULL, 0);
    ok &= check("idiv32 int32 max by minus one",
        idiv32(0x7FFFFFFFu, 0, 0xFFFFFFFFu), 0x80000001ULL, 0);
    ok &= check("idiv32 minus one by two",
        idiv32(0xFFFFFFFFu, 0xFFFFFFFFu, 2), 0, 0xFFFFFFFFULL);
    ok &= check("idiv32 one by minus two", idiv32(1, 0, 0xFFFFFFFEu), 0, 1);
    {
        std::vector<std::uint8_t> code{0xB8};
        put32(code, 100);
        code.push_back(0xBA);
        put32(code, 0);
        code.insert(code.end(), {0xC7, 0x44, 0x24, 0xFC});
        put32(code, 7);
        code.insert(code.end(), {0xF7, 0x7C, 0x24, 0xFC, 0xC3});
        ok &= check("idiv32 memory divisor", code, 14, 2);
    }
    {
        std::vector<std::uint8_t> code{0xB8};
        put32(code, 0xFFFFFF9Cu);
        code.insert(code.end(), {0x99, 0xB9});
        put32(code, 7);
        code.insert(code.end(), {0xF7, 0xF9, 0xC3});
        ok &= check("cdq then idiv32", code, 0xFFFFFFF2ULL, 0xFFFFFFFEULL);
    }
    ok &= expectThrow("idiv32 int32 min by minus one",
        idiv32(0x80000000u, 0xFFFFFFFFu, 0xFFFFFFFFu));
    ok &= expectThrow("idiv32 quotient overflow", idiv32(0, 1, 1));
    ok &= expectThrow("idiv32 divisor zero", idiv32(1, 0, 0));

    ok &= check("idiv64 positive", idiv64(100, 0, 7), 14, 2);
    ok &= check("idiv64 negative dividend",
        idiv64(0xFFFFFFFFFFFFFF9CULL, UINT64_MAX, 7),
        0xFFFFFFFFFFFFFFF2ULL, 0xFFFFFFFFFFFFFFFEULL);
    ok &= check("idiv64 negative divisor",
        idiv64(100, 0, 0xFFFFFFFFFFFFFFF9ULL),
        0xFFFFFFFFFFFFFFF2ULL, 2);
    ok &= check("idiv64 both negative",
        idiv64(0xFFFFFFFFFFFFFF9CULL, UINT64_MAX, 0xFFFFFFFFFFFFFFF9ULL),
        14, 0xFFFFFFFFFFFFFFFEULL);
    ok &= check("idiv64 int64 min by one",
        idiv64(0x8000000000000000ULL, UINT64_MAX, 1),
        0x8000000000000000ULL, 0);
    ok &= check("idiv64 int64 max by minus one",
        idiv64(0x7FFFFFFFFFFFFFFFULL, 0, UINT64_MAX),
        0x8000000000000001ULL, 0);
    ok &= check("idiv64 int64 max by int64 min",
        idiv64(0x7FFFFFFFFFFFFFFFULL, 0, 0x8000000000000000ULL),
        0, 0x7FFFFFFFFFFFFFFFULL);
    ok &= check("idiv64 int64 min by int64 max",
        idiv64(0x8000000000000000ULL, UINT64_MAX, 0x7FFFFFFFFFFFFFFFULL),
        UINT64_MAX, UINT64_MAX);
    ok &= check("idiv64 int64 min by minus two",
        idiv64(0x8000000000000000ULL, UINT64_MAX, 0xFFFFFFFFFFFFFFFEULL),
        0x4000000000000000ULL, 0);
    ok &= check("idiv64 small by int64 min",
        idiv64(7, 0, 0x8000000000000000ULL), 0, 7);
    {
        std::vector<std::uint8_t> code{0x48, 0xB8};
        put64(code, 0xFFFFFFFFFFFFFF9CULL);
        code.insert(code.end(), {0x48, 0x99, 0x48, 0xB9});
        put64(code, 7);
        code.insert(code.end(), {0x48, 0xF7, 0xF9, 0xC3});
        ok &= check("cqo then idiv64", code,
            0xFFFFFFFFFFFFFFF2ULL, 0xFFFFFFFFFFFFFFFEULL);
    }
    ok &= expectThrow("idiv64 int64 min by minus one",
        idiv64(0x8000000000000000ULL, UINT64_MAX, UINT64_MAX));
    ok &= expectThrow("idiv64 quotient overflow",
        idiv64(0, 2, 1));
    ok &= expectThrow("idiv64 positive quotient overflow",
        idiv64(0x8000000000000000ULL, 0, 1));
    ok &= expectThrow("idiv64 divisor zero", idiv64(1, 0, 0));

    ok &= check("mul32 small", multiply32(7, 6, 4), 42, 0, 0, 0);
    ok &= check("mul32 high half", multiply32(0x10000u, 0x10000u, 4),
        0, 1, 1, 1);
    ok &= check("mul32 maximum", multiply32(0xFFFFFFFFu, 0xFFFFFFFFu, 4),
        1, 0xFFFFFFFEULL, 1, 1);
    {
        std::vector<std::uint8_t> code{0xB8};
        put32(code, 7);
        code.insert(code.end(), {0xC7, 0x44, 0x24, 0xFC});
        put32(code, 6);
        code.insert(code.end(), {0xF7, 0x64, 0x24, 0xFC,
            0x0F, 0x92, 0xC1, 0x0F, 0x90, 0xC3, 0xC3});
        ok &= check("mul32 memory operand", code, 42, 0, 0, 0);
    }
    ok &= check("imul32 positive", multiply32(7, 6, 5), 42, 0, 0, 0);
    ok &= check("imul32 both negative", multiply32(0xFFFFFFF9u, 0xFFFFFFFAu, 5),
        42, 0, 0, 0);
    ok &= check("imul32 int32 min by one", multiply32(0x80000000u, 1, 5),
        0x80000000ULL, 0xFFFFFFFFULL, 0, 0);
    ok &= check("imul32 overflow", multiply32(0x80000000u, 0xFFFFFFFFu, 5),
        0x80000000ULL, 0, 1, 1);

    ok &= check("mul64 maximum", multiply64(UINT64_MAX, UINT64_MAX, 4),
        1, 0xFFFFFFFFFFFFFFFEULL, 1, 1);
    ok &= check("mul64 cross limb",
        multiply64(0x100000000ULL, 0x100000000ULL, 4), 0, 1, 1, 1);
    ok &= check("mul64 small", multiply64(0xFFFFFFFFULL, 0xFFFFFFFFULL, 4),
        0xFFFFFFFE00000001ULL, 0, 0, 0);
    ok &= check("mul64 top bit", multiply64(0x8000000000000000ULL, 2, 4),
        0, 1, 1, 1);
    ok &= check("imul64 positive", multiply64(7, 6, 5), 42, 0, 0, 0);
    ok &= check("imul64 both negative", multiply64(
        static_cast<std::uint64_t>(-7), static_cast<std::uint64_t>(-6), 5),
        42, 0, 0, 0);
    ok &= check("imul64 int64 min by one",
        multiply64(0x8000000000000000ULL, 1, 5),
        0x8000000000000000ULL, UINT64_MAX, 0, 0);
    ok &= check("imul64 int64 min by minus one",
        multiply64(0x8000000000000000ULL, UINT64_MAX, 5),
        0x8000000000000000ULL, 0, 1, 1);
    ok &= check("imul64 int64 min by two",
        multiply64(0x8000000000000000ULL, 2, 5),
        0, UINT64_MAX, 1, 1);
    ok &= check("imul64 int64 max squared",
        multiply64(0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL, 5),
        1, 0x3FFFFFFFFFFFFFFFULL, 1, 1);
    ok &= check("imul64 top bit by one",
        multiply64(0x4000000000000000ULL, 1, 5),
        0x4000000000000000ULL, 0, 0, 0);
    ok &= check("imul64 top bit by two",
        multiply64(0x4000000000000000ULL, 2, 5),
        0x8000000000000000ULL, 0, 1, 1);
    ok &= check("imul64 negative top bit by two",
        multiply64(static_cast<std::uint64_t>(-0x4000000000000000LL), 2, 5),
        0x8000000000000000ULL, UINT64_MAX, 0, 0);
    ok &= check("imul64 negative top bit by four",
        multiply64(static_cast<std::uint64_t>(-0x4000000000000000LL), 4, 5),
        0, UINT64_MAX, 1, 1);

    std::uint64_t seed = UINT64_C(0x9E3779B97F4A7C15);
    for (unsigned i = 0; i < 512; ++i) {
        seed = seed * UINT64_C(6364136223846793005) + 1;
        const auto dividend = static_cast<std::int64_t>(seed);
        seed = seed * UINT64_C(6364136223846793005) + 1;
        const auto divisor = static_cast<std::int64_t>(seed | 1);
        if (dividend == INT64_MIN && divisor == -1) continue;
        const auto quotient = dividend / divisor;
        const auto remainder = dividend % divisor;
        ok &= check("idiv64 random",
            idiv64(static_cast<std::uint64_t>(dividend),
                dividend < 0 ? UINT64_MAX : 0,
                static_cast<std::uint64_t>(divisor)),
            static_cast<std::uint64_t>(quotient),
            static_cast<std::uint64_t>(remainder));
    }

    for (unsigned i = 0; i < 512; ++i) {
        seed = seed * UINT64_C(6364136223846793005) + 1;
        const auto dividend = static_cast<std::int32_t>(seed);
        seed = seed * UINT64_C(6364136223846793005) + 1;
        auto divisor = static_cast<std::int32_t>(seed);
        if (divisor == 0) divisor = 1;
        if (dividend == INT32_MIN && divisor == -1) divisor = 2;
        const auto quotient = dividend / divisor;
        const auto remainder = dividend % divisor;
        ok &= check("idiv32 random",
            idiv32(static_cast<std::uint32_t>(dividend),
                dividend < 0 ? UINT32_MAX : 0,
                static_cast<std::uint32_t>(divisor)),
            static_cast<std::uint32_t>(quotient),
            static_cast<std::uint32_t>(remainder));
    }

    for (unsigned i = 0; i < 256; ++i) {
        seed = seed * UINT64_C(6364136223846793005) + 1;
        const auto left = static_cast<std::uint32_t>(seed);
        seed = seed * UINT64_C(6364136223846793005) + 1;
        const auto right = static_cast<std::uint32_t>(seed);
        const auto product = static_cast<std::uint64_t>(left) * right;
        ok &= check("mul32 random", multiply32(left, right, 4),
            static_cast<std::uint32_t>(product), product >> 32,
            (product >> 32) != 0 ? 1 : 0, (product >> 32) != 0 ? 1 : 0);
        const auto signedProduct =
            static_cast<std::int64_t>(static_cast<std::int32_t>(left)) *
            static_cast<std::int64_t>(static_cast<std::int32_t>(right));
        const bool overflow = signedProduct < INT32_MIN || signedProduct > INT32_MAX;
        ok &= check("imul32 random", multiply32(left, right, 5),
            static_cast<std::uint32_t>(signedProduct),
            static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(signedProduct) >> 32),
            overflow ? 1 : 0, overflow ? 1 : 0);
    }

    for (unsigned i = 0; i < 256; ++i) {
        seed = seed * UINT64_C(6364136223846793005) + 1;
        const auto left = seed;
        seed = seed * UINT64_C(6364136223846793005) + 1;
        const auto right = seed;
        std::uint64_t low = 0;
        std::uint64_t high = 0;
        referenceMultiply(left, right, low, high);
        const int carry = high != 0 ? 1 : 0;
        ok &= check("mul64 random", multiply64(left, right, 4), low, high,
            carry, carry);
        referenceSignedMultiply(left, right, low, high);
        const int overflow = fitsSigned64(low, high) ? 0 : 1;
        ok &= check("imul64 random", multiply64(left, right, 5), low, high,
            overflow, overflow);
    }

    if (!ok) return EXIT_FAILURE;
    std::puts("Signed division and wide multiply tests passed");
    return EXIT_SUCCESS;
}
