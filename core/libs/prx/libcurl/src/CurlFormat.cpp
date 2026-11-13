/* Derived from lib/mprintf.c@801bd5138ce31aa0d906fa4e2eabfc599d74e793 */
/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1999 - 2021, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 *
 * Purpose:
 *  A merge of Bjorn Reese's format() function and Daniel's dsprintf()
 *  1.0. A full blooded printf() clone with full support for <num>$
 *  everywhere (parameters, widths and precisions) including variabled
 *  sized parameters (like doubles, long longs, long doubles and even
 *  void * in 64-bit architectures).
 *
 * Current restrictions:
 * - Max 128 parameters
 * - No 'long double' support.
 *
 * If you ever want truly portable and good *printf() clones, the project that
 * took on from here is named 'Trio' and you find more details on the trio web
 * page at https://daniel.haxx.se/projects/trio/
 */
#include "prx/libcurl/src/CurlFormat.hpp"
#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

extern "C" {
extern curl_free_callback Curl_cfree;
extern curl_realloc_callback Curl_crealloc;
extern curl_strdup_callback Curl_cstrdup;
}

namespace CurlFormat {

namespace {

constexpr std::int64_t MaxParameters = 128;
constexpr std::size_t WorkSize = 326;
constexpr std::size_t AllocatedLimit = 8000000;
constexpr std::size_t FirstAllocation = 32;
constexpr char LowerDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
constexpr char UpperDigits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr char NilText[] = "(nil)";

enum class ArgumentType : unsigned char { Unset, Unknown, String, Pointer, Integer, IntegerPointer, Double, Width };

enum Flag : unsigned {
    FlagSpace = 1u << 0,
    FlagShowSign = 1u << 1,
    FlagLeft = 1u << 2,
    FlagAlt = 1u << 3,
    FlagShort = 1u << 4,
    FlagLong = 1u << 5,
    FlagLongLong = 1u << 6,
    FlagLongDouble = 1u << 7,
    FlagPadNil = 1u << 8,
    FlagUnsigned = 1u << 9,
    FlagOctal = 1u << 10,
    FlagHex = 1u << 11,
    FlagUpper = 1u << 12,
    FlagWidth = 1u << 13,
    FlagWidthParam = 1u << 14,
    FlagPrecision = 1u << 15,
    FlagPrecisionParam = 1u << 16,
    FlagChar = 1u << 17,
    FlagFloatE = 1u << 18,
    FlagFloatG = 1u << 19,
};

struct Parameter {
    ArgumentType type = ArgumentType::Unset;
    unsigned flags = 0;
    std::int64_t width = 0;
    std::int64_t precision = 0;
    std::uint64_t data = 0;
};

class IOutput {
public:
    virtual ~IOutput() = default;
    virtual bool Put(unsigned char value) = 0;
};

class BoundedOutput final : public IOutput {
public:
    BoundedOutput(char* buffer, std::size_t maxLength) : buffer(buffer), maxLength(maxLength) {}

    bool Put(unsigned char value) override {
        if (length >= maxLength) return false;
        buffer[length++] = static_cast<char>(value);
        return true;
    }

    std::size_t Length() const { return length; }

private:
    char* buffer;
    std::size_t maxLength;
    std::size_t length = 0;
};

class UnboundedOutput final : public IOutput {
public:
    explicit UnboundedOutput(char* buffer) : buffer(buffer) {}

    bool Put(unsigned char value) override {
        buffer[length++] = static_cast<char>(value);
        return true;
    }

    std::size_t Length() const { return length; }

private:
    char* buffer;
    std::size_t length = 0;
};

class AllocatedOutput final : public IOutput {
public:
    bool Put(unsigned char value) override {
        const std::size_t fit = length + 2;
        if (fit > AllocatedLimit) {
            Release();
            return false;
        }
        std::size_t size = capacity;
        if (!size) {
            size = fit < FirstAllocation ? FirstAllocation : fit;
        } else {
            while (size < fit) size *= 2;
        }
        if (size != capacity) {
            auto* grown = static_cast<char*>(Curl_crealloc(buffer, size));
            if (!grown) {
                Release();
                return false;
            }
            buffer = grown;
            capacity = size;
        }
        buffer[length++] = static_cast<char>(value);
        buffer[length] = '\0';
        return true;
    }

