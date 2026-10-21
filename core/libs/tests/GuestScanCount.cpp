#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>

static int APS5_VABI ScanList(const char* input, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vsscanf_s_nid_postfix(input, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

static int failures = 0;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        ++failures;
    }
}

static int constraintCalls = 0;
static std::string constraintMessage;

static void APS5_VABI RecordConstraint(const char* message, void* pointer, int error) {
    ++constraintCalls;
    constraintMessage = message;
    Require(pointer == nullptr && error == 22, "Constraint handler must receive a null pointer and EINVAL");
}

static bool Constraint(const char* message) {
    const bool matches = message == nullptr ? constraintCalls == 0 : constraintCalls == 1 && constraintMessage == message;
    constraintCalls = 0;
    constraintMessage.clear();
    return matches;
}

struct Field {
    char text[8];
    Field() { std::memset(text, '~', sizeof(text)); }
    bool Holds(const char* expected, std::size_t size) const { return std::memcmp(text, expected, size) == 0; }
};

using Scan = int (APS5_VABI*)(const char*, const char*, ...);

static void CheckConstraints(Scan scan, const char* badArgument) {
    constexpr std::size_t rsizeMax = (std::size_t{1} << 61) - 1;
    const std::size_t four = 4;
    const char* integer = "scanf_s: bad integer argument";
    const char* sized = "scanf_s: bad %c, %s, or %[ argument";
    const char* size = "scanf_s: bad %c, %s, or %[ size";
    Require(scan("7", "%d", static_cast<int*>(nullptr)) == -23 && Constraint(integer), "Null integer destination");
    Require(scan("1", "%p", static_cast<void**>(nullptr)) == -23 && Constraint(integer), "Null pointer destination");
    Require(scan("1.5", "%f", static_cast<float*>(nullptr)) == -23 && Constraint("scanf_s: bad floating-point argument"), "Null real destination");
    Require(scan("abc", "%s", static_cast<char*>(nullptr), four) == -23 && Constraint(sized), "Null string destination");
    Require(scan("a", "%c", static_cast<char*>(nullptr), four) == -23 && Constraint(sized), "Null character destination");
    Require(scan("abc", "%[a-z]", static_cast<char*>(nullptr), four) == -23 && Constraint(sized), "Null scanset destination");
    Require(scan("ab", "%s", static_cast<char*>(nullptr), std::size_t{0}) == -23 && Constraint(sized), "Null string destination with size 0");
    Require(scan("", "%s", static_cast<char*>(nullptr), four) == -23 && Constraint(sized), "Null string destination is checked before input");
    Require(scan("ab", "%n", static_cast<int*>(nullptr)) == -23 && Constraint("scanf_s: bad %n argument"), "Null count destination");
    Require(scan("", "%d", static_cast<int*>(nullptr)) == EOF && Constraint(nullptr), "Input failure before a null integer destination");
    Require(scan("", "%f", static_cast<float*>(nullptr)) == EOF && Constraint(nullptr), "Input failure before a null real destination");
    Require(scan("x", "%d", static_cast<int*>(nullptr)) == 0 && Constraint(nullptr), "Matching failure before a null integer destination");
    Require(scan("x1", "x%d", static_cast<int*>(nullptr)) == -23 && Constraint(integer), "Literals are not conversions");
    int first = -1;
    int later = -1;
    Require(scan("7 8 9", "%d%d%d", &first, static_cast<int*>(nullptr), &later) == 1 && first == 7 && later == -1 && Constraint(integer),
        "Null destination after an assignment returns the assignments");
    first = -1;
    Require(scan("7", "%d%n%d", &first, static_cast<int*>(nullptr), &later) == 1 && first == 7 && later == -1 && Constraint("scanf_s: bad %n argument"),
        "Null count after an assignment returns the assignments");
    Require(scan("1 2", "%*d %d", static_cast<int*>(nullptr)) == 0 && Constraint(integer), "Null destination after a suppressed conversion returns 0");
    first = -1;
    Require(scan("5", "%n%d", &first, static_cast<int*>(nullptr)) == 0 && first == 0 && Constraint(integer), "Null destination after a count returns 0");
    Require(scan("5 ab", "%d %s", &first, static_cast<char*>(nullptr), four) == 1 && first == 5 && Constraint(sized), "Null string after an assignment");
    first = -1;
    Require(scan("x", "%d%d", &first, static_cast<int*>(nullptr)) == 0 && first == -1 && Constraint(nullptr),
        "Matching failure must stop before an unreachable null destination");
    Field field;
    Require(scan("ab", "%s", field.text, rsizeMax) == 1 && field.Holds("ab\0", 3) && Constraint(nullptr), "Size RSIZE_MAX");
    for (const std::size_t huge : {rsizeMax + 1, SIZE_MAX}) {
        Field untouched;
        Require(scan("ab", "%s", untouched.text, huge) == -23 && untouched.Holds("~", 1) && Constraint(size), "Size above RSIZE_MAX");
        Require(scan("a", "%c", untouched.text, huge) == -23 && Constraint(size), "Character size above RSIZE_MAX");
        Require(scan("", "%[a-z]", untouched.text, huge) == -23 && Constraint(size), "Scanset size is checked before input");
        Require(scan("5 ab", "%d %s", &first, untouched.text, huge) == 1 && Constraint(size), "Size above RSIZE_MAX after an assignment");
        Require(scan("x", "%*s %s", untouched.text, huge) == 0 && Constraint(size), "Size above RSIZE_MAX after a suppressed conversion");
    }
    Field wide;
    Require(scan("abcdef", "%s", wide.text, (std::size_t{1} << 32) | 2) == 1 && wide.Holds("abcdef\0", 7) && Constraint(nullptr),
        "The size argument is 64 bits wide");
    Require(scan(nullptr, "%d", &first) == EOF && Constraint(badArgument), "Null input");
    Require(scan("1", nullptr, &first) == EOF && Constraint(badArgument), "Null format");
    Require(scan("7 8", "%*d%d", &first) == 1 && first == 8 && Constraint(nullptr), "Suppressed conversions must not consume destinations");
}

