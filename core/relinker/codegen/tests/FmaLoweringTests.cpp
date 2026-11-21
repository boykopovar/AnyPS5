#include "VectorExecution.hpp"
#include <codegen/x86/FmaLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;
using Codegen::FmaOperation;
using Codegen::FmaOrder;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes fma(std::uint8_t opcode, std::uint8_t dst, std::uint8_t second, std::uint8_t third, bool wide256) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x40 | ((third & 8) ? 0 : 0x20) | 2),
        static_cast<std::uint8_t>(((~second & 15) << 3) | (wide256 ? 4 : 0) | 1), opcode,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (third & 7))};
}

Bytes fmaFromRax(std::uint8_t opcode, std::uint8_t dst, std::uint8_t second, bool wide256) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | 2), static_cast<std::uint8_t>(((~second & 15) << 3) | (wide256 ? 4 : 0) | 1), opcode,
        static_cast<std::uint8_t>((dst & 7) << 3)};
}

struct Form {
    std::uint8_t Opcode;
    FmaOperation Operation;
    FmaOrder Order;
    bool Scalar;
};

std::vector<Form> forms() {
    std::vector<Form> list;
    for (const std::uint8_t high : {0x90, 0xA0, 0xB0}) {
        const auto order = high == 0x90 ? FmaOrder::Order132 : high == 0xA0 ? FmaOrder::Order213 : FmaOrder::Order231;
        for (std::uint8_t low = 6; low < 16; ++low) {
            const FmaOperation operation = low == 6 ? FmaOperation::AddSubtract : low == 7 ? FmaOperation::SubtractAdd
                : low < 10 ? FmaOperation::MultiplyAdd : low < 12 ? FmaOperation::MultiplySubtract : low < 14 ? FmaOperation::NegatedMultiplyAdd : FmaOperation::NegatedMultiplySubtract;
            list.push_back(Form{static_cast<std::uint8_t>(high | low), operation, order, low >= 8 && (low & 1) != 0});
        }
    }
    return list;
}

bool rejects(const Bytes& bytes) {
    try { (void)Codegen::DecodeFma(bytes.data(), bytes.size()); }
    catch (const Codegen::CodegenException&) { return true; }
    return false;
}

void decoding() {
    for (const auto& form : forms()) {
        for (const bool wide : {false, true}) {
            const auto bytes = fma(form.Opcode, 9, 4, 12, wide);
            const auto operands = Codegen::DecodeFma(bytes.data(), bytes.size());
            require(operands && operands->Operation == form.Operation && operands->Order == form.Order && operands->Scalar == form.Scalar && operands->Wide == (wide && !form.Scalar)
                    && operands->Destination == 9 && operands->Second == 4 && operands->Third == 12 && !operands->Memory,
                "FMA opcode " + std::to_string(form.Opcode) + " was not decoded");
            const auto suffix = std::string(form.Order == FmaOrder::Order132 ? "132" : form.Order == FmaOrder::Order213 ? "213" : "231") + (form.Scalar ? "SS" : "PS");
            require(std::string(operands->Name).ends_with(suffix), "FMA opcode " + std::to_string(form.Opcode) + " is named " + operands->Name);
            const auto loaded = fmaFromRax(form.Opcode, 3, 7, wide);
            const auto memory = Codegen::DecodeFma(loaded.data(), loaded.size());
            require(memory && memory->Memory && memory->Destination == 3 && memory->Second == 7, "a memory FMA lost an operand");
            auto trailing = bytes;
            trailing.push_back(0x90);
            require(rejects(trailing), "an FMA with trailing bytes was accepted");
        }
    }
    for (const std::uint8_t opcode : {0x90, 0x92, 0x95, 0xA0, 0xA5, 0xB0, 0xB4})
        require(!Codegen::DecodeFma(fma(opcode, 1, 2, 3, false).data(), 5), "opcode " + std::to_string(opcode) + " was decoded as an FMA");
    require(!Codegen::DecodeFma(Bytes{0xC4, 0xE2, 0x78, 0xA8, 0xC1}.data(), 5), "an FMA opcode without the 66 prefix was decoded");
    const Bytes rip{0x65, 0xC4, 0x62, 0x55, 0xB8, 0x05, 0x10, 0, 0, 0};
    const auto ripOperands = Codegen::DecodeFma(rip.data(), rip.size());
    require(ripOperands && ripOperands->Wide && ripOperands->Destination == 8 && ripOperands->Second == 5 && ripOperands->Memory && ripOperands->Memory->RipRelative
            && ripOperands->Memory->Prefixes == Bytes{0x65} && std::string(ripOperands->Name) == "VFMADD231PS",
        "a RIP-relative FMA lost its prefix or operand");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const auto& form : forms()) {
        const auto bytes = fma(form.Opcode, 0, 1, 2, !form.Scalar);
        require(!intel->Match(bytes.data(), bytes.size()) && !rosetta->Match(bytes.data(), bytes.size()), "Intel or Rosetta mode lowered an FMA");
        const auto match = avx->Match(bytes.data(), bytes.size());
        require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow && !match->Optional && match->InstructionName.starts_with("VF"),
            "AVX mode did not lower FMA opcode " + std::to_string(form.Opcode) + " through a movable stub");
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using VectorExecution::Machine;
using VectorExecution::Runner;

