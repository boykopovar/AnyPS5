#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSFORMATTING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSFORMATTING_HPP

#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <climits>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <type_traits>

extern "C" int APS5_VABI strerror_r_nid_postfix(int error, char* buffer, std::size_t length);
extern "C" std::FILE* GuestStandardOutputFile_nid_no_patch();

namespace LibcDetail {

enum class ArgumentClass : unsigned char { Integer, Double, LongDouble };

class FormatConstraintViolation : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class FormatArguments {
    VaList args;
    std::vector<std::array<unsigned char, 16>> numbered;
    size_t position = 1;

public:
    explicit FormatArguments(const void* source) {
        std::memcpy(&args, source, sizeof(args));
    }

    void Number(const std::vector<ArgumentClass>& classes) {
        numbered.resize(classes.size());
        for (size_t index = 0; index < classes.size(); ++index) {
            const auto store = [&](auto value) { std::memcpy(numbered[index].data(), &value, sizeof(value)); };
            if (classes[index] == ArgumentClass::LongDouble) store(Fetch<long double>());
            else if (classes[index] == ArgumentClass::Double) store(Fetch<double>());
            else store(Fetch<unsigned long long>());
        }
    }

    void Seek(size_t index) { position = index; }

    template<class T> T At(size_t index) {
        if (index == 0 || index > numbered.size()) throw std::out_of_range("Format argument position out of range");
        T value;
        std::memcpy(&value, numbered[index - 1].data(), sizeof(value));
        return value;
    }

    template<class T> T Next() {
        return numbered.empty() ? Fetch<T>() : At<T>(position++);
    }

private:
    template<class T> T Fetch() {
        const void* address;
        if constexpr (std::is_same_v<T, double>) {
            if (args.fp_offset < 176) {
                address = static_cast<const char*>(args.reg_save_area) + args.fp_offset;
                args.fp_offset += 16;
            } else {
                address = args.overflow_arg_area;
                args.overflow_arg_area = static_cast<char*>(args.overflow_arg_area) + 8;
            }
        } else if constexpr (std::is_same_v<T, long double>) {
            const auto aligned = (reinterpret_cast<std::uintptr_t>(args.overflow_arg_area) + 15) & ~std::uintptr_t(15);
            address = reinterpret_cast<const void*>(aligned);
            args.overflow_arg_area = reinterpret_cast<void*>(aligned + 16);
        } else {
            static_assert(sizeof(T) <= 8);
            if (args.gp_offset < 48) {
                address = static_cast<const char*>(args.reg_save_area) + args.gp_offset;
                args.gp_offset += 8;
            } else {
                address = args.overflow_arg_area;
                args.overflow_arg_area = static_cast<char*>(args.overflow_arg_area) + 8;
            }
        }
        T value;
        std::memcpy(&value, address, sizeof(value));
        return value;
    }

};

class FormatOutput {
    char* destination;
    size_t capacity;
    size_t count = 0;
    std::string* complete;

public:
    FormatOutput(char* buffer, size_t size, std::string* text) : destination(buffer), capacity(size), complete(text) {
        if (size && !buffer) throw std::invalid_argument("Null formatting buffer");
        if (capacity) destination[0] = 0;
    }

    void Append(const char* text, size_t size) {
        if (size > static_cast<size_t>(INT_MAX) - count)
            throw std::overflow_error("Formatted output exceeds INT_MAX");
        if (capacity && count < capacity - 1) {
            const size_t available = capacity - 1 - count;
            const size_t copied = size < available ? size : available;
            std::memcpy(destination + count, text, copied);
            destination[count + copied] = 0;
        }
        if (complete) complete->append(text, size);
        count += size;
    }