    void Release() {
        Curl_cfree(buffer);
        buffer = nullptr;
        length = 0;
        capacity = 0;
        failed = true;
    }

    bool Failed() const { return failed; }
    std::size_t Length() const { return length; }
    char* Buffer() const { return buffer; }

private:
    char* buffer = nullptr;
    std::size_t length = 0;
    std::size_t capacity = 0;
    bool failed = false;
};

bool IsDigit(char value) {
    return value >= '0' && value <= '9';
}

std::int64_t DollarString(const char* input, const char** end) {
    int number = 0;
    while (IsDigit(*input)) {
        if (number < MaxParameters) {
            number *= 10;
            number += *input - '0';
        }
        ++input;
    }
    if (number <= MaxParameters && *input == '$') {
        *end = input + 1;
        return number;
    }
    return 0;
}

bool IsQualifier(char value) {
    switch (value) {
    case '-': case '+': case ' ': case '#': case '.':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case 'h': case 'l': case 'L': case 'z': case 'q':
    case '*': case 'O':
        return true;
    default:
        return false;
    }
}

std::int64_t ParseDecimal(const char* text, const char** end) {
    char* stop = nullptr;
    const long long value = std::strtoll(text, &stop, 10);
    *end = stop;
    return value;
}

std::int64_t Signed(std::uint64_t value) {
    return static_cast<std::int64_t>(value);
}

class Formatter {
public:
    Formatter(IOutput& output, const void* args) : output(output), arguments(args) {}

    int Run(const char* format) {
        if (!collect(format)) return -1;
        std::size_t end = 0;
        std::int64_t paramNum = 0;
        const char* f = format;
        while (*f != '\0') {
            if (*f != '%') {
                do {
                    if (!emit(*f)) return done;
                } while (*++f && *f != '%');
                continue;
            }
            ++f;
            if (*f == '%') {
                ++f;
                if (!emit('%')) return done;
                continue;
            }
            std::int64_t param = DollarString(f, &f);
            if (!param) param = paramNum;
            else --param;
            ++paramNum;
            Parameter& p = referenced(param);
            std::int64_t width;
            if (p.flags & FlagWidthParam) {
                width = Signed(referenced(p.width).data);
                ++paramNum;
                if (width < 0) {
                    width = -width;
                    p.flags |= FlagLeft;
                    p.flags &= ~FlagPadNil;
                }
            } else {
                width = p.width;
            }
            std::int64_t precision;
            if (p.flags & FlagPrecisionParam) {
                precision = Signed(referenced(p.precision).data);
                ++paramNum;
                if (precision < 0) precision = -1;
            } else if (p.flags & FlagPrecision) {
                precision = p.precision;
            } else {
                precision = -1;
            }
            if (!convert(p, width, precision)) return done;
            if (end >= ends.size()) throw std::logic_error("curl format conversion has no recorded end");
            f = ends[end++];
        }
        return done;
    }

private:
    IOutput& output;
    LibcDetail::FormatArguments arguments;
    std::array<Parameter, MaxParameters> parameters{};
    std::vector<const char*> ends;
    int done = 0;

    bool emit(char value) {
        if (!output.Put(static_cast<unsigned char>(value))) return false;
        ++done;
        return true;
    }

    Parameter& referenced(std::int64_t index) {
        if (index < 0 || index >= MaxParameters) throw std::out_of_range("curl format references an argument position outside its 128 parameters");
        Parameter& parameter = parameters[static_cast<std::size_t>(index)];
        if (parameter.type == ArgumentType::Unset) throw std::invalid_argument("curl format references an argument position no conversion describes");
        return parameter;
    }