constexpr std::uint32_t kInvalid = 1;
constexpr std::uint32_t kDenormal = 2;
constexpr std::uint32_t kOverflow = 8;
constexpr std::uint32_t kUnderflow = 0x10;
constexpr std::uint32_t kPrecision = 0x20;
constexpr std::uint32_t kDefaultNan = 0xFFC00000;

class Wide {
public:
    static constexpr std::size_t kWords = 10;

    explicit Wide(std::uint64_t value = 0) { _words[0] = value; }

    void ShiftLeft(unsigned count) {
        const unsigned words = count / 64;
        const unsigned bits = count % 64;
        for (std::size_t index = kWords; index-- > 0;) {
            std::uint64_t value = index >= words ? _words[index - words] << bits : 0;
            if (bits != 0 && index >= words + 1) value |= _words[index - words - 1] >> (64 - bits);
            _words[index] = value;
        }
    }

    [[nodiscard]] int Compare(const Wide& other) const {
        for (std::size_t index = kWords; index-- > 0;)
            if (_words[index] != other._words[index]) return _words[index] < other._words[index] ? -1 : 1;
        return 0;
    }

    void Add(const Wide& other) {
        std::uint64_t carry = 0;
        for (std::size_t index = 0; index < kWords; ++index) {
            const auto sum = _words[index] + other._words[index];
            const auto next = static_cast<std::uint64_t>(sum < _words[index]) + static_cast<std::uint64_t>(sum + carry < sum);
            _words[index] = sum + carry;
            carry = next;
        }
    }

    void Subtract(const Wide& other) {
        std::uint64_t borrow = 0;
        for (std::size_t index = 0; index < kWords; ++index) {
            const auto difference = _words[index] - other._words[index];
            const auto next = static_cast<std::uint64_t>(_words[index] < other._words[index]) + static_cast<std::uint64_t>(difference < borrow);
            _words[index] = difference - borrow;
            borrow = next;
        }
    }

    [[nodiscard]] bool Zero() const {
        for (const auto word : _words) if (word != 0) return false;
        return true;
    }

    [[nodiscard]] int Length() const {
        for (std::size_t index = kWords; index-- > 0;)
            if (_words[index] != 0) return static_cast<int>(index * 64) + std::bit_width(_words[index]);
        return 0;
    }

    [[nodiscard]] bool Bit(int position) const {
        if (position < 0 || position >= static_cast<int>(kWords * 64)) return false;
        return ((_words[static_cast<std::size_t>(position) / 64] >> (position % 64)) & 1) != 0;
    }

    [[nodiscard]] bool AnyBelow(int position) const {
        for (int bit = 0; bit < position && bit < static_cast<int>(kWords * 64); ++bit)
            if (Bit(bit)) return true;
        return false;
    }

