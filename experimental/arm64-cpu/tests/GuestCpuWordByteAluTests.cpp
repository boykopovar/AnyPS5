#include "GuestCpu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

using anyps5::experimental::GuestCpu;

struct Observation {
    std::uint64_t value;
    std::uint64_t high;
    unsigned flags;
};

static void put16(std::vector<std::uint8_t>& code, std::uint16_t value) {
    code.push_back(static_cast<std::uint8_t>(value));
    code.push_back(static_cast<std::uint8_t>(value >> 8));
}

static void put64(std::vector<std::uint8_t>& code, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        code.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

static void appendFlagCapture(std::vector<std::uint8_t>& code) {
    code.insert(code.end(), {
        0x49, 0x89, 0xC0,
        0x49, 0x89, 0xD1,
        0x0F, 0x92, 0xC2,
        0x0F, 0x90, 0xC3,
        0x0F, 0x94, 0xC1,
        0x0F, 0x98, 0xC0,
        0x40, 0x0F, 0x9A, 0xC6,
        0x0F, 0xB6, 0xD2,
        0x0F, 0xB6, 0xDB,
        0x0F, 0xB6, 0xC9,
        0x0F, 0xB6, 0xC0,
        0x40, 0x0F, 0xB6, 0xF6,
        0xD1, 0xE3,
        0xC1, 0xE1, 0x02,
        0xC1, 0xE0, 0x03,
        0xC1, 0xE6, 0x04,
        0x09, 0xDA,
        0x09, 0xCA,
        0x09, 0xC2,
        0x09, 0xF2,
        0x89, 0xD0,
        0xC3
    });
}

static void appendCarryOverflowCapture(std::vector<std::uint8_t>& code) {
    code.insert(code.end(), {
        0x49, 0x89, 0xC0,
        0x49, 0x89, 0xD1,
        0x0F, 0x92, 0xC2,
        0x0F, 0x90, 0xC3,
        0x0F, 0xB6, 0xD2,
        0x0F, 0xB6, 0xDB,
        0xD1, 0xE3,
        0x09, 0xDA,
        0x89, 0xD0,
        0xC3
    });
}

static Observation runBody(const std::vector<std::uint8_t>& body,
                           std::size_t stackBytes = 512) {
    auto code = body;
    appendFlagCapture(code);
    GuestCpu cpu(code, stackBytes);
    const auto flags = cpu.run(4096);
    return {cpu.registerValue(8), cpu.registerValue(9),
            static_cast<unsigned>(flags & 0x1F)};
}

static Observation runCarryOverflowBody(const std::vector<std::uint8_t>& body,
                                        std::size_t stackBytes = 512) {
    auto code = body;
    appendCarryOverflowCapture(code);
    GuestCpu cpu(code, stackBytes);
    const auto flags = cpu.run(4096);
    return {cpu.registerValue(8), cpu.registerValue(9),
            static_cast<unsigned>(flags & 0x03)};
}

static bool expect(const char* name, const std::vector<std::uint8_t>& body,
                   std::uint64_t expectedValue, unsigned expectedFlags,
                   std::uint64_t valueMask = ~std::uint64_t{0},
                   std::size_t stackBytes = 512) {
    try {
        const auto observed = runBody(body, stackBytes);
        if ((observed.value & valueMask) == (expectedValue & valueMask) &&
            observed.flags == expectedFlags)
            return true;
        std::fprintf(stderr,
            "%s value expected=%llx actual=%llx flags expected=%02x actual=%02x\n",
            name, static_cast<unsigned long long>(expectedValue & valueMask),
            static_cast<unsigned long long>(observed.value & valueMask),
            expectedFlags, observed.flags);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s exception: %s\n", name, e.what());
    }
    return false;
}

static bool expectCarryOverflow(const char* name,
                                const std::vector<std::uint8_t>& body,
                                std::uint64_t expectedValue,
                                std::uint64_t expectedHigh,
                                unsigned expectedFlags) {
    try {
        const auto observed = runCarryOverflowBody(body);
        if (observed.value == expectedValue && observed.high == expectedHigh &&
            observed.flags == expectedFlags)
            return true;
        std::fprintf(stderr,
            "%s value expected=%llx actual=%llx high expected=%llx actual=%llx flags expected=%u actual=%u\n",
            name, static_cast<unsigned long long>(expectedValue),
            static_cast<unsigned long long>(observed.value),
            static_cast<unsigned long long>(expectedHigh),
            static_cast<unsigned long long>(observed.high),
            expectedFlags, observed.flags);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s exception: %s\n", name, e.what());
    }
    return false;
}

static bool expectRegisters(const char* name, std::vector<std::uint8_t> body,
                            std::uint64_t expectedRax,
                            std::uint64_t expectedRdx) {
    body.push_back(0xC3);
    try {
        GuestCpu cpu(body, 512);
        const auto rax = cpu.run(4096);
        const auto rdx = cpu.registerValue(2);
        if (rax == expectedRax && rdx == expectedRdx) return true;
        std::fprintf(stderr,
            "%s rax expected=%llx actual=%llx rdx expected=%llx actual=%llx\n",
            name, static_cast<unsigned long long>(expectedRax),
            static_cast<unsigned long long>(rax),
            static_cast<unsigned long long>(expectedRdx),
            static_cast<unsigned long long>(rdx));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s exception: %s\n", name, e.what());
    }
    return false;
}

static bool expectReject(const char* name,
                         const std::vector<std::uint8_t>& code) {
    try {
        (void)GuestCpu(code, 512).run(4096);
    } catch (const std::runtime_error&) {
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s wrong exception: %s\n", name, e.what());
        return false;
    }
    std::fprintf(stderr, "%s did not throw\n", name);
    return false;
}

static std::vector<std::uint8_t> withAccumulator(std::uint64_t value) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, value);
    return code;
}

