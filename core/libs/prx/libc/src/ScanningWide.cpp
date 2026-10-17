#include "prx/libc/include/GuestLongDouble.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"

#ifdef _WIN32
#define APS5_VA_BEGIN(last) __builtin_sysv_va_list args; __builtin_sysv_va_start(args, last)
#define APS5_VA_END() __builtin_sysv_va_end(args)
#else
#define APS5_VA_BEGIN(last) std::va_list args; va_start(args, last)
#define APS5_VA_END() va_end(args)
#endif

extern "C" int APS5_VABI iswspace_nid_postfix(std::uint32_t c);
extern "C" std::size_t APS5_VABI wcrtomb_nid_postfix(char* destination, std::uint16_t value, void* state);

namespace {

enum class Fit { Invalid, Prefix, Valid };

enum class Outcome { Done, Matching, Input, Violation };

struct Directive {
    bool suppress = false;
    std::size_t width = 0;
    std::string length;
    char16_t conversion = 0;
    const char16_t* setBegin = nullptr;
    const char16_t* setEnd = nullptr;
    bool setExcluded = false;
};

bool IsSpace(char16_t character) {
    return iswspace_nid_postfix(character) != 0;
}

int DigitValue(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'z') return character - 'a' + 10;
    if (character >= 'A' && character <= 'Z') return character - 'A' + 10;
    return 36;
}

bool IsDecimal(char character) {
    return character >= '0' && character <= '9';
}

bool IsHex(char character) {
    return DigitValue(character) < 16;
}

char Lower(char character) {
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
}

bool StartsWord(std::string_view text, std::string_view word) {
    if (text.size() > word.size()) return false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (Lower(text[index]) != word[index]) return false;
    }
    return true;
}

std::string_view Unsigned(std::string_view text) {
    if (!text.empty() && (text[0] == '+' || text[0] == '-')) text.remove_prefix(1);
    return text;
}

Fit IntegerFit(std::string_view text, int base) {
    text = Unsigned(text);
    if (text.empty()) return Fit::Prefix;
    int radix = base;
    std::size_t index = 0;
    if ((base == 0 || base == 16) && text[0] == '0' && text.size() >= 2 && (text[1] == 'x' || text[1] == 'X')) {
        radix = 16;
        index = 2;
        if (index == text.size()) return Fit::Prefix;
    } else if (base == 0) {
        radix = text[0] == '0' ? 8 : 10;
    }
    for (; index < text.size(); ++index) {
        if (DigitValue(text[index]) >= radix) return Fit::Invalid;
    }
    return Fit::Valid;
}

Fit SpecialFloatFit(std::string_view text) {
    if (StartsWord(text, "infinity")) return text.size() == 3 || text.size() == 8 ? Fit::Valid : Fit::Prefix;
    if (StartsWord(text, "nan")) return text.size() == 3 ? Fit::Valid : Fit::Prefix;
    if (text.size() < 4 || !StartsWord(text.substr(0, 3), "nan") || text[3] != '(') return Fit::Invalid;
    for (std::size_t index = 4; index < text.size(); ++index) {
        const char character = text[index];
        if (character == ')') return index + 1 == text.size() ? Fit::Valid : Fit::Invalid;
        if (DigitValue(character) == 36 && character != '_') return Fit::Invalid;
    }
    return Fit::Prefix;
}

Fit FloatFit(std::string_view text) {
    text = Unsigned(text);
    if (text.empty()) return Fit::Prefix;
    if (Lower(text[0]) == 'i' || Lower(text[0]) == 'n') return SpecialFloatFit(text);
    const bool hex = text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const auto digit = [hex](char character) { return hex ? IsHex(character) : IsDecimal(character); };
    std::size_t index = hex ? 2 : 0;
    std::size_t digits = 0;
    for (; index < text.size() && digit(text[index]); ++index) ++digits;
    if (index < text.size() && text[index] == '.') {
        for (++index; index < text.size() && digit(text[index]); ++index) ++digits;
    }
    if (index == text.size()) return digits != 0 ? Fit::Valid : Fit::Prefix;
    if (digits == 0) return Fit::Invalid;
    if (Lower(text[index]) != (hex ? 'p' : 'e')) return Fit::Invalid;
    ++index;
    if (index < text.size() && (text[index] == '+' || text[index] == '-')) ++index;
    if (index == text.size()) return Fit::Prefix;
    for (; index < text.size(); ++index) {
        if (!IsDecimal(text[index])) return Fit::Invalid;
    }
    return Fit::Valid;
}

class WideScanner {
    const char* const name;
    const bool sized;
    const char16_t* const start;
    const char16_t* input;
    const char16_t* format;
    LibcDetail::FormatArguments args;
    int assigned = 0;
    bool converted = false;

public:
    WideScanner(const char* function, bool withSizes, const char16_t* buffer, const char16_t* formatText, const void* source)
        : name(function), sized(withSizes), start(buffer), input(buffer), format(formatText), args(source) {}

