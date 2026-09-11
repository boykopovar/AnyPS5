#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSFORMATTING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSFORMATTING_HPP

#include "SceTypes.hpp"
#include <cstdio>
#include <climits>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <type_traits>

namespace LibcDetail {

class FormatArguments {
    VaList args;

public:
    explicit FormatArguments(const void* source) {
        std::memcpy(&args, source, sizeof(args));
    }

    template<class T> T Next() {
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

inline int FormatWindows(char* buffer, size_t size, const char* format, const void* source, std::string* complete = nullptr) {
    if (!format || !source) throw std::invalid_argument("Null formatting argument");
    FormatArguments args(source);
    FormatOutput output(buffer, size, complete);
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
        std::string spec = "%";
        while (*format && std::strchr("-+ #0", *format)) spec += *format++;
        if (*format == '*') {
            ++format;
            const int width = args.Next<int>();
            if (width < 0) spec += '-';
            spec += std::to_string(width < 0 ? -static_cast<long long>(width) : width);
        } else {
            while (*format >= '0' && *format <= '9') spec += *format++;
        }
        if (*format == '.') {
            ++format;
            if (*format == '*') {
                ++format;
                const int precision = args.Next<int>();
                if (precision >= 0) spec += "." + std::to_string(precision);
            } else {
                spec += '.';
                while (*format >= '0' && *format <= '9') spec += *format++;
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
            unsigned long long value;
            if (length.empty() || length == "h" || length == "hh") {
                value = args.Next<unsigned int>();
                if (length == "h") value = static_cast<unsigned short>(value);
                if (length == "hh") value = static_cast<unsigned char>(value);
            } else value = args.Next<unsigned long long>();
            output.Value(spec + "ll" + conversion, value);
        } else if (std::strchr("aAeEfFgG", conversion)) {
            if (length == "L") {
                static_assert(sizeof(long double) == 16);
                static_assert(std::numeric_limits<long double>::digits == 64);
                output.Value(spec + "L" + conversion, args.Next<long double>());
            } else {
                if (!length.empty() && length != "l") throw std::invalid_argument("Invalid floating length");
                output.Value(spec + conversion, args.Next<double>());
            }
        } else if (conversion == 'c' && length.empty()) {
            output.Value(spec + conversion, args.Next<int>());
        } else if (conversion == 's' && length.empty()) {
            const char* value = args.Next<const char*>();
            if (!value) throw std::invalid_argument("Null formatted string");
            output.Value(spec + conversion, value);
        } else if (conversion == 'p' && length.empty()) {
            output.Value(spec + conversion, args.Next<void*>());
        } else if (conversion == 'n' && integerLength && spec == "%") {
            void* pointer = args.Next<void*>();
            if (!pointer) throw std::invalid_argument("Null format count pointer");
            const int count = output.Count();
            if (length == "hh") *static_cast<signed char*>(pointer) = static_cast<signed char>(count);
            else if (length == "h") *static_cast<short*>(pointer) = static_cast<short>(count);
            else if (length.empty()) *static_cast<int*>(pointer) = count;
            else *static_cast<long long*>(pointer) = count;
        } else {
            throw std::invalid_argument("Unsupported format conversion");
        }
    }
    return output.Count();
}

inline int PrintWindows(const char* format, const void* args) {
    std::string buffer;
    const int size = FormatWindows(nullptr, 0, format, args, &buffer);
    if (std::fwrite(buffer.data(), 1, static_cast<size_t>(size), stdout) != static_cast<size_t>(size))
        throw std::runtime_error("Formatted output write failed");
    return size;
}

}

#endif