    bool collect(const char* format) {
        const char* fmt = format;
        std::int64_t paramNum = 0;
        std::int64_t maxParam = 0;
        while (*fmt) {
            if (*fmt++ != '%') continue;
            if (*fmt == '%') {
                ++fmt;
                continue;
            }
            unsigned flags = 0;
            ++paramNum;
            std::int64_t thisParam = DollarString(fmt, &fmt);
            if (thisParam == 0) thisParam = paramNum;
            if (thisParam > maxParam) maxParam = thisParam;
            std::int64_t width = 0;
            std::int64_t precision = 0;
            while (IsQualifier(*fmt)) {
                switch (*fmt++) {
                case ' ':
                    flags |= FlagSpace;
                    break;
                case '+':
                    flags |= FlagShowSign;
                    break;
                case '-':
                    flags |= FlagLeft;
                    flags &= ~FlagPadNil;
                    break;
                case '#':
                    flags |= FlagAlt;
                    break;
                case '.':
                    if (*fmt == '*') {
                        flags |= FlagPrecisionParam;
                        ++fmt;
                        ++paramNum;
                        const std::int64_t position = DollarString(fmt, &fmt);
                        precision = position ? position : paramNum;
                        if (precision > maxParam) maxParam = precision;
                    } else {
                        flags |= FlagPrecision;
                        precision = ParseDecimal(fmt, &fmt);
                    }
                    break;
                case 'h':
                    flags |= FlagShort;
                    break;
                case 'l':
                    if (flags & FlagLong) flags |= FlagLongLong;
                    else flags |= FlagLong;
                    break;
                case 'L':
                    flags |= FlagLongDouble;
                    break;
                case 'q':
                    flags |= FlagLongLong;
                    break;
                case 'z':
                case 'O':
                    flags |= FlagLong;
                    break;
                case '0':
                    if (!(flags & FlagLeft)) flags |= FlagPadNil;
                    [[fallthrough]];
                case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    flags |= FlagWidth;
                    width = ParseDecimal(fmt - 1, &fmt);
                    break;
                case '*': {
                    flags |= FlagWidthParam;
                    ++paramNum;
                    const std::int64_t position = DollarString(fmt, &fmt);
                    width = position ? position : paramNum;
                    if (width > maxParam) maxParam = width;
                    break;
                }
                default:
                    break;
                }
            }
            const std::int64_t index = thisParam - 1;
            if (index < 0 || index >= MaxParameters) return false;
            Parameter& entry = parameters[static_cast<std::size_t>(index)];
            switch (*fmt) {
            case 'S':
                flags |= FlagAlt;
                [[fallthrough]];
            case 's':
                entry.type = ArgumentType::String;
                break;
            case 'n':
                entry.type = ArgumentType::IntegerPointer;
                break;
            case 'p':
                entry.type = ArgumentType::Pointer;
                break;
            case 'd': case 'i':
                entry.type = ArgumentType::Integer;
                break;
            case 'u':
                entry.type = ArgumentType::Integer;
                flags |= FlagUnsigned;
                break;
            case 'o':
                entry.type = ArgumentType::Integer;
                flags |= FlagOctal;
                break;
            case 'x':
                entry.type = ArgumentType::Integer;
                flags |= FlagHex | FlagUnsigned;
                break;
            case 'X':
                entry.type = ArgumentType::Integer;
                flags |= FlagHex | FlagUpper | FlagUnsigned;
                break;
            case 'c':
                entry.type = ArgumentType::Integer;
                flags |= FlagChar;
                break;
            case 'f':
                entry.type = ArgumentType::Double;
                break;
            case 'e':
                entry.type = ArgumentType::Double;
                flags |= FlagFloatE;
                break;
            case 'E':
                entry.type = ArgumentType::Double;
                flags |= FlagFloatE | FlagUpper;
                break;
            case 'g':
                entry.type = ArgumentType::Double;
                flags |= FlagFloatG;
                break;
            case 'G':
                entry.type = ArgumentType::Double;
                flags |= FlagFloatG | FlagUpper;
                break;
            default:
                entry.type = ArgumentType::Unknown;
                break;
            }
            entry.flags = flags;
            entry.width = width;
            entry.precision = precision;
            if (flags & FlagWidthParam) {
                const std::int64_t k = width - 1;
                if (k < 0 || k >= MaxParameters) return false;
                entry.width = k;
                markWidth(parameters[static_cast<std::size_t>(k)]);
            }
            if (flags & FlagPrecisionParam) {
                const std::int64_t k = precision - 1;
                if (k < 0 || k >= MaxParameters) return false;
                entry.precision = k;
                markWidth(parameters[static_cast<std::size_t>(k)]);
            }
            if (ends.size() == static_cast<std::size_t>(MaxParameters)) throw std::length_error("curl format has more than 128 conversions");
            ends.push_back(fmt + (*fmt == '\0' ? 0 : 1));
        }
        for (std::int64_t i = 0; i < maxParam; ++i) {
            Parameter& entry = parameters[static_cast<std::size_t>(i)];
            if (entry.type == ArgumentType::Unset) throw std::invalid_argument("curl format leaves an argument position unused");
            if (entry.flags & FlagWidthParam) parameters[static_cast<std::size_t>(entry.width)].data = static_cast<std::uint64_t>(static_cast<std::int64_t>(arguments.Next<std::int32_t>()));
            if (entry.flags & FlagPrecisionParam) parameters[static_cast<std::size_t>(entry.precision)].data = static_cast<std::uint64_t>(static_cast<std::int64_t>(arguments.Next<std::int32_t>()));
            switch (entry.type) {
            case ArgumentType::String:
            case ArgumentType::IntegerPointer:
            case ArgumentType::Unknown:
            case ArgumentType::Pointer:
                entry.data = arguments.Next<std::uint64_t>();
                break;
            case ArgumentType::Integer:
                if (entry.flags & (FlagLongLong | FlagLong)) entry.data = arguments.Next<std::uint64_t>();
                else if (entry.flags & FlagUnsigned) entry.data = arguments.Next<std::uint32_t>();
                else entry.data = static_cast<std::uint64_t>(static_cast<std::int64_t>(arguments.Next<std::int32_t>()));
                break;
            case ArgumentType::Double: {
                const double value = arguments.Next<double>();
                std::memcpy(&entry.data, &value, sizeof(value));
                break;
            }
            case ArgumentType::Width:
                entry.type = ArgumentType::Integer;
                break;
            default:
                break;
            }
        }
        return true;
    }