    int Run() {
        while (*format != 0) {
            Outcome outcome = Outcome::Done;
            if (IsSpace(*format)) {
                while (IsSpace(*format)) ++format;
                SkipSpace();
            } else if (*format != u'%') {
                outcome = Literal(*format++);
            } else {
                ++format;
                outcome = Conversion();
            }
            if (outcome == Outcome::Matching) return assigned;
            if (outcome == Outcome::Input) return converted ? assigned : EOF;
            if (outcome == Outcome::Violation) return EOF;
        }
        return assigned;
    }

private:
    [[noreturn]] void Fail(const std::string& message) const {
        throw std::invalid_argument(std::string(name) + ": " + message);
    }

    std::size_t IntegerSize(const std::string& length) const {
        if (length.empty()) return sizeof(std::int32_t);
        if (length == "hh") return sizeof(std::int8_t);
        if (length == "h") return sizeof(std::int16_t);
        if (length == "l" || length == "ll" || length == "j" || length == "z" || length == "t") return sizeof(std::int64_t);
        Fail("invalid integer length modifier " + length);
    }

    void SkipSpace() {
        while (*input != 0 && IsSpace(*input)) ++input;
    }

    Outcome Literal(char16_t expected) {
        if (*input == 0) return Outcome::Input;
        if (*input != expected) return Outcome::Matching;
        ++input;
        return Outcome::Done;
    }

    Directive Parse() {
        Directive directive;
        if (*format == u'*') {
            directive.suppress = true;
            ++format;
        }
        const char16_t* digits = format;
        for (; *format >= u'0' && *format <= u'9'; ++format) {
            const std::size_t digit = static_cast<std::size_t>(*format - u'0');
            const std::size_t limit = (std::numeric_limits<std::size_t>::max() - digit) / 10;
            directive.width = directive.width > limit ? std::numeric_limits<std::size_t>::max() : directive.width * 10 + digit;
        }
        if (format != digits && directive.width == 0) Fail("zero field width");
        if (*format == u'h' || *format == u'l') {
            directive.length += static_cast<char>(*format++);
            if (*format == directive.length[0]) directive.length += static_cast<char>(*format++);
        } else if (*format == u'j' || *format == u'z' || *format == u't' || *format == u'L') {
            directive.length += static_cast<char>(*format++);
        }
        directive.conversion = *format;
        if (directive.conversion == 0) Fail("incomplete conversion specification");
        ++format;
        if (directive.conversion == u'[') {
            directive.setExcluded = *format == u'^';
            if (directive.setExcluded) ++format;
            directive.setBegin = format;
            if (*format == u']') ++format;
            while (*format != 0 && *format != u']') ++format;
            if (*format == 0) Fail("unterminated scanset");
            directive.setEnd = format++;
            for (const char16_t* member = directive.setBegin + 1; member + 1 < directive.setEnd; ++member) {
                if (*member == u'-') Fail("scanset range is implementation-defined");
            }
        }
        return directive;
    }

    Outcome Conversion() {
        if (*format == u'%') {
            ++format;
            SkipSpace();
            return Literal(u'%');
        }
        const Directive directive = Parse();
        switch (directive.conversion) {
        case u'd': return Integer(directive, 10, true);
        case u'i': return Integer(directive, 0, true);
        case u'o': return Integer(directive, 8, false);
        case u'u': return Integer(directive, 10, false);
        case u'x':
        case u'X': return Integer(directive, 16, false);
        case u'a': case u'A': case u'e': case u'E':
        case u'f': case u'F': case u'g': case u'G': return Float(directive);
        case u's':
        case u'c':
        case u'[': return Text(directive);
        case u'n': return Count(directive);
        default: Fail("unsupported conversion " + std::to_string(directive.conversion));
        }
    }

    template<typename TFit>
    std::string Field(std::size_t width, TFit fit) const {
        std::string field;
        const std::size_t limit = width == 0 ? std::numeric_limits<std::size_t>::max() : width;
        while (field.size() < limit) {
            const char16_t next = input[field.size()];
            if (next == 0 || next >= 0x80) break;
            field.push_back(static_cast<char>(next));
            if (fit(field) == Fit::Invalid) {
                field.pop_back();
                break;
            }
        }
        return field;
    }

    Outcome Integer(const Directive& directive, int base, bool isSigned) {
        const std::size_t size = IntegerSize(directive.length);
        void* target = nullptr;
        if (!directive.suppress && (target = args.Next<void*>()) == nullptr) return Outcome::Violation;
        SkipSpace();
        if (*input == 0) return Outcome::Input;
        const std::string field = Field(directive.width, [base](std::string_view text) { return IntegerFit(text, base); });
        if (IntegerFit(field, base) != Fit::Valid) return Outcome::Matching;
        input += field.size();
        if (target != nullptr) {
            const std::uint64_t value = isSigned ? static_cast<std::uint64_t>(std::strtoll(field.c_str(), nullptr, base)) : std::strtoull(field.c_str(), nullptr, base);
            std::memcpy(target, &value, size);
            ++assigned;
        }
        converted = true;
        return Outcome::Done;
    }

