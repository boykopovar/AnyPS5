#include <codegen/x86/Sha1Lowering.hpp>
#include <array>

namespace Codegen {

namespace {

constexpr std::uint8_t kPrefixPacked = 0x66;
constexpr std::uint8_t kShiftRightDwords = 2;
constexpr std::uint8_t kShiftLeftDwords = 6;
constexpr std::uint8_t kShiftRightBytes = 3;
constexpr std::uint8_t kShiftLeftBytes = 7;
constexpr std::uint8_t kWordBits = 32;
constexpr std::uint8_t kPunpckldq = 0x62;
constexpr std::uint8_t kPunpcklqdq = 0x6C;
constexpr std::uint8_t kMovdqa = 0x6F;
constexpr std::uint8_t kPshufd = 0x70;
constexpr std::uint8_t kShufpd = 0xC6;
constexpr std::uint8_t kPand = 0xDB;
constexpr std::uint8_t kPor = 0xEB;
constexpr std::uint8_t kPxor = 0xEF;
constexpr std::uint8_t kPaddd = 0xFE;
constexpr std::array<std::uint32_t, 4> kRoundConstants = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};

template<std::size_t TCount>
std::array<std::uint8_t, TCount> _scratch(const Sha1Operands& operands) {
    std::array<std::uint8_t, TCount> scratch{};
    for (std::uint8_t reg = 0, found = 0; found < TCount; ++reg)
        if (reg != operands.Destination && reg != operands.Source) scratch[found++] = reg;
    return scratch;
}

void _op(StubBodyBuilder& body, const std::uint8_t opcode, const std::uint8_t dst, const std::uint8_t src) {
    body.Sse(kPrefixPacked, {0x0F, opcode}, dst, src);
}

void _lane(StubBodyBuilder& body, const std::uint8_t dst, const std::uint8_t src, const std::uint8_t lane) {
    body.SseImm(kPrefixPacked, {0x0F, kPshufd}, dst, src, lane);
}

void _rotate(StubBodyBuilder& body, const std::uint8_t reg, const std::uint8_t tmp, const std::uint8_t count) {
    _op(body, kMovdqa, tmp, reg);
    body.ShiftDwordImm(kShiftLeftDwords, tmp, count);
    body.ShiftDwordImm(kShiftRightDwords, reg, static_cast<std::uint8_t>(kWordBits - count));
    _op(body, kPor, reg, tmp);
}

void _function(StubBodyBuilder& body, const std::uint8_t function, const std::uint8_t out, const std::uint8_t b, const std::uint8_t c, const std::uint8_t d, const std::uint8_t tmp) {
    switch (function) {
    case 0:
        _op(body, kMovdqa, out, c);
        _op(body, kPxor, out, d);
        _op(body, kPand, out, b);
        _op(body, kPxor, out, d);
        return;
    case 2:
        _op(body, kMovdqa, out, b);
        _op(body, kPand, out, c);
        _op(body, kMovdqa, tmp, b);
        _op(body, kPor, tmp, c);
        _op(body, kPand, tmp, d);
        _op(body, kPor, out, tmp);
        return;
    default:
        _op(body, kMovdqa, out, b);
        _op(body, kPxor, out, c);
        _op(body, kPxor, out, d);
        return;
    }
}

StubConstant _roundConstant(const std::uint8_t function) {
    StubConstant constant{};
    for (std::size_t index = 0; index < constant.size(); ++index)
        constant[index] = static_cast<std::uint8_t>(kRoundConstants[function] >> ((index % 4) * 8));
    return constant;
}

void _emitRounds(StubBodyBuilder& body, const Sha1Operands& operands) {
    const auto scratch = _scratch<7>(operands);
    std::array<std::uint8_t, 5> state = {scratch[0], scratch[1], scratch[2], scratch[3], scratch[4]};
    const auto t = scratch[5];
    const auto u = scratch[6];
    for (const auto reg : scratch)
        body.Spill(reg);
    for (std::uint8_t index = 0; index < 4; ++index)
        _lane(body, state[index], operands.Destination, static_cast<std::uint8_t>(3 - index));
    _op(body, kPxor, state[4], state[4]);
    for (std::uint8_t round = 0; round < 4; ++round) {
        const auto [a, b, c, d, e] = state;
        _function(body, operands.Function, t, b, c, d, u);
        _op(body, kPaddd, e, t);
        _op(body, kMovdqa, t, a);
        _rotate(body, t, u, 5);
        _op(body, kPaddd, e, t);
        _lane(body, t, operands.Source, static_cast<std::uint8_t>(3 - round));
        _op(body, kPaddd, e, t);
        body.RipOperand({0x0F, kPaddd}, e, _roundConstant(operands.Function));
        _rotate(body, b, u, 30);
        state = {e, a, b, c, d};
    }
    const auto [a, b, c, d, e] = state;
    _op(body, kPunpckldq, d, c);
    _op(body, kPunpckldq, b, a);
    _op(body, kPunpcklqdq, d, b);
    _op(body, kMovdqa, operands.Destination, d);
    for (auto reg = scratch.rbegin(); reg != scratch.rend(); ++reg)
        body.Restore(*reg);
}

void _emitNext(StubBodyBuilder& body, const Sha1Operands& operands) {
    const auto scratch = _scratch<2>(operands);
    const auto rotated = scratch[0];
    const auto tmp = scratch[1];
    for (const auto reg : scratch)
        body.Spill(reg);
    _op(body, kMovdqa, rotated, operands.Destination);
    _rotate(body, rotated, tmp, 30);
    body.ShiftImm(kShiftRightBytes, rotated, 12);
    body.ShiftImm(kShiftLeftBytes, rotated, 12);
    _op(body, kMovdqa, operands.Destination, operands.Source);
    _op(body, kPaddd, operands.Destination, rotated);
    for (auto reg = scratch.rbegin(); reg != scratch.rend(); ++reg)
        body.Restore(*reg);
}

void _emitMessage1(StubBodyBuilder& body, const Sha1Operands& operands) {
    const auto scratch = _scratch<1>(operands);
    const auto words = scratch[0];
    body.Spill(words);
    _op(body, kMovdqa, words, operands.Source);
    body.SseImm(kPrefixPacked, {0x0F, kShufpd}, words, operands.Destination, 1);
    _op(body, kPxor, operands.Destination, words);
    body.Restore(words);
}

void _emitMessage2(StubBodyBuilder& body, const Sha1Operands& operands) {
    const auto dst = operands.Destination;
    const auto scratch = _scratch<2>(operands);
    const auto words = scratch[0];
    const auto tmp = scratch[1];
    for (const auto reg : scratch)
        body.Spill(reg);
    _op(body, kMovdqa, words, operands.Source);
    body.ShiftImm(kShiftLeftBytes, words, 4);
    _op(body, kPxor, dst, words);
    _rotate(body, dst, tmp, 1);
    _op(body, kMovdqa, words, dst);
    body.ShiftImm(kShiftRightBytes, words, 12);
    _rotate(body, words, tmp, 1);
    _op(body, kPxor, dst, words);
    for (auto reg = scratch.rbegin(); reg != scratch.rend(); ++reg)
        body.Restore(*reg);
}

}

void Sha1Lowering::EmitOutOfLine(StubBodyBuilder& body, const Sha1Operands& operands) const {
    if (operands.Memory) {
        auto loaded = operands;
        loaded.Memory.reset();
        loaded.Source = loaded.Destination;
        loaded.Source = _scratch<1>(loaded)[0];
        body.Spill(loaded.Source);
        body.Load(loaded.Source, *operands.Memory);
        EmitOutOfLine(body, loaded);
        body.Restore(loaded.Source);
        return;
    }
    switch (operands.Operation) {
    case Sha1Operation::Rnds4:
        _emitRounds(body, operands);
        return;
    case Sha1Operation::Nexte:
        _emitNext(body, operands);
        return;
    case Sha1Operation::Msg1:
        _emitMessage1(body, operands);
        return;
    case Sha1Operation::Msg2:
        _emitMessage2(body, operands);
        return;
    }
}

LoweredBody Sha1Lowering::LowerOutOfLine(const Sha1Operands& operands, std::span<const std::uint8_t> trailing) const {
    StubBodyBuilder body;
    EmitOutOfLine(body, operands);
    body.Raw(trailing);
    return body.Finish();
}

}