static void appendBase(std::vector<std::uint8_t>& code, std::uint64_t value) {
    code.insert(code.end(), {0x48, 0xBB});
    put64(code, value);
}

static void appendHigh(std::vector<std::uint8_t>& code, std::uint64_t value) {
    code.insert(code.end(), {0x48, 0xBA});
    put64(code, value);
}

static void appendCount(std::vector<std::uint8_t>& code, std::uint64_t value) {
    code.insert(code.end(), {0x48, 0xB9});
    put64(code, value);
}

static void appendZeroFlags(std::vector<std::uint8_t>& code) {
    code.insert(code.end(), {0xB9, 0x02, 0x00, 0x00, 0x00, 0x83, 0xF9, 0x01});
}

static void appendZeroTestFlags(std::vector<std::uint8_t>& code) {
    code.insert(code.end(), {0xB9, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC9});
}

static void appendCarryFlag(std::vector<std::uint8_t>& code, bool carry) {
    code.insert(code.end(), {
        0xB9, static_cast<std::uint8_t>(carry ? 1 : 2), 0x00, 0x00, 0x00,
        0x83, 0xF9, static_cast<std::uint8_t>(carry ? 2 : 1)
    });
}

static std::vector<std::uint8_t> wordAlu(std::uint64_t rax, std::uint64_t rbx,
                                         std::vector<std::uint8_t> instruction) {
    auto body = withAccumulator(rax);
    appendBase(body, rbx);
    body.insert(body.end(), instruction.begin(), instruction.end());
    return body;
}

static std::vector<std::uint8_t> wordAluCarry(std::uint64_t rax,
                                              std::uint64_t rbx, bool carry,
                                              std::vector<std::uint8_t> instruction) {
    auto body = withAccumulator(rax);
    appendBase(body, rbx);
    appendCarryFlag(body, carry);
    body.insert(body.end(), instruction.begin(), instruction.end());
    return body;
}