static void CheckCapacities(Scan scan) {
    {
        Field a;
        Require(scan("abc", "%s", a.text, std::size_t{4}) == 1 && a.Holds("abc\0~", 5), "String that fits");
    }
    {
        Field a;
        Require(scan("abcd", "%s", a.text, std::size_t{4}) == 0 && a.Holds("abcd~", 5), "String without room for the terminator");
    }
    {
        Field a;
        Field b;
        int first = -1;
        int last = -1;
        Require(scan("12 ab abcd 9", "%d %s %s %d", &first, a.text, std::size_t{4}, b.text, std::size_t{4}, &last) == 2 && first == 12 &&
                a.Holds("ab\0", 3) && b.Holds("abcd~", 5) && last == -1,
            "Undersized later string returns the assignments");
    }
    {
        Field a;
        Require(scan("x", "%s", a.text, std::size_t{1}) == 0 && a.Holds("x~", 2), "String size 1");
        Field b;
        Require(scan("x", "%s", b.text, std::size_t{0}) == 0 && b.Holds("~", 1), "String size 0");
        Require(scan("", "%s", b.text, std::size_t{0}) == EOF && b.Holds("~", 1), "String size 0 on empty input");
    }
    {
        Field a;
        Require(scan("abcdef", "%3s", a.text, std::size_t{4}) == 1 && a.Holds("abc\0~", 5), "Width below the size");
        Field b;
        Require(scan("abcdef", "%5s", b.text, std::size_t{4}) == 0 && b.Holds("abcd~", 5), "Width above the size");
        Field c;
        Require(scan("ab", "%5s", c.text, std::size_t{4}) == 1 && c.Holds("ab\0~", 4), "Width above the size with a short field");
        Field d;
        Require(scan("abcd", "%4s", d.text, std::size_t{4}) == 0 && d.Holds("abcd~", 5), "Width equal to the size");
    }
    {
        Field a;
        Require(scan("xy", "%2c", a.text, std::size_t{1}) == 1 && a.Holds("x~", 2), "Character width clamped to the size");
        Field b;
        Field c;
        Require(scan("xyz", "%2c%c", b.text, std::size_t{1}, c.text, std::size_t{1}) == 2 && b.Holds("x~", 2) && c.Holds("y~", 2),
            "Clamped character width consumes only the stored characters");
        Field d;
        Field e;
        Require(scan("xyzw", "%3c%c", d.text, std::size_t{2}, e.text, std::size_t{1}) == 2 && d.Holds("xy~", 3) && e.Holds("z~", 2),
            "Clamped character width of 2");
        Field f;
        Require(scan("x", "%3c", f.text, std::size_t{2}) == 1 && f.Holds("x~", 2), "Short character field");
        Field g;
        Require(scan("xy", "%c", g.text, std::size_t{0}) == 0 && g.Holds("~", 1), "Character size 0");
        Require(scan("", "%c", g.text, std::size_t{0}) == EOF, "Character size 0 on empty input");
        int number = -1;
        Field h;
        Require(scan("5 xy", "%d %2c", &number, h.text, std::size_t{1}) == 2 && number == 5 && h.Holds("x~", 2), "Clamped character after an integer");
    }
    {
        Field a;
        Require(scan("abc1", "%[a-z]", a.text, std::size_t{4}) == 1 && a.Holds("abc\0~", 5), "Scanset that fits");
        Field b;
        Require(scan("abcd1", "%[a-z]", b.text, std::size_t{4}) == 0 && b.Holds("abcd~", 5), "Scanset without room for the terminator");
        Field c;
        int number = -1;
        Require(scan("7abcd1", "%d%[a-z]", &number, c.text, std::size_t{4}) == 1 && number == 7 && c.Holds("abcd~", 5), "Undersized later scanset");
        Field d;
        Require(scan("abcdef", "%3[a-z]", d.text, std::size_t{4}) == 1 && d.Holds("abc\0~", 5), "Scanset width below the size");
        Field e;
        Require(scan("abcdef", "%4[a-z]", e.text, std::size_t{4}) == 0 && e.Holds("abcd~", 5), "Scanset width equal to the size");
        Field f;
        Require(scan("", "%[a-z]", f.text, std::size_t{0}) == EOF && scan("", "%[a-z]", f.text, std::size_t{4}) == EOF, "Scanset on empty input");
        Require(scan("1", "%[a-z]", f.text, std::size_t{0}) == 0 && scan(" a", "%[a-z]", f.text, std::size_t{4}) == 0 && f.Holds("~", 1),
            "Scanset matching failure");
        Field g;
        Require(scan("ab cd", "%*s %s", g.text, std::size_t{4}) == 1 && g.Holds("cd\0", 3), "Suppressed string takes no size");
    }
}