    [[nodiscard]] std::uint64_t Bits(int from, int count) const {
        std::uint64_t value = 0;
        for (int bit = 0; bit < count; ++bit)
            if (Bit(from + bit)) value |= std::uint64_t{1} << bit;
        return value;
    }

private:
    std::array<std::uint64_t, kWords> _words{};
};

struct Rounded {
    std::uint64_t Significand;
    bool Inexact;
};

Rounded roundAt(const Wide& magnitude, int drop, unsigned mode, bool negative) {
    if (drop <= 0) return {magnitude.Bits(0, 62) << -drop, false};
    const auto kept = magnitude.Bits(drop, 62);
    const bool half = magnitude.Bit(drop - 1);
    const bool sticky = magnitude.AnyBelow(drop - 1);
    const bool inexact = half || sticky;
    bool up = false;
    if (mode == 0) up = half && (sticky || (kept & 1) != 0);
    else if (mode == 1) up = inexact && negative;
    else if (mode == 2) up = inexact && !negative;
    return {kept + (up ? 1 : 0), inexact};
}

struct Result {
    std::uint32_t Bits;
    std::uint32_t Flags;
};

bool isNan(std::uint32_t bits) { return (bits & 0x7FFFFFFF) > 0x7F800000; }
bool isInfinite(std::uint32_t bits) { return (bits & 0x7FFFFFFF) == 0x7F800000; }
bool isDenormal(std::uint32_t bits) { return (bits & 0x7F800000) == 0 && (bits & 0x007FFFFF) != 0; }
bool isZero(std::uint32_t bits) { return (bits & 0x7FFFFFFF) == 0; }

Result fused(std::uint32_t a, std::uint32_t b, std::uint32_t c, bool negateProduct, bool negateAddend, std::uint32_t mxcsr) {
    const unsigned mode = (mxcsr >> 13) & 3;
    const bool daz = (mxcsr & 0x40) != 0;
    const bool ftz = (mxcsr & 0x8000) != 0;
    if (isNan(a) || isNan(b) || isNan(c)) {
        std::uint32_t flags = 0;
        for (const auto value : {a, b, c})
            if (isNan(value) && (value & 0x00400000) == 0) flags |= kInvalid;
        const auto first = isNan(a) ? a : isNan(b) ? b : c;
        return {first | 0x00400000, flags};
    }
    std::uint32_t flags = 0;
    for (auto* value : {&a, &b, &c}) {
        if (!isDenormal(*value)) continue;
        if (daz) *value &= 0x80000000;
        else flags |= kDenormal;
    }
    if (negateProduct) a ^= 0x80000000;
    if (negateAddend) c ^= 0x80000000;
    const bool productNegative = ((a ^ b) & 0x80000000) != 0;
    const bool addendNegative = (c & 0x80000000) != 0;
    if ((isInfinite(a) && isZero(b)) || (isZero(a) && isInfinite(b))) return {kDefaultNan, kInvalid};
    const bool productInfinite = isInfinite(a) || isInfinite(b);
    if (productInfinite && isInfinite(c) && productNegative != addendNegative) return {kDefaultNan, kInvalid};
    if (productInfinite) return {(productNegative ? 0x80000000u : 0u) | 0x7F800000u, flags};
    if (isInfinite(c)) return {c, flags};

    const auto parts = [](std::uint32_t bits, std::uint64_t& mantissa, int& exponent) {
        const auto field = (bits >> 23) & 0xFF;
        mantissa = field == 0 ? (bits & 0x007FFFFF) : (bits & 0x007FFFFF) | 0x00800000;
        exponent = field == 0 ? -149 : static_cast<int>(field) - 150;
    };
    std::uint64_t ma = 0, mb = 0, mc = 0;
    int ea = 0, eb = 0, ec = 0;
    parts(a, ma, ea);
    parts(b, mb, eb);
    parts(c, mc, ec);
    Wide product(ma * mb);
    Wide addend(mc);
    const int productExponent = ea + eb;
    const int lowest = productExponent < ec ? productExponent : ec;
    if (ma * mb != 0) product.ShiftLeft(static_cast<unsigned>(productExponent - lowest));
    if (mc != 0) addend.ShiftLeft(static_cast<unsigned>(ec - lowest));
    Wide magnitude;
    bool negative = false;
    if (productNegative == addendNegative) {
        magnitude = product;
        magnitude.Add(addend);
        negative = productNegative;
    } else if (product.Compare(addend) >= 0) {
        magnitude = product;
        magnitude.Subtract(addend);
        negative = productNegative;
    } else {
        magnitude = addend;
        magnitude.Subtract(product);
        negative = addendNegative;
    }
    const auto sign = negative ? 0x80000000u : 0u;
    if (magnitude.Zero()) {
        const bool zeroNegative = mode == 1 ? (productNegative || addendNegative) : (productNegative && addendNegative);
        return {zeroNegative ? 0x80000000u : 0u, flags};
    }
    const int length = magnitude.Length();
    const int power = length - 1 + lowest;
    const auto unbounded = roundAt(magnitude, length - 24, mode, negative);
    const int unboundedPower = unbounded.Significand >> 24 != 0 ? power + 1 : power;
    const bool tiny = unboundedPower < -126;
    if (!tiny) {
        auto significand = unbounded.Significand;
        int exponent = power;
        if (significand >> 24 != 0) {
            significand >>= 1;
            ++exponent;
        }
        if (exponent > 127) {
            const bool infinite = mode == 0 || (mode == 2 && !negative) || (mode == 1 && negative);
            return {sign | (infinite ? 0x7F800000u : 0x7F7FFFFFu), flags | kOverflow | kPrecision};
        }
        return {sign | (static_cast<std::uint32_t>(exponent + 127) << 23) | static_cast<std::uint32_t>(significand & 0x007FFFFF), flags | (unbounded.Inexact ? kPrecision : 0)};
    }
    if (ftz) return {sign, flags | kUnderflow | kPrecision};
    const auto denormal = roundAt(magnitude, -149 - lowest, mode, negative);
    if (denormal.Inexact) flags |= kUnderflow | kPrecision;
    return {sign | static_cast<std::uint32_t>(denormal.Significand), flags};
}