    static void markWidth(Parameter& parameter) {
        parameter.type = ArgumentType::Width;
        parameter.flags = 0;
        parameter.width = 0;
        parameter.precision = 0;
    }

    bool convert(Parameter& p, std::int64_t width, std::int64_t precision) {
        const bool alt = (p.flags & FlagAlt) != 0;
        switch (p.type) {
        case ArgumentType::Integer:
            return integer(p, width, precision, alt);
        case ArgumentType::String:
            return text(p, width, precision);
        case ArgumentType::Pointer:
            return pointer(p, width, precision);
        case ArgumentType::Double:
            return floating(p);
        case ArgumentType::IntegerPointer:
            count(p);
            return true;
        default:
            return true;
        }
    }

    bool integer(const Parameter& p, std::int64_t width, std::int64_t precision, bool alt) {
        std::uint64_t num = p.data;
        if (p.flags & FlagChar) {
            if (!(p.flags & FlagLeft))
                while (--width > 0)
                    if (!emit(' ')) return false;
            if (!emit(static_cast<char>(num))) return false;
            if (p.flags & FlagLeft)
                while (--width > 0)
                    if (!emit(' ')) return false;
            return true;
        }
        if (p.flags & FlagOctal) return number(p, num, 8, false, alt, LowerDigits, width, precision);
        if (p.flags & FlagHex) return number(p, num, 16, false, alt, (p.flags & FlagUpper) ? UpperDigits : LowerDigits, width, precision);
        if (p.flags & FlagUnsigned) return number(p, num, 10, false, alt, LowerDigits, width, precision);
        const bool negative = Signed(p.data) < 0;
        if (negative) {
            std::int64_t signedNum = Signed(p.data) + 1;
            signedNum = -signedNum;
            num = static_cast<std::uint64_t>(signedNum);
            num += 1;
        }
        return number(p, num, 10, negative, alt, LowerDigits, width, precision);
    }