struct WideField {
    char16_t text[8];
    WideField() { std::fill(std::begin(text), std::end(text), u'~'); }
    bool Holds(const char16_t* expected, std::size_t size) const { return std::memcmp(text, expected, size * sizeof(char16_t)) == 0; }
};

static void CheckWideFields(Scan scan) {
    {
        WideField a;
        Require(scan("abc", "%ls", a.text, std::size_t{4}) == 1 && a.Holds(u"abc\0~", 5), "Wide string that fits");
        WideField b;
        Require(scan("abcd", "%ls", b.text, std::size_t{4}) == 0 && b.Holds(u"abcd~", 5), "Wide string without room for the terminator");
        WideField c;
        Require(scan("abcde", "%ls", c.text, std::size_t{4}) == 0 && c.Holds(u"abcd~", 5), "Wide string longer than the size");
        WideField d;
        Require(scan("abc", "%ls", d.text, std::size_t{3}) == 0 && d.Holds(u"abc~", 4), "Wide string of the size");
        WideField e;
        Require(scan("abcde", "%l[a-z]", e.text, std::size_t{4}) == 0 && e.Holds(u"abcd~", 5), "Wide scanset longer than the size");
    }
    {
        WideField a;
        int count = -1;
        Require(scan("abc", "%3lc%n", a.text, std::size_t{2}, &count) == 1 && a.Holds(u"ab~", 3) && count == 3,
            "Clamped wide character width consumes the whole width");
        WideField b;
        count = -1;
        Require(scan("abc", "%4lc%n", b.text, std::size_t{2}, &count) == 1 && b.Holds(u"ab~", 3) && count == 3,
            "Clamped wide character width stops at the end of input");
        WideField c;
        count = -1;
        Require(scan("ab", "%4lc%n", c.text, std::size_t{8}, &count) == 1 && c.Holds(u"ab~", 3) && count == 2, "Short wide character field");
    }
}

