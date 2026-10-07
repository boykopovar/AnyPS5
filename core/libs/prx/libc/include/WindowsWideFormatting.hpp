#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSWIDEFORMATTING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSWIDEFORMATTING_HPP

#include "WindowsFormatting.hpp"
#include "General.hpp"
#include <cerrno>
#include <cwchar>
#include <deque>

namespace LibcDetail {

inline int FormatWideWindows(wchar_t* output, size_t capacity, const wchar_t* format, const void* source) {
    if (!capacity)
        return -1;
    if (!output || !format || !source) {
        errno = 22;
        return -1;
    }
    static_assert(sizeof(wchar_t) == 2);
    FormatArguments args(source);
    std::wstring translated;
    std::wstring result;
    std::vector<std::uint64_t> slots;
    std::deque<long double> extended;
    auto append = [&slots](auto value) {
        static_assert(sizeof(value) <= 8);
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(value));
        slots.push_back(bits);
    };
    while (*format) {
        const wchar_t value = *format++;
        if (value != L'%') {
            result += value;
            continue;
        }
        if (*format == L'%') {
            result += *format++;
            continue;
        }
        translated = L"%";
        slots.clear();
        extended.clear();
        bool nullCharacter = false;
        while (*format && std::wcschr(L"-+ #0", *format))
            translated += *format++;
        if (*format == L'*') {
            translated += *format++;
            append(args.Next<int>());
        } else
            while (*format >= L'0' && *format <= L'9')
                translated += *format++;
        if (*format == L'.') {
            translated += *format++;
            if (*format == L'*') {
                translated += *format++;
                append(args.Next<int>());
            } else
                while (*format >= L'0' && *format <= L'9')
                    translated += *format++;
        }
        std::wstring length;
        if (*format && std::wcschr(L"hljztL", *format)) {
            length += *format++;
            if ((length == L"h" && *format == L'h') || (length == L"l" && *format == L'l'))
                length += *format++;
        }
        const wchar_t conversion = *format;
        if (!conversion) {
            errno = 22;
            return -1;
        }
        if (!std::wcschr(L"diouxXaAeEfFgGcspn", conversion)) {
            NotImplemented_nid_no_patch("swprintf format conversion");
            return -1;
        }
        ++format;
        if (std::wcschr(L"diouxXn", conversion) &&
            (length == L"l" || length == L"j" || length == L"z" || length == L"t"))
            translated += L"ll";
        else if (conversion == L's' && length.empty())
            translated += L"h";
        else if (conversion == L'c' && length.empty())
            translated += L"l";
        else
            translated += length;
        translated += conversion;
        if (conversion == L'n') {
            void* pointer = args.Next<void*>();
            if (length == L"hh")
                *static_cast<signed char*>(pointer) = static_cast<signed char>(result.size());
            else if (length == L"h")
                *static_cast<short*>(pointer) = static_cast<short>(result.size());
            else if (length.empty())
                *static_cast<int*>(pointer) = static_cast<int>(result.size());
            else
                *static_cast<std::int64_t*>(pointer) = static_cast<std::int64_t>(result.size());
            continue;
        }
        if (conversion == L's' || conversion == L'p')
            append(args.Next<void*>());
        else if (conversion == L'c') {
            const int character = args.Next<int>();
            const int converted = length.empty() ? static_cast<int>(static_cast<unsigned char>(character)) : character;
            nullCharacter = converted == 0;
            append(nullCharacter ? 1 : converted);
        } else if (std::wcschr(L"aAeEfFgG", conversion)) {
            if (length == L"L") {
                extended.push_back(args.Next<long double>());
                append(&extended.back());
            } else
                append(args.Next<double>());
        } else if (length.empty() || length == L"h" || length == L"hh")
            append(args.Next<unsigned int>());
        else
            append(args.Next<std::uint64_t>());
        if (result.size() >= capacity)
            return -1;
        std::vector<wchar_t> part(capacity - result.size());
        const int count =
            std::vswprintf(part.data(), part.size(), translated.c_str(), reinterpret_cast<char*>(slots.data()));
        if (count < 0 || static_cast<size_t>(count) >= part.size())
            return -1;
        if (nullCharacter) {
            for (int i = 0; i < count; ++i)
                if (part[i] == 1)
                    part[i] = 0;
        }
        result.append(part.data(), static_cast<size_t>(count));
    }
    if (result.size() >= capacity || result.size() > INT_MAX)
        return -1;
    std::memcpy(output, result.c_str(), (result.size() + 1) * sizeof(wchar_t));
    return static_cast<int>(result.size());
}

}

#endif