struct Lanes {
    std::array<std::uint32_t, 8> Value;
};

bool hostFlushesRoundedMinimumNormal() {
    static const bool flushes = [] {
        Runner narrow(Bytes{0xC5, 0xFD, 0x5A, 0xC1});
        auto machine = VectorExecution::Initial();
        machine.Mxcsr = 0x9F80;
        machine.Ymm[1] = {0x380FFFFFFF000000ull, 0x380FFFFFFF000000ull, 0x380FFFFFFF000000ull, 0x380FFFFFFF000000ull};
        narrow.Run(machine);
        return (machine.Ymm[0][0] & 0x7FFFFFFFull) == 0;
    }();
    return flushes;
}

std::size_t uncheckedMinimumNormal = 0;

bool hostFma() {
    static const bool available = LoweringExecution::HostExecutes(Bytes{0xC4, 0xE2, 0x79, 0xA8, 0xC0});
    return available;
}

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitFma(body, *Codegen::DecodeFma(instruction.data(), instruction.size()));
    auto finished = body.Finish();
    auto bytes = finished.Bytes;
    const auto skip = static_cast<std::uint32_t>(bytes.size() - (finished.ReturnBranchOffset + 5));
    bytes[finished.ReturnBranchOffset] = 0xE9;
    for (unsigned index = 0; index < 4; ++index)
        bytes[finished.ReturnBranchOffset + 1 + index] = static_cast<std::uint8_t>(skip >> (8 * index));
    return bytes;
}