static void CheckFailures(Scan scan) {
    int first = -1;
    int second = -1;
    Require(scan("", "x%d", &first) == 0 && scan("", "x") == 0 && scan(" ", " x%d", &first) == 0 && scan("y", "x%d", &first) == 0,
        "A literal mismatch is a matching failure, also at the end of input");
    Require(scan("x", "x%d", &first) == EOF, "A matched literal is not a conversion");
    Require(scan("", "%%") == EOF && scan("  ", "%%%d", &first) == EOF && scan("", " %d", &first) == EOF && scan("", "%*d") == EOF,
        "Input failure before the first conversion");
    Require(scan("", "%n%d", &first, &second) == 0 && first == 0 && second == -1, "A count is a conversion for the input failure result");
    Require(scan("", "%*n%d", &second) == 0 && second == -1, "A suppressed count is a conversion for the input failure result");
    Require(scan("1", "%*d %d", &second) == 0 && second == -1, "A suppressed conversion is a conversion for the input failure result");
    Require(scan("1", "%d x", &first) == 1 && first == 1, "Literal at the end of input after an assignment");
    first = -1;
    Require(scan("1-2", "%d+%d", &first, &second) == 1 && first == 1 && second == -1, "Literal mismatch after an assignment");
    Require(scan("x", "%*d") == 0 && scan("abc", "") == 0, "Matching failure and empty format");
    Require(Constraint(nullptr), "Failures do not call the constraint handler");
}

template<class TValue> static void CheckCount(const char* modifier, int count) {
    struct alignas(8) Destination {
        TValue value;
        std::array<unsigned char, 8> guard;
    } destination{static_cast<TValue>(-1), {}};
    destination.guard.fill(0xa5);
    const auto guard = destination.guard;
    const std::string input(static_cast<std::size_t>(count), 'x');
    const std::string format = input + "%" + modifier + "n";
    const int assigned = sscanf_s_nid_postfix(input.c_str(), format.c_str(), &destination.value);
    if (destination.value != count || destination.guard != guard || assigned != 0) {
        std::fprintf(stderr, "%%%sn after %d bytes: count=%lld, guard=%s, assignments=%d\n",
            modifier, count, static_cast<long long>(destination.value),
            destination.guard == guard ? "unchanged" : "overwritten", assigned);
        ++failures;
    }
}

int main() {
    void* previous = set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(RecordConstraint));
    CheckConstraints(sscanf_s_nid_postfix, "sscanf_s: bad argument");
    CheckConstraints(ScanList, "vsscanf_s: bad argument");
    for (const auto scan : {sscanf_s_nid_postfix, ScanList}) {
        CheckCapacities(scan);
        CheckWideFields(scan);
        CheckFailures(scan);
    }
    for (const int count : {0, 3, 127}) {
        CheckCount<signed char>("hh", count);
        CheckCount<short>("h", count);
        CheckCount<int>("", count);
        CheckCount<std::int64_t>("l", count);
        CheckCount<long long>("ll", count);
        CheckCount<std::intmax_t>("j", count);
        CheckCount<std::int64_t>("z", count);
        CheckCount<std::ptrdiff_t>("t", count);
    }
    CheckCount<short>("h", 257);
    CheckCount<std::int64_t>("l", 257);

    int number = -1;
    std::int64_t before = -1;
    std::int64_t after = -1;
    char word[4] = {};
    Require(sscanf_s_nid_postfix(" 12 abc!", "%ln%d %3[a-z]%jn!", &before, &number,
        word, std::size_t{4}, &after) == 2, "Count conversions must not increase assignments");
    Require(before == 0 && number == 12 && std::strcmp(word, "abc") == 0 && after == 7,
        "Counts must include leading whitespace and earlier conversions");

    after = -1;
    Require(sscanf_s_nid_postfix("123x", "%*d%tn", &after) == 0 && after == 3,
        "Count must include suppressed input without consuming a capacity argument");
    after = -1;
    Require(sscanf_s_nid_postfix("x", "%d%lln", &number, &after) == 0 && after == -1,
        "Matching failure must leave a later count untouched");
    Require(sscanf_s_nid_postfix("", "%d%lln", &number, &after) == EOF && after == -1,
        "Input failure must leave a later count untouched");
    set_constraint_handler_s_nid_postfix(previous);
    return failures == 0 ? 0 : 1;
}