    Outcome Float(const Directive& directive) {
        if (!directive.length.empty() && directive.length != "l" && directive.length != "L")
            Fail("invalid floating length modifier " + directive.length);
        void* target = nullptr;
        if (!directive.suppress && (target = args.Next<void*>()) == nullptr) return Outcome::Violation;
        SkipSpace();
        if (*input == 0) return Outcome::Input;
        const std::string field = Field(directive.width, FloatFit);
        if (FloatFit(field) != Fit::Valid) return Outcome::Matching;
        input += field.size();
        if (target != nullptr) {
            if (directive.length == "L") {
                LibcDetail::GuestLongDoubleStore(target, std::strtold(field.c_str(), nullptr));
            } else if (directive.length == "l") {
                const double value = std::strtod(field.c_str(), nullptr);
                std::memcpy(target, &value, sizeof(value));
            } else {
                const float value = std::strtof(field.c_str(), nullptr);
                std::memcpy(target, &value, sizeof(value));
            }
            ++assigned;
        }
        converted = true;
        return Outcome::Done;
    }

    static bool InSet(const Directive& directive, char16_t character) {
        bool found = false;
        for (const char16_t* member = directive.setBegin; member != directive.setEnd && !found; ++member) found = *member == character;
        return found != directive.setExcluded;
    }

    static Outcome Truncated(void* target, std::size_t capacity, bool wide) {
        if (capacity != 0) {
            if (wide) static_cast<char16_t*>(target)[0] = 0;
            else static_cast<char*>(target)[0] = 0;
        }
        return Outcome::Matching;
    }

    Outcome Text(const Directive& directive) {
        if (!directive.length.empty() && directive.length != "l")
            Fail("invalid string length modifier " + directive.length);
        void* target = nullptr;
        std::size_t capacity = 0;
        if (!directive.suppress) {
            target = args.Next<void*>();
            if (sized) capacity = args.Next<unsigned int>();
            if (target == nullptr) return Outcome::Violation;
        }
        if (directive.conversion == u's') SkipSpace();
        if (*input == 0) return Outcome::Input;
        std::u16string item;
        if (directive.conversion == u'c') {
            const std::size_t count = directive.width == 0 ? 1 : directive.width;
            while (item.size() < count && input[item.size()] != 0) item.push_back(input[item.size()]);
            if (item.size() < count) return Outcome::Matching;
        } else {
            const std::size_t limit = directive.width == 0 ? std::numeric_limits<std::size_t>::max() : directive.width;
            while (item.size() < limit && input[item.size()] != 0) {
                const char16_t next = input[item.size()];
                if (directive.conversion == u's' ? IsSpace(next) : !InSet(directive, next)) break;
                item.push_back(next);
            }
            if (item.empty()) return Outcome::Matching;
        }
        input += item.size();
        if (target == nullptr) {
            converted = true;
            return Outcome::Done;
        }
        const std::size_t terminator = directive.conversion == u'c' ? 0 : 1;
        if (directive.length == "l") {
            if (sized && item.size() + terminator > capacity) return Truncated(target, capacity, true);
            std::memcpy(target, item.data(), item.size() * sizeof(char16_t));
            if (terminator != 0) static_cast<char16_t*>(target)[item.size()] = 0;
        } else {
            std::string bytes;
            for (const char16_t unit : item) {
                char byte[8];
                const std::size_t written = wcrtomb_nid_postfix(byte, unit, nullptr);
                if (written == static_cast<std::size_t>(-1)) Fail("wide character has no single-byte form");
                bytes.append(byte, written);
            }
            if (sized && bytes.size() + terminator > capacity) return Truncated(target, capacity, false);
            std::memcpy(target, bytes.data(), bytes.size());
            if (terminator != 0) static_cast<char*>(target)[bytes.size()] = 0;
        }
        ++assigned;
        converted = true;
        return Outcome::Done;
    }

    Outcome Count(const Directive& directive) {
        if (directive.suppress || directive.width != 0) Fail("%n with suppression or width");
        const std::size_t size = IntegerSize(directive.length);
        void* target = args.Next<void*>();
        if (target == nullptr) return Outcome::Violation;
        const std::int64_t count = input - start;
        std::memcpy(target, &count, size);
        return Outcome::Done;
    }
};

}

extern "C" {

int APS5_VABI swscanf_nid_postfix(const char16_t* buffer, const char16_t* format, ...) {
    if (buffer == nullptr || format == nullptr) return EOF;
    APS5_VA_BEGIN(format);
    try {
        const int result = WideScanner("swscanf", false, buffer, format, args).Run();
        APS5_VA_END();
        return result;
    } catch (...) {
        APS5_VA_END();
        throw;
    }
}

int APS5_VABI swscanf_s_nid_postfix(const char16_t* buffer, const char16_t* format, ...) {
    if (buffer == nullptr || format == nullptr) return EOF;
    APS5_VA_BEGIN(format);
    try {
        const int result = WideScanner("swscanf_s", true, buffer, format, args).Run();
        APS5_VA_END();
        return result;
    } catch (...) {
        APS5_VA_END();
        throw;
    }
}

}
