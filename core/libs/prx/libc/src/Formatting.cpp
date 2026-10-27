#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <cstdarg>
#include <cctype>
#include <cstring>
#include <string>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <climits>
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include "prx/libc/include/WindowsFormatting.hpp"
#include "prx/libc/include/ScanWideField.hpp"

#ifdef _WIN32
#include "prx/libc/include/WindowsScanning.hpp"
#endif

#if defined(__GLIBC__)
extern "C" {
int __isoc99_vsscanf(const char*, const char*, std::va_list) noexcept;
int __isoc99_vfscanf(std::FILE*, const char*, std::va_list);
}
#endif

namespace {

#ifndef _WIN32
bool RequiresGuestFormatting(const char* format) {
    if (format == nullptr) return false;
    while ((format = std::strchr(format, '%')) != nullptr) {
        ++format;
        if (*format == '%') { ++format; continue; }
        while (*format && std::strchr("-+ #0'0123456789.*$", *format)) ++format;
        if (*format == 'S' || *format == 'C' || *format == 'p' ||
            (*format == 'l' && (format[1] == 's' || format[1] == 'c' || format[1] == 'p'))) return true;
        while (*format && std::strchr("hljztL", *format)) ++format;
        if (*format && !std::strchr(LibcDetail::KnownConversions, *format)) return true;
        if (*format && std::strchr("aAeEfFgG", *format)) return true;
        if (*format) ++format;
    }
    return false;
}

std::size_t HostFormatSize(std::size_t size) {
    return std::min(size, static_cast<std::size_t>(INT_MAX) + 1);
}

bool HasGuestWideScan(const char* format) {
    if (format == nullptr) return false;
    while ((format = std::strchr(format, '%')) != nullptr) {
        ++format;
        if (*format == '%') { ++format; continue; }
        while (*format && std::strchr("-+ #0'0123456789.*$", *format)) ++format;
        if (*format == 'S' || *format == 'C' || (*format == 'l' && format[1] != '\0' && std::strchr("sc[", format[1])))
            return true;
        if (*format == '[') {
            ++format;
            if (*format == '^') ++format;
            if (*format == ']') ++format;
            while (*format && *format != ']') ++format;
        }
        if (*format) ++format;
    }
    return false;
}

bool ScansWideInProcess(const char* format) {
    if (!HasGuestWideScan(format)) return false;
    LibcDetail::RequireNoPositionalWideScan(format);
    return true;
}

void RejectGuestWideScan(const char* format) {
    if (HasGuestWideScan(format)) NotImplemented_nid_no_patch("fscanf 16-bit wide conversion on Linux");
}

bool IsWideScan(const std::string& length, char conversion) {
    return conversion == 'S' || conversion == 'C' || (length == "l" && (conversion == 's' || conversion == 'c' || conversion == '['));
}

#endif

std::string HostLength(const std::string& length, char conversion) {
#ifdef _WIN32
    if (length == "l" && std::strchr("diouxXn", conversion) != nullptr) return "ll";
#else
    (void)conversion;
#endif
    return length;
}

int HostVsscanf(const char* input, const char* format, std::va_list args) {
#if defined(__GLIBC__)
    return __isoc99_vsscanf(input, format, args);
#else
    return std::vsscanf(input, format, args);
#endif
}

int HostSscanf(const char* input, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = HostVsscanf(input, format, args);
    va_end(args);
    return result;
}

#ifndef _WIN32
int HostVfscanf(std::FILE* stream, const char* format, std::va_list args) {
#if defined(__GLIBC__)
    return __isoc99_vfscanf(stream, format, args);
#else
    return std::vfscanf(stream, format, args);
#endif
}
#endif

int HostFscanf(std::FILE* stream, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
#ifdef _WIN32
    const int result = std::vfscanf(stream, format, args);
#else
    const int result = HostVfscanf(stream, format, args);
#endif
    va_end(args);
    return result;
}

class StringSource {
    const char* const start;
    const char* position;

public:
    static constexpr bool Contiguous = true;

