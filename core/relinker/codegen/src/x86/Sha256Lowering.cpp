#include <codegen/x86/Sha256Lowering.hpp>
#include <array>
#include <initializer_list>

namespace Codegen {

namespace {

constexpr std::uint8_t kPrefixPacked = 0x66;
constexpr std::uint8_t kShiftRightDwords = 2;
constexpr std::uint8_t kShiftLeftDwords = 6;
constexpr std::uint8_t kShiftRightBytes = 3;
constexpr std::uint8_t kShiftLeftBytes = 7;
constexpr std::uint8_t kWordBits = 32;
constexpr std::uint8_t kRoundKeys = 0;
constexpr std::uint8_t kPunpckldq = 0x62;
constexpr std::uint8_t kPunpcklqdq = 0x6C;
constexpr std::uint8_t kMovdqa = 0x6F;
constexpr std::uint8_t kPshufd = 0x70;
constexpr std::uint8_t kPand = 0xDB;
constexpr std::uint8_t kPor = 0xEB;
constexpr std::uint8_t kPxor = 0xEF;
constexpr std::uint8_t kPaddd = 0xFE;

template<std::size_t TCount>
std::array<std::uint8_t, TCount> _scratch(const Sha256Operands& operands) {
    std::array<std::uint8_t, TCount> scratch{};
    for (std::uint8_t reg = 0, found = 0; found < TCount; ++reg)
        if (reg != operands.Destination && reg != operands.Source && reg != kRoundKeys) scratch[found++] = reg;
    return scratch;
}

void _op(StubBodyBuilder& body, const std::uint8_t opcode, const std::uint8_t dst, const std::uint8_t src) {
    body.Sse(kPrefixPacked, {0x0F, opcode}, dst, src);
}

void _lane(StubBodyBuilder& body, const std::uint8_t dst, const std::uint8_t src, const std::uint8_t lane) {
    body.SseImm(kPrefixPacked, {0x0F, kPshufd}, dst, src, lane);
}

void _sigma(StubBodyBuilder& body, const std::uint8_t out, const std::uint8_t value, const std::uint8_t tmp, const std::initializer_list<std::uint8_t> rotations, const std::uint8_t shift) {
    bool first = true;
    const auto term = [&](const std::uint8_t extension, const std::uint8_t count) {
        const auto target = first ? out : tmp;
        _op(body, kMovdqa, target, value);
        body.ShiftDwordImm(extension, target, count);
        if (!first)
            _op(body, kPxor, out, tmp);
        first = false;
    };
    for (const auto rotation : rotations) {
        term(kShiftRightDwords, rotation);
        term(kShiftLeftDwords, static_cast<std::uint8_t>(kWordBits - rotation));
    }
    if (shift != 0)
        term(kShiftRightDwords, shift);
}

void _choose(StubBodyBuilder& body, const std::uint8_t out, const std::uint8_t e, const std::uint8_t f, const std::uint8_t g) {
    _op(body, kMovdqa, out, f);
    _op(body, kPxor, out, g);
    _op(body, kPand, out, e);
    _op(body, kPxor, out, g);
}

void _majority(StubBodyBuilder& body, const std::uint8_t x, const std::uint8_t y, const std::uint8_t z, const std::uint8_t tmp) {
    _op(body, kMovdqa, tmp, y);
    _op(body, kPand, tmp, z);
    _op(body, kPxor, y, z);
    _op(body, kPand, y, x);
    _op(body, kPxor, y, tmp);
}

void _bigSigma0(StubBodyBuilder& body, const std::uint8_t out, const std::uint8_t value, const std::uint8_t tmp) {
    _sigma(body, out, value, tmp, {2, 13, 22}, 0);
}

void _bigSigma1(StubBodyBuilder& body, const std::uint8_t out, const std::uint8_t value, const std::uint8_t tmp) {
    _sigma(body, out, value, tmp, {6, 11, 25}, 0);
}

void _smallSigma0(StubBodyBuilder& body, const std::uint8_t out, const std::uint8_t value, const std::uint8_t tmp) {
    _sigma(body, out, value, tmp, {7, 18}, 3);
}

void _smallSigma1(StubBodyBuilder& body, const std::uint8_t out, const std::uint8_t value, const std::uint8_t tmp) {
    _sigma(body, out, value, tmp, {17, 19}, 10);
}

void _emitRounds(StubBodyBuilder& body, const Sha256Operands& operands) {
    const auto state = operands.Destination;
    const auto words = operands.Source;
    const auto scratch = _scratch<8>(operands);
    const auto a = scratch[0];
    const auto t2 = scratch[1];
    const auto e = scratch[2];
    const auto g = scratch[3];
    const auto t1 = scratch[4];
    const auto x = scratch[5];
    const auto tmp = scratch[6];
    const auto e1 = scratch[7];
    for (const auto reg : scratch)
        body.Spill(reg);
    _lane(body, a, words, 3);
    _bigSigma0(body, t2, a, tmp);
    _lane(body, e, words, 2);
    _lane(body, g, state, 3);
    _majority(body, a, e, g, tmp);
    _op(body, kPaddd, t2, e);
    _lane(body, e, words, 1);
    _lane(body, g, state, 1);
    _bigSigma1(body, t1, e, tmp);
    _choose(body, x, e, words, g);
    _op(body, kPaddd, t1, x);
    _op(body, kPaddd, t1, state);
    _op(body, kPaddd, t1, kRoundKeys);
    _lane(body, e1, state, 2);
    _op(body, kPaddd, e1, t1);
    _op(body, kPaddd, t2, t1);
    _bigSigma1(body, t1, e1, tmp);
    _choose(body, x, e1, e, words);
    _op(body, kPaddd, t1, x);
    _op(body, kPaddd, t1, g);
    _lane(body, x, kRoundKeys, 1);
    _op(body, kPaddd, t1, x);
    _lane(body, g, state, 3);
    _op(body, kPaddd, g, t1);
    _bigSigma0(body, x, t2, tmp);
    _lane(body, e, words, 2);
    _majority(body, t2, a, e, tmp);
    _op(body, kPaddd, x, a);
    _op(body, kPaddd, x, t1);
    _op(body, kPunpckldq, e1, g);
    _op(body, kPunpckldq, t2, x);
    _op(body, kPunpcklqdq, e1, t2);
    _op(body, kMovdqa, state, e1);
    for (auto reg = scratch.rbegin(); reg != scratch.rend(); ++reg)
        body.Restore(*reg);
}

void _emitMessage1(StubBodyBuilder& body, const Sha256Operands& operands) {
    const auto dst = operands.Destination;
    const auto src = operands.Source;
    const auto scratch = _scratch<3>(operands);
    const auto next = scratch[0];
    const auto sigma = scratch[1];
    const auto tmp = scratch[2];
    for (const auto reg : scratch)
        body.Spill(reg);
    _op(body, kMovdqa, next, dst);
    body.ShiftImm(kShiftRightBytes, next, 4);
    _op(body, kMovdqa, sigma, src);
    body.ShiftImm(kShiftLeftBytes, sigma, 12);
    _op(body, kPor, next, sigma);
    _smallSigma0(body, sigma, next, tmp);
    _op(body, kPaddd, dst, sigma);
    for (auto reg = scratch.rbegin(); reg != scratch.rend(); ++reg)
        body.Restore(*reg);
}

void _emitMessage2(StubBodyBuilder& body, const Sha256Operands& operands) {
    const auto dst = operands.Destination;
    const auto src = operands.Source;
    const auto scratch = _scratch<3>(operands);
    const auto words = scratch[0];
    const auto sigma = scratch[1];
    const auto tmp = scratch[2];
    for (const auto reg : scratch)
        body.Spill(reg);
    _op(body, kMovdqa, words, src);
    body.ShiftImm(kShiftRightBytes, words, 8);
    _smallSigma1(body, sigma, words, tmp);
    _op(body, kPaddd, dst, sigma);
    _op(body, kMovdqa, words, dst);
    body.ShiftImm(kShiftLeftBytes, words, 8);
    _smallSigma1(body, sigma, words, tmp);
    _op(body, kPaddd, dst, sigma);
    for (auto reg = scratch.rbegin(); reg != scratch.rend(); ++reg)
        body.Restore(*reg);
}

}

void Sha256Lowering::EmitOutOfLine(StubBodyBuilder& body, const Sha256Operands& operands) const {
    if (operands.Memory) {
        auto loaded = operands;
        loaded.Memory.reset();
        loaded.Source = _scratch<1>({operands.Operation, operands.Destination, operands.Destination, {}})[0];
        body.Spill(loaded.Source);
        body.Load(loaded.Source, *operands.Memory);
        EmitOutOfLine(body, loaded);
        body.Restore(loaded.Source);
        return;
    }
    switch (operands.Operation) {
    case Sha256Operation::Rnds2:
        _emitRounds(body, operands);
        return;
    case Sha256Operation::Msg1:
        _emitMessage1(body, operands);
        return;
    case Sha256Operation::Msg2:
        _emitMessage2(body, operands);
        return;
    }
}

LoweredBody Sha256Lowering::LowerOutOfLine(const Sha256Operands& operands) const {
    StubBodyBuilder body;
    EmitOutOfLine(body, operands);
    return body.Finish();
}

}
