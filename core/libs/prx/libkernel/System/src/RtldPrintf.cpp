#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

extern "C" std::int64_t APS5_VABI write_nid_postfix(int descriptor, const char* buffer, std::int64_t size);

namespace {

class RtldOutput {
public:
    void Put(int character) {
        if (_used < sizeof(_buffer)) _buffer[_used++] = static_cast<char>(character);
        else Flush();
    }

    void Put(const char* text) {
        while (*text != '\0') Put(*text++);
    }

    void Flush() {
        if (_used == 0) return;
        write_nid_postfix(1, _buffer, static_cast<std::int64_t>(_used));
        _used = 0;
    }

private:
    char _buffer[512];
    std::size_t _used = 0;
};

class RtldArguments {
public:
#ifdef _WIN32
    explicit RtldArguments(__builtin_sysv_va_list& list) : _list(list) {}
    template <typename T> T Next() { return __builtin_va_arg(_list, T); }

private:
    __builtin_sysv_va_list& _list;
#else
    explicit RtldArguments(__builtin_va_list& list) : _list(list) {}
    template <typename T> T Next() { return __builtin_va_arg(_list, T); }

private:
    __builtin_va_list& _list;
#endif
};

struct RtldSpec {
    char pad = ' ';
    int width = 0;
    int precision = 0;
    int base = 10;
    bool dot = false;
    bool alternate = false;
    bool sign = false;
    bool left = false;
    bool upper = false;
    bool charSize = false;
    bool shortSize = false;
    bool longSize = false;
    bool quadSize = false;
    bool maxSize = false;
    bool ptrdiffSize = false;
    bool sizeSize = false;
};

constexpr int RtldRadix = 10;

int RtldDigits(char* reversed, std::uint64_t value, int base, bool upper) {
    const char* digits = upper ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
    int count = 0;
    do {
        reversed[count++] = digits[value % static_cast<unsigned>(base)];
        value /= static_cast<unsigned>(base);
    } while (value != 0);
    return count;
}

std::uint64_t RtldUnsigned(const RtldSpec& spec, RtldArguments& arguments) {
    if (spec.maxSize || spec.quadSize || spec.ptrdiffSize || spec.longSize || spec.sizeSize) return arguments.Next<std::uint64_t>();
    if (spec.shortSize) return static_cast<std::uint16_t>(arguments.Next<int>());
    if (spec.charSize) return static_cast<std::uint8_t>(arguments.Next<int>());
    return arguments.Next<unsigned>();
}

std::uint64_t RtldSigned(const RtldSpec& spec, RtldArguments& arguments) {
    if (spec.maxSize || spec.quadSize || spec.ptrdiffSize || spec.longSize || spec.sizeSize) return arguments.Next<std::uint64_t>();
    if (spec.shortSize) return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int16_t>(arguments.Next<int>())));
    if (spec.charSize) return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int8_t>(arguments.Next<int>())));
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(arguments.Next<int>()));
}

void RtldNumber(RtldOutput& out, RtldSpec spec, std::uint64_t value) {
    const bool negative = spec.sign && static_cast<std::int64_t>(value) < 0;
    if (negative) value = 0 - value;
    char reversed[65];
    const int length = RtldDigits(reversed, value, spec.base, spec.upper);
    const bool prefixed = spec.alternate && value != 0 && (spec.base == 8 || spec.base == 16);
    const int prefix = (prefixed ? (spec.base == 8 ? 1 : 2) : 0) + (negative ? 1 : 0);
    if (!spec.left && spec.pad == '0') spec.precision = spec.width - prefix;
    spec.width -= prefix + (spec.precision > length ? spec.precision : length);
    spec.precision -= length;
    if (!spec.left)
        while (spec.width-- > 0) out.Put(' ');
    if (negative) out.Put('-');
    if (prefixed) out.Put(spec.base == 8 ? "0" : "0x");
    while (spec.precision-- > 0) out.Put('0');
    for (int index = length; index-- > 0;) out.Put(reversed[index]);
    if (spec.left)
        while (spec.width-- > 0) out.Put(' ');
}

void RtldString(RtldOutput& out, RtldSpec spec, const char* text) {
    if (text == nullptr) text = "(null)";
    int length = 0;
    while ((!spec.dot || length < spec.precision) && text[length] != '\0') ++length;
    spec.width -= length;
    if (!spec.left && spec.width > 0)
        while (spec.width--) out.Put(spec.pad);
    for (int index = 0; index < length; ++index) out.Put(text[index]);
    if (spec.left && spec.width > 0)
        while (spec.width--) out.Put(spec.pad);
}

void RtldBits(RtldOutput& out, RtldArguments& arguments) {
    const auto value = static_cast<std::uint64_t>(static_cast<unsigned>(arguments.Next<int>()));
    const char* names = arguments.Next<const char*>();
    char reversed[65];
    for (int index = RtldDigits(reversed, value, *names++, false); index-- > 0;) out.Put(reversed[index]);
    if (value == 0) return;
    bool listed = false;
    while (*names != '\0') {
        const int bit = *names++;
        const bool set = (value & (1ull << (bit - 1))) != 0;
        if (set) out.Put(listed ? ',' : '<');
        for (; *names > ' '; ++names) {
            if (set) out.Put(*names);
        }
        listed |= set;
    }
    if (listed) out.Put('>');
}

