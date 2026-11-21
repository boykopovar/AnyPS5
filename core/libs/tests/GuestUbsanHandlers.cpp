#include "prx/libSceAddressSanitizer/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stdout, "failed: %s\n", what);
    std::fflush(stdout);
    std::abort();
}

struct Outcome {
    std::string output;
    int status = 0;
};

Outcome Run(const std::function<void()>& body) {
    int channel[2] = {-1, -1};
    Require(::pipe(channel) == 0, "pipe");
    std::fflush(nullptr);
    const pid_t child = ::fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        ::close(channel[0]);
        if (::dup2(channel[1], 2) != 2) ::_exit(70);
        ::close(channel[1]);
        body();
        std::fflush(nullptr);
        ::_exit(0);
    }
    ::close(channel[1]);
    Outcome outcome;
    char buffer[512];
    for (ssize_t count; (count = ::read(channel[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(count));
    ::close(channel[0]);
    Require(::waitpid(child, &outcome.status, 0) == child, "waitpid");
    return outcome;
}

bool ExitedWith(const Outcome& outcome, int code) {
    return WIFEXITED(outcome.status) && WEXITSTATUS(outcome.status) == code;
}

void Expect(const Outcome& outcome, int code, const std::string& output, const char* what) {
    if (ExitedWith(outcome, code) && outcome.output == output) return;
    std::fprintf(stdout, "%s: status %d, output:\n%s---\nexpected:\n%s---\n", what, outcome.status, outcome.output.c_str(), output.c_str());
    std::fflush(stdout);
    std::abort();
}

struct Location {
    const char* file;
    std::uint32_t line;
    std::uint32_t column;
};

template <std::size_t N>
struct Type {
    std::uint16_t kind;
    std::uint16_t info;
    char name[N];
};

template <std::size_t N>
constexpr Type<N> Integer(unsigned log2Bits, bool isSigned, const char (&name)[N]) {
    Type<N> type{0, static_cast<std::uint16_t>((log2Bits << 1) | (isSigned ? 1 : 0)), {}};
    for (std::size_t i = 0; i < N; ++i) type.name[i] = name[i];
    return type;
}

template <std::size_t N>
constexpr Type<N> Float(unsigned bits, const char (&name)[N]) {
    Type<N> type{1, static_cast<std::uint16_t>(bits), {}};
    for (std::size_t i = 0; i < N; ++i) type.name[i] = name[i];
    return type;
}

struct OverflowData {
    Location loc;
    const void* type;
};

struct TwoTypeData {
    Location loc;
    const void* first;
    const void* second;
};

struct TypeMismatchData {
    Location loc;
    const void* type;
    unsigned char logAlignment;
    unsigned char kind;
};

struct AlignmentAssumptionData {
    Location loc;
    Location assumption;
    const void* type;
};

struct ImplicitConversionData {
    Location loc;
    const void* from;
    const void* to;
    unsigned char kind;
};

struct InvalidBuiltinData {
    Location loc;
    unsigned char kind;
};

struct NonNullArgData {
    Location loc;
    Location attribute;
    int index;
};

struct FloatCastV1 {
    const void* from;
    const void* to;
};

static_assert(sizeof(Location) == 16);
static_assert(offsetof(TypeMismatchData, logAlignment) == 24 && offsetof(TypeMismatchData, kind) == 25);
static_assert(offsetof(AlignmentAssumptionData, type) == 32);
static_assert(offsetof(ImplicitConversionData, kind) == 32);
static_assert(offsetof(InvalidBuiltinData, kind) == 16);
static_assert(offsetof(NonNullArgData, index) == 32);

constexpr auto Int = Integer(5, true, "int");
constexpr auto UInt = Integer(5, false, "unsigned int");
constexpr auto ULong = Integer(6, false, "unsigned long");
constexpr auto Long = Integer(6, true, "long");
constexpr auto Int128 = Integer(7, true, "__int128");
constexpr auto UChar = Integer(3, false, "unsigned char");
constexpr auto Bool = Integer(3, false, "bool");
constexpr auto Double = Float(64, "double");
constexpr auto FloatT = Float(32, "float");
constexpr auto LongDouble = Float(80, "long double");
constexpr auto IntArray = Type<7>{0xffff, 0, "int[4]"};

std::string Summary(const char* where) {
    return std::string("SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior ") + where + " in \n";
}

std::uintptr_t Bits(std::int64_t value) {
    return static_cast<std::uintptr_t>(value);
}

void IntegerOverflows() {
    OverflowData add{{"t.c", 3, 5}, &Int};
    Expect(Run([&] {
               __ubsan_handle_add_overflow(&add, 0x7fffffff, 1);
               __ubsan_handle_add_overflow(&add, 0x7fffffff, 1);
           }),
           0,
           "t.c:3:5: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'\n" + Summary("t.c:3:5"),
           "add reported once");

    OverflowData sub{{"t.c", 4, 0}, &UInt};
    Expect(Run([&] { __ubsan_handle_sub_overflow(&sub, 0, 1); }), 0,
           "t.c:4: runtime error: unsigned integer overflow: 0 - 1 cannot be represented in type 'unsigned int'\n" + Summary("t.c:4"),
           "sub without column");

    std::uint64_t big = 0xffffffffffffffffull;
    OverflowData mul{{"t.c", 5, 7}, &ULong};
    Expect(Run([&] { __ubsan_handle_mul_overflow(&mul, big, 2); }), 0,
           "t.c:5:7: runtime error: unsigned integer overflow: 18446744073709551615 * 2 cannot be represented in type 'unsigned long'\n" +
               Summary("t.c:5:7"),
           "mul 64-bit inline");

    __int128 wide = static_cast<__int128>(1) << 100;
    __int128 one = 1;
    OverflowData add128{{"w.c", 1, 1}, &Int128};
    Expect(Run([&] {
               __ubsan_handle_add_overflow(&add128, reinterpret_cast<std::uintptr_t>(&wide), reinterpret_cast<std::uintptr_t>(&one));
           }),
           0,
           "w.c:1:1: runtime error: signed integer overflow: 0x00000010000000000000000000000000 + 1 cannot be represented in type '__int128'\n" +
               Summary("w.c:1:1"),
           "128-bit values by pointer");

    OverflowData negate{{"n.c", 2, 3}, &Int};
    Expect(Run([&] { __ubsan_handle_negate_overflow(&negate, 0x80000000u); }), 0,
           "n.c:2:3: runtime error: negation of -2147483648 cannot be represented in type 'int'; cast to an unsigned type to negate this value to "
           "itself\n" + Summary("n.c:2:3"),
           "negate signed");

    OverflowData negateUnsigned{{"n.c", 3, 3}, &UInt};
    Expect(Run([&] { __ubsan_handle_negate_overflow(&negateUnsigned, 5); }), 0,
           "n.c:3:3: runtime error: negation of 5 cannot be represented in type 'unsigned int'\n" + Summary("n.c:3:3"),
           "negate unsigned");

    OverflowData divide{{"d.c", 1, 2}, &Long};
    Expect(Run([&] { __ubsan_handle_divrem_overflow(&divide, Bits(INT64_MIN), Bits(-1)); }), 0,
           "d.c:1:2: runtime error: division of -9223372036854775808 by -1 cannot be represented in type 'long'\n" + Summary("d.c:1:2"),
           "divide by -1");

    OverflowData zero{{"d.c", 2, 2}, &Int};
    Expect(Run([&] { __ubsan_handle_divrem_overflow(&zero, 7, 0); }), 0,
           "d.c:2:2: runtime error: division by zero\n" + Summary("d.c:2:2"), "divide by zero");

    OverflowData abortAdd{{"a.c", 9, 9}, &Int};
    Expect(Run([&] {
               __ubsan_handle_add_overflow_abort(&abortAdd, 1, 2);
               std::fputs("returned\n", stderr);
           }),
           1, "a.c:9:9: runtime error: signed integer overflow: 1 + 2 cannot be represented in type 'int'\n" + Summary("a.c:9:9"),
           "abort variant exits 1");

    OverflowData abortTwice{{"a.c", 10, 1}, &Int};
    Expect(Run([&] {
               __ubsan_handle_add_overflow(&abortTwice, 1, 2);
               __ubsan_handle_add_overflow_abort(&abortTwice, 3, 4);
           }),
           1,
           "a.c:10:1: runtime error: signed integer overflow: 1 + 2 cannot be represented in type 'int'\n" + Summary("a.c:10:1") +
               "a.c:10:4294967295: runtime error: signed integer overflow: 3 + 4 cannot be represented in type 'int'\n" +
               Summary("a.c:10:4294967295"),
           "abort variant reports a disabled location");
}

void Shifts() {
    TwoTypeData shift{{"s.c", 1, 1}, &Int, &Int};
    Expect(Run([&] { __ubsan_handle_shift_out_of_bounds(&shift, 1, Bits(-2)); }), 0,
           "s.c:1:1: runtime error: shift exponent -2 is negative\n" + Summary("s.c:1:1"), "negative exponent");
    TwoTypeData large{{"s.c", 2, 1}, &Int, &ULong};
    Expect(Run([&] { __ubsan_handle_shift_out_of_bounds(&large, 1, 40); }), 0,
           "s.c:2:1: runtime error: shift exponent 40 is too large for 32-bit type 'int'\n" + Summary("s.c:2:1"), "exponent too large");
    TwoTypeData negative{{"s.c", 3, 1}, &Int, &Int};
    Expect(Run([&] { __ubsan_handle_shift_out_of_bounds(&negative, 0xffffffffu, 1); }), 0,
           "s.c:3:1: runtime error: left shift of negative value -1\n" + Summary("s.c:3:1"), "negative base");
    TwoTypeData base{{"s.c", 4, 1}, &Int, &Int};
    Expect(Run([&] { __ubsan_handle_shift_out_of_bounds(&base, 0x40000000, 2); }), 0,
           "s.c:4:1: runtime error: left shift of 1073741824 by 2 places cannot be represented in type 'int'\n" + Summary("s.c:4:1"),
           "base overflow");
}

void Bounds() {
    TwoTypeData index{{"b.c", 6, 10}, &IntArray, &Long};
    Expect(Run([&] { __ubsan_handle_out_of_bounds(&index, 5); }), 0,
           "b.c:6:10: runtime error: index 5 out of bounds for type 'int[4]'\n" + Summary("b.c:6:10"), "out of bounds");

    OverflowData vla{{"v.c", 2, 9}, &Int};
    Expect(Run([&] { __ubsan_handle_vla_bound_not_positive(&vla, 0xfffffffdu); }), 0,
           "v.c:2:9: runtime error: variable length array bound evaluates to non-positive value -3\n" + Summary("v.c:2:9"), "vla bound");

    Location unreachable{"u.c", 7, 1};
    Expect(Run([&] { __ubsan_handle_builtin_unreachable(&unreachable); }), 1,
           "u.c:7:1: runtime error: execution reached an unreachable program point\n" + Summary("u.c:7:1"), "unreachable");
    Location missing{"u.c", 9, 1};
    Expect(Run([&] { __ubsan_handle_missing_return(&missing); }), 1,
           "u.c:9:1: runtime error: execution reached the end of a value-returning function without returning a value\n" + Summary("u.c:9:1"),
           "missing return");
}

void Floats() {
    double huge = 1e20;
    std::uint64_t hugeBits;
    std::memcpy(&hugeBits, &huge, sizeof(hugeBits));
    TwoTypeData cast{{"f.c", 1, 5}, &Double, &Int};
    Expect(Run([&] { __ubsan_handle_float_cast_overflow(&cast, hugeBits); }), 0,
           "f.c:1:5: runtime error: 1e+20 is outside the range of representable values of type 'int'\n" + Summary("f.c:1:5"),
           "float cast v2");

    float negative = -3.5f;
    std::uint32_t negativeBits;
    std::memcpy(&negativeBits, &negative, sizeof(negativeBits));
    FloatCastV1 old{&FloatT, &UInt};
    const Outcome v1 = Run([&] { __ubsan_handle_float_cast_overflow(&old, negativeBits); });
    Require(ExitedWith(v1, 0), "float cast v1 status");
    Require(v1.output.rfind("0x", 0) == 0, "float cast v1 caller address");
    Require(v1.output.find(": runtime error: -3.5 is outside the range of representable values of type 'unsigned int'\n"
                           "SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior\n") != std::string::npos,
            "float cast v1 text");

    unsigned char extended[16] = {0, 0, 0, 0, 0, 0, 0, 0xc0, 0xff, 0x3f};
    TwoTypeData longCast{{"f.c", 2, 5}, &LongDouble, &Int};
    Expect(Run([&] { __ubsan_handle_float_cast_overflow(&longCast, reinterpret_cast<std::uintptr_t>(extended)); }), 0,
           "f.c:2:5: runtime error: 1.5 is outside the range of representable values of type 'int'\n" + Summary("f.c:2:5"),
           "80-bit long double by pointer");
}

void Values() {
    OverflowData invalid{{"l.c", 1, 2}, &Bool};
    Expect(Run([&] { __ubsan_handle_load_invalid_value(&invalid, 2); }), 0,
           "l.c:1:2: runtime error: load of value 2, which is not a valid value for type 'bool'\n" + Summary("l.c:1:2"), "invalid bool");

    ImplicitConversionData conversion{{"i.c", 3, 4}, &Int, &UChar, 1};
    Expect(Run([&] { __ubsan_handle_implicit_conversion(&conversion, 300, 44); }), 0,
           "i.c:3:4: runtime error: implicit conversion from type 'int' of value 300 (32-bit, signed) to type 'unsigned char' changed the "
           "value to 44 (8-bit, unsigned)\n" + Summary("i.c:3:4"),
           "implicit conversion");

    InvalidBuiltinData ctz{{"z.c", 1, 1}, 0};
    InvalidBuiltinData clz{{"z.c", 2, 1}, 1};
    Expect(Run([&] {
               __ubsan_handle_invalid_builtin(&ctz);
               __ubsan_handle_invalid_builtin(&ctz);
               __ubsan_handle_invalid_builtin(&clz);
           }),
           0,
           "z.c:1:1: runtime error: passing zero to ctz(), which is not a valid argument\n" + Summary("z.c:1:1") +
               "z.c:1:4294967295: runtime error: passing zero to ctz(), which is not a valid argument\n" + Summary("z.c:1:4294967295") +
               "z.c:2:1: runtime error: passing zero to clz(), which is not a valid argument\n" + Summary("z.c:2:1"),
           "invalid builtin is never deduplicated");
}

void Nullability() {
    Location attribute{"h.h", 1, 2};
    Location site{"r.c", 5, 3};
    Expect(Run([&] { __ubsan_handle_nonnull_return_v1(&attribute, &site); }), 0,
           "r.c:5:3: runtime error: null pointer returned from function declared to never return null\n"
           "h.h:1:2: note: returns_nonnull attribute specified here\n" + Summary("r.c:5:3"),
           "nonnull return");
    Location noAttribute{nullptr, 0, 0};
    Location site2{"r.c", 6, 3};
    Expect(Run([&] { __ubsan_handle_nullability_return_v1(&noAttribute, &site2); }), 0,
           "r.c:6:3: runtime error: null pointer returned from function declared to never return null\n" + Summary("r.c:6:3"),
           "nullability return without attribute");

    NonNullArgData argument{{"g.c", 8, 1}, {"g.h", 2, 3}, 2};
    Expect(Run([&] { __ubsan_handle_nonnull_arg(&argument); }), 0,
           "g.c:8:1: runtime error: null pointer passed as argument 2, which is declared to never be null\n"
           "g.h:2:3: note: nonnull attribute specified here\n" + Summary("g.c:8:1"),
           "nonnull arg");
    NonNullArgData nullability{{"g.c", 9, 1}, {"g.h", 3, 3}, 1};
    Expect(Run([&] { __ubsan_handle_nullability_arg(&nullability); }), 0,
           "g.c:9:1: runtime error: null pointer passed as argument 1, which is declared to never be null\n"
           "g.h:3:3: note: _Nonnull type annotation specified here\n" + Summary("g.c:9:1"),
           "nullability arg");
}

void Pointers() {
    Location zero{"p.c", 1, 1};
    Expect(Run([&] { __ubsan_handle_pointer_overflow(&zero, 0, 0); }), 0,
           "p.c:1:1: runtime error: applying zero offset to null pointer\n" + Summary("p.c:1:1"), "zero offset");
    Location nonZero{"p.c", 2, 1};
    Expect(Run([&] { __ubsan_handle_pointer_overflow(&nonZero, 0, 16); }), 0,
           "p.c:2:1: runtime error: applying non-zero offset 16 to null pointer\n" + Summary("p.c:2:1"), "offset on null");
    Location toNull{"p.c", 3, 1};
    Expect(Run([&] { __ubsan_handle_pointer_overflow(&toNull, 0x10, 0); }), 0,
           "p.c:3:1: runtime error: applying non-zero offset to non-null pointer 0x000000000010 produced null pointer\n" + Summary("p.c:3:1"),
           "result null");
    Location wrap{"p.c", 4, 1};
    Expect(Run([&] { __ubsan_handle_pointer_overflow(&wrap, 0x7ffffffffff0, 0x10); }), 0,
           "p.c:4:1: runtime error: addition of unsigned offset to 0x7ffffffffff0 overflowed to 0x000000000010\n" + Summary("p.c:4:1"),
           "addition wrap");
    Location sign{"p.c", 5, 1};
    Expect(Run([&] { __ubsan_handle_pointer_overflow(&sign, 0x10, 0xfffffffffffffff0ull); }), 0,
           "p.c:5:1: runtime error: pointer index expression with base 0x000000000010 overflowed to 0xfffffffffffffff0\n" + Summary("p.c:5:1"),
           "sign change");

    TypeMismatchData null{{"m.c", 1, 1}, &Int, 2, 0};
    Expect(Run([&] { __ubsan_handle_type_mismatch_v1(&null, 0); }), 0,
           "m.c:1:1: runtime error: load of null pointer of type 'int'\n" + Summary("m.c:1:1"), "null load");
    TypeMismatchData misaligned{{"m.c", 2, 1}, &Long, 3, 1};
    Expect(Run([&] { __ubsan_handle_type_mismatch_v1(&misaligned, 0x1004); }), 0,
           "m.c:2:1: runtime error: store to misaligned address 0x000000001004 for type 'long', which requires 8 byte alignment\n"
           "0x000000001004: note: pointer points here\n" + Summary("m.c:2:1"),
           "misaligned store");
    TypeMismatchData small{{"m.c", 3, 1}, &Int, 0, 4};
    Expect(Run([&] { __ubsan_handle_type_mismatch_v1(&small, 0x2000); }), 0,
           "m.c:3:1: runtime error: member call on address 0x000000002000 with insufficient space for an object of type 'int'\n"
           "0x000000002000: note: pointer points here\n" + Summary("m.c:3:1"),
           "insufficient space");

    AlignmentAssumptionData assume{{"q.c", 1, 1}, {"q.c", 0, 0}, &Int};
    Expect(Run([&] { __ubsan_handle_alignment_assumption(&assume, 0x1004, 16, 0); }), 0,
           "q.c:1:1: runtime error: assumption of 16 byte alignment for pointer of type 'int' failed\n"
           "q.c: note: alignment assumption was specified here\n"
           "0x000000001004: note: address is 4 aligned, misalignment offset is 4 bytes\n" + Summary("q.c:1:1"),
           "alignment assumption");
    AlignmentAssumptionData offset{{"q.c", 2, 1}, {nullptr, 0, 0}, &Int};
    Expect(Run([&] { __ubsan_handle_alignment_assumption(&offset, 0x1006, 8, 2); }), 0,
           "q.c:2:1: runtime error: assumption of 8 byte alignment (with offset of 2 byte) for pointer of type 'int' failed\n"
           "0x000000001004: note: offset address is 4 aligned, misalignment offset is 4 bytes\n" + Summary("q.c:2:1"),
           "alignment assumption with offset");
}

void UnknownLocation() {
    OverflowData unknown{{nullptr, 0, 0}, &Int};
    Expect(Run([&] { __ubsan_handle_add_overflow(&unknown, 1, 1); }), 0,
           "<unknown>: runtime error: signed integer overflow: 1 + 1 cannot be represented in type 'int'\n"
           "SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior\n",
           "unknown location");
}

void Faults() {
    ImplicitConversionData floating{{"x.c", 1, 1}, &Double, &Int, 1};
    Expect(Run([&] {
               try {
                   __ubsan_handle_implicit_conversion(&floating, 0, 0);
               } catch (const std::invalid_argument&) {
                   std::fputs("threw\n", stderr);
               }
           }),
           0, "threw\n", "a failed report prints nothing");
    OverflowData nullType{{"x.c", 2, 1}, nullptr};
    Expect(Run([&] {
               try {
                   __ubsan_handle_add_overflow(&nullType, 1, 1);
               } catch (const std::invalid_argument&) {
                   std::fputs("threw\n", stderr);
               }
           }),
           0, "threw\n", "unreadable type descriptor");
}

void Unaligned() {
    unsigned char bytes[16] = {};
    __sanitizer_unaligned_store16(bytes + 1, 0x1234);
    __sanitizer_unaligned_store32(bytes + 3, 0x89abcdefu);
    __sanitizer_unaligned_store64(bytes + 7, 0x0102030405060708ull);
    Require(__sanitizer_unaligned_load16(bytes + 1) == 0x1234, "load16");
    Require(__sanitizer_unaligned_load32(bytes + 3) == 0x89abcdefu, "load32");
    Require(__sanitizer_unaligned_load64(bytes + 7) == 0x0102030405060708ull, "load64");
    Require(bytes[1] == 0x34 && bytes[2] == 0x12 && bytes[0] == 0 && bytes[15] == 0, "store bytes");
    Require(std::strcmp(__ubsan_default_options(), "") == 0, "default options");
}

}

int main() {
    IntegerOverflows();
    Shifts();
    Bounds();
    Floats();
    Values();
    Nullability();
    Pointers();
    UnknownLocation();
    Faults();
    Unaligned();
    std::puts("guest_ubsan_handlers passed");
    return 0;
}