    explicit StringSource(const char* text) : start(text), position(text) {}

    int Peek() const { return *position == '\0' ? EOF : static_cast<unsigned char>(*position); }
    void Skip() { ++position; }
    void Advance(std::ptrdiff_t count) { position += count; }
    std::ptrdiff_t Consumed() const { return position - start; }
    const char* Remaining() const { return position; }

    int Scan(const char* directive, bool suppress, void* target, int* consumed) const {
        return suppress ? HostSscanf(position, directive, consumed) : HostSscanf(position, directive, target, consumed);
    }
};

class StreamSource {
    std::FILE* const file;
    std::ptrdiff_t consumed = 0;

public:
    static constexpr bool Contiguous = false;

    explicit StreamSource(std::FILE* native) : file(native) {}

    int Peek() const {
        const int next = std::fgetc(file);
        if (next != EOF) std::ungetc(next, file);
        return next;
    }

    void Skip() {
        std::fgetc(file);
        ++consumed;
    }

    void Advance(std::ptrdiff_t count) { consumed += count; }
    std::ptrdiff_t Consumed() const { return consumed; }

    int Scan(const char* directive, bool suppress, void* target, int* count) const {
        return suppress ? HostFscanf(file, directive, count) : HostFscanf(file, directive, target, count);
    }
};

template <typename TSource, typename NextPointer, typename NextCapacity>
int ScanSource(TSource source, const char* format, bool secure, NextPointer nextPointer, NextCapacity nextCapacity) {
    if (format == nullptr) return EOF;
    int assigned = 0;
    bool converted = false;
    const char* cursor = format;
    const auto finish = [](int result) { return result; };
    while (*cursor != '\0') {
        if (std::isspace(static_cast<unsigned char>(*cursor))) {
            while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
            while (std::isspace(source.Peek())) source.Skip();
            continue;
        }
        if (*cursor != '%' || cursor[1] == '%') {
            const char expected = *cursor == '%' ? '%' : *cursor;
            cursor += *cursor == '%' ? 2 : 1;
            if (expected == '%') while (std::isspace(source.Peek())) source.Skip();
            if (source.Peek() != static_cast<unsigned char>(expected)) return finish(!converted && source.Peek() == EOF ? EOF : assigned);
            source.Skip();
            continue;
        }
        ++cursor;
        const bool suppress = *cursor == '*';
        if (suppress) ++cursor;
        unsigned long width = 0;
        while (std::isdigit(static_cast<unsigned char>(*cursor))) width = width * 10 + static_cast<unsigned long>(*cursor++ - '0');
        const char* lengthStart = cursor;
        while (std::strchr("hljztL", *cursor) != nullptr && *cursor != '\0') ++cursor;
        const std::string length(lengthStart, cursor);
        const char conversion = *cursor;
        if (conversion == '\0') return finish(assigned);
        std::string specifier(1, conversion);
        if (conversion == '[') {
            const char* setStart = cursor++;
            if (*cursor == '^') ++cursor;
            if (*cursor == ']') ++cursor;
            while (*cursor != '\0' && *cursor != ']') ++cursor;
            if (*cursor != ']') return finish(assigned);
            specifier.assign(setStart, cursor + 1);
        }
        ++cursor;
        if (conversion == 'n') {
            if (!suppress) {
                void* target = nextPointer();
                if (target == nullptr) return EOF;
                const auto count = source.Consumed();
                if (length == "hh") *static_cast<signed char*>(target) = static_cast<signed char>(count);
                else if (length == "h") *static_cast<short*>(target) = static_cast<short>(count);
                else if (length.empty()) *static_cast<int*>(target) = static_cast<int>(count);
                else if (length == "l" || length == "ll" || length == "j" || length == "z" || length == "t") {
                    const std::int64_t value = count;
                    std::memcpy(target, &value, sizeof(value));
                } else throw std::invalid_argument("Invalid scan count length");
            }
            continue;
        }
#ifndef _WIN32
        if constexpr (TSource::Contiguous) if (IsWideScan(length, conversion)) {
            const char* const input = source.Remaining();
            const char kind = conversion == 'S' ? 's' : conversion == 'C' ? 'c' : conversion;
            const char* field = input;
            if (kind == 's') {
                while (std::isspace(static_cast<unsigned char>(*field))) ++field;
            }
            if (*field == '\0') return finish(converted ? assigned : EOF);
            auto* target = suppress ? nullptr : static_cast<char16_t*>(nextPointer());
            unsigned long limit = ULONG_MAX;
            if (secure && !suppress) {
                const auto capacity = static_cast<unsigned long>(nextCapacity());
                limit = kind == 'c' ? capacity : (capacity == 0 ? 0 : capacity - 1);
                if (limit == 0 || (kind == 'c' && (width == 0 ? 1 : width) > limit)) {
                    if (capacity != 0) target[0] = u'\0';
                    return finish(assigned);
                }
            }
            const unsigned long wanted = width != 0 ? width : (kind == 'c' ? 1 : ULONG_MAX);
            const unsigned long taken = std::min(wanted, limit);
            std::u16string text;
            const auto accepts = [&](char32_t point) {
                if (kind == 's') return point >= 0x80 || !std::isspace(static_cast<int>(point));
                if (kind == '[') return LibcDetail::InWideScanset(specifier, point);
                return true;
            };
            while (text.size() < taken && *field != '\0') {
                char32_t point = 0;
                const std::size_t size = LibcDetail::ReadWideScanCharacter(field, &point);
                if (!accepts(point)) break;
                text.push_back(static_cast<char16_t>(point));
                field += size;
            }
            if (text.empty()) return finish(assigned);
            if (target != nullptr && kind != 'c' && wanted > limit && text.size() == limit && *field != '\0') {
                char32_t point = 0;
                LibcDetail::ReadWideScanCharacter(field, &point);
                if (accepts(point)) {
                    target[0] = u'\0';
                    return finish(assigned);
                }
            }
            if (target != nullptr) {
                std::memcpy(target, text.data(), text.size() * sizeof(char16_t));
                if (kind != 'c') target[text.size()] = u'\0';
                ++assigned;
            }
            source.Advance(field - input);
            converted = true;
            continue;
        }
#endif
        if (conversion != 'c' && conversion != '[') {
            while (std::isspace(source.Peek())) source.Skip();
            if (source.Peek() == EOF) return finish(converted ? assigned : EOF);
        }
        const bool sized = secure && (conversion == 's' || conversion == 'c' || conversion == '[');
        bool capped = false;
        std::string directive = "%";
        void* target = nullptr;
        if (suppress) directive += '*';
        else {
            target = nextPointer();
            if (target == nullptr) return EOF;
        }
        if (sized && !suppress) {
            const auto capacity = static_cast<unsigned long>(nextCapacity());
            const unsigned long limit = conversion == 'c' ? capacity : (capacity == 0 ? 0 : capacity - 1);
            const unsigned long wanted = conversion == 'c' ? (width == 0 ? 1 : width) : width;
            if (limit == 0 || (conversion == 'c' && wanted > limit)) {
                if (capacity != 0) static_cast<char*>(target)[0] = '\0';
                return finish(assigned);
            }
            capped = wanted == 0 || wanted > limit;
            width = wanted == 0 ? limit : std::min(wanted, limit);
        }
        if (width != 0) directive += std::to_string(width);
        directive += HostLength(length, conversion);
        directive += specifier;
        directive += "%n";
        int consumed = -1;
        const int matched = source.Scan(directive.c_str(), suppress, target, &consumed);
        if (consumed < 0 || (!suppress && matched != 1)) return finish(!converted && source.Peek() == EOF ? EOF : assigned);
        source.Advance(consumed);
        if (capped && conversion != 'c') {
            const int next = source.Peek();
            bool overflow = false;
            if (conversion == 's') overflow = next != EOF && !std::isspace(next);
            else if (next != EOF) {
                char probe[2];
                const char text[2] = {static_cast<char>(next), '\0'};
                const std::string test = "%1" + specifier;
                overflow = std::sscanf(text, test.c_str(), probe) == 1;
            }
            if (overflow) {
                static_cast<char*>(target)[0] = '\0';
                return finish(assigned);
            }
        }
        converted = true;
        if (!suppress) ++assigned;
    }
    return finish(assigned);
}

template <typename NextPointer, typename NextCapacity>
int ScanGuest(const char* buffer, const char* format, bool secure, NextPointer nextPointer, NextCapacity nextCapacity) {
    if (buffer == nullptr) return EOF;
    return ScanSource(StringSource(buffer), format, secure, nextPointer, nextCapacity);
}

template <typename NextPointer, typename NextCapacity>
int ScanGuestStream(FileStream* stream, const char* format, NextPointer nextPointer, NextCapacity nextCapacity) {
    if (stream == nullptr || format == nullptr) return EOF;
    if (!stream->IsOpen()) { errno = 9; return EOF; }
    auto* native = GetNativeStream(stream);
    if (!stream->SwitchToReading()) return EOF;
#ifndef _WIN32
    RejectGuestWideScan(format);
#endif
    const int result = ScanSource(StreamSource(native), format, true, nextPointer, nextCapacity);
    stream->SyncStatus();
    return result;
}

}