    bool number(const Parameter& p, std::uint64_t num, unsigned base, bool negative, bool alt, const char* digits, std::int64_t width, std::int64_t precision) {
        std::array<char, WorkSize> work{};
        const std::ptrdiff_t workEnd = static_cast<std::ptrdiff_t>(WorkSize) - 2;
        std::ptrdiff_t w = workEnd;
        if (precision == -1) precision = 1;
        while (num > 0) {
            work[static_cast<std::size_t>(w--)] = digits[num % base];
            num /= base;
        }
        width -= workEnd - w;
        precision -= workEnd - w;
        if (alt && base == 8 && precision <= 0) {
            work[static_cast<std::size_t>(w--)] = '0';
            --width;
        }
        if (precision > 0) {
            width -= precision;
            while (precision-- > 0 && w >= 0) work[static_cast<std::size_t>(w--)] = '0';
        }
        if (alt && base == 16) width -= 2;
        if (negative || (p.flags & FlagShowSign) || (p.flags & FlagSpace)) --width;
        if (!(p.flags & FlagLeft) && !(p.flags & FlagPadNil))
            while (width-- > 0)
                if (!emit(' ')) return false;
        if (negative) {
            if (!emit('-')) return false;
        } else if (p.flags & FlagShowSign) {
            if (!emit('+')) return false;
        } else if (p.flags & FlagSpace) {
            if (!emit(' ')) return false;
        }
        if (alt && base == 16) {
            if (!emit('0')) return false;
            if (!emit((p.flags & FlagUpper) ? 'X' : 'x')) return false;
        }
        if (!(p.flags & FlagLeft) && (p.flags & FlagPadNil))
            while (width-- > 0)
                if (!emit('0')) return false;
        while (++w <= workEnd)
            if (!emit(work[static_cast<std::size_t>(w)])) return false;
        if (p.flags & FlagLeft)
            while (width-- > 0)
                if (!emit(' ')) return false;
        return true;
    }

    bool text(Parameter& p, std::int64_t width, std::int64_t precision) {
        const char* str = reinterpret_cast<const char*>(p.data);
        std::size_t length;
        if (!str) {
            if (precision == -1 || precision >= static_cast<std::int64_t>(sizeof(NilText)) - 1) {
                str = NilText;
                length = sizeof(NilText) - 1;
                p.flags &= ~FlagAlt;
            } else {
                str = "";
                length = 0;
            }
        } else if (precision != -1) {
            length = static_cast<std::size_t>(precision);
        } else {
            length = std::strlen(str);
        }
        constexpr auto longMax = std::numeric_limits<std::int64_t>::max();
        width -= length > static_cast<std::size_t>(longMax) ? longMax : static_cast<std::int64_t>(length);
        if (p.flags & FlagAlt)
            if (!emit('"')) return false;
        if (!(p.flags & FlagLeft))
            while (width-- > 0)
                if (!emit(' ')) return false;
        for (; length && *str; --length)
            if (!emit(*str++)) return false;
        if (p.flags & FlagLeft)
            while (width-- > 0)
                if (!emit(' ')) return false;
        if (p.flags & FlagAlt)
            if (!emit('"')) return false;
        return true;
    }

    bool pointer(const Parameter& p, std::int64_t width, std::int64_t precision) {
        if (p.data) return number(p, p.data, 16, false, true, (p.flags & FlagUpper) ? UpperDigits : LowerDigits, width, precision);
        width -= static_cast<std::int64_t>(sizeof(NilText) - 1);
        if (p.flags & FlagLeft)
            while (width-- > 0)
                if (!emit(' ')) return false;
        for (const char* point = NilText; *point != '\0'; ++point)
            if (!emit(*point)) return false;
        if (!(p.flags & FlagLeft))
            while (width-- > 0)
                if (!emit(' ')) return false;
        return true;
    }

