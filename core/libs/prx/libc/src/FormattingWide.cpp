#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <stdexcept>
#include <string>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"

#ifdef _WIN32
#define APS5_VA_BEGIN(last) __builtin_sysv_va_list args; __builtin_sysv_va_start(args, last)
#define APS5_VA_END() __builtin_sysv_va_end(args)
#else
#define APS5_VA_BEGIN(last) std::va_list args; va_start(args, last)
#define APS5_VA_END() va_end(args)
#endif

namespace {

bool In(char16_t character, const char* set) {
    return character != 0 && character < 128 && std::strchr(set, static_cast<char>(character)) != nullptr;
}

void AppendUtf16(std::u16string& out, const char* text, std::size_t limit) {
    for (std::size_t index = 0; out.size() < limit && text[index] != '\0';) {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        std::size_t extra = lead >= 0xf0 ? 3 : lead >= 0xe0 ? 2 : lead >= 0xc0 ? 1 : 0;
        std::uint32_t code = extra == 3 ? lead & 0x07 : extra == 2 ? lead & 0x0f : extra == 1 ? lead & 0x1f : lead;
        std::size_t used = 1;
        for (; used <= extra; ++used) {
            const unsigned char next = static_cast<unsigned char>(text[index + used]);
            if ((next & 0xc0) != 0x80) { extra = 0; code = lead; used = 1; break; }
            code = (code << 6) | (next & 0x3f);
        }
        if (extra != 0) used = extra + 1;
        index += used;
        if (code >= 0x10000) {
            if (limit - out.size() < 2) break;
            code -= 0x10000;
            out.push_back(static_cast<char16_t>(0xd800 + (code >> 10)));
            out.push_back(static_cast<char16_t>(0xdc00 + (code & 0x3ff)));
        } else out.push_back(static_cast<char16_t>(code));
    }
}

std::string ToUtf8(const std::u16string& text) {
    std::string out;
    for (std::size_t index = 0; index < text.size(); ++index) {
        std::uint32_t code = text[index];
        if (code >= 0xd800 && code < 0xdc00 && index + 1 < text.size() && text[index + 1] >= 0xdc00 && text[index + 1] < 0xe000)
            code = 0x10000 + ((code - 0xd800) << 10) + (text[++index] - 0xdc00);
        if (code < 0x80) out.push_back(static_cast<char>(code));
        else if (code < 0x800) {
            out.push_back(static_cast<char>(0xc0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xe0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xf0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }
    return out;
}

template<class TValue> void AppendNumber(std::u16string& out, const std::string& spec, TValue value) {
    const int size = std::snprintf(nullptr, 0, spec.c_str(), value);
    if (size < 0) throw std::runtime_error("Formatting conversion failed");
    std::string text(static_cast<std::size_t>(size) + 1, '\0');
    std::snprintf(&text[0], text.size(), spec.c_str(), value);
    out.append(text.begin(), text.begin() + size);
}

void AppendPadded(std::u16string& out, const std::u16string& text, bool left, int width) {
    const std::size_t pad = width > 0 && static_cast<std::size_t>(width) > text.size() ? static_cast<std::size_t>(width) - text.size() : 0;
    if (!left) out.append(pad, u' ');
    out += text;
    if (left) out.append(pad, u' ');
}

std::u16string FormatWide(const char16_t* format, VaList* source, bool secure = false) {
    if (format == nullptr || source == nullptr) throw std::invalid_argument("Null formatting argument");
    LibcDetail::FormatArguments args(source);
    std::u16string out;
    while (*format) {
        if (*format != u'%') { out.push_back(*format++); continue; }
        ++format;
        if (*format == u'%') { out.push_back(*format++); continue; }
        std::string spec = "%";
        bool left = false;
        int width = 0;
        int precision = -1;
        while (In(*format, "-+ #0")) {
            if (*format == u'-') left = true;
            spec += static_cast<char>(*format++);
        }
        if (*format == u'*') {
            ++format;
            width = args.Next<int>();
            if (width < 0) { left = true; spec += '-'; width = -width; }
            spec += std::to_string(width);
        } else {
            while (*format >= u'0' && *format <= u'9') { width = width * 10 + (*format - u'0'); spec += static_cast<char>(*format++); }
        }
        if (*format == u'.') {
            ++format;
            precision = 0;
            if (*format == u'*') {
                ++format;
                precision = args.Next<int>();
                if (precision >= 0) spec += "." + std::to_string(precision);
            } else {
                spec += '.';
                while (*format >= u'0' && *format <= u'9') { precision = precision * 10 + (*format - u'0'); spec += static_cast<char>(*format++); }
            }
        }
        std::string length;
        if (In(*format, "hljztL")) {
            length += static_cast<char>(*format++);
            if ((length == "h" && *format == u'h') || (length == "l" && *format == u'l')) length += static_cast<char>(*format++);
        }
        const char16_t conversion = *format;
        if (!conversion) throw std::invalid_argument("Incomplete format conversion");
        ++format;
        const bool integerLength = length.empty() || length == "h" || length == "hh" || length == "l" || length == "ll" ||
            length == "j" || length == "z" || length == "t";
        if (conversion == u'd' || conversion == u'i') {
            if (!integerLength) throw std::invalid_argument("Invalid integer length");
            long long value;
            if (length.empty() || length == "h" || length == "hh") {
                value = args.Next<int>();
                if (length == "h") value = static_cast<short>(value);
                if (length == "hh") value = static_cast<signed char>(value);
            } else value = args.Next<long long>();
            AppendNumber(out, spec + "ll" + static_cast<char>(conversion), value);
        } else if (In(conversion, "ouxX")) {
            if (!integerLength) throw std::invalid_argument("Invalid integer length");
            unsigned long long value;
            if (length.empty() || length == "h" || length == "hh") {
                value = args.Next<unsigned int>();
                if (length == "h") value = static_cast<unsigned short>(value);
                if (length == "hh") value = static_cast<unsigned char>(value);
            } else value = args.Next<unsigned long long>();
            AppendNumber(out, spec + "ll" + static_cast<char>(conversion), value);
        } else if (In(conversion, "aAeEfFgG")) {
            if (length == "L") AppendNumber(out, spec + "L" + static_cast<char>(conversion), args.Next<long double>());
            else {
                if (!length.empty() && length != "l") throw std::invalid_argument("Invalid floating length");
                AppendNumber(out, spec + static_cast<char>(conversion), args.Next<double>());
            }
        } else if (conversion == u'p' && length.empty()) {
            AppendNumber(out, spec + 'p', args.Next<void*>());
        } else if (conversion == u'c' || conversion == u'C') {
            const int value = args.Next<int>();
            std::u16string text;
            if (conversion == u'C' || length == "l") text.push_back(static_cast<char16_t>(value));
            else text.push_back(static_cast<char16_t>(static_cast<unsigned char>(value)));
            AppendPadded(out, text, left, width);
        } else if (conversion == u's' || conversion == u'S') {
            std::u16string text;
            const std::size_t limit = precision < 0 ? SIZE_MAX : static_cast<std::size_t>(precision);
            if (conversion == u'S' || length == "l") {
                const char16_t* value = args.Next<const char16_t*>();
                if (value == nullptr && secure) throw std::invalid_argument("Null string argument");
                if (value == nullptr) value = u"(null)";
                for (std::size_t index = 0; index < limit && value[index] != 0; ++index) text.push_back(value[index]);
            } else {
                const char* value = args.Next<const char*>();
                if (value == nullptr && secure) throw std::invalid_argument("Null string argument");
                AppendUtf16(text, value != nullptr ? value : "(null)", limit);
            }
            AppendPadded(out, text, left, width);
        } else if (conversion == u'n' && integerLength && spec == "%" && !secure) {
            void* pointer = args.Next<void*>();
            if (!pointer) throw std::invalid_argument("Null format count pointer");
            const int count = static_cast<int>(out.size());
            if (length == "hh") *static_cast<signed char*>(pointer) = static_cast<signed char>(count);
            else if (length == "h") *static_cast<short*>(pointer) = static_cast<short>(count);
            else if (length.empty()) *static_cast<int*>(pointer) = count;
            else *static_cast<long long*>(pointer) = count;
        } else {
            throw std::invalid_argument("Unsupported format conversion");
        }
    }
    return out;
}

}  // namespace

extern "C" {

int APS5_VABI vswprintf_nid_postfix(char16_t* buffer, std::size_t size, const char16_t* format, VaList* args) {
    if (buffer == nullptr || size == 0) { errno = 22; return -1; }
    try {
        const std::u16string text = FormatWide(format, args);
        const std::size_t copied = text.size() < size - 1 ? text.size() : size - 1;
        std::memcpy(buffer, text.data(), copied * sizeof(char16_t));
        buffer[copied] = 0;
        return copied == text.size() ? static_cast<int>(copied) : -1;
    } catch (const std::exception&) {
        buffer[0] = 0;
        errno = 22;
        return -1;
    }
}

int APS5_VABI swprintf_nid_postfix(char16_t* buffer, std::size_t size, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vswprintf_nid_postfix(buffer, size, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

int APS5_VABI snwprintf_s_nid_postfix(char16_t* buffer, std::size_t size, const char16_t* format, ...) {
    constexpr std::size_t RsizeMax = SIZE_MAX >> 1;
    int result = -1;
    if (buffer != nullptr && format != nullptr && size != 0 && size <= RsizeMax) {
        APS5_VA_BEGIN(format);
        try {
            const std::u16string text = FormatWide(format, reinterpret_cast<VaList*>(args), true);
            if (text.size() <= static_cast<std::size_t>(INT_MAX)) {
                const std::size_t copied = text.size() < size - 1 ? text.size() : size - 1;
                std::memcpy(buffer, text.data(), copied * sizeof(char16_t));
                buffer[copied] = 0;
                result = static_cast<int>(text.size());
            }
        } catch (const std::invalid_argument&) {
        }
        APS5_VA_END();
    }
    if (result < 0 && buffer != nullptr && size != 0 && size < RsizeMax) buffer[0] = 0;
    return result;
}

int APS5_VABI wprintf_nid_postfix(const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    int result = -1;
    try {
        const std::u16string text = FormatWide(format, reinterpret_cast<VaList*>(args));
        const std::string bytes = ToUtf8(text);
        if (std::fwrite(bytes.data(), 1, bytes.size(), stdout) == bytes.size()) result = static_cast<int>(text.size());
    } catch (const std::exception&) {
        errno = 22;
    }
    APS5_VA_END();
    return result;
}

int APS5_VABI fputwc_nid_postfix(char16_t value, FileStream* stream) {
    if (stream == nullptr) { errno = 22; return -1; }
    try {
        const std::string bytes = ToUtf8(std::u16string(1, value));
        auto* native = GetNativeStream(stream);
        if (std::fwrite(bytes.data(), 1, bytes.size(), native) != bytes.size()) throw std::runtime_error("stream write failed");
        stream->SyncStatus();
        return value;
    } catch (const std::exception&) {
        errno = 22;
        return -1;
    }
}

int APS5_VABI fputws_nid_postfix(const char16_t* str, FileStream* stream) {
    if (str == nullptr || stream == nullptr) { errno = 22; return -1; }
    try {
        const std::u16string text(str);
        const std::string bytes = ToUtf8(text);
        auto* native = GetNativeStream(stream);
        if (std::fwrite(bytes.data(), 1, bytes.size(), native) != bytes.size()) throw std::runtime_error("stream write failed");
        stream->SyncStatus();
        return static_cast<int>(text.size());
    } catch (const std::exception&) {
        errno = 22;
        return -1;
    }
}

}