extern "C" {

int APS5_VABI vasprintf_nid_postfix(char** destination, const char* format, VaList* args) {
    if (!destination) { errno = 22; return -1; }
    *destination = nullptr;
    if (!format || !args) { errno = 22; return -1; }
    try {
#ifdef _WIN32
        std::string text;
        const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &text);
        const char* source = text.c_str();
#else
        std::string guestText;
        const bool guestFormatting = RequiresGuestFormatting(format);
        char* text = nullptr;
        std::va_list copy;
        va_copy(copy, *reinterpret_cast<std::va_list*>(args));
        const int count = guestFormatting ? LibcDetail::FormatWindows(nullptr, 0, format, args, &guestText) : ::vasprintf(&text, format, copy);
        va_end(copy);
        std::unique_ptr<char, decltype(&std::free)> owner(text, std::free);
        if (count < 0) return -1;
        const char* source = guestFormatting ? guestText.c_str() : text;
#endif
        auto* output = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(static_cast<size_t>(count) + 1));
        if (!output) {
            errno = 12;
            return -1;
        }
        std::memcpy(output, source, static_cast<size_t>(count) + 1);
        *destination = output;
        return count;
    } catch (const std::bad_alloc&) {
        errno = 12;
        return -1;
    }
}

int APS5_VABI vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
#ifdef _WIN32
    std::string buffer;
    const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &buffer);
    const int result = std::fwrite(buffer.data(), 1, static_cast<size_t>(count), native) ==
        static_cast<size_t>(count) ? count : -1;