void same(const Machine& actual, const Machine& expected, const std::string& what) {
    for (std::uint8_t reg = 0; reg < 16; ++reg)
        require(actual.Ymm[reg] == expected.Ymm[reg], what + ": ymm" + std::to_string(reg) + " is wrong");
    for (std::uint8_t reg = 0; reg < 16; ++reg)
        require(actual.Gpr[reg] == expected.Gpr[reg], what + ": general-purpose register " + std::to_string(reg) + " is wrong");
    require((actual.Flags & VectorExecution::kStatusFlags) == (expected.Flags & VectorExecution::kStatusFlags), what + ": RFLAGS changed");
    require(actual.Mxcsr == expected.Mxcsr, what + ": MXCSR is " + std::to_string(actual.Mxcsr) + ", wanted " + std::to_string(expected.Mxcsr));
    for (const auto slot : actual.RedZone) require(slot == LoweringExecution::kSentinel, what + ": the red zone was overwritten");
}

std::array<std::uint64_t, 4> pack(const Lanes& lanes) {
    std::array<std::uint64_t, 4> packed{};
    for (unsigned index = 0; index < 8; ++index) packed[index / 2] |= static_cast<std::uint64_t>(lanes.Value[index]) << (32 * (index % 2));
    return packed;
}

std::uint32_t lane(const std::array<std::uint64_t, 4>& value, unsigned index) {
    return static_cast<std::uint32_t>(value[index / 2] >> (32 * (index % 2)));
}

struct Site {
    Form Shape;
    bool Wide;
    bool FromMemory;
    std::uint8_t Destination;
    std::uint8_t Second;
    std::uint8_t Third;
};

class Checker {
public:
    explicit Checker(const Site& site)
        : _site(site),
          _instruction(site.FromMemory ? fmaFromRax(site.Shape.Opcode, site.Destination, site.Second, site.Wide) : fma(site.Shape.Opcode, site.Destination, site.Second, site.Third, site.Wide)),
          _lowered(lowered(_instruction)),
          _native(_instruction) {}

    void Check(const Lanes& destination, const Lanes& second, const Lanes& third, std::uint32_t mxcsr) const {
        auto start = VectorExecution::Initial();
        start.Mxcsr = mxcsr;
        start.Ymm[_site.Destination] = pack(destination);
        start.Ymm[_site.Second] = pack(second);
        if (_site.FromMemory) start.Memory = pack(third);
        else start.Ymm[_site.Third] = pack(third);
        auto machine = start;
        machine.Gpr[0] = reinterpret_cast<std::uint64_t>(machine.Memory.data());
        auto expected = machine;
        const auto& d = machine.Ymm[_site.Destination];
        const auto& s = machine.Ymm[_site.Second];
        const auto& t = _site.FromMemory ? machine.Memory : machine.Ymm[_site.Third];
        const unsigned lanes = _site.Shape.Scalar ? 1 : _site.Wide ? 8 : 4;
        std::array<std::uint64_t, 4> result = _site.Shape.Scalar ? std::array<std::uint64_t, 4>{d[0], d[1], 0, 0} : std::array<std::uint64_t, 4>{};
        if (_site.Shape.Scalar) result[0] &= 0xFFFFFFFF00000000ull;
        std::uint32_t flags = 0;
        bool minimumNormal = false;
        for (unsigned index = 0; index < lanes; ++index) {
            const auto dv = lane(d, index);
            const auto sv = lane(s, index);
            const auto tv = _site.Shape.Scalar && _site.FromMemory && index > 0 ? 0 : lane(t, index);
            const auto a = _site.Shape.Order == FmaOrder::Order132 ? dv : sv;
            const auto b = _site.Shape.Order == FmaOrder::Order213 ? dv : tv;
            const auto c = _site.Shape.Order == FmaOrder::Order132 ? sv : _site.Shape.Order == FmaOrder::Order213 ? tv : dv;
            const auto operation = _site.Shape.Operation;
            const bool negateProduct = operation == FmaOperation::NegatedMultiplyAdd || operation == FmaOperation::NegatedMultiplySubtract;
            const bool negateAddend = operation == FmaOperation::MultiplySubtract || operation == FmaOperation::NegatedMultiplySubtract
                || (operation == FmaOperation::AddSubtract && index % 2 == 0) || (operation == FmaOperation::SubtractAdd && index % 2 == 1);
            const auto value = fused(a, b, c, negateProduct, negateAddend, mxcsr);
            result[index / 2] |= static_cast<std::uint64_t>(value.Bits) << (32 * (index % 2));
            flags |= value.Flags;
            if ((mxcsr & 0x8000) != 0 && (value.Bits & 0x7FFFFFFF) == 0x00800000 && (value.Flags & kPrecision) != 0) minimumNormal = true;
        }
        expected.Ymm[_site.Destination] = result;
        expected.Mxcsr |= flags;
        const auto what = std::string(Codegen::DecodeFma(_instruction.data(), _instruction.size())->Name) + (_site.Wide ? " ymm" : " xmm") + " dst " + std::to_string(_site.Destination) +
            " second " + std::to_string(_site.Second) + " third " + (_site.FromMemory ? std::string("[rax]") : std::to_string(_site.Third)) + " lane0 " +
            std::to_string(destination.Value[0]) + "," + std::to_string(second.Value[0]) + "," + std::to_string(third.Value[0]) + " mxcsr " + std::to_string(mxcsr);
        if (minimumNormal && hostFlushesRoundedMinimumNormal()) {
            ++uncheckedMinimumNormal;
            return;
        }
        _lowered.Run(machine);
        same(machine, expected, what);
        if (!hostFma()) return;
        auto native = start;
        native.Gpr[0] = reinterpret_cast<std::uint64_t>(native.Memory.data());
        _native.Run(native);
        native.Gpr[0] = expected.Gpr[0];
        same(native, expected, what + " on the host");
    }

private:
    Site _site;
    Bytes _instruction;
    Runner _lowered;
    Runner _native;
};