void RtldDump(RtldOutput& out, RtldSpec spec, RtldArguments& arguments) {
    const auto* bytes = arguments.Next<const unsigned char*>();
    const char* separator = arguments.Next<const char*>();
    const char* digits = "0123456789abcdef";
    if (spec.width == 0) spec.width = 16;
    while (spec.width--) {
        out.Put(digits[*bytes >> 4]);
        out.Put(digits[*bytes & 0x0f]);
        ++bytes;
        if (spec.width != 0) out.Put(separator);
    }
}

void RtldStoreCount(const RtldSpec& spec, RtldArguments& arguments) {
    if (spec.maxSize || spec.quadSize || spec.longSize || spec.sizeSize) *arguments.Next<std::int64_t*>() = 0;
    else if (spec.shortSize) *arguments.Next<std::int16_t*>() = 0;
    else if (spec.charSize) *arguments.Next<char*>() = 0;
    else *arguments.Next<int*>() = 0;
}

void RtldFormat(RtldOutput& out, const char* format, RtldArguments& arguments) {
    if (format == nullptr) format = "(fmt null)\n";
    bool literal = false;
    for (;;) {
        int character = static_cast<unsigned char>(*format++);
        while (character != '%' || literal) {
            if (character == '\0') return;
            out.Put(character);
            character = static_cast<unsigned char>(*format++);
        }
        const char* percent = format - 1;
        RtldSpec spec;
        for (bool modifier = true; modifier;) {
            character = static_cast<unsigned char>(*format++);
            if (character == '0' && !spec.dot) {
                spec.pad = '0';
                continue;
            }
            if (character >= '0' && character <= '9') {
                int value = character - '0';
                while (*format >= '0' && *format <= '9') value = value * 10 + (*format++ - '0');
                (spec.dot ? spec.precision : spec.width) = value;
                continue;
            }
            modifier = false;
            switch (character) {
            case '.': spec.dot = modifier = true; break;
            case '#': spec.alternate = modifier = true; break;
            case '+': spec.sign = modifier = true; break;
            case '-': spec.left = modifier = true; break;
            case '*':
                if (spec.dot) spec.precision = arguments.Next<int>();
                else {
                    spec.width = arguments.Next<int>();
                    if (spec.width < 0) {
                        spec.left = !spec.left;
                        spec.width = -spec.width;
                    }
                }
                modifier = true;
                break;
            case 'h':
                if (spec.shortSize) spec.charSize = true;
                spec.shortSize = !spec.shortSize;
                modifier = true;
                break;
            case 'l':
                if (spec.longSize) spec.quadSize = true;
                spec.longSize = !spec.longSize;
                modifier = true;
                break;
            case 'j': spec.maxSize = modifier = true; break;
            case 'q': spec.quadSize = modifier = true; break;
            case 't': spec.ptrdiffSize = modifier = true; break;
            case 'z': spec.sizeSize = modifier = true; break;
            case '%': out.Put('%'); break;
            case 'b': RtldBits(out, arguments); break;
            case 'c': out.Put(arguments.Next<int>()); break;
            case 'D': RtldDump(out, spec, arguments); break;
            case 'n': RtldStoreCount(spec, arguments); break;
            case 's': RtldString(out, spec, arguments.Next<const char*>()); break;
            case 'p':
                spec.base = 16;
                spec.alternate = spec.width == 0;
                spec.sign = false;
                RtldNumber(out, spec, reinterpret_cast<std::uintptr_t>(arguments.Next<void*>()));
                break;
            case 'd':
            case 'i':
                spec.sign = true;
                RtldNumber(out, spec, RtldSigned(spec, arguments));
                break;
            case 'y':
                spec.base = 16;
                spec.sign = true;
                RtldNumber(out, spec, RtldSigned(spec, arguments));
                break;
            case 'r':
                spec.base = RtldRadix;
                RtldNumber(out, spec, spec.sign ? RtldSigned(spec, arguments) : RtldUnsigned(spec, arguments));
                break;
            case 'o':
            case 'u':
            case 'x':
            case 'X':
                spec.base = character == 'o' ? 8 : character == 'u' ? 10 : 16;
                spec.upper = character == 'X';
                spec.sign = false;
                RtldNumber(out, spec, RtldUnsigned(spec, arguments));
                break;
            default:
                while (percent < format) out.Put(*percent++);
                if (character == '\0') return;
                literal = true;
                break;
            }
        }
    }
}

}

extern "C" {

int APS5_VABI rtld_printf_nid_postfix(const char* format, ...) {
    RtldOutput out;
#ifdef _WIN32
    __builtin_sysv_va_list list;
    __builtin_sysv_va_start(list, format);
    RtldArguments arguments(list);
    RtldFormat(out, format, arguments);
    __builtin_sysv_va_end(list);
#else
    __builtin_va_list list;
    __builtin_va_start(list, format);
    RtldArguments arguments(list);
    RtldFormat(out, format, arguments);
    __builtin_va_end(list);
#endif
    out.Flush();
    return 0;
}

}