#else
    if (RequiresGuestFormatting(format)) {
        std::string buffer;
        const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &buffer);
        const int result = std::fwrite(buffer.data(), 1, static_cast<size_t>(count), native) == static_cast<size_t>(count) ? count : -1;
        stream->SyncStatus();
        return result;
    }
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = std::vfprintf(native, format, copy);
    va_end(copy);
#endif
    stream->SyncStatus();
    return result;
}

int APS5_VABI fprintf_nid_postfix(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfprintf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int APS5_VABI fscanf_nid_postfix(FileStream* stream, const char* format, ...) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    if (!stream->SwitchToReading()) return EOF;
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::ScanFileWindows_nid_no_patch(native, format, args);
    __builtin_sysv_va_end(args);
#else
    RejectGuestWideScan(format);
    std::va_list args;
    va_start(args, format);
    const int result = HostVfscanf(native, format, args);
    va_end(args);
#endif
    stream->SyncStatus();
    stream->KeepDirectoryReadError(result == -1);
    return result;
}

#ifdef _WIN32

int APS5_VABI printf_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI snprintf_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::FormatWindows(buffer, size, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI sprintf_nid_postfix(char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::FormatWindows(buffer, SIZE_MAX, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI printf_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    std::va_list args;
    va_start(args, format);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::PrintWindows(format, args) : std::vfprintf(GuestStandardOutputFile_nid_no_patch(), format, args);
    va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    std::va_list args;
    va_start(args, format);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::PrintWindows(format, args) : std::vfprintf(GuestStandardOutputFile_nid_no_patch(), format, args);
    va_end(args);
    return result;
}

int APS5_VABI snprintf_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::FormatWindows(buffer, size, format, args) : std::vsnprintf(buffer, HostFormatSize(size), format, args);
    va_end(args);
    return result;
}

int APS5_VABI sprintf_nid_postfix(char* buffer, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::FormatWindows(buffer, SIZE_MAX, format, args) : std::vsprintf(buffer, format, args);
    va_end(args);
    return result;
}

#endif

int APS5_VABI vsscanf_nid_postfix(const char* input, const char* format, VaList* args) {
#ifdef _WIN32
    return LibcDetail::ScanWindows(input, format, args);
#else
    if (ScansWideInProcess(format)) {
        LibcDetail::FormatArguments arguments(args);
        return ScanGuest(input, format, false, [&] { return arguments.Next<void*>(); }, [] { return 0u; });
    }
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = HostVsscanf(input, format, copy);
    va_end(copy);
    return result;
#endif
}

#ifdef _WIN32
int APS5_VABI sscanf_nid_postfix(const char* input, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = vsscanf_nid_postfix(input, format, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
    return result;
}
#else
int APS5_VABI sscanf_nid_postfix(const char* input, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = ScansWideInProcess(format) ? ScanGuest(input, format, false, [&] { return va_arg(args, void*); }, [] { return 0u; })
                                                  : HostVsscanf(input, format, args);
    va_end(args);
    return result;
}
#endif

#ifdef _WIN32

int APS5_VABI sscanf_s_nid_postfix(const char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = ScanGuest(buffer, format, true, [&] { return __builtin_va_arg(args, void*); }, [&] { return __builtin_va_arg(args, unsigned int); });
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI sscanf_s_nid_postfix(const char* buffer, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = ScanGuest(buffer, format, true, [&] { return va_arg(args, void*); }, [&] { return va_arg(args, unsigned int); });
    va_end(args);
    return result;
}

#endif

int APS5_VABI vprintf_nid_postfix(const char* str, VaList* c) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
#ifdef _WIN32
    return LibcDetail::PrintWindows(str, c);
#else
    if (RequiresGuestFormatting(str)) return LibcDetail::PrintWindows(str, c);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(c));
    const int result = std::vfprintf(GuestStandardOutputFile_nid_no_patch(), str, copy);
    va_end(copy);
    return result;
#endif
}

int APS5_VABI vsprintf_nid_postfix(char* str, const char* format, VaList* args) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, SIZE_MAX, format, args);
#else
    if (RequiresGuestFormatting(format)) return LibcDetail::FormatWindows(str, SIZE_MAX, format, args);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = std::vsprintf(str, format, copy);
    va_end(copy);
    return result;