constexpr std::array<std::uint32_t, 30> kSpecials{
    0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00001, 0xFFC00002, 0x7F800003, 0xFF900004,
    0x7F7FFFFF, 0xFF7FFFFF, 0x00800000, 0x80800000, 0x00000001, 0x80000001, 0x007FFFFF, 0x3F000000, 0x40000000, 0x3F800001,
    0x33800000, 0x4B800000, 0x00400000, 0x3EAAAAAB, 0x5F000000, 0x1F000000, 0x00FFFFFF, 0x3F7FFFFF, 0x7FFFFFFF, 0x80400000};

class Inputs {
public:
    explicit Inputs(std::uint32_t seed) : _random(seed) {}

    std::uint32_t Pick() {
        const auto bits = static_cast<std::uint32_t>(_random());
        switch (_random() % 8) {
        case 0: return bits;
        case 1: return (bits & 0x807FFFFF) | ((96 + bits % 64) << 23);
        case 2: return bits & 0x807FFFFF;
        case 3: return (bits & 0x807FFFFF) | ((1 + bits % 30) << 23);
        case 4: return (bits & 0x807FFFFF) | ((220 + bits % 34) << 23);
        case 5: return (bits & 0x80000FFF) | 0x3F800000;
        case 6: return kSpecials[bits % kSpecials.size()];
        default: return (bits & 0x807FFFFF) | ((50 + bits % 30) << 23);
        }
    }

    std::uint32_t Cancel(std::uint32_t a, std::uint32_t b) {
        const double product = static_cast<double>(std::bit_cast<float>(a)) * static_cast<double>(std::bit_cast<float>(b));
        auto bits = std::bit_cast<std::uint32_t>(-static_cast<float>(product));
        if (_random() % 2 == 0) bits += static_cast<std::uint32_t>(_random() % 5) - 2u;
        return bits;
    }

    std::uint32_t Next() { return static_cast<std::uint32_t>(_random()); }

private:
    std::mt19937 _random;
};

constexpr std::array<std::uint32_t, 8> kControls{0x1F80, 0x3F80, 0x5F80, 0x7F80, 0x1FC0, 0x9F80, 0x9FC0 | 0x2000, 0x1F80 | 0x3F};

