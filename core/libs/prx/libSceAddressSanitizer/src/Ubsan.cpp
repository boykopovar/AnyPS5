/* Derived from LLVM compiler-rt lib/ubsan (ubsan_handlers.cpp, ubsan_value.cpp, ubsan_diag.cpp) at llvmorg-16.0.6 (af2ed9f794c422923414d162dd1f48d986e2d6e3) */
/* Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdarg>
#include <cstddef>
#include <exception>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

using UIntMax = unsigned __int128;
using SIntMax = __int128;

constexpr std::uint32_t DisabledColumn = ~std::uint32_t(0);
constexpr std::size_t TypeNameMaximum = 4096;
constexpr std::size_t FileNameMaximum = 4096;

template <class T>
T ReadGuest(std::uintptr_t address) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value)))
        throw std::invalid_argument("ubsan: handler data is not readable");
    return value;
}

struct SourceLocation {
    std::uintptr_t filename = 0;
    std::uint32_t line = 0;
    std::uint32_t column = 0;

    bool IsInvalid() const { return filename == 0; }
    bool IsDisabled() const { return column == DisabledColumn; }
};

static_assert(sizeof(SourceLocation) == 16);

SourceLocation ReadLocation(std::uintptr_t address) {
    return ReadGuest<SourceLocation>(address);
}

SourceLocation AcquireLocation(std::uintptr_t address) {
    SourceLocation location = ReadLocation(address);
    auto* column = reinterpret_cast<std::uint32_t*>(address + offsetof(SourceLocation, column));
    if (!GuestWritable_nid_no_patch(column, sizeof(*column)))
        throw std::invalid_argument("ubsan: source location is not writable");
    location.column = __atomic_exchange_n(column, DisabledColumn, __ATOMIC_RELAXED);
    return location;
}

struct TypeDescriptor {
    std::uint16_t kind = 0;
    std::uint16_t info = 0;
    std::string name;

    bool IsInteger() const { return kind == 0; }
    bool IsSignedInteger() const { return IsInteger() && (info & 1) != 0; }
    bool IsUnsignedInteger() const { return IsInteger() && (info & 1) == 0; }
    bool IsFloat() const { return kind == 1; }
    unsigned IntegerBitWidth() const {
        if (!IsInteger()) throw std::invalid_argument("ubsan: integer width of a non-integer type");
        return 1u << (info >> 1);
    }
    unsigned FloatBitWidth() const {
        if (!IsFloat()) throw std::invalid_argument("ubsan: float width of a non-float type");
        return info;
    }
};

TypeDescriptor ReadType(std::uintptr_t address) {
    TypeDescriptor type;
    type.kind = ReadGuest<std::uint16_t>(address);
    type.info = ReadGuest<std::uint16_t>(address + 2);
    if (CopyGuestString_nid_no_patch(reinterpret_cast<const char*>(address + 4), TypeNameMaximum, type.name) != 0)
        throw std::invalid_argument("ubsan: type name is not readable");
    return type;
}

TypeDescriptor ReadTypeAt(std::uintptr_t reference) {
    return ReadType(ReadGuest<std::uintptr_t>(reference));
}

double DecodeX87(const unsigned char* bytes) {
    std::uint64_t mantissa = 0;
    std::memcpy(&mantissa, bytes, sizeof(mantissa));
    std::uint16_t top = 0;
    std::memcpy(&top, bytes + 8, sizeof(top));
    const bool negative = (top & 0x8000u) != 0;
    const int exponent = top & 0x7fff;
    double value = 0;
    if (exponent == 0x7fff) {
        value = (mantissa << 1) == 0 ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
    } else if (exponent != 0 || mantissa != 0) {
        const int unbiased = (exponent == 0 ? 1 : exponent) - 16383 - 63;
        value = std::ldexp(static_cast<double>(mantissa), unbiased);
    }
    return negative ? -value : value;
}

struct Value {
    TypeDescriptor type;
    std::uintptr_t handle = 0;

    bool IsInlineInt() const { return type.IntegerBitWidth() <= 64; }

    SIntMax SignedValue() const {
        if (!type.IsSignedInteger()) throw std::invalid_argument("ubsan: signed value of an unsigned type");
        const unsigned bits = type.IntegerBitWidth();
        if (IsInlineInt()) {
            const unsigned extra = 128 - bits;
            return static_cast<SIntMax>(static_cast<UIntMax>(handle) << extra) >> extra;
        }
        if (bits == 64) return ReadGuest<std::int64_t>(handle);
        if (bits == 128) return ReadGuest<SIntMax>(handle);
        throw std::invalid_argument("ubsan: unexpected integer width");
    }

    UIntMax UnsignedValue() const {
        if (!type.IsUnsignedInteger()) throw std::invalid_argument("ubsan: unsigned value of a signed type");
        const unsigned bits = type.IntegerBitWidth();
        if (IsInlineInt()) return handle;
        if (bits == 64) return ReadGuest<std::uint64_t>(handle);
        if (bits == 128) return ReadGuest<UIntMax>(handle);
        throw std::invalid_argument("ubsan: unexpected integer width");
    }

    UIntMax PositiveValue() const {
        if (type.IsUnsignedInteger()) return UnsignedValue();
        const SIntMax value = SignedValue();
        if (value < 0) throw std::invalid_argument("ubsan: negative value where a positive one was checked");
        return static_cast<UIntMax>(value);
    }

    bool IsMinusOne() const { return type.IsSignedInteger() && SignedValue() == -1; }
    bool IsNegative() const { return type.IsSignedInteger() && SignedValue() < 0; }

    double FloatValue() const {
        const unsigned bits = type.FloatBitWidth();
        if (bits <= 64) {
            if (bits == 32) return std::bit_cast<float>(static_cast<std::uint32_t>(handle));
            if (bits == 64) return std::bit_cast<double>(static_cast<std::uint64_t>(handle));
        } else if (bits == 80 || bits == 96 || bits == 128) {
            unsigned char bytes[10];
            if (!GuestCopyIn_nid_no_patch(bytes, reinterpret_cast<const void*>(handle), sizeof(bytes)))
                throw std::invalid_argument("ubsan: float value is not readable");
            return DecodeX87(bytes);
        }
        throw std::invalid_argument("ubsan: unexpected floating point width");
    }
};

std::string Format(const char* format, ...) {
    char buffer[64];
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    return length < 0 ? std::string() : std::string(buffer, std::min<std::size_t>(static_cast<std::size_t>(length), sizeof(buffer) - 1));
}

std::string RenderHex(UIntMax value) {
    return Format("0x%08x%08x%08x%08x", static_cast<unsigned>(value >> 96), static_cast<unsigned>(value >> 64),
                  static_cast<unsigned>(value >> 32), static_cast<unsigned>(value));
}

std::string RenderSigned(SIntMax value) {
    if (value >= std::numeric_limits<std::int64_t>::min() && value <= std::numeric_limits<std::int64_t>::max())
        return Format("%lld", static_cast<long long>(value));
    return RenderHex(static_cast<UIntMax>(value));
}

std::string RenderUnsigned(UIntMax value) {
    if (value <= std::numeric_limits<std::uint64_t>::max())
        return Format("%llu", static_cast<unsigned long long>(value));
    return RenderHex(value);
}

std::string RenderPointer(std::uintptr_t value) {
    return Format("0x%012llx", static_cast<unsigned long long>(value));
}

std::string RenderType(const TypeDescriptor& type) {
    return "'" + type.name + "'";
}

std::string RenderValue(const Value& value) {
    if (value.type.IsSignedInteger()) return RenderSigned(value.SignedValue());
    if (value.type.IsUnsignedInteger()) return RenderUnsigned(value.UnsignedValue());
    if (value.type.IsFloat()) return Format("%g", value.FloatValue());
    return "<unknown>";
}

struct Location {
    bool source = false;
    SourceLocation at;
    std::uintptr_t pc = 0;
};

Location AtSource(const SourceLocation& at) { return Location{true, at, 0}; }
Location AtCaller(std::uintptr_t pc) { return Location{false, {}, pc}; }

std::string RenderLocation(const Location& location) {
    if (!location.source) return RenderPointer(location.pc);
    if (location.at.IsInvalid()) return "<unknown>";
    std::string file;
    if (CopyGuestString_nid_no_patch(reinterpret_cast<const char*>(location.at.filename), FileNameMaximum, file) != 0)
        throw std::invalid_argument("ubsan: file name is not readable");
    std::string rendered = file;
    if (location.at.line != 0) {
        rendered += Format(":%u", location.at.line);
        if (location.at.column != 0) rendered += Format(":%u", location.at.column);
    }
    return rendered;
}

class Report {
public:
    explicit Report(const Location& summary) : exceptions(std::uncaught_exceptions()) {
        tail = "SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior";
        if (summary.source && !summary.at.IsInvalid()) tail += " " + RenderLocation(summary) + " in ";
        tail += "\n";
    }

    void Error(const Location& location, const std::string& message) {
        text += RenderLocation(location) + ": runtime error: " + message + "\n";
    }

    void Note(const Location& location, const std::string& message) {
        text += RenderLocation(location) + ": note: " + message + "\n";
    }

    void Note(std::uintptr_t address, const std::string& message) {
        text += RenderPointer(address) + ": note: " + message + "\n";
    }

    ~Report() {
        if (std::uncaught_exceptions() > exceptions) return;
        text += tail;
        static std::mutex mutex;
        std::lock_guard lock(mutex);
        std::fwrite(text.data(), 1, text.size(), stderr);
        std::fflush(stderr);
    }

private:
    int exceptions;
    std::string text;
    std::string tail;
};

[[noreturn]] void Die() {
    LibcExitWithoutHandlers_nid_no_patch(1);
}

bool Ignore(const SourceLocation& location, bool unrecoverable) {
    return !unrecoverable && location.IsDisabled();
}

std::uintptr_t Field(const void* data, std::size_t offset) {
    if (data == nullptr) throw std::invalid_argument("ubsan: null handler data");
    return reinterpret_cast<std::uintptr_t>(data) + offset;
}

const char* const TypeCheckKinds[] = {
    "load of", "store to", "reference binding to", "member access within",
    "member call on", "constructor call on", "downcast of", "downcast of",
    "upcast of", "cast to virtual base of", "_Nonnull binding to",
    "dynamic operation on"};

void TypeMismatch(const void* data, std::uintptr_t pointer, bool unrecoverable, std::uintptr_t pc) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor type = ReadTypeAt(Field(data, 16));
    const auto logAlignment = ReadGuest<std::uint8_t>(Field(data, 24));
    const auto kind = ReadGuest<std::uint8_t>(Field(data, 25));
    if (kind >= std::size(TypeCheckKinds)) throw std::invalid_argument("ubsan: unknown type check kind");
    if (logAlignment >= 64) throw std::invalid_argument("ubsan: alignment out of range");
    const std::uintptr_t alignment = std::uintptr_t(1) << logAlignment;
    if (Ignore(at, unrecoverable)) return;
    const Location location = at.IsInvalid() ? AtCaller(pc) : AtSource(at);
    Report report(location);
    if (pointer == 0)
        report.Error(location, std::string(TypeCheckKinds[kind]) + " null pointer of type " + RenderType(type));
    else if ((pointer & (alignment - 1)) != 0)
        report.Error(location, std::string(TypeCheckKinds[kind]) + " misaligned address " + RenderPointer(pointer) + " for type " +
                                   RenderType(type) + ", which requires " + RenderUnsigned(alignment) + " byte alignment");
    else
        report.Error(location, std::string(TypeCheckKinds[kind]) + " address " + RenderPointer(pointer) +
                                   " with insufficient space for an object of type " + RenderType(type));
    if (pointer != 0) report.Note(pointer, "pointer points here");
}

void AlignmentAssumption(const void* data, std::uintptr_t pointer, std::uintptr_t alignment, std::uintptr_t offset, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const SourceLocation assumption = AcquireLocation(Field(data, 16));
    const TypeDescriptor type = ReadTypeAt(Field(data, 32));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    const std::uintptr_t real = pointer - offset;
    const std::uintptr_t actual = real == 0 ? 0 : std::uintptr_t(1) << std::countr_zero(real);
    const std::uintptr_t misalignment = real & (alignment - 1);
    if (offset == 0)
        report.Error(location, "assumption of " + RenderUnsigned(alignment) + " byte alignment for pointer of type " + RenderType(type) + " failed");
    else
        report.Error(location, "assumption of " + RenderUnsigned(alignment) + " byte alignment (with offset of " + RenderUnsigned(offset) +
                                   " byte) for pointer of type " + RenderType(type) + " failed");
    if (!assumption.IsInvalid()) report.Note(AtSource(assumption), "alignment assumption was specified here");
    report.Note(real, std::string(offset != 0 ? "offset " : "") + "address is " + RenderUnsigned(actual) + " aligned, misalignment offset is " +
                          RenderUnsigned(misalignment) + " bytes");
}

void IntegerOverflow(const void* data, std::uintptr_t lhs, const char* op, std::uintptr_t rhs, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor type = ReadTypeAt(Field(data, 16));
    const bool isSigned = type.IsSignedInteger();
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    report.Error(location, std::string(isSigned ? "signed" : "unsigned") + " integer overflow: " + RenderValue({type, lhs}) + " " + op + " " +
                               RenderValue({type, rhs}) + " cannot be represented in type " + RenderType(type));
}

void NegateOverflow(const void* data, std::uintptr_t old, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor type = ReadTypeAt(Field(data, 16));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    std::string message = "negation of " + RenderValue({type, old}) + " cannot be represented in type " + RenderType(type);
    if (type.IsSignedInteger()) message += "; cast to an unsigned type to negate this value to itself";
    report.Error(location, message);
}

void DivremOverflow(const void* data, std::uintptr_t lhs, std::uintptr_t rhs, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor type = ReadTypeAt(Field(data, 16));
    const Value left{type, lhs};
    const Value right{type, rhs};
    const bool minusOne = right.IsMinusOne();
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    if (minusOne)
        report.Error(location, "division of " + RenderValue(left) + " by -1 cannot be represented in type " + RenderType(type));
    else
        report.Error(location, "division by zero");
}

void ShiftOutOfBounds(const void* data, std::uintptr_t lhs, std::uintptr_t rhs, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor lhsType = ReadTypeAt(Field(data, 16));
    const TypeDescriptor rhsType = ReadTypeAt(Field(data, 24));
    const Value left{lhsType, lhs};
    const Value right{rhsType, rhs};
    const bool badExponent = right.IsNegative() || right.PositiveValue() >= lhsType.IntegerBitWidth();
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    if (badExponent) {
        if (right.IsNegative())
            report.Error(location, "shift exponent " + RenderValue(right) + " is negative");
        else
            report.Error(location, "shift exponent " + RenderValue(right) + " is too large for " + RenderUnsigned(lhsType.IntegerBitWidth()) +
                                       "-bit type " + RenderType(lhsType));
    } else if (left.IsNegative()) {
        report.Error(location, "left shift of negative value " + RenderValue(left));
    } else {
        report.Error(location, "left shift of " + RenderValue(left) + " by " + RenderValue(right) + " places cannot be represented in type " +
                                   RenderType(lhsType));
    }
}

void OutOfBounds(const void* data, std::uintptr_t index, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor arrayType = ReadTypeAt(Field(data, 16));
    const TypeDescriptor indexType = ReadTypeAt(Field(data, 24));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    report.Error(location, "index " + RenderValue({indexType, index}) + " out of bounds for type " + RenderType(arrayType));
}

void Unreachable(const void* data, const char* message) {
    const Location location = AtSource(ReadLocation(Field(data, 0)));
    Report report(location);
    report.Error(location, message);
}

void VlaBoundNotPositive(const void* data, std::uintptr_t bound, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor type = ReadTypeAt(Field(data, 16));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    report.Error(location, "variable length array bound evaluates to non-positive value " + RenderValue({type, bound}));
}

bool LooksLikeFloatCastOverflowDataV1(const void* data) {
    const auto first = ReadGuest<std::uintptr_t>(Field(data, 0));
    const auto bytes = ReadGuest<std::array<std::uint8_t, 2>>(first);
    const unsigned kind = bytes[0] + bytes[1];
    return kind < 2 || bytes[0] == 0xff || bytes[1] == 0xff;
}

void FloatCastOverflow(const void* data, std::uintptr_t from, bool unrecoverable, std::uintptr_t pc) {
    Location location;
    TypeDescriptor fromType;
    TypeDescriptor toType;
    if (LooksLikeFloatCastOverflowDataV1(data)) {
        location = AtCaller(pc);
        fromType = ReadTypeAt(Field(data, 0));
        toType = ReadTypeAt(Field(data, 8));
    } else {
        const SourceLocation at = AcquireLocation(Field(data, 0));
        fromType = ReadTypeAt(Field(data, 16));
        toType = ReadTypeAt(Field(data, 24));
        if (Ignore(at, unrecoverable)) return;
        location = AtSource(at);
    }
    Report report(location);
    report.Error(location, RenderValue({fromType, from}) + " is outside the range of representable values of type " + RenderType(toType));
}

void LoadInvalidValue(const void* data, std::uintptr_t value, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor type = ReadTypeAt(Field(data, 16));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    report.Error(location, "load of value " + RenderValue({type, value}) + ", which is not a valid value for type " + RenderType(type));
}

void ImplicitConversion(const void* data, std::uintptr_t source, std::uintptr_t destination, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const TypeDescriptor fromType = ReadTypeAt(Field(data, 16));
    const TypeDescriptor toType = ReadTypeAt(Field(data, 24));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    report.Error(location, "implicit conversion from type " + RenderType(fromType) + " of value " + RenderValue({fromType, source}) + " (" +
                               RenderUnsigned(fromType.IntegerBitWidth()) + "-bit, " + (fromType.IsSignedInteger() ? "" : "un") + "signed) to type " +
                               RenderType(toType) + " changed the value to " + RenderValue({toType, destination}) + " (" +
                               RenderUnsigned(toType.IntegerBitWidth()) + "-bit, " + (toType.IsSignedInteger() ? "" : "un") + "signed)");
}

void InvalidBuiltin(const void* data) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const auto kind = ReadGuest<std::uint8_t>(Field(data, 16));
    const Location location = AtSource(at);
    Report report(location);
    report.Error(location, std::string("passing zero to ") + (kind == 0 ? "ctz()" : "clz()") + ", which is not a valid argument");
}

void NonNullReturn(const void* data, const void* site, bool unrecoverable, bool attribute) {
    if (site == nullptr) throw std::invalid_argument("ubsan: source location pointer is null");
    const SourceLocation at = AcquireLocation(reinterpret_cast<std::uintptr_t>(site));
    const SourceLocation attributeAt = ReadLocation(Field(data, 0));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    report.Error(location, "null pointer returned from function declared to never return null");
    if (!attributeAt.IsInvalid())
        report.Note(AtSource(attributeAt), std::string(attribute ? "returns_nonnull attribute" : "_Nonnull return type annotation") + " specified here");
}

void NonNullArg(const void* data, bool unrecoverable, bool attribute) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    const SourceLocation attributeAt = ReadLocation(Field(data, 16));
    const auto index = ReadGuest<std::int32_t>(Field(data, 32));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    report.Error(location, "null pointer passed as argument " + RenderSigned(index) + ", which is declared to never be null");
    if (!attributeAt.IsInvalid())
        report.Note(AtSource(attributeAt), std::string(attribute ? "nonnull attribute" : "_Nonnull type annotation") + " specified here");
}

void PointerOverflow(const void* data, std::uintptr_t base, std::uintptr_t result, bool unrecoverable) {
    const SourceLocation at = AcquireLocation(Field(data, 0));
    if (Ignore(at, unrecoverable)) return;
    const Location location = AtSource(at);
    Report report(location);
    if (base == 0 && result == 0)
        report.Error(location, "applying zero offset to null pointer");
    else if (base == 0)
        report.Error(location, "applying non-zero offset " + RenderUnsigned(result) + " to null pointer");
    else if (result == 0)
        report.Error(location, "applying non-zero offset to non-null pointer " + RenderPointer(base) + " produced null pointer");
    else if ((static_cast<std::intptr_t>(base) >= 0) == (static_cast<std::intptr_t>(result) >= 0))
        report.Error(location, std::string(base > result ? "addition of unsigned offset to " : "subtraction of unsigned offset from ") +
                                   RenderPointer(base) + " overflowed to " + RenderPointer(result));
    else
        report.Error(location, "pointer index expression with base " + RenderPointer(base) + " overflowed to " + RenderPointer(result));
}

template <class T>
T UnalignedLoad(const void* pointer) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value)))
        throw std::invalid_argument("ubsan: unaligned load from unreadable memory");
    return value;
}

template <class T>
void UnalignedStore(void* pointer, T value) {
    if (!GuestCopyOut_nid_no_patch(pointer, &value, sizeof(value)))
        throw std::invalid_argument("ubsan: unaligned store to unwritable memory");
}

}

#define APS5_CALLER_PC reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))

extern "C" {

void APS5_VABI __ubsan_handle_type_mismatch_v1(const void* data, std::uintptr_t pointer) {
    TypeMismatch(data, pointer, false, APS5_CALLER_PC);
}

void APS5_VABI __ubsan_handle_type_mismatch_v1_abort(const void* data, std::uintptr_t pointer) {
    TypeMismatch(data, pointer, true, APS5_CALLER_PC);
    Die();
}

void APS5_VABI __ubsan_handle_alignment_assumption(const void* data, std::uintptr_t pointer, std::uintptr_t alignment, std::uintptr_t offset) {
    AlignmentAssumption(data, pointer, alignment, offset, false);
}

void APS5_VABI __ubsan_handle_alignment_assumption_abort(const void* data, std::uintptr_t pointer, std::uintptr_t alignment, std::uintptr_t offset) {
    AlignmentAssumption(data, pointer, alignment, offset, true);
    Die();
}

void APS5_VABI __ubsan_handle_add_overflow(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    IntegerOverflow(data, lhs, "+", rhs, false);
}

void APS5_VABI __ubsan_handle_add_overflow_abort(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    IntegerOverflow(data, lhs, "+", rhs, true);
    Die();
}

void APS5_VABI __ubsan_handle_sub_overflow(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    IntegerOverflow(data, lhs, "-", rhs, false);
}

void APS5_VABI __ubsan_handle_sub_overflow_abort(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    IntegerOverflow(data, lhs, "-", rhs, true);
    Die();
}

void APS5_VABI __ubsan_handle_mul_overflow(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    IntegerOverflow(data, lhs, "*", rhs, false);
}

void APS5_VABI __ubsan_handle_mul_overflow_abort(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    IntegerOverflow(data, lhs, "*", rhs, true);
    Die();
}

void APS5_VABI __ubsan_handle_negate_overflow(const void* data, std::uintptr_t old) {
    NegateOverflow(data, old, false);
}

void APS5_VABI __ubsan_handle_negate_overflow_abort(const void* data, std::uintptr_t old) {
    NegateOverflow(data, old, true);
    Die();
}

void APS5_VABI __ubsan_handle_divrem_overflow(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    DivremOverflow(data, lhs, rhs, false);
}

void APS5_VABI __ubsan_handle_divrem_overflow_abort(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    DivremOverflow(data, lhs, rhs, true);
    Die();
}

void APS5_VABI __ubsan_handle_shift_out_of_bounds(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    ShiftOutOfBounds(data, lhs, rhs, false);
}

void APS5_VABI __ubsan_handle_shift_out_of_bounds_abort(const void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    ShiftOutOfBounds(data, lhs, rhs, true);
    Die();
}

void APS5_VABI __ubsan_handle_out_of_bounds(const void* data, std::uintptr_t index) {
    OutOfBounds(data, index, false);
}

void APS5_VABI __ubsan_handle_out_of_bounds_abort(const void* data, std::uintptr_t index) {
    OutOfBounds(data, index, true);
    Die();
}

void APS5_VABI __ubsan_handle_builtin_unreachable(const void* data) {
    Unreachable(data, "execution reached an unreachable program point");
    Die();
}

void APS5_VABI __ubsan_handle_missing_return(const void* data) {
    Unreachable(data, "execution reached the end of a value-returning function without returning a value");
    Die();
}

void APS5_VABI __ubsan_handle_vla_bound_not_positive(const void* data, std::uintptr_t bound) {
    VlaBoundNotPositive(data, bound, false);
}

void APS5_VABI __ubsan_handle_vla_bound_not_positive_abort(const void* data, std::uintptr_t bound) {
    VlaBoundNotPositive(data, bound, true);
    Die();
}

void APS5_VABI __ubsan_handle_float_cast_overflow(const void* data, std::uintptr_t from) {
    FloatCastOverflow(data, from, false, APS5_CALLER_PC);
}

void APS5_VABI __ubsan_handle_float_cast_overflow_abort(const void* data, std::uintptr_t from) {
    FloatCastOverflow(data, from, true, APS5_CALLER_PC);
    Die();
}

void APS5_VABI __ubsan_handle_load_invalid_value(const void* data, std::uintptr_t value) {
    LoadInvalidValue(data, value, false);
}

void APS5_VABI __ubsan_handle_load_invalid_value_abort(const void* data, std::uintptr_t value) {
    LoadInvalidValue(data, value, true);
    Die();
}

void APS5_VABI __ubsan_handle_implicit_conversion(const void* data, std::uintptr_t source, std::uintptr_t destination) {
    ImplicitConversion(data, source, destination, false);
}

void APS5_VABI __ubsan_handle_implicit_conversion_abort(const void* data, std::uintptr_t source, std::uintptr_t destination) {
    ImplicitConversion(data, source, destination, true);
    Die();
}

void APS5_VABI __ubsan_handle_invalid_builtin(const void* data) {
    InvalidBuiltin(data);
}

void APS5_VABI __ubsan_handle_invalid_builtin_abort(const void* data) {
    InvalidBuiltin(data);
    Die();
}

void APS5_VABI __ubsan_handle_nonnull_return_v1(const void* data, const void* site) {
    NonNullReturn(data, site, false, true);
}

void APS5_VABI __ubsan_handle_nonnull_return_v1_abort(const void* data, const void* site) {
    NonNullReturn(data, site, true, true);
    Die();
}

void APS5_VABI __ubsan_handle_nullability_return_v1(const void* data, const void* site) {
    NonNullReturn(data, site, false, false);
}

void APS5_VABI __ubsan_handle_nullability_return_v1_abort(const void* data, const void* site) {
    NonNullReturn(data, site, true, false);
    Die();
}

void APS5_VABI __ubsan_handle_nonnull_arg(const void* data) {
    NonNullArg(data, false, true);
}

void APS5_VABI __ubsan_handle_nonnull_arg_abort(const void* data) {
    NonNullArg(data, true, true);
    Die();
}

void APS5_VABI __ubsan_handle_nullability_arg(const void* data) {
    NonNullArg(data, false, false);
}

void APS5_VABI __ubsan_handle_nullability_arg_abort(const void* data) {
    NonNullArg(data, true, false);
    Die();
}

void APS5_VABI __ubsan_handle_pointer_overflow(const void* data, std::uintptr_t base, std::uintptr_t result) {
    PointerOverflow(data, base, result, false);
}

void APS5_VABI __ubsan_handle_pointer_overflow_abort(const void* data, std::uintptr_t base, std::uintptr_t result) {
    PointerOverflow(data, base, result, true);
    Die();
}

const char* APS5_VABI __ubsan_default_options() {
    return "";
}

std::uint16_t APS5_VABI __sanitizer_unaligned_load16(const void* pointer) {
    return UnalignedLoad<std::uint16_t>(pointer);
}

std::uint32_t APS5_VABI __sanitizer_unaligned_load32(const void* pointer) {
    return UnalignedLoad<std::uint32_t>(pointer);
}

std::uint64_t APS5_VABI __sanitizer_unaligned_load64(const void* pointer) {
    return UnalignedLoad<std::uint64_t>(pointer);
}

void APS5_VABI __sanitizer_unaligned_store16(void* pointer, std::uint16_t value) {
    UnalignedStore(pointer, value);
}

void APS5_VABI __sanitizer_unaligned_store32(void* pointer, std::uint32_t value) {
    UnalignedStore(pointer, value);
}

void APS5_VABI __sanitizer_unaligned_store64(void* pointer, std::uint64_t value) {
    UnalignedStore(pointer, value);
}

}