#endif
}

int APS5_VABI vsnprintf_nid_postfix(char* str, size_t size, const char* format, VaList* c) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, size, format, c);
#else
    if (RequiresGuestFormatting(format)) return LibcDetail::FormatWindows(str, size, format, c);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(c));
    const int result = std::vsnprintf(str, HostFormatSize(size), format, copy);
    va_end(copy);
    return result;
#endif
}

static int FormatSecure(char* buffer, size_t size, const char* format, const void* args) {
    constexpr size_t RsizeMax = SIZE_MAX >> 1;
    int result = -1;
    if (buffer != nullptr && format != nullptr && args != nullptr && size != 0 && size <= RsizeMax) {
        try {
            result = LibcDetail::FormatWindows(buffer, size, format, args, nullptr, true);
        } catch (const LibcDetail::FormatConstraintViolation&) {
        }
    }
    if (result < 0 && buffer != nullptr && size != 0 && size <= RsizeMax) buffer[0] = 0;
    return result;
}

int APS5_VABI vsnprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    return FormatSecure(buffer, size, format, args);
}

static int WriteSecure(FileStream* stream, const char* format, const void* args) {
    if (stream == nullptr || format == nullptr || args == nullptr) return -1;
    if (!stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    std::string text;
    int count;
    try {
        count = LibcDetail::FormatWindows(nullptr, 0, format, args, &text, true);
    } catch (const LibcDetail::FormatConstraintViolation&) {
        return -1;
    }
    const int result = std::fwrite(text.data(), 1, static_cast<size_t>(count), native) == static_cast<size_t>(count) ? count : -1;
    stream->SyncStatus();
    return result;
}

int APS5_VABI vfprintf_s_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    return WriteSecure(stream, format, args);
}