void sweep(const Checker& checker, const Form& form, Inputs& inputs, unsigned count) {
    for (unsigned round = 0; round < count; ++round) {
        Lanes x{}, y{}, z{};
        for (unsigned index = 0; index < 8; ++index) {
            x.Value[index] = inputs.Pick();
            y.Value[index] = inputs.Pick();
            z.Value[index] = inputs.Pick();
        }
        if (round % 3 == 0) {
            for (unsigned index = 0; index < 8; ++index) {
                auto& a = form.Order == FmaOrder::Order132 ? x : y;
                auto& b = form.Order == FmaOrder::Order213 ? x : z;
                auto& c = form.Order == FmaOrder::Order132 ? y : form.Order == FmaOrder::Order213 ? z : x;
                if (!isNan(a.Value[index]) && !isNan(b.Value[index])) c.Value[index] = inputs.Cancel(a.Value[index], b.Value[index]);
            }
        }
        checker.Check(x, y, z, kControls[round % kControls.size()]);
        if (round % 4 == 0) {
            Lanes single = x, plainY{}, plainZ{};
            single.Value.fill(0x3F800000);
            plainY.Value.fill(0x3F800000);
            plainZ.Value.fill(0x3F800000);
            const auto position = inputs.Next() % 8;
            single.Value[position] = x.Value[position];
            plainY.Value[position] = y.Value[position];
            plainZ.Value[position] = z.Value[position];
            checker.Check(single, plainY, plainZ, kControls[(round / 4) % kControls.size()]);
        }
    }
}

void allForms() {
    Inputs inputs(20261117);
    for (const auto& form : forms()) {
        for (const bool wide : {false, true}) {
            if (form.Scalar && wide) continue;
            const Checker checker(Site{form, wide, false, 1, 2, 3});
            sweep(checker, form, inputs, 2000);
        }
    }
}

void registers() {
    Inputs inputs(7);
    const std::vector<std::array<std::uint8_t, 3>> assignments{{0, 0, 1}, {0, 1, 0}, {0, 1, 1}, {5, 5, 5}, {15, 14, 13}, {8, 15, 9}, {13, 12, 13}};
    for (const auto& form : forms()) {
        if ((form.Opcode & 0x0F) != 8 && (form.Opcode & 0x0F) != 9 && (form.Opcode & 0x0F) != 6 && (form.Opcode & 0x0F) != 0xF) continue;
        for (const bool wide : {false, true}) {
            if (form.Scalar && wide) continue;
            for (const auto& registers : assignments) {
                const Checker checker(Site{form, wide, false, registers[0], registers[1], registers[2]});
                sweep(checker, form, inputs, 24);
            }
            for (const auto& registers : std::vector<std::array<std::uint8_t, 2>>{{0, 1}, {6, 6}, {15, 9}}) {
                const Checker checker(Site{form, wide, true, registers[0], registers[1], 0});
                sweep(checker, form, inputs, 24);
            }
        }
    }
}

void hardCases() {
    const auto form = forms()[22];
    require(form.Opcode == 0xB8, "the cancellation sweep expects VFMADD231PS");
    const Checker checker(Site{form, true, false, 4, 5, 6});
    Inputs inputs(99);
    for (unsigned round = 0; round < 100000; ++round) {
        Lanes x{}, y{}, z{};
        for (unsigned index = 0; index < 8; ++index) {
            y.Value[index] = inputs.Pick();
            z.Value[index] = inputs.Pick();
            x.Value[index] = round % 2 == 0 ? inputs.Cancel(y.Value[index], z.Value[index]) : inputs.Pick();
        }
        checker.Check(x, y, z, kControls[round % kControls.size()]);
    }
}

void execution() {
    allForms();
    registers();
    hardCases();
    if (!hostFma()) std::cout << "the host has no FMA: the lowering was checked against the model only\n";
    if (uncheckedMinimumNormal != 0)
        std::cout << uncheckedMinimumNormal << " cases with an inexact result of the smallest normal under FTZ were not run: this host's VCVTPD2PS flushes a value that rounds up to it, which x86 processors do not\n";
}

#else

void execution() { std::cout << "FMA execution checks require an x86-64 Linux, macOS or Windows build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "FMA lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