    bool floating(const Parameter& p) {
        std::array<char, 32> formatBuffer{};
        formatBuffer[0] = '%';
        std::size_t position = 1;
        std::size_t left = formatBuffer.size() - 1;
        std::int64_t width = -1;
        if (p.flags & FlagWidth) width = p.width;
        else if (p.flags & FlagWidthParam) width = Signed(referenced(p.width).data);
        std::int64_t precision = -1;
        if (p.flags & FlagPrecision) precision = p.precision;
        else if (p.flags & FlagPrecisionParam) precision = Signed(referenced(p.precision).data);
        if (p.flags & FlagLeft) formatBuffer[position++] = '-';
        if (p.flags & FlagShowSign) formatBuffer[position++] = '+';
        if (p.flags & FlagSpace) formatBuffer[position++] = ' ';
        if (p.flags & FlagAlt) formatBuffer[position++] = '#';
        left -= position - 1;
        if (width >= 0) {
            if (width >= static_cast<std::int64_t>(WorkSize)) width = static_cast<std::int64_t>(WorkSize) - 1;
            const int length = std::snprintf(&formatBuffer[position], left, "%lld", static_cast<long long>(width));
            position += static_cast<std::size_t>(length);
            left -= static_cast<std::size_t>(length);
        }
        double value;
        std::memcpy(&value, &p.data, sizeof(value));
        if (precision >= 0) {
            std::size_t maxPrecision = WorkSize - 2;
            double magnitude = value;
            if (width > 0 && precision <= width) maxPrecision -= static_cast<std::size_t>(width);
            while (magnitude >= 10.0) {
                magnitude /= 10;
                --maxPrecision;
            }
            if (precision > static_cast<std::int64_t>(maxPrecision)) precision = static_cast<std::int64_t>(maxPrecision) - 1;
            if (precision < 0) precision = 0;
            const int length = std::snprintf(&formatBuffer[position], left, ".%lld", static_cast<long long>(precision));
            position += static_cast<std::size_t>(length);
        }
        if (p.flags & FlagLong) formatBuffer[position++] = 'l';
        if (p.flags & FlagFloatE) formatBuffer[position++] = (p.flags & FlagUpper) ? 'E' : 'e';
        else if (p.flags & FlagFloatG) formatBuffer[position++] = (p.flags & FlagUpper) ? 'G' : 'g';
        else formatBuffer[position++] = 'f';
        formatBuffer[position] = '\0';
        std::array<char, WorkSize> work{};
        const int written = snprintf_nid_postfix(work.data(), work.size(), formatBuffer.data(), value);
        if (written < 0 || static_cast<std::size_t>(written) >= work.size()) throw std::length_error("curl floating-point conversion exceeds its 326-byte work buffer");
        for (const char* character = work.data(); *character; ++character)
            if (!emit(*character)) return false;
        return true;
    }

    void count(const Parameter& p) {
        void* target = reinterpret_cast<void*>(p.data);
        bool written;
        if (p.flags & (FlagLongLong | FlagLong)) {
            const std::int64_t value = done;
            written = GuestCopyOut_nid_no_patch(target, &value, sizeof(value));
        } else if (!(p.flags & FlagShort)) {
            const std::int32_t value = done;
            written = GuestCopyOut_nid_no_patch(target, &value, sizeof(value));
        } else {
            const auto value = static_cast<std::int16_t>(done);
            written = GuestCopyOut_nid_no_patch(target, &value, sizeof(value));
        }
        if (!written) throw std::invalid_argument("curl %n destination is not writable guest memory");
    }
};

}

int FormatBounded(char* buffer, std::size_t maxLength, const char* format, const void* args) {
    BoundedOutput output(buffer, maxLength);
    int result = Formatter(output, args).Run(format);
    if (result != -1 && maxLength) {
        if (output.Length() == maxLength) {
            buffer[maxLength - 1] = '\0';
            --result;
        } else {
            buffer[output.Length()] = '\0';
        }
    }
    return result;
}

int FormatUnbounded(char* buffer, const char* format, const void* args) {
    UnboundedOutput output(buffer);
    const int result = Formatter(output, args).Run(format);
    buffer[output.Length()] = '\0';
    return result;
}

char* FormatAllocated(const char* format, const void* args) {
    AllocatedOutput output;
    const int result = Formatter(output, args).Run(format);
    if (result == -1 || output.Failed()) {
        output.Release();
        return nullptr;
    }
    if (output.Length()) return output.Buffer();
    return Curl_cstrdup("");
}

}