int APS5_VABI vprintf_s_nid_postfix(const char* format, VaList* args) {
    return WriteSecure(&_Stdout_nid_postfix, format, args);
}

int APS5_VABI fprintf_s_nid_postfix(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfprintf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

#ifdef _WIN32

int APS5_VABI vsscanf_s_nid_postfix(const char* buffer, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuest(buffer, format, true, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned int>(); });
}

int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = FormatSecure(buffer, size, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI printf_s_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI vsscanf_s_nid_postfix(const char* buffer, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuest(buffer, format, true, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned int>(); });
}

int APS5_VABI vsprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    if (RequiresGuestFormatting(format)) return LibcDetail::FormatWindows(buffer, size, format, args);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = std::vsnprintf(buffer, size, format, copy);
    va_end(copy);
    return result;
}

int APS5_VABI sprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::FormatWindows(buffer, size, format, args) : std::vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = FormatSecure(buffer, size, format, args);
    va_end(args);
    return result;
}

int APS5_VABI printf_s_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    std::va_list args;
    va_start(args, format);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::PrintWindows(format, args) : std::vfprintf(GuestStandardOutputFile_nid_no_patch(), format, args);
    va_end(args);
    return result;
}

#endif

int APS5_VABI puts_nid_postfix(const char* s) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(&_Stdout_nid_postfix);
    if (std::fputs(s, native) < 0 || std::fputc('\n', native) == EOF) return EOF;
    return 0;
}

extern FileStream* __stdinp_nid_postfix;

int APS5_VABI vfscanf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    if (!stream->SwitchToReading()) return EOF;
#ifdef _WIN32
    const int result = LibcDetail::ScanFileWindows_nid_no_patch(native, format, args);
#else
    RejectGuestWideScan(format);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = HostVfscanf(native, format, copy);
    va_end(copy);
#endif
    stream->SyncStatus();
    stream->KeepDirectoryReadError(result == -1);
    return result;
}

int APS5_VABI vscanf_nid_postfix(const char* format, VaList* args) {
    return vfscanf_nid_postfix(__stdinp_nid_postfix, format, args);
}

int APS5_VABI scanf_nid_postfix(const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfscanf_nid_postfix(__stdinp_nid_postfix, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int APS5_VABI vfscanf_s_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuestStream(stream, format, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned int>(); });
}

int APS5_VABI vscanf_s_nid_postfix(const char* format, VaList* args) {
    return vfscanf_s_nid_postfix(__stdinp_nid_postfix, format, args);
}

int APS5_VABI fscanf_s_nid_postfix(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfscanf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int APS5_VABI scanf_s_nid_postfix(const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfscanf_s_nid_postfix(__stdinp_nid_postfix, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

}