    template<class T> void Value(const std::string& format, T value) {
        const int size = std::snprintf(nullptr, 0, format.c_str(), value);
        if (size < 0) throw std::runtime_error("Formatting conversion failed");
        if (static_cast<size_t>(size) > static_cast<size_t>(INT_MAX) - count)
            throw std::overflow_error("Formatted output exceeds INT_MAX");
        if (complete) {
            std::vector<char> text(static_cast<size_t>(size) + 1);
            const int written = std::snprintf(text.data(), text.size(), format.c_str(), value);
            if (written != size) throw std::runtime_error("Inconsistent formatting conversion");
            complete->append(text.data(), static_cast<size_t>(size));
        }
        if (capacity && count < capacity - 1) {
            const size_t remaining = capacity - count;
            const size_t required = static_cast<size_t>(size) + 1;
            const int written = std::snprintf(destination + count, remaining < required ? remaining : required, format.c_str(), value);
            if (written != size) throw std::runtime_error("Inconsistent formatting conversion");
        }
        count += static_cast<size_t>(size);
    }

    int Count() const { return static_cast<int>(count); }
};

// The guest's wchar_t is 16 bits (UTF-16, as on the PS4/PS5 toolchain); wide conversions are
// formatted as UTF-8 through %s.
inline void AppendUtf8(std::string& utf8, char32_t code) {
    if (code > 0x10ffff) code = 0xfffd;
    if (code < 0x80) {
        utf8 += static_cast<char>(code);
    } else if (code < 0x800) {
        utf8 += static_cast<char>(0xc0 | (code >> 6));
        utf8 += static_cast<char>(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
        utf8 += static_cast<char>(0xe0 | (code >> 12));
        utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        utf8 += static_cast<char>(0x80 | (code & 0x3f));
    } else {
        utf8 += static_cast<char>(0xf0 | (code >> 18));
        utf8 += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
        utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        utf8 += static_cast<char>(0x80 | (code & 0x3f));
    }
}

struct FloatSpec {
    bool left = false;
    bool plus = false;
    bool space = false;
    bool alternate = false;
    bool zero = false;
    std::size_t width = 0;
    int precision = -1;
};

inline FloatSpec ParseFloatSpec(const std::string& spec) {
    FloatSpec result;
    std::size_t index = 1;
    for (; index < spec.size() && std::strchr("-+ #0", spec[index]); ++index) {
        result.left |= spec[index] == '-';
        result.plus |= spec[index] == '+';
        result.space |= spec[index] == ' ';
        result.alternate |= spec[index] == '#';
        result.zero |= spec[index] == '0';
    }
    for (; index < spec.size() && spec[index] >= '0' && spec[index] <= '9'; ++index) {
        const auto digit = static_cast<std::size_t>(spec[index] - '0');
        if (result.width > (static_cast<std::size_t>(INT_MAX) - digit) / 10)
            throw std::overflow_error("Formatted output exceeds INT_MAX");
        result.width = result.width * 10 + digit;
    }
    if (index < spec.size() && spec[index] == '.') {
        result.precision = 0;
        for (++index; index < spec.size(); ++index) {
            const int digit = spec[index] - '0';
            if (result.precision > (INT_MAX - digit) / 10)
                throw std::overflow_error("Floating precision exceeds int range");
            result.precision = result.precision * 10 + digit;
        }
    }
    return result;
}

inline std::string PadFloat(const FloatSpec& spec, const std::string& prefix, const std::string& body) {
    const std::size_t length = prefix.size() + body.size();
    if (length >= spec.width) return prefix + body;
    const std::string padding(spec.width - length, spec.left || !spec.zero ? ' ' : '0');
    if (spec.left) return prefix + body + padding;
    return spec.zero ? prefix + padding + body : padding + prefix + body;
}

inline std::string HexDigits(const std::string& spec, char conversion, bool negative, std::uint64_t lead, std::uint64_t fraction, int total, int exponent,
    std::size_t maximumLength) {
    const FloatSpec parsed = ParseFloatSpec(spec);
    const int precision = parsed.precision;
    const bool upper = conversion == 'A';
    int digits = total;
    if (precision >= 0 && precision < total) {
        const int dropped = (total - precision) * 4;
        const std::uint64_t remainder = dropped == 64 ? fraction : fraction & ((std::uint64_t{1} << dropped) - 1);
        fraction = dropped == 64 ? 0 : fraction >> dropped;
        if (remainder >= std::uint64_t{1} << (dropped - 1) && ++fraction >> (precision * 4)) {
            fraction &= (std::uint64_t{1} << (precision * 4)) - 1;
            ++lead;
        }
        digits = precision;
    } else if (precision < 0) {
        for (; digits > 0 && (fraction & 0xf) == 0; --digits) fraction >>= 4;
    }
    const char* hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string suffix = upper ? "P" : "p";
    suffix += exponent < 0 ? '-' : '+';
    suffix += std::to_string(exponent < 0 ? -exponent : exponent);
    std::string prefix = negative ? "-" : parsed.plus ? "+" : parsed.space ? " " : "";
    prefix += upper ? "0X" : "0x";
    const auto fractionalDigits = static_cast<std::size_t>(precision > total ? precision : digits);
    const bool point = fractionalDigits != 0 || parsed.alternate;
    const std::size_t length = prefix.size() + 1 + point + fractionalDigits + suffix.size();
    if (length > maximumLength || parsed.width > maximumLength)
        throw std::overflow_error("Formatted output exceeds INT_MAX");
    std::string body(1, hex[lead]);
    if (point) body += '.';
    for (int digit = digits - 1; digit >= 0; --digit) body += hex[(fraction >> (digit * 4)) & 0xf];
    if (precision > total) body.append(static_cast<std::size_t>(precision - total), '0');
    body += suffix;
    return PadFloat(parsed, prefix, body);
}

inline std::string HexFloat(const std::string& spec, char conversion, double value, std::size_t maximumLength = INT_MAX) {
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    int exponent = static_cast<int>((bits >> 52) & 0x7ff);
    std::uint64_t fraction = bits & ((std::uint64_t{1} << 52) - 1);
    std::uint64_t lead = 1;
    if (exponent != 0) {
        exponent -= 1023;
    } else if (fraction == 0) {
        lead = 0;
    } else {
        exponent = -1022;
        while ((fraction & (std::uint64_t{1} << 52)) == 0) {
            fraction <<= 1;
            --exponent;
        }
        fraction &= (std::uint64_t{1} << 52) - 1;
    }
    return HexDigits(spec, conversion, (bits >> 63) != 0, lead, fraction, 13, exponent, maximumLength);
}

inline std::string HexFloat(const std::string& spec, char conversion, long double value, std::size_t maximumLength = INT_MAX) {
    if (value == 0) return HexDigits(spec, conversion, std::signbit(value), 0, 0, 16, 0, maximumLength);
    int exponent = 0;
    const auto mantissa = static_cast<std::uint64_t>(std::ldexp(std::frexp(std::fabs(value), &exponent), 64));
    return HexDigits(spec, conversion, std::signbit(value), 1, mantissa << 1, 16, exponent - 1, maximumLength);
}

struct DecimalDigits {
    std::string digits;
    long long exponent = 0;
    bool truncated = false;
    bool tiesDown = false;
};

inline std::string DecimalChunks(std::vector<std::uint32_t> words) {
    std::string text;
    while (!words.empty()) {
        std::uint64_t remainder = 0;
        for (std::size_t index = words.size(); index-- > 0;) {
            const std::uint64_t current = (remainder << 32) | words[index];
            words[index] = static_cast<std::uint32_t>(current / 1000000000u);
            remainder = current % 1000000000u;
        }
        while (!words.empty() && words.back() == 0) words.pop_back();
        std::string chunk = std::to_string(remainder);
        if (!words.empty()) chunk.insert(0, 9 - chunk.size(), '0');
        text.insert(0, chunk);
    }
    return text;
}

inline DecimalDigits ConsoleDecimalDigits(long double value) {
    DecimalDigits result;
    if (value == 0) return result;
    int binary = 0;
    const auto mantissa = static_cast<std::uint64_t>(std::ldexp(std::frexp(std::fabs(value), &binary), 64));
    const long long estimate = static_cast<long long>(binary) * 30103 / 100000;
    const long long last = (estimate >= 0 ? estimate / 4 : -((3 - estimate) / 4)) * 4 - 20;
    result.tiesDown = estimate >= 8;
    binary -= 64;
    std::string integer;
    std::string fraction;
    bool remaining = false;
    if (binary >= 0) {
        const auto word = static_cast<std::size_t>(binary) / 32;
        const auto bit = static_cast<unsigned>(binary) % 32;
        std::vector<std::uint32_t> words(word + 3, 0);
        for (std::size_t part = 0; part < 2; ++part) {
            const std::uint64_t shifted = ((mantissa >> (32 * part)) & 0xffffffffu) << bit;
            words[word + part] |= static_cast<std::uint32_t>(shifted);
            words[word + part + 1] |= static_cast<std::uint32_t>(shifted >> 32);
        }
        while (!words.empty() && words.back() == 0) words.pop_back();
        integer = DecimalChunks(std::move(words));
    } else {
        const auto shift = static_cast<std::size_t>(-static_cast<long long>(binary));
        const std::uint64_t whole = shift >= 64 ? 0 : mantissa >> shift;
        if (whole != 0) integer = std::to_string(whole);
        std::vector<std::uint32_t> words(shift / 32 + 2, 0);
        const std::uint64_t remainder = shift >= 64 ? mantissa : mantissa & ((std::uint64_t{1} << shift) - 1);
        words[0] = static_cast<std::uint32_t>(remainder);
        words[1] = static_cast<std::uint32_t>(remainder >> 32);
        const std::size_t top = shift / 32;
        const auto bit = static_cast<unsigned>(shift % 32);
        const auto significant = [&] {
            const std::size_t leading = fraction.find_first_not_of('0');
            return integer.size() + (leading == std::string::npos ? 0 : fraction.size() - leading);
        };
        remaining = remainder != 0;
        while (remaining && significant() < 30) {
            std::uint64_t carry = 0;
            for (auto& word : words) {
                const std::uint64_t product = static_cast<std::uint64_t>(word) * 1000000000u + carry;
                word = static_cast<std::uint32_t>(product);
                carry = product >> 32;
            }
            const std::uint64_t window = (static_cast<std::uint64_t>(words[top + 1]) << 32) | words[top];
            const std::string chunk = std::to_string(static_cast<std::uint32_t>(window >> bit));
            words[top + 1] = 0;
            words[top] &= bit == 0 ? 0 : (std::uint32_t{1} << bit) - 1;
            fraction += std::string(9 - chunk.size(), '0') + chunk;
            remaining = std::any_of(words.begin(), words.end(), [](std::uint32_t word) { return word != 0; });
        }
    }
    if (!integer.empty()) {
        result.exponent = static_cast<long long>(integer.size()) - 1;
        result.digits = integer + fraction;
    } else {
        const std::size_t leading = fraction.find_first_not_of('0');
        result.exponent = -static_cast<long long>(leading) - 1;
        result.digits = fraction.substr(leading);
    }
    const long long kept = result.exponent - last + 1;
    if (static_cast<long long>(result.digits.size()) > kept) {
        result.truncated = result.digits.find_first_not_of('0', static_cast<std::size_t>(kept)) != std::string::npos;
        result.digits.resize(static_cast<std::size_t>(kept));
    }
    result.truncated |= remaining;
    return result;
}

inline DecimalDigits RoundDecimal(const DecimalDigits& value, long long count) {
    if (value.digits.empty()) return value;
    if (count <= 0) {
        if (count == 0 && value.digits[0] >= '5') return {"1", value.exponent + 1};
        return {};
    }
    if (count >= static_cast<long long>(value.digits.size())) return value;
    DecimalDigits result{value.digits.substr(0, static_cast<std::size_t>(count)), value.exponent};
    const auto next = static_cast<std::size_t>(count);
    if (value.digits[next] < '5') return result;
    if (value.digits[next] == '5' && value.tiesDown && !value.truncated && value.digits.find_first_not_of('0', next + 1) == std::string::npos)
        return result;
    for (std::size_t index = result.digits.size(); index-- > 0;) {
        if (result.digits[index] != '9') {
            ++result.digits[index];
            return result;
        }
        result.digits[index] = '0';
    }
    result.digits.insert(result.digits.begin(), '1');
    result.digits.pop_back();
    ++result.exponent;
    return result;
}

inline char DigitAt(const DecimalDigits& value, long long position) {
    const long long index = value.exponent - position;
    if (value.digits.empty() || index < 0 || index >= static_cast<long long>(value.digits.size())) return '0';
    return value.digits[static_cast<std::size_t>(index)];
}

inline std::string DecimalFloat(const std::string& spec, char conversion, long double value, std::size_t maximumLength = INT_MAX) {
    const FloatSpec parsed = ParseFloatSpec(spec);
    const DecimalDigits exact = ConsoleDecimalDigits(value);
    const bool upper = conversion == 'E' || conversion == 'G';
    const char lower = static_cast<char>(conversion | 0x20);
    long long precision = parsed.precision < 0 ? 6 : parsed.precision;
    bool exponential = lower == 'e';
    bool strip = false;
    if (lower == 'g') {
        if (precision == 0) precision = 1;
        const long long exponent = exact.digits.empty() ? 0 : RoundDecimal(exact, precision).exponent;
        exponential = !(precision > exponent && exponent >= -4);
        precision = exponential ? precision - 1 : precision - 1 - exponent;
        strip = !parsed.alternate;
    }
    const DecimalDigits rounded = RoundDecimal(exact, exponential ? precision + 1 : exact.exponent + 1 + precision);
    const long long exponent = rounded.digits.empty() ? 0 : rounded.exponent;
    if (strip) {
        const long long last = rounded.digits.empty() ? 0 : rounded.exponent - static_cast<long long>(rounded.digits.size()) + 1;
        long long needed = exponential ? exponent - last : -last;
        while (needed > 0 && DigitAt(rounded, (exponential ? exponent : 0) - needed) == '0') --needed;
        precision = std::max(0LL, std::min(precision, needed));
    }
    const std::string prefix = std::signbit(value) ? "-" : parsed.plus ? "+" : parsed.space ? " " : "";
    const bool point = precision > 0 || parsed.alternate;
    const std::string magnitude = std::to_string(exponent < 0 ? -exponent : exponent);
    const long long whole = exponential || exponent < 0 ? 1 : exponent + 1;
    const long long exponentLength = exponential ? 2 + static_cast<long long>(std::max<std::size_t>(magnitude.size(), 2)) : 0;
    const long long length = static_cast<long long>(prefix.size()) + whole + point + precision + exponentLength;
    if (length > static_cast<long long>(maximumLength) || parsed.width > maximumLength)
        throw std::overflow_error("Formatted output exceeds INT_MAX");
    std::string body;
    body.reserve(static_cast<std::size_t>(length));
    const long long first = exponential ? exponent : std::max(exponent, 0LL);
    for (long long position = first; position >= (exponential ? exponent : 0); --position) body += DigitAt(rounded, position);
    if (point) body += '.';
    const long long start = exponential ? exponent - 1 : -1;
    for (long long position = start; position > start - precision; --position) body += DigitAt(rounded, position);
    if (exponential) {
        body += upper ? 'E' : 'e';
        body += exponent < 0 ? '-' : '+';
        if (magnitude.size() < 2) body += '0';
        body += magnitude;
    }
    return PadFloat(parsed, prefix, body);
}

inline std::string NanSpec(const std::string& spec) {
    std::string result = "%";
    std::size_t index = 1;
    for (; index < spec.size() && std::strchr("-+ #0", spec[index]); ++index)
        if (spec[index] == '-' && result.size() == 1) result += '-';
    for (; index < spec.size() && spec[index] >= '0' && spec[index] <= '9'; ++index) result += spec[index];
    return result + 's';
}

inline std::string FormatPointer(const std::string& spec, const void* pointer) {
    std::size_t index = 1;
    bool left = false;
    bool zero = false;
    for (; index < spec.size() && std::strchr("-+ #0", spec[index]); ++index) {
        if (spec[index] == '-') left = true;
        if (spec[index] == '0') zero = true;
    }
    const auto readNumber = [&] {
        std::size_t number = 0;
        for (; index < spec.size() && spec[index] >= '0' && spec[index] <= '9'; ++index) {
            number = number * 10 + static_cast<std::size_t>(spec[index] - '0');
            if (number > static_cast<std::size_t>(INT_MAX)) throw std::overflow_error("Formatted output exceeds INT_MAX");
        }
        return number;
    };
    const std::size_t width = readNumber();
    bool hasPrecision = false;
    std::size_t precision = 0;
    if (index < spec.size() && spec[index] == '.') {
        ++index;
        hasPrecision = true;
        precision = readNumber();
    }
    const auto value = static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(pointer));
    std::string digits;
    for (unsigned long long rest = value; rest != 0; rest >>= 4) digits.insert(digits.begin(), "0123456789abcdef"[rest & 15]);
    if (digits.empty() && !(hasPrecision && precision == 0)) digits = "0";
    if (digits.size() < precision) digits.insert(0, precision - digits.size(), '0');
    const std::size_t padding = width > digits.size() ? width - digits.size() : 0;
    if (left) return digits + std::string(padding, ' ');
    if (zero && !hasPrecision) return std::string(padding, '0') + digits;
    return std::string(padding, ' ') + digits;
}

constexpr size_t maximumArgumentPosition = 99;

inline size_t ArgumentPosition(const char*& format) {
    const char* end = format;
    while (*end >= '0' && *end <= '9') ++end;
    if (end == format || *end != '$') return 0;
    size_t position = 0;
    for (const char* digit = format; digit != end && position <= maximumArgumentPosition; ++digit)
        position = position * 10 + static_cast<size_t>(*digit - '0');
    if (position == 0 || position > maximumArgumentPosition) throw std::invalid_argument("Format argument position out of range");
    format = end + 1;
    return position;
}

inline std::vector<ArgumentClass> NumberedArgumentClasses(const char* format) {
    if (!std::strchr(format, '$')) return {};
    std::vector<ArgumentClass> classes;
    bool numbered = false;
    size_t next = 1;
    const auto take = [&](size_t position, ArgumentClass type) {
        if (classes.size() < position) classes.resize(position);
        classes[position - 1] = type;
    };
    while ((format = std::strchr(format, '%')) != nullptr) {
        ++format;
        if (*format == '%') {
            ++format;
            continue;
        }
        if (const size_t position = ArgumentPosition(format)) {
            next = position;
            numbered = true;
        }
        while (*format && std::strchr("-+ #0'123456789.*", *format)) {
            if (*format++ != '*') continue;
            const size_t position = ArgumentPosition(format);
            if (position) numbered = true;
            take(position ? position : next++, ArgumentClass::Integer);
        }
        bool longDouble = false;
        while (*format && std::strchr("hljztL", *format)) longDouble = *format++ == 'L';
        const char conversion = *format;
        if (!conversion) break;
        ++format;
        if (std::strchr("aAeEfFgG", conversion)) take(next++, longDouble ? ArgumentClass::LongDouble : ArgumentClass::Double);
        else if (std::strchr("diouxXcCsSpn", conversion)) take(next++, ArgumentClass::Integer);
    }
    if (!numbered) classes.clear();
    return classes;
}

inline int FormatWindows(char* buffer, size_t size, const char* format, const void* source, std::string* complete = nullptr, bool secure = false) {
    if (!format || !source) throw std::invalid_argument("Null formatting argument");
    const int savedError = errno;
    const char* const formatStart = format;
    FormatArguments args(source);
    args.Number(NumberedArgumentClasses(format));
    FormatOutput output(buffer, size, complete);
    const auto star = [&] {
        const size_t position = ArgumentPosition(format);
        return position ? args.At<int>(position) : args.Next<int>();
    };
    while (*format) {
        const char* literal = format;
        while (*format && *format != '%') ++format;
        output.Append(literal, static_cast<size_t>(format - literal));
        if (!*format) break;
        ++format;
        if (*format == '%') {
            output.Append(format++, 1);
            continue;
        }
        if (const size_t position = ArgumentPosition(format)) args.Seek(position);
        std::string spec = "%";
        for (; *format && std::strchr("-+ #0'", *format); ++format)
            if (*format != '\'') spec += *format;
        if (*format == '*') {
            ++format;
            const int width = star();
            if (width < 0) spec += '-';
            spec += std::to_string(width < 0 ? -static_cast<long long>(width) : width);
        } else {
            while (*format >= '0' && *format <= '9') spec += *format++;
        }
        size_t precisionLimit = std::numeric_limits<size_t>::max();
        if (*format == '.') {
            ++format;
            if (*format == '*') {
                ++format;
                const int precision = star();
                if (precision >= 0) {
                    spec += "." + std::to_string(precision);
                    precisionLimit = static_cast<size_t>(precision);
                }
            } else {
                spec += '.';
                precisionLimit = 0;
                while (*format >= '0' && *format <= '9') {
                    const auto digit = static_cast<size_t>(*format - '0');
                    precisionLimit = precisionLimit <= (std::numeric_limits<size_t>::max() - digit) / 10
                        ? precisionLimit * 10 + digit : std::numeric_limits<size_t>::max();
                    spec += *format++;
                }
            }
        }
        std::string length;
        if (*format && std::strchr("hljztL", *format)) {
            length += *format++;
            if ((length == "h" && *format == 'h') || (length == "l" && *format == 'l'))
                length += *format++;
        }
        const char conversion = *format;
        if (!conversion) throw std::invalid_argument("Incomplete format conversion");
        ++format;
        const bool integerLength = length.empty() || length == "h" || length == "hh" ||
            length == "l" || length == "ll" || length == "j" || length == "z" || length == "t";
        if (conversion == 'd' || conversion == 'i') {
            if (!integerLength) throw std::invalid_argument("Invalid integer length");
            long long value;
            if (length.empty() || length == "h" || length == "hh") {
                value = args.Next<int>();
                if (length == "h") value = static_cast<short>(value);
                if (length == "hh") value = static_cast<signed char>(value);
            } else value = args.Next<long long>();
            output.Value(spec + "ll" + conversion, value);
        } else if (std::strchr("ouxX", conversion)) {
            if (!integerLength) throw std::invalid_argument("Invalid integer length");
            for (size_t flag = 1; flag < spec.size() && std::strchr("-+ #0", spec[flag]);) {
                if (spec[flag] == ' ' || spec[flag] == '+') spec.erase(flag, 1);
                else ++flag;
            }
            unsigned long long value;
            if (length.empty() || length == "h" || length == "hh") {
                value = args.Next<unsigned int>();
                if (length == "h") value = static_cast<unsigned short>(value);
                if (length == "hh") value = static_cast<unsigned char>(value);
            } else value = args.Next<unsigned long long>();
            if (conversion == 'o' && value == 0 && precisionLimit == 0 && spec.find('#') != std::string::npos)
                spec = spec.substr(0, spec.find('.')) + ".1";
            output.Value(spec + "ll" + conversion, value);
        } else if (std::strchr("aAeEfFgG", conversion)) {
            const char* nan = conversion >= 'a' ? "nan" : "NAN";
            if (length == "L") {
                static_assert(sizeof(long double) == 16);
                static_assert(std::numeric_limits<long double>::digits == 64);
                const long double value = args.Next<long double>();
                if (std::isnan(value)) {
                    output.Value(NanSpec(spec), nan);
                } else if (std::isfinite(value)) {
                    const auto remaining = static_cast<std::size_t>(INT_MAX - output.Count());
                    const std::string text = conversion == 'a' || conversion == 'A' ? HexFloat(spec, conversion, value, remaining) : DecimalFloat(spec, conversion, value, remaining);
                    output.Append(text.data(), text.size());
                } else {
                    output.Value(spec + "L" + conversion, value);
                }
            } else {
                if (!length.empty() && length != "l") throw std::invalid_argument("Invalid floating length");
                const double value = args.Next<double>();
                if (std::isnan(value)) {
                    output.Value(NanSpec(spec), nan);
                } else if ((conversion == 'a' || conversion == 'A') && std::isfinite(value)) {
                    const std::string text = HexFloat(spec, conversion, value, static_cast<std::size_t>(INT_MAX - output.Count()));
                    output.Append(text.data(), text.size());
                } else if (std::isfinite(value)) {
                    const std::string text = DecimalFloat(spec, conversion, value, static_cast<std::size_t>(INT_MAX - output.Count()));
                    output.Append(text.data(), text.size());
                } else {
                    output.Value(spec + conversion, value);
                }
            }
        } else if (conversion == 'c' && (length.empty() || length == "h")) {
            output.Value(spec + conversion, args.Next<int>());
        } else if ((conversion == 'c' && length == "l") || (conversion == 'C' && length.empty())) {
            std::string utf8;
            AppendUtf8(utf8, static_cast<char16_t>(args.Next<unsigned int>()));
            if (utf8[0] == 0) output.Value(spec + 'c', 0);
            else output.Value(spec + 's', utf8.c_str());
        } else if (conversion == 's' && (length.empty() || length == "h")) {
            const char* value = args.Next<const char*>();
            if (!value && secure) throw FormatConstraintViolation("Null string argument");
            if (!value) value = "(null)";
            output.Value(spec + 's', value);
        } else if ((conversion == 's' && length == "l") || (conversion == 'S' && length.empty())) {
            const char16_t* value = args.Next<const char16_t*>();
            if (!value && secure) throw FormatConstraintViolation("Null string argument");
            if (!value) value = u"(null)";
            std::string utf8;
            while (utf8.size() < precisionLimit && *value) {
                char32_t code = *value++;
                if (code >= 0xd800 && code < 0xdc00 && *value >= 0xdc00 && *value < 0xe000) code = 0x10000 + ((code - 0xd800) << 10) + (*value++ - 0xdc00);
                const auto previousSize = utf8.size();
                AppendUtf8(utf8, code);
                if (utf8.size() > precisionLimit) {
                    utf8.resize(previousSize);
                    break;
                }
            }
            output.Value(spec + 's', utf8.c_str());
        } else if (conversion == 'p' && (length.empty() || length == "l")) {
            const std::string text = FormatPointer(spec, args.Next<void*>());
            output.Append(text.data(), text.size());
        } else if (conversion == 'm') {
            char message[128];
            strerror_r_nid_postfix(savedError, message, sizeof(message));
            output.Value(spec + 's', static_cast<const char*>(message));
        } else if (conversion == 'n' && secure) {
            throw FormatConstraintViolation("Format count conversion");
        } else if (conversion == 'n' && integerLength && spec == "%") {
            void* pointer = args.Next<void*>();
            if (!pointer) throw std::invalid_argument("Null format count pointer");
            const int count = output.Count();
            if (length == "hh") *static_cast<signed char*>(pointer) = static_cast<signed char>(count);
            else if (length == "h") *static_cast<short*>(pointer) = static_cast<short>(count);
            else if (length.empty()) *static_cast<int*>(pointer) = count;
            else *static_cast<long long*>(pointer) = count;
        } else {
            throw std::invalid_argument("Unsupported format conversion '" + spec + length + conversion + "' in \"" + std::string(formatStart).substr(0, 160) + "\"");
        }
    }
    return output.Count();
}

inline int PrintWindows(const char* format, const void* args) {
    std::string buffer;
    const int size = FormatWindows(nullptr, 0, format, args, &buffer);
    if (std::fwrite(buffer.data(), 1, static_cast<size_t>(size), GuestStandardOutputFile_nid_no_patch()) != static_cast<size_t>(size))
        throw std::runtime_error("Formatted output write failed");
    return size;
}

}

#endif
