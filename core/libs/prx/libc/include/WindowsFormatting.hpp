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
#include <string_view>
#include <vector>
#include <type_traits>

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
        if (size == 0) return;
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

    void Pad(char ch, size_t repeat) {
        if (repeat == 0) return;
        if (repeat > static_cast<size_t>(INT_MAX) - count)
            throw std::overflow_error("Formatted output exceeds INT_MAX");
        if (capacity && count < capacity - 1) {
            const size_t available = capacity - 1 - count;
            const size_t written = repeat < available ? repeat : available;
            std::memset(destination + count, ch, written);
            destination[count + written] = 0;
        }
        if (complete) complete->append(repeat, ch);
        count += repeat;
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

struct FormattedField {
    std::string_view prefix{};
    size_t leadingZeros = 0;
    std::string_view body{};
    size_t trailingZeros = 0;
    std::string_view suffix{};
    size_t width = 0;
    bool left = false;
    bool zero = false;
};

inline void EmitField(FormatOutput& output, const FormattedField& field) {
    if (field.prefix.size() > static_cast<size_t>(INT_MAX) ||
        field.leadingZeros > static_cast<size_t>(INT_MAX) - field.prefix.size() ||
        field.body.size() > static_cast<size_t>(INT_MAX) - field.prefix.size() - field.leadingZeros ||
        field.trailingZeros > static_cast<size_t>(INT_MAX) - field.prefix.size() - field.leadingZeros - field.body.size() ||
        field.suffix.size() > static_cast<size_t>(INT_MAX) - field.prefix.size() - field.leadingZeros - field.body.size() - field.trailingZeros)
        throw std::overflow_error("Formatted output exceeds INT_MAX");
    const size_t contentLen = field.prefix.size() + field.leadingZeros + field.body.size() + field.trailingZeros + field.suffix.size();
    const size_t padCount = field.width > contentLen ? field.width - contentLen : 0;
    if (padCount > static_cast<size_t>(INT_MAX) - contentLen)
        throw std::overflow_error("Formatted output exceeds INT_MAX");
    const size_t totalFieldLen = contentLen + padCount;
    if (totalFieldLen > static_cast<size_t>(INT_MAX) - output.Count())
        throw std::overflow_error("Formatted output exceeds INT_MAX");
    if (!field.left && !field.zero) output.Pad(' ', padCount);
    if (!field.prefix.empty()) output.Append(field.prefix.data(), field.prefix.size());
    if (!field.left && field.zero) output.Pad('0', padCount);
    output.Pad('0', field.leadingZeros);
    if (!field.body.empty()) output.Append(field.body.data(), field.body.size());
    output.Pad('0', field.trailingZeros);
    if (!field.suffix.empty()) output.Append(field.suffix.data(), field.suffix.size());
    if (field.left) output.Pad(' ', padCount);
}

inline std::string_view FormatUnsignedDigits(char* bufferEnd, unsigned long long value, int base, bool upper, bool omitZero) {
    char* p = bufferEnd;
    if (value == 0) {
        if (!omitZero) *--p = '0';
    } else if (base == 10) {
        while (value > 0) {
            *--p = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
    } else if (base == 8) {
        while (value > 0) {
            *--p = static_cast<char>('0' + (value & 7));
            value >>= 3;
        }
    } else if (base == 16) {
        const char* hexDigits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
        while (value > 0) {
            *--p = hexDigits[value & 15];
            value >>= 4;
        }
    }
    return std::string_view(p, bufferEnd - p);
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

inline void HexFloat(FormatOutput& output, const FloatSpec& parsed, char conversion, double value) {
    const bool plus = parsed.plus, space = parsed.space, alternate = parsed.alternate;
    const std::size_t width = parsed.width;
    const int precision = parsed.precision;
    const bool upper = conversion == 'A';
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
    int digits = 13;
    if (precision >= 0 && precision < 13) {
        const int dropped = (13 - precision) * 4;
        const std::uint64_t remainder = fraction & ((std::uint64_t{1} << dropped) - 1);
        fraction >>= dropped;
        if (remainder >= std::uint64_t{1} << (dropped - 1) && ++fraction >> (precision * 4)) {
            fraction &= (std::uint64_t{1} << (precision * 4)) - 1;
            ++lead;
        }
        digits = precision;
    } else if (precision < 0) {
        for (; digits > 0 && (fraction & 0xf) == 0; --digits) fraction >>= 4;
    }
    const char* hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char suffixBuf[32];
    const int suffixLen = std::snprintf(suffixBuf, sizeof(suffixBuf), "%c%+d", upper ? 'P' : 'p', exponent);
    char prefixBuf[8];
    char* pp = prefixBuf;
    if ((bits >> 63) != 0) *pp++ = '-';
    else if (plus) *pp++ = '+';
    else if (space) *pp++ = ' ';
    *pp++ = '0';
    *pp++ = upper ? 'X' : 'x';
    const auto fractionalDigits = static_cast<std::size_t>(precision > 13 ? precision : digits);
    const bool point = fractionalDigits != 0 || alternate;
    char mantissaBuf[32];
    char* mp = mantissaBuf;
    *mp++ = hex[lead];
    if (point) *mp++ = '.';
    for (int digit = digits - 1; digit >= 0; --digit) *mp++ = hex[(fraction >> (digit * 4)) & 0xf];
    const size_t extraZeros = precision > 13 ? static_cast<size_t>(precision - 13) : 0;
    EmitField(output, FormattedField{
        std::string_view(prefixBuf, pp - prefixBuf),
        0,
        std::string_view(mantissaBuf, mp - mantissaBuf),
        extraZeros,
        std::string_view(suffixBuf, static_cast<size_t>(suffixLen)),
        width,
        parsed.left,
        parsed.zero
    });
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

inline void DecimalFloat(FormatOutput& output, const FloatSpec& parsed, char conversion, long double value) {
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
    if (length > static_cast<long long>(INT_MAX) || parsed.width > static_cast<size_t>(INT_MAX))
        throw std::overflow_error("Formatted output exceeds INT_MAX");
    const size_t contentLen = static_cast<size_t>(length);
    const size_t padCount = parsed.width > contentLen ? parsed.width - contentLen : 0;
    if (padCount > static_cast<size_t>(INT_MAX) - contentLen)
        throw std::overflow_error("Formatted output exceeds INT_MAX");
    const size_t totalFieldLen = contentLen + padCount;
    if (totalFieldLen > static_cast<size_t>(INT_MAX) - output.Count())
        throw std::overflow_error("Formatted output exceeds INT_MAX");

    const auto emitSegment = [&](long long firstExponent, long long count) {
        if (count <= 0) return;
        const long long startIndex = rounded.exponent - firstExponent;
        const long long endIndex = startIndex + count - 1;
        const long long digitCount = static_cast<long long>(rounded.digits.size());
        const long long leadingZeros = std::max(0LL, std::min(count, -startIndex));
        const long long digitsStart = std::max(0LL, startIndex);
        const long long digitsEnd = std::min(digitCount - 1, endIndex);
        const long long overlapCount = digitsStart <= digitsEnd ? digitsEnd - digitsStart + 1 : 0;
        const long long trailingZeros = count - leadingZeros - overlapCount;
        output.Pad('0', static_cast<size_t>(leadingZeros));
        if (overlapCount > 0) output.Append(rounded.digits.data() + digitsStart, static_cast<size_t>(overlapCount));
        output.Pad('0', static_cast<size_t>(trailingZeros));
    };

    const auto emitBody = [&] {
        const long long first = exponential ? exponent : std::max(exponent, 0LL);
        emitSegment(first, whole);
        if (point) output.Append(".", 1);
        const long long start = exponential ? exponent - 1 : -1;
        emitSegment(start, precision);
        if (exponential) {
            output.Append(upper ? "E" : "e", 1);
            output.Append(exponent < 0 ? "-" : "+", 1);
            if (magnitude.size() < 2) output.Append("0", 1);
            output.Append(magnitude.data(), magnitude.size());
        }
    };

    if (parsed.left) {
        if (!prefix.empty()) output.Append(prefix.data(), prefix.size());
        emitBody();
        output.Pad(' ', padCount);
    } else if (parsed.zero) {
        if (!prefix.empty()) output.Append(prefix.data(), prefix.size());
        output.Pad('0', padCount);
        emitBody();
    } else {
        output.Pad(' ', padCount);
        if (!prefix.empty()) output.Append(prefix.data(), prefix.size());
        emitBody();
    }
}

inline std::string NanText(const std::string& spec, bool negative, bool upper) {
    std::string sign;
    if (negative) sign = "-";
    else if (spec.find('+') != std::string::npos) sign = "+";
    else if (spec.find(' ') != std::string::npos) sign = " ";
    return sign + (upper ? "NAN" : "nan");
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

inline constexpr char KnownConversions[] = "diouxXeEfFgGaAcCsSpn";
constexpr size_t maximumArgumentPosition = 65535;

inline size_t ArgumentPosition(const char*& format) {
    const char* end = format;
    while (*end >= '0' && *end <= '9') ++end;
    if (end == format || *end != '$') return 0;
    size_t position = 0;
    for (const char* digit = format; digit != end && position <= maximumArgumentPosition; ++digit)
        position = position * 10 + static_cast<size_t>(*digit - '0');
    if (position == 0) return 0;
    if (position > maximumArgumentPosition) throw std::invalid_argument("Format argument position out of range");
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
    try {
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
            bool unmeasuredUnknownFlag = false;
            bool zeroFlag = false;
            bool leftFlag = false;
            bool plusFlag = false;
            bool spaceFlag = false;
            bool altFlag = false;
            for (; *format && std::strchr("-+ #0'", *format); ++format) {
                if (*format != '\'') spec += *format;
                if (std::strchr("+ '", *format)) unmeasuredUnknownFlag = true;
                if (*format == '0') zeroFlag = true;
                if (*format == '-') leftFlag = true;
                if (*format == '+') plusFlag = true;
                if (*format == ' ') spaceFlag = true;
                if (*format == '#') altFlag = true;
            }
            bool widthGiven = false;
            int width = 0;
            if (*format == '*') {
                ++format;
                const int starWidth = star();
                const long long mag = starWidth < 0 ? -static_cast<long long>(starWidth) : static_cast<long long>(starWidth);
                if (mag > INT_MAX)
                    throw std::overflow_error("Field width exceeds INT_MAX");
                if (starWidth < 0) {
                    leftFlag = true;
                    spec += '-';
                }
                width = static_cast<int>(mag);
                spec += std::to_string(mag);
                widthGiven = true;
            } else {
                long long parsedWidth = 0;
                for (; *format >= '0' && *format <= '9'; widthGiven = true) {
                    const int digit = *format++ - '0';
                    if (parsedWidth > (static_cast<long long>(INT_MAX) - digit) / 10)
                        throw std::overflow_error("Field width exceeds INT_MAX");
                    parsedWidth = parsedWidth * 10 + digit;
                    spec += static_cast<char>('0' + digit);
                }
                if (widthGiven) width = static_cast<int>(parsedWidth);
            }
            if (zeroFlag && widthGiven) unmeasuredUnknownFlag = true;
            bool hasPrecision = false;
            int precision = 0;
            size_t precisionLimit = std::numeric_limits<size_t>::max();
            if (*format == '.') {
                ++format;
                hasPrecision = true;
                if (*format == '*') {
                    ++format;
                    const int starPrecision = star();
                    if (starPrecision >= 0) {
                        precision = starPrecision;
                        spec += "." + std::to_string(precision);
                        precisionLimit = static_cast<size_t>(precision);
                    } else {
                        hasPrecision = false;
                    }
                } else {
                    spec += '.';
                    long long parsedPrecision = 0;
                    while (*format >= '0' && *format <= '9') {
                        const int digit = *format - '0';
                        if (parsedPrecision > (static_cast<long long>(INT_MAX) - digit) / 10)
                            throw std::overflow_error("Precision exceeds INT_MAX");
                        parsedPrecision = parsedPrecision * 10 + digit;
                        spec += *format++;
                    }
                    precision = static_cast<int>(parsedPrecision);
                    precisionLimit = static_cast<size_t>(precision);
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
            if (!std::strchr(KnownConversions, conversion)) {
                if (secure || unmeasuredUnknownFlag)
                    throw std::invalid_argument("Unsupported format conversion '" + spec + length + conversion + "' in \"" + std::string(formatStart).substr(0, 160) + "\"");
                const char text[] = {'%', conversion, '\0'};
                const char* body = conversion == '%' ? "%" : text;
                const size_t bodyLen = conversion == '%' ? 1 : 2;
                EmitField(output, FormattedField{{}, 0, std::string_view(body, bodyLen), 0, {}, static_cast<size_t>(width), leftFlag, false});
                continue;
            }
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
                const char* prefix = "";
                size_t prefixLen = 0;
                unsigned long long mag;
                if (value < 0) {
                    prefix = "-";
                    prefixLen = 1;
                    mag = 0 - static_cast<unsigned long long>(value);
                } else if (plusFlag) {
                    prefix = "+";
                    prefixLen = 1;
                    mag = static_cast<unsigned long long>(value);
                } else if (spaceFlag) {
                    prefix = " ";
                    prefixLen = 1;
                    mag = static_cast<unsigned long long>(value);
                } else {
                    mag = static_cast<unsigned long long>(value);
                }
                char digitBuf[32];
                const auto digits = FormatUnsignedDigits(digitBuf + sizeof(digitBuf), mag, 10, false, hasPrecision && precision == 0);
                const size_t zeroCount = (hasPrecision && precision > static_cast<int>(digits.size()))
                    ? static_cast<size_t>(precision) - digits.size() : 0;
                const bool zeroPad = zeroFlag && !hasPrecision && !leftFlag;
                EmitField(output, FormattedField{std::string_view(prefix, prefixLen), zeroCount, digits, 0, {}, static_cast<size_t>(width), leftFlag, zeroPad});
            } else if (std::strchr("ouxX", conversion)) {
                if (!integerLength) throw std::invalid_argument("Invalid integer length");
                unsigned long long value;
                if (length.empty() || length == "h" || length == "hh") {
                    value = args.Next<unsigned int>();
                    if (length == "h") value = static_cast<unsigned short>(value);
                    if (length == "hh") value = static_cast<unsigned char>(value);
                } else value = args.Next<unsigned long long>();
                char digitBuf[32];
                const int base = conversion == 'u' ? 10 : conversion == 'o' ? 8 : 16;
                auto digits = FormatUnsignedDigits(digitBuf + sizeof(digitBuf), value, base, conversion == 'X', hasPrecision && precision == 0);
                const char* prefix = "";
                size_t prefixLen = 0;
                if (altFlag) {
                    if (conversion == 'x' && value != 0) {
                        prefix = "0x";
                        prefixLen = 2;
                    } else if (conversion == 'X' && value != 0) {
                        prefix = "0X";
                        prefixLen = 2;
                    } else if (conversion == 'o') {
                        if (value == 0 && hasPrecision && precision == 0) {
                            digits = "0";
                        }
                    }
                }
                size_t effectivePrecision = hasPrecision ? static_cast<size_t>(precision) : 0;
                if (altFlag && conversion == 'o' && value != 0) {
                    if (effectivePrecision <= digits.size()) effectivePrecision = digits.size() + 1;
                }
                const size_t zeroCount = effectivePrecision > digits.size() ? effectivePrecision - digits.size() : 0;
                const bool zeroPad = zeroFlag && !hasPrecision && !leftFlag;
                EmitField(output, FormattedField{std::string_view(prefix, prefixLen), zeroCount, digits, 0, {}, static_cast<size_t>(width), leftFlag, zeroPad});
            } else if (std::strchr("aAeEfFgG", conversion)) {
                const FloatSpec parsed{leftFlag, plusFlag, spaceFlag, altFlag, zeroFlag, static_cast<size_t>(width), hasPrecision ? precision : -1};
                if (length == "L") {
                    static_assert(sizeof(long double) == 16);
                    static_assert(std::numeric_limits<long double>::digits == 64);
                    const long double value = args.Next<long double>();
                    if (std::isnan(value)) {
                        const std::string nanStr = NanText(spec, std::signbit(value), conversion < 'a');
                        EmitField(output, FormattedField{{}, 0, nanStr, 0, {}, static_cast<size_t>(width), leftFlag, false});
                    } else if (std::isinf(value)) {
                        char coreBuf[32];
                        std::string coreSpec = "%";
                        if (plusFlag) coreSpec += '+';
                        else if (spaceFlag) coreSpec += ' ';
                        coreSpec += "L";
                        coreSpec += conversion;
                        const int coreLen = std::snprintf(coreBuf, sizeof(coreBuf), coreSpec.c_str(), value);
                        if (coreLen < 0) throw std::runtime_error("Formatting conversion failed");
                        EmitField(output, FormattedField{{}, 0, std::string_view(coreBuf, static_cast<size_t>(coreLen)), 0, {}, static_cast<size_t>(width), leftFlag, false});
                    } else if (conversion != 'a' && conversion != 'A') {
                        DecimalFloat(output, parsed, conversion, value);
                    } else {
                        std::string coreSpec = "%";
                        if (altFlag) coreSpec += '#';
                        if (plusFlag) coreSpec += '+';
                        else if (spaceFlag) coreSpec += ' ';
                        if (hasPrecision) {
                            if (precision > 16) coreSpec += ".16";
                            else coreSpec += "." + std::to_string(precision);
                        }
                        coreSpec += "L";
                        coreSpec += conversion;
                        char coreBuf[64];
                        const int coreLen = std::snprintf(coreBuf, sizeof(coreBuf), coreSpec.c_str(), value);
                        if (coreLen < 0) throw std::runtime_error("Formatting conversion failed");
                        size_t prefixLen = 0;
                        if (coreBuf[0] == '-' || coreBuf[0] == '+' || coreBuf[0] == ' ') prefixLen = 3;
                        else prefixLen = 2;
                        if (static_cast<size_t>(coreLen) < prefixLen) prefixLen = 0;
                        if (hasPrecision && precision > 16) {
                            const char* pMarker = std::strpbrk(coreBuf + prefixLen, "pP");
                            if (!pMarker) throw std::runtime_error("Long double hex missing exponent");
                            const size_t mantissaLen = pMarker - (coreBuf + prefixLen);
                            const size_t extraZeros = static_cast<size_t>(precision - 16);
                            const size_t suffixLen = coreLen - (pMarker - coreBuf);
                            EmitField(output, FormattedField{
                                std::string_view(coreBuf, prefixLen),
                                0,
                                std::string_view(coreBuf + prefixLen, mantissaLen),
                                extraZeros,
                                std::string_view(pMarker, suffixLen),
                                static_cast<size_t>(width),
                                leftFlag,
                                zeroFlag
                            });
                        } else {
                            const size_t bodyLen = coreLen - prefixLen;
                            EmitField(output, FormattedField{
                                std::string_view(coreBuf, prefixLen),
                                0,
                                std::string_view(coreBuf + prefixLen, bodyLen),
                                0,
                                {},
                                static_cast<size_t>(width),
                                leftFlag,
                                zeroFlag
                            });
                        }
                    }
                } else {
                    if (!length.empty() && length != "l") throw std::invalid_argument("Invalid floating length");
                    const double value = args.Next<double>();
                    if (std::isnan(value)) {
                        const std::string nanStr = NanText(spec, std::signbit(value), conversion < 'a');
                        EmitField(output, FormattedField{{}, 0, nanStr, 0, {}, static_cast<size_t>(width), leftFlag, false});
                    } else if (std::isinf(value)) {
                        char coreBuf[32];
                        std::string coreSpec = "%";
                        if (plusFlag) coreSpec += '+';
                        else if (spaceFlag) coreSpec += ' ';
                        coreSpec += conversion;
                        const int coreLen = std::snprintf(coreBuf, sizeof(coreBuf), coreSpec.c_str(), value);
                        if (coreLen < 0) throw std::runtime_error("Formatting conversion failed");
                        EmitField(output, FormattedField{{}, 0, std::string_view(coreBuf, static_cast<size_t>(coreLen)), 0, {}, static_cast<size_t>(width), leftFlag, false});
                    } else if (conversion == 'a' || conversion == 'A') {
                        HexFloat(output, parsed, conversion, value);
                    } else {
                        DecimalFloat(output, parsed, conversion, value);
                    }
                }
            } else if (conversion == 'c' && (length.empty() || length == "h")) {
                const int ch = args.Next<int>();
                const char byte = static_cast<char>(ch);
                EmitField(output, FormattedField{{}, 0, std::string_view(&byte, 1), 0, {}, static_cast<size_t>(width), leftFlag, false});
            } else if ((conversion == 'c' && length == "l") || (conversion == 'C' && length.empty())) {
                std::string utf8;
                AppendUtf8(utf8, static_cast<char16_t>(args.Next<unsigned int>()));
                const char nullByte = '\0';
                const char* data = utf8[0] == 0 ? &nullByte : utf8.data();
                const size_t len = utf8[0] == 0 ? 1 : utf8.size();
                EmitField(output, FormattedField{{}, 0, std::string_view(data, len), 0, {}, static_cast<size_t>(width), leftFlag, false});
            } else if (conversion == 's' && (length.empty() || length == "h")) {
                const char* value = args.Next<const char*>();
                if (!value && secure) throw FormatConstraintViolation("Null string argument");
                if (!value) value = "(null)";
                size_t len = 0;
                if (hasPrecision) {
                    while (len < static_cast<size_t>(precision) && value[len] != '\0') ++len;
                } else {
                    len = std::strlen(value);
                }
                EmitField(output, FormattedField{{}, 0, std::string_view(value, len), 0, {}, static_cast<size_t>(width), leftFlag, false});
            } else if ((conversion == 's' && length == "l") || (conversion == 'S' && length.empty())) {
                const char16_t* value = args.Next<const char16_t*>();
                if (!value && secure) throw FormatConstraintViolation("Null string argument");
                if (!value) value = u"(null)";
                std::string utf8;
                while (utf8.size() < precisionLimit && *value) {
                    const char32_t code = *value++;
                    const auto previousSize = utf8.size();
                    AppendUtf8(utf8, code);
                    if (utf8.size() > precisionLimit) {
                        utf8.resize(previousSize);
                        break;
                    }
                }
                EmitField(output, FormattedField{{}, 0, utf8, 0, {}, static_cast<size_t>(width), leftFlag, false});
            } else if (conversion == 'p' && (length.empty() || length == "l")) {
                const void* pointer = args.Next<void*>();
                const auto value = static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(pointer));
                char digitBuf[32];
                const auto digits = FormatUnsignedDigits(digitBuf + sizeof(digitBuf), value, 16, false, hasPrecision && precision == 0);
                const size_t zeroCount = (hasPrecision && precision > static_cast<int>(digits.size()))
                    ? static_cast<size_t>(precision) - digits.size() : 0;
                const bool zeroPad = zeroFlag && !hasPrecision && !leftFlag;
                EmitField(output, FormattedField{{}, zeroCount, digits, 0, {}, static_cast<size_t>(width), leftFlag, zeroPad});
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
    } catch (const std::overflow_error&) {
        errno = 84;
        return -1;
    }
}

inline int PrintWindows(const char* format, const void* args) {
    std::string buffer;
    const int size = FormatWindows(nullptr, 0, format, args, &buffer);
    if (size < 0) return -1;
    if (std::fwrite(buffer.data(), 1, static_cast<size_t>(size), GuestStandardOutputFile_nid_no_patch()) != static_cast<size_t>(size))
        throw std::runtime_error("Formatted output write failed");
    return size;
}

}

#endif