int main() {
    constexpr std::uint64_t upper = UINT64_C(0x1111222233330000);
    constexpr std::uint64_t base = UINT64_C(0x4444555566660000);
    constexpr std::uint64_t high = UINT64_C(0xDDDD000000000000);

    if (!expect("add ax,bx",
            wordAlu(upper | 0x7FFF, base | 0x0001, {0x66, 0x01, 0xD8}),
            upper | 0x8000, 26)) return EXIT_FAILURE;
    if (!expect("sub ax,bx",
            wordAlu(upper | 0x0000, base | 0x0001, {0x66, 0x29, 0xD8}),
            upper | 0xFFFF, 25)) return EXIT_FAILURE;
    if (!expect("adc ax,bx",
            wordAluCarry(upper | 0xFFFF, base | 0x0000, true, {0x66, 0x11, 0xD8}),
            upper | 0x0000, 21)) return EXIT_FAILURE;
    if (!expect("sbb ax,bx",
            wordAluCarry(upper | 0x0000, base | 0x0000, true, {0x66, 0x19, 0xD8}),
            upper | 0xFFFF, 25)) return EXIT_FAILURE;
    if (!expect("and ax,bx",
            wordAlu(upper | 0xF0F0, base | 0x0FF0, {0x66, 0x21, 0xD8}),
            upper | 0x00F0, 16)) return EXIT_FAILURE;
    if (!expect("or ax,bx",
            wordAlu(upper | 0x00F0, base | 0x0F00, {0x66, 0x09, 0xD8}),
            upper | 0x0FF0, 16)) return EXIT_FAILURE;
    if (!expect("xor ax,bx",
            wordAlu(upper | 0xFFFF, base | 0x00FF, {0x66, 0x31, 0xD8}),
            upper | 0xFF00, 24)) return EXIT_FAILURE;
    if (!expect("cmp ax,bx equal",
            wordAlu(upper | 0x0005, base | 0x0005, {0x66, 0x39, 0xD8}),
            upper | 0x0005, 20)) return EXIT_FAILURE;
    if (!expect("cmp ax,bx borrow",
            wordAlu(upper | 0x0000, base | 0x0001, {0x66, 0x39, 0xD8}),
            upper | 0x0000, 25)) return EXIT_FAILURE;
    if (!expect("test ax,bx zero",
            wordAlu(upper | 0x8000, base | 0x0001, {0x66, 0x85, 0xD8}),
            upper | 0x8000, 20)) return EXIT_FAILURE;
    if (!expect("test ax,bx sign",
            wordAlu(upper | 0x8000, base | 0x8000, {0x66, 0x85, 0xD8}),
            upper | 0x8000, 24)) return EXIT_FAILURE;
    {
        auto body = wordAlu(upper | 0x0002, base | 0x0003,
                            {0x66, 0x03, 0xD8, 0x48, 0x89, 0xD8});
        if (!expect("add bx,ax preserves upper", body, base | 0x0005, 16))
            return EXIT_FAILURE;
    }

    if (!expect("81 add ax,imm16",
            wordAlu(upper | 0x0001, 0, {0x66, 0x81, 0xC0, 0xFF, 0x7F}),
            upper | 0x8000, 26)) return EXIT_FAILURE;
    if (!expect("81 sub ax,imm16",
            wordAlu(upper | 0x0000, 0, {0x66, 0x81, 0xE8, 0x01, 0x00}),
            upper | 0xFFFF, 25)) return EXIT_FAILURE;
    if (!expect("81 and ax,imm16",
            wordAlu(upper | 0xF0F0, 0, {0x66, 0x81, 0xE0, 0xF0, 0x0F}),
            upper | 0x00F0, 16)) return EXIT_FAILURE;
    if (!expect("81 cmp ax,imm16",
            wordAlu(upper | 0x0005, 0, {0x66, 0x81, 0xF8, 0x05, 0x00}),
            upper | 0x0005, 20)) return EXIT_FAILURE;
    if (!expect("81 sub ax,negative imm16",
            wordAlu(upper | 0x8000, 0, {0x66, 0x81, 0xE8, 0xFF, 0xFF}),
            upper | 0x8001, 9)) return EXIT_FAILURE;
    if (!expect("81 adc ax,imm16 carry",
            wordAluCarry(upper | 0x0000, 0, true, {0x66, 0x81, 0xD0, 0xFF, 0xFF}),
            upper | 0x0000, 21)) return EXIT_FAILURE;
    if (!expect("81 sbb ax,imm16 borrow",
            wordAluCarry(upper | 0x0000, 0, true, {0x66, 0x81, 0xD8, 0x01, 0x00}),
            upper | 0xFFFE, 9)) return EXIT_FAILURE;
    if (!expect("83 add ax,sign extended imm8",
            wordAlu(upper | 0x0001, 0, {0x66, 0x83, 0xC0, 0xFF}),
            upper | 0x0000, 21)) return EXIT_FAILURE;
    if (!expect("83 sub ax,imm8",
            wordAlu(upper | 0x0000, 0, {0x66, 0x83, 0xE8, 0x01}),
            upper | 0xFFFF, 25)) return EXIT_FAILURE;
    if (!expect("83 cmp ax,imm8",
            wordAlu(upper | 0x0001, 0, {0x66, 0x83, 0xF8, 0x02}),
            upper | 0x0001, 25)) return EXIT_FAILURE;
    if (!expect("83 adc ax,negative imm8",
            wordAlu(upper | 0x0000, 0, {0x66, 0x83, 0xD0, 0xFF}),
            upper | 0xFFFF, 24)) return EXIT_FAILURE;
    if (!expect("83 sbb ax,imm8",
            wordAlu(upper | 0x0002, 0, {0x66, 0x83, 0xD8, 0x01}),
            upper | 0x0001, 0)) return EXIT_FAILURE;

    if (!expect("inc ax preserves carry",
            wordAluCarry(upper | 0x7FFF, 0, true, {0x66, 0xFF, 0xC0}),
            upper | 0x8000, 27)) return EXIT_FAILURE;
    if (!expect("dec ax preserves carry",
            wordAluCarry(upper | 0x8000, 0, true, {0x66, 0xFF, 0xC8}),
            upper | 0x7FFF, 19)) return EXIT_FAILURE;

    if (!expect("shl ax,1",
            wordAlu(upper | 0x8000, 0, {0x66, 0xD1, 0xE0}),
            upper | 0x0000, 23)) return EXIT_FAILURE;
    if (!expect("shr ax,1",
            wordAlu(upper | 0x8001, 0, {0x66, 0xD1, 0xE8}),
            upper | 0x4000, 19)) return EXIT_FAILURE;
    if (!expect("sar ax,1",
            wordAlu(upper | 0x8001, 0, {0x66, 0xD1, 0xF8}),
            upper | 0xC000, 25)) return EXIT_FAILURE;
    if (!expect("shl ax,4",
            wordAlu(upper | 0x0001, 0, {0x66, 0xC1, 0xE0, 0x04}),
            upper | 0x0010, 0)) return EXIT_FAILURE;
    if (!expect("shl ax,16",
            wordAlu(upper | 0x4000, 0, {0x66, 0xC1, 0xE0, 0x10}),
            upper | 0x0000, 20)) return EXIT_FAILURE;
    if (!expect("shl ax,31",
            wordAlu(upper | 0x0001, 0, {0x66, 0xC1, 0xE0, 0x1F}),
            upper | 0x0000, 20)) return EXIT_FAILURE;
    if (!expect("shr ax,16",
            wordAlu(upper | 0x8000, 0, {0x66, 0xC1, 0xE8, 0x10}),
            upper | 0x0000, 21)) return EXIT_FAILURE;
    if (!expect("shr ax,31",
            wordAlu(upper | 0x8000, 0, {0x66, 0xC1, 0xE8, 0x1F}),
            upper | 0x0000, 20)) return EXIT_FAILURE;
    if (!expect("sar ax,16",
            wordAlu(upper | 0x8000, 0, {0x66, 0xC1, 0xF8, 0x10}),
            upper | 0xFFFF, 25)) return EXIT_FAILURE;
    if (!expect("sar ax,31",
            wordAlu(upper | 0x8000, 0, {0x66, 0xC1, 0xF8, 0x1F}),
            upper | 0xFFFF, 25)) return EXIT_FAILURE;
    {
        auto body = withAccumulator(upper | 0x1234);
        appendZeroTestFlags(body);
        body.insert(body.end(), {0xB1, 0x00, 0x66, 0xD3, 0xE0});
        if (!expect("shl ax,cl zero", body, upper | 0x1234, 20))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1234);
        appendZeroTestFlags(body);
        body.insert(body.end(), {0xB1, 0x20, 0x66, 0xD3, 0xE0});
        if (!expect("shl ax,32 masked", body, upper | 0x1234, 20))
            return EXIT_FAILURE;
    }

    if (!expect("rol ax,1",
            wordAlu(upper | 0x8001, 0, {0x66, 0xD1, 0xC0}),
            upper | 0x0003, 19)) return EXIT_FAILURE;
    if (!expect("ror ax,1",
            wordAlu(upper | 0x0001, 0, {0x66, 0xD1, 0xC8}),
            upper | 0x8000, 19)) return EXIT_FAILURE;
    {
        auto body = withAccumulator(upper | 0x8001);
        appendZeroTestFlags(body);
        body.insert(body.end(), {0x66, 0xC1, 0xC0, 0x02});
        if (!expect("rol ax,2 preserves status", body, upper | 0x0006, 20))
            return EXIT_FAILURE;
    }
    if (!expect("rol ax,16",
            wordAlu(upper | 0x0001, 0, {0x66, 0xC1, 0xC0, 0x10}),
            upper | 0x0001, 17)) return EXIT_FAILURE;
    if (!expect("ror ax,16",
            wordAlu(upper | 0x0001, 0, {0x66, 0xC1, 0xC8, 0x10}),
            upper | 0x0001, 16)) return EXIT_FAILURE;
    if (!expect("rol ax,17",
            wordAlu(upper | 0x8001, 0, {0x66, 0xC1, 0xC0, 0x11}),
            upper | 0x0003, 19)) return EXIT_FAILURE;
    if (!expect("ror ax,17",
            wordAlu(upper | 0x0001, 0, {0x66, 0xC1, 0xC8, 0x11}),
            upper | 0x8000, 19)) return EXIT_FAILURE;
    {
        auto body = withAccumulator(upper | 0x1234);
        appendZeroTestFlags(body);
        body.insert(body.end(), {0xB1, 0x00, 0x66, 0xD3, 0xC0});
        if (!expect("rol ax,cl zero", body, upper | 0x1234, 20))
            return EXIT_FAILURE;
    }

    {
        auto body = withAccumulator(upper | 0x0F0F);
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0xF7, 0xD0});
        if (!expect("not ax leaves flags", body, upper | 0xF0F0, 0))
            return EXIT_FAILURE;
    }
    if (!expect("neg ax",
            wordAlu(upper | 0x0001, 0, {0x66, 0xF7, 0xD8}),
            upper | 0xFFFF, 25)) return EXIT_FAILURE;
    if (!expect("neg ax 0x8000",
            wordAlu(upper | 0x8000, 0, {0x66, 0xF7, 0xD8}),
            upper | 0x8000, 27)) return EXIT_FAILURE;
    if (!expect("neg ax zero",
            wordAlu(upper | 0x0000, 0, {0x66, 0xF7, 0xD8}),
            upper | 0x0000, 20)) return EXIT_FAILURE;
    if (!expect("test ax,imm16 sign",
            wordAlu(upper | 0x8000, 0, {0x66, 0xF7, 0xC0, 0x00, 0x80}),
            upper | 0x8000, 24)) return EXIT_FAILURE;
    if (!expect("test ax,imm16 zero",
            wordAlu(upper | 0x8000, 0, {0x66, 0xF7, 0xC0, 0x01, 0x00}),
            upper | 0x8000, 20)) return EXIT_FAILURE;

    if (!expectCarryOverflow("mul bx wide",
            wordAluCarry(UINT64_C(0xAAAA000000001234),
                         UINT64_C(0xBBBB000000005678), false, {0x66, 0xF7, 0xE3}),
            UINT64_C(0xAAAA000000000060), UINT64_C(0x0000000000000626), 3))
        return EXIT_FAILURE;
    {
        auto body = withAccumulator(UINT64_C(0xAAAA000000000003));
        appendBase(body, UINT64_C(0xBBBB000000000004));
        appendHigh(body, high);
        body.insert(body.end(), {0x66, 0xF7, 0xE3});
        if (!expectCarryOverflow("mul bx small", body,
                UINT64_C(0xAAAA00000000000C), high, 0))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAAAA00000000FFFE));
        appendBase(body, UINT64_C(0xBBBB000000000003));
        appendHigh(body, high);
        body.insert(body.end(), {0x66, 0xF7, 0xEB});
        if (!expectCarryOverflow("imul bx negative", body,
                UINT64_C(0xAAAA00000000FFFA), UINT64_C(0xDDDD00000000FFFF), 0))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAAAA000000004000));
        appendBase(body, UINT64_C(0xBBBB000000000003));
        appendHigh(body, high);
        body.insert(body.end(), {0x66, 0xF7, 0xEB});
        if (!expectCarryOverflow("imul bx overflow", body,
                UINT64_C(0xAAAA00000000C000), high, 3))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAAAA000000008000));
        appendBase(body, UINT64_C(0xBBBB000000000002));
        appendHigh(body, high);
        body.insert(body.end(), {0x66, 0xF7, 0xEB});
        if (!expectCarryOverflow("imul bx negative overflow", body,
                UINT64_C(0xAAAA000000000000), UINT64_C(0xDDDD00000000FFFF), 3))
            return EXIT_FAILURE;
    }

    {
        auto body = withAccumulator(UINT64_C(0xAAAA000000000000));
        appendHigh(body, UINT64_C(0xDDDD000000000001));
        appendBase(body, UINT64_C(0xBBBB000000000002));
        body.insert(body.end(), {0x66, 0xF7, 0xF3});
        if (!expectRegisters("div bx", body,
                UINT64_C(0xAAAA000000008000), UINT64_C(0xDDDD000000000000)))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAAAA00000000FFFA));
        appendHigh(body, UINT64_C(0xDDDD00000000FFFF));
        appendBase(body, UINT64_C(0xBBBB000000000002));
        body.insert(body.end(), {0x66, 0xF7, 0xFB});
        if (!expectRegisters("idiv bx", body,
                UINT64_C(0xAAAA00000000FFFD), UINT64_C(0xDDDD000000000000)))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAAAA00000000FFF9));
        appendHigh(body, UINT64_C(0xDDDD00000000FFFF));
        appendBase(body, UINT64_C(0xBBBB000000000002));
        body.insert(body.end(), {0x66, 0xF7, 0xFB});
        if (!expectRegisters("idiv bx remainder", body,
                UINT64_C(0xAAAA00000000FFFD), UINT64_C(0xDDDD00000000FFFF)))
            return EXIT_FAILURE;
    }

    if (!expectCarryOverflow("imul ax,bx,imm8",
            wordAlu(UINT64_C(0xAAAA000000000002), UINT64_C(0xBBBB000000000003),
                    {0x66, 0x6B, 0xC3, 0x05}),
            UINT64_C(0xAAAA00000000000F), 0, 0)) return EXIT_FAILURE;
    if (!expectCarryOverflow("imul ax,bx,imm16",
            wordAlu(UINT64_C(0xAAAA000000000002), UINT64_C(0xBBBB000000000001),
                    {0x66, 0x69, 0xC3, 0x34, 0x12}),
            UINT64_C(0xAAAA000000001234), 0, 0)) return EXIT_FAILURE;
    if (!expectCarryOverflow("imul ax,bx,negative imm8",
            wordAlu(UINT64_C(0xAAAA000000000002), UINT64_C(0xBBBB000000000003),
                    {0x66, 0x6B, 0xC3, 0xFF}),
            UINT64_C(0xAAAA00000000FFFD), 0, 0)) return EXIT_FAILURE;
    if (!expectCarryOverflow("imul imm8 overflow",
            wordAlu(UINT64_C(0xAAAA000000000000), UINT64_C(0xBBBB000000004000),
                    {0x66, 0x6B, 0xC3, 0x02}),
            UINT64_C(0xAAAA000000008000), 0, 3)) return EXIT_FAILURE;
    if (!expectCarryOverflow("imul imm16 overflow",
            wordAlu(UINT64_C(0xAAAA000000000000), UINT64_C(0xBBBB000000000002),
                    {0x66, 0x69, 0xC3, 0x00, 0x40}),
            UINT64_C(0xAAAA000000008000), 0, 3)) return EXIT_FAILURE;
    if (!expectCarryOverflow("imul ax,bx",
            wordAlu(UINT64_C(0xAAAA000000000005), UINT64_C(0xBBBB000000000003),
                    {0x66, 0x0F, 0xAF, 0xC3}),
            UINT64_C(0xAAAA00000000000F), 0, 0)) return EXIT_FAILURE;
    if (!expectCarryOverflow("imul ax,bx overflow",
            wordAlu(UINT64_C(0xAAAA000000001000), UINT64_C(0xBBBB000000000010),
                    {0x66, 0x0F, 0xAF, 0xC3}),
            UINT64_C(0xAAAA000000000000), 0, 3)) return EXIT_FAILURE;
    if (!expectCarryOverflow("imul ax,bx negative overflow",
            wordAlu(UINT64_C(0xAAAA000000008001), UINT64_C(0xBBBB000000000002),
                    {0x66, 0x0F, 0xAF, 0xC3}),
            UINT64_C(0xAAAA000000000002), 0, 3)) return EXIT_FAILURE;

    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x2222);
        appendZeroTestFlags(body);
        body.insert(body.end(), {0x66, 0x0F, 0x44, 0xC3});
        if (!expect("cmovz taken", body, upper | 0x2222, 20))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x2222);
        appendZeroTestFlags(body);
        body.insert(body.end(), {0x66, 0x0F, 0x45, 0xC3});
        if (!expect("cmovnz taken", body, upper | 0x1111, 20))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x2222);
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x0F, 0x44, 0xC3});
        if (!expect("cmovz not taken", body, upper | 0x1111, 0))
            return EXIT_FAILURE;
    }

    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x0080);
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x0F, 0xB6, 0xC3});
        if (!expect("movzx ax,bl", body, upper | 0x0080, 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x0080);
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x0F, 0xBE, 0xC3});
        if (!expect("movsx ax,bl", body, upper | 0xFF80, 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x8000);
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x0F, 0xB7, 0xC3});
        if (!expect("movzx ax,bx", body, upper | 0x8000, 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x8001);
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x0F, 0xBF, 0xC3});
        if (!expect("movsx ax,bx", body, upper | 0x8001, 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x8000);
        appendZeroFlags(body);
        body.insert(body.end(), {0x0F, 0xBF, 0xC3});
        if (!expect("movsx eax,bx", body, UINT64_C(0x00000000FFFF8000), 0))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, base | 0x8000);
        appendZeroFlags(body);
        body.insert(body.end(), {0x0F, 0xB7, 0xC3});
        if (!expect("movzx eax,bx", body, UINT64_C(0x0000000000008000), 0))
            return EXIT_FAILURE;
    }

    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, UINT64_C(0xBBBB000000001000));
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x8D, 0x43, 0x04});
        if (!expect("lea ax,[rbx+4]", body, upper | 0x1004, 0))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendBase(body, UINT64_C(0xBBBBCCCD0000FFFE));
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x8D, 0x43, 0x04});
        if (!expect("lea ax wraps low word", body, upper | 0x0002, 0))
            return EXIT_FAILURE;
    }

    {
        auto body = withAccumulator(upper | 0x1111);
        appendZeroFlags(body);
        appendCount(body, UINT64_C(0xCCCC000000002222));
        body.insert(body.end(), {0x66, 0x91});
        if (!expect("xchg ax,cx", body, upper | 0x2222, 0))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendZeroFlags(body);
        appendCount(body, UINT64_C(0xCCCC000000002222));
        body.insert(body.end(), {0x66, 0x91, 0x48, 0x89, 0xC8});
        if (!expect("xchg ax,cx other half", body,
                UINT64_C(0xCCCC000000001111), 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendZeroFlags(body);
        appendBase(body, base | 0x3333);
        body.insert(body.end(), {0x66, 0x93});
        if (!expect("xchg ax,bx", body, upper | 0x3333, 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x90});
        if (!expect("66 90 nop", body, upper | 0x1111, 0)) return EXIT_FAILURE;
    }

    {
        auto body = withAccumulator(upper | 0x1111);
        appendZeroFlags(body);
        body.insert(body.end(), {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00,
                                 0xB8, 0x2A, 0x00, 0x00, 0x00});
        if (!expect("66 0f 1f multi-byte nop", body, 42, 0))
            return EXIT_FAILURE;
    }

    if (!expect("accumulator add ax,imm16",
            wordAlu(upper | 0x0001, 0, {0x66, 0x05, 0x34, 0x12}),
            upper | 0x1235, 16)) return EXIT_FAILURE;
    if (!expect("accumulator cmp ax,imm16",
            wordAlu(upper | 0x0000, 0, {0x66, 0x3D, 0x01, 0x00}),
            upper | 0x0000, 25)) return EXIT_FAILURE;

    {
        auto body = withAccumulator(upper | 0x1111);
        body.insert(body.end(), {0x66, 0xC7, 0x44, 0x24, 0xF8, 0x05, 0x00});
        appendBase(body, base | 0x0003);
        body.insert(body.end(), {0x66, 0x01, 0x5C, 0x24, 0xF8,
                                 0x66, 0x8B, 0x44, 0x24, 0xF8});
        if (!expect("add word memory,bx", body, upper | 0x0008, 0))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        body.insert(body.end(), {0x66, 0xC7, 0x44, 0x24, 0xF8, 0x05, 0x00});
        appendBase(body, base | 0x0005);
        body.insert(body.end(), {0x66, 0x39, 0x5C, 0x24, 0xF8});
        if (!expect("cmp word memory,bx", body, upper | 0x1111, 20))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(upper | 0x1111);
        body.insert(body.end(), {0x66, 0xC7, 0x44, 0x24, 0xF8, 0xFF, 0xFF,
                                 0x66, 0x81, 0x44, 0x24, 0xF8, 0x01, 0x00});
        if (!expect("add word memory,imm16", body, upper | 0x1111, 21))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAAAABBBBCCCC1234));
        body.insert(body.end(), {0x66, 0x89, 0x44, 0x24, 0xF8, 0xC3});
        GuestCpu cpu(body, 512);
        const auto returned = cpu.run(4096);
        const auto rsp = cpu.registerValue(4);
        if (returned != UINT64_C(0xAAAABBBBCCCC1234) || rsp != 512 ||
            cpu.readMemory32(rsp - 8) != 0x00001234u) {
            std::fprintf(stderr, "word store wrote more than two bytes\n");
            return EXIT_FAILURE;
        }
    }

    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF0012));
        body.insert(body.end(), {0x66, 0xF6, 0xC0, 0x0F});
        if (!expect("f6 test al immediate", body,
                UINT64_C(0xAABBCCDDEEFF0012), 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF1200));
        body.insert(body.end(), {0xF6, 0xC4, 0x12});
        if (!expect("f6 test ah immediate", body,
                UINT64_C(0xAABBCCDDEEFF1200), 16)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF1000));
        body.insert(body.end(), {0xF6, 0xC4, 0x0F});
        if (!expect("f6 test ah zero", body,
                UINT64_C(0xAABBCCDDEEFF1000), 20)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF00F0));
        body.insert(body.end(), {0xF6, 0xC0, 0x0F});
        if (!expect("f6 test al zero", body,
                UINT64_C(0xAABBCCDDEEFF00F0), 20)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF0011));
        appendZeroTestFlags(body);
        body.insert(body.end(), {0xB4, 0x12, 0xF6, 0xD4});
        if (!expect("f6 not ah", body, UINT64_C(0xAABBCCDDEEFFED11), 20))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF000F));
        appendZeroTestFlags(body);
        body.insert(body.end(), {0xF6, 0xD0});
        if (!expect("f6 not al", body, UINT64_C(0xAABBCCDDEEFF00F0), 20))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(0);
        appendZeroFlags(body);
        body.insert(body.end(), {0x48, 0xBC});
        put64(body, UINT64_C(0x0000000000000012));
        body.insert(body.end(), {0x40, 0xF6, 0xD4, 0x48, 0x89, 0xE0});
        if (!expect("f6 rex not spl", body, 0xED, 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF0080));
        body.insert(body.end(), {0xF6, 0xD8});
        if (!expect("f6 neg al", body, UINT64_C(0xAABBCCDDEEFF0080), 11))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF8000));
        body.insert(body.end(), {0xF6, 0xDC});
        if (!expect("f6 neg ah", body, UINT64_C(0xAABBCCDDEEFF8000), 11))
            return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF0000));
        body.insert(body.end(), {0xF6, 0xD8});
        if (!expect("f6 neg zero", body, UINT64_C(0xAABBCCDDEEFF0000), 20))
            return EXIT_FAILURE;
    }

    if (!expectCarryOverflow("f6 mul ah",
            wordAlu(UINT64_C(0xAABBCCDDEEFF1203), 0, {0xF6, 0xE4}),
            UINT64_C(0xAABBCCDDEEFF0036), 0, 0)) return EXIT_FAILURE;
    if (!expectCarryOverflow("f6 mul overflow",
            wordAlu(UINT64_C(0xAABBCCDDEEFF0280), 0, {0xF6, 0xE4}),
            UINT64_C(0xAABBCCDDEEFF0100), 0, 3)) return EXIT_FAILURE;
    if (!expectCarryOverflow("f6 imul ah",
            wordAlu(UINT64_C(0xAABBCCDDEEFF02FF), 0, {0xF6, 0xEC}),
            UINT64_C(0xAABBCCDDEEFFFFFE), 0, 0)) return EXIT_FAILURE;
    if (!expectCarryOverflow("f6 imul overflow positive",
            wordAlu(UINT64_C(0xAABBCCDDEEFF7F02), 0, {0xF6, 0xEC}),
            UINT64_C(0xAABBCCDDEEFF00FE), 0, 3)) return EXIT_FAILURE;
    if (!expectCarryOverflow("f6 imul overflow negative",
            wordAlu(UINT64_C(0xAABBCCDDEEFF8002), 0, {0xF6, 0xEC}),
            UINT64_C(0xAABBCCDDEEFFFF00), 0, 3)) return EXIT_FAILURE;
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFF00C8));
        appendBase(body, UINT64_C(0xBBBB000000000007));
        body.insert(body.end(), {0xF6, 0xF3});
        if (!expectRegisters("f6 div bl", body,
                UINT64_C(0xAABBCCDDEEFF041C), 0)) return EXIT_FAILURE;
    }
    {
        auto body = withAccumulator(UINT64_C(0xAABBCCDDEEFFFFF9));
        appendBase(body, UINT64_C(0xBBBB000000000002));
        body.insert(body.end(), {0xF6, 0xFB});
        if (!expectRegisters("f6 idiv bl", body,
                UINT64_C(0xAABBCCDDEEFFFFFD), 0)) return EXIT_FAILURE;
    }

    if (!expectReject("16-bit push", {0x66, 0x50})) return EXIT_FAILURE;
    if (!expectReject("16-bit pop", {0x66, 0x58})) return EXIT_FAILURE;
    if (!expectReject("16-bit xchg modrm", {0x66, 0x87, 0xC0}))
        return EXIT_FAILURE;
    if (!expectReject("16-bit leave", {0x66, 0xC9})) return EXIT_FAILURE;
    if (!expectReject("16-bit ret", {0x66, 0xC3})) return EXIT_FAILURE;
    if (!expectReject("16-bit call", {0x66, 0xE8, 0x00, 0x00, 0x00, 0x00}))
        return EXIT_FAILURE;
    if (!expectReject("16-bit jump", {0x66, 0xE9, 0x00, 0x00, 0x00, 0x00}))
        return EXIT_FAILURE;
    if (!expectReject("16-bit short jump", {0x66, 0xEB, 0x00}))
        return EXIT_FAILURE;
    if (!expectReject("16-bit near jcc", {0x66, 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00}))
        return EXIT_FAILURE;
    if (!expectReject("16-bit indirect call", {0x66, 0xFF, 0xD0}))
        return EXIT_FAILURE;
    if (!expectReject("16-bit indirect jump", {0x66, 0xFF, 0xE0}))
        return EXIT_FAILURE;
    if (!expectReject("truncated imm16", {0x66, 0x81, 0xC0, 0xFF}))
        return EXIT_FAILURE;
    if (!expectReject("truncated f6", {0xF6})) return EXIT_FAILURE;
    if (!expectReject("div word overflow", {
            0x66, 0xB8, 0x00, 0x00, 0x66, 0xBA, 0x01, 0x00,
            0x66, 0xBB, 0x01, 0x00, 0x66, 0xF7, 0xF3, 0xC3
        })) return EXIT_FAILURE;
    if (!expectReject("div word zero", {
            0x66, 0xB8, 0x00, 0x00, 0x66, 0xBB, 0x00, 0x00,
            0x66, 0xF7, 0xF3, 0xC3
        })) return EXIT_FAILURE;
    if (!expectReject("idiv word overflow", {
            0x66, 0xB8, 0x00, 0x80, 0x66, 0xBA, 0xFF, 0xFF,
            0x66, 0xBB, 0xFF, 0xFF, 0x66, 0xF7, 0xFB, 0xC3
        })) return EXIT_FAILURE;
    if (!expectReject("idiv word quotient overflow", {
            0x66, 0xB8, 0x00, 0x80, 0x66, 0xBB, 0x01, 0x00,
            0x66, 0xF7, 0xFB, 0xC3
        })) return EXIT_FAILURE;
    if (!expectReject("f6 div byte overflow", {
            0x66, 0xB8, 0x00, 0x01, 0xB3, 0x01, 0xF6, 0xF3, 0xC3
        })) return EXIT_FAILURE;
    if (!expectReject("f6 div byte zero", {
            0x66, 0xB8, 0x00, 0x01, 0xB3, 0x00, 0xF6, 0xF3, 0xC3
        })) return EXIT_FAILURE;
    if (!expectReject("f6 idiv byte overflow", {
            0x66, 0xB8, 0x80, 0x00, 0xB3, 0x01, 0xF6, 0xFB, 0xC3
        })) return EXIT_FAILURE;
    if (!expectReject("f6 idiv byte min overflow", {
            0x66, 0xB8, 0x00, 0x80, 0xB3, 0xFF, 0xF6, 0xFB, 0xC3
        })) return EXIT_FAILURE;

    std::puts("Word and byte ALU instruction tests passed");
    return EXIT_SUCCESS;
}
