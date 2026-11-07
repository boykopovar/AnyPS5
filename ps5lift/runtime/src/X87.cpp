#include "Runtime.hpp"

#include <ps5lift/State.h>

extern "C" {
#include <softfloat.h>
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cfenv>
#include <cfloat>
#include <cstring>

namespace {

constexpr std::uint16_t StatusInvalid = 1u << 0;
constexpr std::uint16_t StatusDenormal = 1u << 1;
constexpr std::uint16_t StatusZeroDivide = 1u << 2;
constexpr std::uint16_t StatusOverflow = 1u << 3;
constexpr std::uint16_t StatusUnderflow = 1u << 4;
constexpr std::uint16_t StatusPrecision = 1u << 5;
constexpr std::uint16_t StatusStackFault = 1u << 6;
constexpr std::uint16_t StatusSummary = 1u << 7;
constexpr std::uint16_t StatusC0 = 1u << 8;
constexpr std::uint16_t StatusC1 = 1u << 9;
constexpr std::uint16_t StatusC2 = 1u << 10;
constexpr std::uint16_t StatusC3 = 1u << 14;
constexpr std::uint16_t StatusBusy = 1u << 15;
constexpr std::uint16_t StatusConditions = StatusC0 | StatusC1 | StatusC2 | StatusC3;
constexpr unsigned TopShift = 11;
constexpr std::size_t FxStateBytes = 416;
constexpr std::uint16_t InitialControl = 0x037f;


extFloat80_t ReadExtended(const void* source) {
    extFloat80_t value;
    std::memcpy(&value.signif, source, 8);
    std::memcpy(&value.signExp, static_cast<const std::uint8_t*>(source) + 8, 2);
    return value;
}

void WriteExtended(void* destination, const extFloat80_t value) {
    std::memcpy(destination, &value.signif, 8);
    std::memcpy(static_cast<std::uint8_t*>(destination) + 8, &value.signExp, 2);
}

std::uint_fast8_t SoftFloatRounding(const std::uint16_t control) {
    static constexpr std::uint_fast8_t Rounding[4] = {softfloat_round_near_even, softfloat_round_min, softfloat_round_max, softfloat_round_minMag};
    return Rounding[(control >> 10) & 3];
}

extFloat80_t Indefinite() {
    extFloat80_t value;
    value.signExp = 0xffff;
    value.signif = 0xc000000000000000ull;
    return value;
}

extFloat80_t Zero(const bool negative) {
    extFloat80_t value;
    value.signExp = negative ? 0x8000 : 0;
    value.signif = 0;
    return value;
}

bool IsNan(const extFloat80_t value) {
    return (value.signExp & 0x7fff) == 0x7fff && (value.signif & 0x7fffffffffffffffull) != 0;
}

bool IsDenormal(const extFloat80_t value) {
    return (value.signExp & 0x7fff) == 0 && value.signif != 0;
}

bool IsNegative(const extFloat80_t value) {
    return (value.signExp & 0x8000) != 0;
}

bool IsZero(const extFloat80_t value) {
    return (value.signExp & 0x7fff) == 0 && value.signif == 0;
}

bool IsInfinity(const extFloat80_t value) {
    return (value.signExp & 0x7fff) == 0x7fff && value.signif == 0x8000000000000000ull;
}

extFloat80_t Infinity(const bool negative) {
    extFloat80_t value;
    value.signExp = negative ? 0xffff : 0x7fff;
    value.signif = 0x8000000000000000ull;
    return value;
}

class Fpu {
public:
    explicit Fpu(Ps5LiftState* state) : _state(*state) {
        const auto control = _state.X87Control;
        static constexpr std::uint_fast8_t Precision[4] = {32, 80, 64, 80};
        softfloat_roundingMode = SoftFloatRounding(control);
        extF80_roundingPrecision = Precision[(control >> 8) & 3];
        softfloat_detectTininess = softfloat_tininess_afterRounding;
        softfloat_exceptionFlags = 0;
    }

    ~Fpu() {
        std::uint16_t raised = 0;
        const auto flags = softfloat_exceptionFlags;
        if (flags & softfloat_flag_invalid) raised |= StatusInvalid;
        if (flags & softfloat_flag_infinite) raised |= StatusZeroDivide;
        if (flags & softfloat_flag_overflow) raised |= StatusOverflow;
        if (flags & softfloat_flag_underflow) raised |= StatusUnderflow;
        if (flags & softfloat_flag_inexact) raised |= StatusPrecision;
        raised |= _extra;
        auto status = static_cast<std::uint16_t>(_state.X87Status | raised);
        if ((status & ~_state.X87Control & 0x3f) != 0) status |= StatusSummary | StatusBusy;
        _state.X87Status = status;
    }

    unsigned Top() const { return (_state.X87Status >> TopShift) & 7; }

    void SetTop(const unsigned top) {
        _state.X87Status = static_cast<std::uint16_t>((_state.X87Status & ~(7u << TopShift)) | ((top & 7) << TopShift));
    }

    unsigned Physical(const unsigned index) const { return (Top() + index) & 7; }

    bool Valid(const unsigned index) const { return (_state.X87Valid >> Physical(index)) & 1; }

    extFloat80_t Get(const unsigned index) {
        if (!Valid(index)) {
            StackFault(false);
            return Indefinite();
        }
        return Contents(index);
    }

    extFloat80_t Contents(const unsigned index) const {
        return ReadExtended(_state.X87Registers[Physical(index)]);
    }

    void Set(const unsigned index, const extFloat80_t value) {
        const auto physical = Physical(index);
        WriteExtended(_state.X87Registers[physical], value);
        _state.X87Valid = static_cast<std::uint8_t>(_state.X87Valid | (1u << physical));
    }

    void Push(const extFloat80_t value) {
        SetTop(Top() - 1);
        if (Valid(0)) {
            StackFault(true);
            Set(0, Indefinite());
            return;
        }
        Set(0, value);
    }

    void Pop() {
        _state.X87Valid = static_cast<std::uint8_t>(_state.X87Valid & ~(1u << Physical(0)));
        SetTop(Top() + 1);
    }

    void Free(const unsigned index) {
        _state.X87Valid = static_cast<std::uint8_t>(_state.X87Valid & ~(1u << Physical(index)));
    }

    void SetConditions(const std::uint16_t conditions) {
        _state.X87Status = static_cast<std::uint16_t>((_state.X87Status & ~StatusConditions) | conditions);
    }

    void Raise(const std::uint16_t flags) { _extra |= flags; }

    void ClearRoundedUp() {
        _state.X87Status = static_cast<std::uint16_t>(_state.X87Status & ~StatusC1);
    }

    void SetRoundedUp() {
        _state.X87Status = static_cast<std::uint16_t>(_state.X87Status | StatusC1);
    }

    std::uint16_t Conditions() const {
        return static_cast<std::uint16_t>(_state.X87Status & StatusConditions);
    }

    void Operands(const extFloat80_t left, const extFloat80_t right) {
        if (IsNan(left) || IsNan(right)) return;
        if (IsDenormal(left) || IsDenormal(right)) _extra |= StatusDenormal;
    }

private:
    Ps5LiftState& _state;
    std::uint16_t _extra = 0;

    void StackFault(const bool overflow) {
        _extra |= StatusInvalid | StatusStackFault;
        _state.X87Status = static_cast<std::uint16_t>((_state.X87Status & ~StatusC1) | (overflow ? StatusC1 : 0));
    }
};

class FullPrecision {
public:
    FullPrecision() : _saved(extF80_roundingPrecision) { extF80_roundingPrecision = 80; }
    ~FullPrecision() { extF80_roundingPrecision = _saved; }

private:
    std::uint_fast8_t _saved;
};

bool SameBits(const extFloat80_t left, const extFloat80_t right) {
    return left.signif == right.signif && left.signExp == right.signExp;
}

bool SameBits(const float32_t left, const float32_t right) {
    return left.v == right.v;
}

bool SameBits(const float64_t left, const float64_t right) {
    return left.v == right.v;
}

template <typename TCompute>
auto WithRoundUp(Fpu& fpu, TCompute compute) -> decltype(compute()) {
    const auto saved = softfloat_exceptionFlags;
    softfloat_exceptionFlags = 0;
    const auto result = compute();
    const auto raised = softfloat_exceptionFlags;
    if (raised & softfloat_flag_inexact) {
        const auto rounding = softfloat_roundingMode;
        softfloat_roundingMode = softfloat_round_minMag;
        const auto truncated = compute();
        softfloat_roundingMode = rounding;
        if (!SameBits(result, truncated)) fpu.SetRoundedUp();
    }
    softfloat_exceptionFlags = static_cast<std::uint_fast8_t>(saved | raised);
    return result;
}

extFloat80_t LoadMemory(Fpu& fpu, const std::uint32_t format, const std::uint64_t address) {
    const void* source = reinterpret_cast<const void*>(address);
    switch (format & 7) {
    case PS5LIFT_X87_FLOAT32: {
        float32_t value;
        std::memcpy(&value, source, 4);
        if ((value.v & 0x7f800000u) == 0 && (value.v & 0x7fffffu) != 0) fpu.Raise(StatusDenormal);
        return f32_to_extF80(value);
    }
    case PS5LIFT_X87_FLOAT64: {
        float64_t value;
        std::memcpy(&value, source, 8);
        if ((value.v & 0x7ff0000000000000ull) == 0 && (value.v & 0xfffffffffffffull) != 0) fpu.Raise(StatusDenormal);
        return f64_to_extF80(value);
    }
    case PS5LIFT_X87_FLOAT80:
        return ReadExtended(source);
    case PS5LIFT_X87_INTEGER16: {
        std::int16_t value;
        std::memcpy(&value, source, 2);
        return i32_to_extF80(value);
    }
    case PS5LIFT_X87_INTEGER32: {
        std::int32_t value;
        std::memcpy(&value, source, 4);
        return i32_to_extF80(value);
    }
    case PS5LIFT_X87_INTEGER64: {
        std::int64_t value;
        std::memcpy(&value, source, 8);
        return i64_to_extF80(value);
    }
    default:
        ps5lift_fail("unknown x87 memory format 0x%x", format);
    }
}

template <typename T>
void StoreInteger(Fpu& fpu, const extFloat80_t value, const bool truncate, void* destination) {
    const auto rounding = truncate ? static_cast<std::uint_fast8_t>(softfloat_round_minMag) : softfloat_roundingMode;
    T result = static_cast<T>(std::uint64_t{1} << (sizeof(T) * 8 - 1));
    if (IsNan(value)) {
        fpu.Raise(StatusInvalid);
    } else {
        const auto saved = softfloat_exceptionFlags;
        softfloat_exceptionFlags = 0;
        const auto wide = extF80_to_i64(value, rounding, true);
        const auto raised = softfloat_exceptionFlags;
        const bool fits = (raised & softfloat_flag_invalid) == 0 && wide >= INT64_C(-1) << (sizeof(T) * 8 - 1) && (sizeof(T) == 8 || wide < INT64_C(1) << (sizeof(T) * 8 - 1));
        if (fits) {
            result = static_cast<T>(wide);
            if ((raised & softfloat_flag_inexact) != 0 && !truncate && extF80_to_i64(value, softfloat_round_minMag, false) != wide) fpu.SetRoundedUp();
        }
        softfloat_exceptionFlags = static_cast<std::uint_fast8_t>(saved | (fits ? raised : softfloat_flag_invalid));
    }
    std::memcpy(destination, &result, sizeof(T));
}

void StoreMemory(Fpu& fpu, const std::uint32_t format, const std::uint64_t address, const extFloat80_t value) {
    void* destination = reinterpret_cast<void*>(address);
    const bool truncate = (format & PS5LIFT_X87_TRUNCATE) != 0;
    switch (format & 7) {
    case PS5LIFT_X87_FLOAT32: {
        const auto result = WithRoundUp(fpu, [&] { return extF80_to_f32(value); });
        std::memcpy(destination, &result, 4);
        return;
    }
    case PS5LIFT_X87_FLOAT64: {
        const auto result = WithRoundUp(fpu, [&] { return extF80_to_f64(value); });
        std::memcpy(destination, &result, 8);
        return;
    }
    case PS5LIFT_X87_FLOAT80:
        WriteExtended(destination, value);
        return;
    case PS5LIFT_X87_INTEGER16: StoreInteger<std::int16_t>(fpu, value, truncate, destination); return;
    case PS5LIFT_X87_INTEGER32: StoreInteger<std::int32_t>(fpu, value, truncate, destination); return;
    case PS5LIFT_X87_INTEGER64: StoreInteger<std::int64_t>(fpu, value, truncate, destination); return;
    default: ps5lift_fail("unknown x87 memory format 0x%x", format);
    }
}

extFloat80_t Arithmetic(const std::uint32_t operation, const extFloat80_t left, const extFloat80_t right) {
    switch (operation) {
    case PS5LIFT_X87_ADD: return extF80_add(left, right);
    case PS5LIFT_X87_SUBTRACT: return extF80_sub(left, right);
    case PS5LIFT_X87_SUBTRACT_REVERSE: return extF80_sub(right, left);
    case PS5LIFT_X87_MULTIPLY: return extF80_mul(left, right);
    case PS5LIFT_X87_DIVIDE: return extF80_div(left, right);
    case PS5LIFT_X87_DIVIDE_REVERSE: return extF80_div(right, left);
    default: ps5lift_fail("unknown x87 arithmetic operation %u", operation);
    }
}

extFloat80_t Compute(Fpu& fpu, const std::uint32_t operation, const extFloat80_t left, const extFloat80_t right) {
    fpu.Operands(left, right);
    switch (operation) {
    case PS5LIFT_X87_SQUARE_ROOT: return WithRoundUp(fpu, [&] { return extF80_sqrt(left); });
    case PS5LIFT_X87_ROUND: return WithRoundUp(fpu, [&] { return extF80_roundToInt(left, softfloat_roundingMode, true); });
    default: return WithRoundUp(fpu, [&] { return Arithmetic(operation, left, right); });
    }
}

std::uint32_t Compare(const extFloat80_t left, const extFloat80_t right, const bool unordered) {
    const bool nan = IsNan(left) || IsNan(right);
    if (nan) {
        if (!unordered || extF80_isSignalingNaN(left) || extF80_isSignalingNaN(right)) softfloat_exceptionFlags |= softfloat_flag_invalid;
        return PS5LIFT_X87_UNORDERED;
    }
    if (extF80_eq(left, right)) return PS5LIFT_X87_EQUAL;
    return extF80_lt_quiet(left, right) ? PS5LIFT_X87_LESS : PS5LIFT_X87_GREATER;
}

std::uint16_t CompareConditions(const std::uint32_t result) {
    switch (result) {
    case PS5LIFT_X87_GREATER: return 0;
    case PS5LIFT_X87_LESS: return StatusC0;
    case PS5LIFT_X87_EQUAL: return StatusC3;
    case PS5LIFT_X87_UNORDERED: return StatusC0 | StatusC2 | StatusC3;
    default: ps5lift_fail("unknown x87 comparison result %u", result);
    }
}

extFloat80_t Constant(const std::uint32_t index) {
    static constexpr std::uint64_t Constants[5][2] = {
        {0x4000a934f0979a37ull, 0x15fc9257edfe9b60ull},
        {0x3fff71547652b82full, 0xe1777d0ffda0d23aull},
        {0x4000921fb54442d1ull, 0x8469898cc51701b8ull},
        {0x3ffd34413509f79full, 0xef311f12b35816f9ull},
        {0x3ffe62e42fefa39eull, 0xf35793c7673007e6ull},
    };
    if (index == PS5LIFT_X87_CONSTANT_ONE) return i32_to_extF80(1);
    if (index == PS5LIFT_X87_CONSTANT_ZERO) return Zero(false);
    if (index > PS5LIFT_X87_CONSTANT_ZERO) ps5lift_fail("unknown x87 constant 0x%x", index);
    float128_t value;
    value.v[0] = Constants[index - PS5LIFT_X87_CONSTANT_LOG2_10][1];
    value.v[1] = Constants[index - PS5LIFT_X87_CONSTANT_LOG2_10][0];
    const auto savedPrecision = extF80_roundingPrecision;
    extF80_roundingPrecision = 80;
    const auto result = f128_to_extF80(value);
    extF80_roundingPrecision = savedPrecision;
    softfloat_exceptionFlags &= static_cast<std::uint_fast8_t>(~softfloat_flag_inexact);
    return result;
}

struct Significand {
    std::uint64_t Bits;
    int Exponent;
};

Significand Normalize(const extFloat80_t value) {
    Significand result{value.signif, (value.signExp & 0x7fff) == 0 ? 1 : value.signExp & 0x7fff};
    while (result.Bits != 0 && (result.Bits >> 63) == 0) {
        result.Bits <<= 1;
        --result.Exponent;
    }
    return result;
}

unsigned TruncatedQuotientBits(const extFloat80_t dividend, const extFloat80_t divisor) {
    const auto x = Normalize(dividend);
    const auto y = Normalize(divisor);
    if (x.Bits == 0 || y.Bits == 0) return 0;
    const int difference = x.Exponent - y.Exponent;
    if (difference < 0) return 0;
    unsigned __int128 remainder = x.Bits;
    unsigned bits = 0;
    for (int step = 0; step <= difference; ++step) {
        const bool digit = remainder >= y.Bits;
        if (digit) remainder -= y.Bits;
        bits = ((bits << 1) | (digit ? 1u : 0u)) & 7u;
        remainder <<= 1;
    }
    return bits;
}

std::uint16_t Examine(Fpu& fpu) {
    const auto value = fpu.Contents(0);
    const auto sign = IsNegative(value) ? StatusC1 : 0;
    if (!fpu.Valid(0)) return static_cast<std::uint16_t>(StatusC3 | StatusC0 | sign);
    const auto exponent = value.signExp & 0x7fff;
    const bool integerBit = (value.signif >> 63) != 0;
    if (exponent == 0x7fff) {
        if (!integerBit) return sign;
        return static_cast<std::uint16_t>(((value.signif << 1) != 0 ? StatusC0 : StatusC2 | StatusC0) | sign);
    }
    if (exponent == 0) {
        if (value.signif == 0) return static_cast<std::uint16_t>(StatusC3 | sign);
        return static_cast<std::uint16_t>(StatusC3 | StatusC2 | sign);
    }
    if (!integerBit) return sign;
    return static_cast<std::uint16_t>(StatusC2 | sign);
}

std::uint16_t TagWord(const Ps5LiftState& state) {
    std::uint16_t tags = 0;
    for (unsigned physical = 0; physical < 8; ++physical) {
        unsigned tag = 3;
        if ((state.X87Valid >> physical) & 1) {
            const auto value = ReadExtended(state.X87Registers[physical]);
            const auto exponent = value.signExp & 0x7fff;
            if (exponent == 0 && value.signif == 0) tag = 1;
            else if (exponent == 0x7fff || exponent == 0 || (value.signif >> 63) == 0) tag = 2;
            else tag = 0;
        }
        tags = static_cast<std::uint16_t>(tags | (tag << (2 * physical)));
    }
    return tags;
}

}

void Ps5Lift::RequireNoPendingX87Exceptions(const Ps5LiftState& state) {
    const auto pending = state.X87Status & ~state.X87Control & 0x3fu;
    if (pending != 0) ps5lift_fail("fwait with unmasked x87 exceptions 0x%x pending; the console raises SIGFPE", pending);
}

extern "C" std::uint32_t ps5lift_x87(Ps5LiftState* state, const std::uint32_t operation, const std::uint32_t format, const std::uint64_t address, const std::uint32_t index) {
    switch (operation) {
    case PS5LIFT_X87_INITIALIZE:
        state->X87Control = InitialControl;
        state->X87Status = 0;
        state->X87Valid = 0;
        return 0;
    case PS5LIFT_X87_CLEAR_EXCEPTIONS:
        state->X87Status = static_cast<std::uint16_t>(state->X87Status & 0x7f00);
        return 0;
    case PS5LIFT_X87_WAIT:
        Ps5Lift::RequireNoPendingX87Exceptions(*state);
        return 0;
    case PS5LIFT_X87_STORE_STATUS:
        return state->X87Status;
    case PS5LIFT_X87_STORE_CONTROL:
        std::memcpy(reinterpret_cast<void*>(address), &state->X87Control, 2);
        return 0;
    case PS5LIFT_X87_LOAD_CONTROL:
        std::memcpy(&state->X87Control, reinterpret_cast<const void*>(address), 2);
        return 0;
    case PS5LIFT_X87_STORE_ENVIRONMENT: {
        const std::uint32_t environment[7] = {0xffff0000u | state->X87Control, 0xffff0000u | state->X87Status, 0xffff0000u | TagWord(*state), 0, 0, 0, 0xffff0000u};
        std::memcpy(reinterpret_cast<void*>(address), environment, sizeof(environment));
        state->X87Control = static_cast<std::uint16_t>(state->X87Control | 0x3f);
        return 0;
    }
    case PS5LIFT_X87_SAVE_STATE: {
        if ((address & 15) != 0) ps5lift_fail("fxsave to 0x%llx, which is not 16-byte aligned; the console raises #GP", static_cast<unsigned long long>(address));
        std::uint8_t area[FxStateBytes] = {};
        std::memcpy(area, &state->X87Control, 2);
        std::memcpy(area + 2, &state->X87Status, 2);
        area[4] = state->X87Valid;
        const std::uint32_t mxcsr[2] = {ps5lift_read_mxcsr(state), PS5LIFT_MXCSR_MASK};
        std::memcpy(area + 24, mxcsr, sizeof(mxcsr));
        const unsigned top = (state->X87Status >> TopShift) & 7;
        for (unsigned index = 0; index < 8; ++index) std::memcpy(area + 32 + 16 * index, state->X87Registers[(top + index) & 7], 10);
        for (unsigned reg = 0; reg < 16; ++reg) std::memcpy(area + 160 + 16 * reg, state->Vector[reg], 16);
        std::memcpy(reinterpret_cast<void*>(address), area, sizeof(area));
        return 0;
    }
    case PS5LIFT_X87_RESTORE_STATE: {
        if ((address & 15) != 0) ps5lift_fail("fxrstor from 0x%llx, which is not 16-byte aligned; the console raises #GP", static_cast<unsigned long long>(address));
        std::uint8_t area[FxStateBytes];
        std::memcpy(area, reinterpret_cast<const void*>(address), sizeof(area));
        std::uint32_t mxcsr;
        std::memcpy(&mxcsr, area + 24, 4);
        if ((mxcsr & ~PS5LIFT_MXCSR_MASK) != 0) ps5lift_fail("fxrstor of MXCSR 0x%x, which sets reserved bits; the console raises #GP", mxcsr);
        std::memcpy(&state->X87Control, area, 2);
        std::memcpy(&state->X87Status, area + 2, 2);
        state->X87Valid = area[4];
        const unsigned top = (state->X87Status >> TopShift) & 7;
        for (unsigned index = 0; index < 8; ++index) {
            auto* reg = state->X87Registers[(top + index) & 7];
            std::memcpy(reg, area + 32 + 16 * index, 10);
            std::memset(reg + 10, 0, sizeof(state->X87Registers[0]) - 10);
        }
        for (unsigned reg = 0; reg < 16; ++reg) std::memcpy(state->Vector[reg], area + 160 + 16 * reg, 16);
        state->Mxcsr = mxcsr;
        ps5lift_apply_mxcsr(state);
        return 0;
    }
    case PS5LIFT_X87_LOAD_ENVIRONMENT: {
        std::uint32_t environment[7];
        std::memcpy(environment, reinterpret_cast<const void*>(address), sizeof(environment));
        state->X87Control = static_cast<std::uint16_t>(environment[0]);
        state->X87Status = static_cast<std::uint16_t>(environment[1]);
        std::uint8_t valid = 0;
        for (unsigned physical = 0; physical < 8; ++physical) {
            if (((environment[2] >> (2 * physical)) & 3) != 3) valid = static_cast<std::uint8_t>(valid | (1u << physical));
        }
        state->X87Valid = valid;
        return 0;
    }
    default:
        break;
    }

    Fpu fpu(state);
    const bool memory = (format & 7) != PS5LIFT_X87_REGISTER;
    switch (operation) {
    case PS5LIFT_X87_LOAD: {
        fpu.ClearRoundedUp();
        const auto value = memory ? LoadMemory(fpu, format, address) : fpu.Get(index);
        fpu.Push(value);
        return 0;
    }
    case PS5LIFT_X87_LOAD_CONSTANT:
        fpu.ClearRoundedUp();
        fpu.Push(Constant(index));
        return 0;
    case PS5LIFT_X87_STORE: {
        fpu.ClearRoundedUp();
        const auto value = fpu.Get(0);
        if (memory) StoreMemory(fpu, format, address, value);
        else fpu.Set(index, value);
        if (format & PS5LIFT_X87_POP) fpu.Pop();
        return 0;
    }
    case PS5LIFT_X87_ADD:
    case PS5LIFT_X87_SUBTRACT:
    case PS5LIFT_X87_SUBTRACT_REVERSE:
    case PS5LIFT_X87_MULTIPLY:
    case PS5LIFT_X87_DIVIDE:
    case PS5LIFT_X87_DIVIDE_REVERSE: {
        fpu.ClearRoundedUp();
        const auto destination = memory ? 0u : (format & PS5LIFT_X87_TO_REGISTER) ? index : 0u;
        const auto left = fpu.Get(destination);
        const auto right = memory ? LoadMemory(fpu, format, address) : fpu.Get(destination == 0 ? index : 0);
        fpu.Set(destination, Compute(fpu, operation, left, right));
        if (!memory && (format & PS5LIFT_X87_TO_REGISTER) && (format & PS5LIFT_X87_POP)) fpu.Pop();
        return 0;
    }
    case PS5LIFT_X87_COMPARE:
    case PS5LIFT_X87_COMPARE_UNORDERED: {
        const auto right = memory ? LoadMemory(fpu, format, address) : fpu.Get(index);
        const auto left = fpu.Get(0);
        fpu.Operands(left, right);
        fpu.SetConditions(CompareConditions(Compare(left, right, operation == PS5LIFT_X87_COMPARE_UNORDERED)));
        if (format & (PS5LIFT_X87_POP | PS5LIFT_X87_POP_TWICE)) fpu.Pop();
        if (format & PS5LIFT_X87_POP_TWICE) fpu.Pop();
        return 0;
    }
    case PS5LIFT_X87_COMPARE_FLAGS:
    case PS5LIFT_X87_COMPARE_FLAGS_UNORDERED: {
        fpu.ClearRoundedUp();
        const auto left = fpu.Get(0);
        const auto right = fpu.Get(index);
        fpu.Operands(left, right);
        const auto result = Compare(left, right, operation == PS5LIFT_X87_COMPARE_FLAGS_UNORDERED);
        if (format & PS5LIFT_X87_POP) fpu.Pop();
        return result;
    }
    case PS5LIFT_X87_TEST: {
        const auto value = fpu.Get(0);
        fpu.Operands(value, Zero(false));
        fpu.SetConditions(CompareConditions(Compare(value, Zero(false), false)));
        return 0;
    }
    case PS5LIFT_X87_EXAMINE:
        fpu.SetConditions(Examine(fpu));
        return 0;
    case PS5LIFT_X87_CHANGE_SIGN: {
        fpu.ClearRoundedUp();
        auto value = fpu.Get(0);
        value.signExp = static_cast<std::uint16_t>(value.signExp ^ 0x8000);
        fpu.Set(0, value);
        return 0;
    }
    case PS5LIFT_X87_ABSOLUTE: {
        fpu.ClearRoundedUp();
        auto value = fpu.Get(0);
        value.signExp = static_cast<std::uint16_t>(value.signExp & 0x7fff);
        fpu.Set(0, value);
        return 0;
    }
    case PS5LIFT_X87_SQUARE_ROOT:
    case PS5LIFT_X87_ROUND: {
        fpu.ClearRoundedUp();
        const auto value = fpu.Get(0);
        fpu.Set(0, Compute(fpu, operation, value, value));
        return 0;
    }
    case PS5LIFT_X87_SCALE: {
        const FullPrecision precision;
        fpu.ClearRoundedUp();
        const auto value = fpu.Get(0);
        const auto scale = fpu.Get(1);
        fpu.Operands(value, scale);
        if (IsNan(value) || IsNan(scale)) {
            fpu.Set(0, extF80_add(value, scale));
            return 0;
        }
        if (IsInfinity(scale)) {
            const bool up = !IsNegative(scale);
            if (up ? IsZero(value) : IsInfinity(value)) {
                fpu.Raise(StatusInvalid);
                fpu.Set(0, Indefinite());
            } else {
                fpu.Set(0, up ? Infinity(IsNegative(value)) : Zero(IsNegative(value)));
            }
            return 0;
        }
        constexpr std::int64_t Saturated = 0x10000;
        std::int64_t power = IsNegative(scale) ? -Saturated : Saturated;
        if ((scale.signExp & 0x7fff) < 0x3fff + 16) {
            const auto saved = softfloat_exceptionFlags;
            power = extF80_to_i64(scale, softfloat_round_minMag, false);
            softfloat_exceptionFlags = saved;
        }
        const auto scaled = [&] {
            auto result = value;
            auto remaining = power;
            const auto step = remaining > 0 ? 16000 : -16000;
            while (remaining != 0) {
                const auto chunk = (remaining > 0 ? remaining > step : remaining < step) ? step : remaining;
                extFloat80_t factor;
                factor.signExp = static_cast<std::uint16_t>(0x3fff + chunk);
                factor.signif = 0x8000000000000000ull;
                result = extF80_mul(result, factor);
                remaining -= chunk;
            }
            return result;
        };
        fpu.Set(0, WithRoundUp(fpu, scaled));
        return 0;
    }
    case PS5LIFT_X87_PARTIAL_REMAINDER:
    case PS5LIFT_X87_IEEE_REMAINDER: {
        const FullPrecision precision;
        const auto dividend = fpu.Get(0);
        const auto divisor = fpu.Get(1);
        fpu.Operands(dividend, divisor);
        const auto nearest = extF80_rem(dividend, divisor);
        auto truncated = nearest;
        if (!IsNan(nearest) && (nearest.signif != 0 || (nearest.signExp & 0x7fff) != 0) && IsNegative(nearest) != IsNegative(dividend)) {
            auto magnitude = divisor;
            magnitude.signExp = static_cast<std::uint16_t>((magnitude.signExp & 0x7fff) | (dividend.signExp & 0x8000));
            const auto savedFlags = softfloat_exceptionFlags;
            truncated = extF80_add(nearest, magnitude);
            softfloat_exceptionFlags = savedFlags;
        }
        const auto remainder = operation == PS5LIFT_X87_PARTIAL_REMAINDER ? truncated : nearest;
        std::uint16_t conditions = IsNan(remainder) ? static_cast<std::uint16_t>(fpu.Conditions() & ~StatusC2) : 0;
        if (!IsNan(remainder) && (divisor.signExp & 0x7fff) != 0x7fff) {
            auto bits = TruncatedQuotientBits(dividend, divisor);
            const bool roundedUp = truncated.signif != nearest.signif || truncated.signExp != nearest.signExp;
            if (operation == PS5LIFT_X87_IEEE_REMAINDER && roundedUp) bits = (bits + 1) & 7;
            if (bits & 1) conditions |= StatusC1;
            if (bits & 2) conditions |= StatusC3;
            if (bits & 4) conditions |= StatusC0;
        }
        fpu.SetConditions(conditions);
        fpu.Set(0, remainder);
        return 0;
    }
    case PS5LIFT_X87_EXCHANGE: {
        fpu.ClearRoundedUp();
        const auto top = fpu.Get(0);
        const auto other = fpu.Get(index);
        fpu.Set(0, other);
        fpu.Set(index, top);
        return 0;
    }
    case PS5LIFT_X87_FREE:
        fpu.Free(index);
        return 0;
    case PS5LIFT_X87_INCREMENT_TOP:
        fpu.ClearRoundedUp();
        fpu.SetTop(fpu.Top() + 1);
        return 0;
    case PS5LIFT_X87_DECREMENT_TOP:
        fpu.ClearRoundedUp();
        fpu.SetTop(fpu.Top() - 1);
        return 0;
    case PS5LIFT_X87_MOVE_REGISTER:
        fpu.Set(0, fpu.Get(index));
        return 0;
    default:
        ps5lift_fail("unknown x87 operation 0x%x", operation);
    }
}

extern "C" void ps5lift_extended_operation(Ps5LiftState* state, const std::uint32_t operation, const void* left, const void* right, void* result) {
    Fpu fpu(state);
    fpu.ClearRoundedUp();
    const auto first = LoadMemory(fpu, PS5LIFT_X87_FLOAT80, reinterpret_cast<std::uint64_t>(left));
    const auto second = LoadMemory(fpu, PS5LIFT_X87_FLOAT80, reinterpret_cast<std::uint64_t>(right));
    StoreMemory(fpu, PS5LIFT_X87_FLOAT80, reinterpret_cast<std::uint64_t>(result), Compute(fpu, operation, first, second));
}

extern "C" void ps5lift_extended_load(Ps5LiftState* state, const std::uint32_t format, const void* source, void* result) {
    Fpu fpu(state);
    fpu.ClearRoundedUp();
    StoreMemory(fpu, PS5LIFT_X87_FLOAT80, reinterpret_cast<std::uint64_t>(result), LoadMemory(fpu, format, reinterpret_cast<std::uint64_t>(source)));
}

extern "C" void ps5lift_extended_store(Ps5LiftState* state, const std::uint32_t format, const void* value, void* destination) {
    Fpu fpu(state);
    fpu.ClearRoundedUp();
    StoreMemory(fpu, format, reinterpret_cast<std::uint64_t>(destination), LoadMemory(fpu, PS5LIFT_X87_FLOAT80, reinterpret_cast<std::uint64_t>(value)));
}

#if LDBL_MANT_DIG == 113
static_assert(sizeof(long double) == sizeof(float128_t));

namespace {

float128_t HostDefaultNan() {
    std::fenv_t environment;
    std::feholdexcept(&environment);
    volatile long double zero = 0;
    const long double nan = zero / zero;
    std::fesetenv(&environment);
    float128_t bits;
    std::memcpy(&bits, &nan, sizeof(bits));
    return bits;
}

}
#endif

#ifdef PS5LIFT_HOST_LONG_DOUBLE_HOLDS_EXTENDED
extern "C" void ps5lift_extended_to_host(const void* extended, long double* host) {
    std::memset(host, 0, sizeof(*host));
#if LDBL_MANT_DIG == 64
    std::memcpy(host, extended, 10);
#else
    const auto value = ReadExtended(extended);
    float128_t converted;
    if (IsNan(value)) {
        converted.v[1] = static_cast<std::uint64_t>(value.signExp) << 48 | (value.signif & 0x7fffffffffffffffull) >> 15;
        converted.v[0] = value.signif << 49;
    } else {
        const auto flags = softfloat_exceptionFlags;
        converted = extF80_to_f128(value);
        softfloat_exceptionFlags = flags;
    }
    std::memcpy(host, &converted, sizeof(converted));
#endif
}

extern "C" void ps5lift_extended_from_host(const Ps5LiftState* state, const long double* host, void* extended) {
#if LDBL_MANT_DIG == 64
    (void)state;
    std::memcpy(extended, host, 10);
#else
    static const float128_t DefaultNan = HostDefaultNan();
    float128_t value;
    std::memcpy(&value, host, sizeof(value));
    if (value.v[0] == DefaultNan.v[0] && value.v[1] == DefaultNan.v[1]) {
        WriteExtended(extended, Indefinite());
        return;
    }
    const auto flags = softfloat_exceptionFlags;
    const auto rounding = softfloat_roundingMode;
    const auto tininess = softfloat_detectTininess;
    softfloat_roundingMode = SoftFloatRounding(state->X87Control);
    softfloat_detectTininess = softfloat_tininess_afterRounding;
    const extFloat80_t converted = f128_to_extF80(value);
    softfloat_exceptionFlags = flags;
    softfloat_roundingMode = rounding;
    softfloat_detectTininess = tininess;
    WriteExtended(extended, converted);
#endif
}
#endif
