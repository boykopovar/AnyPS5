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
        if (*format == 'S' || *format == 'C' || *format == 'm' || *format == 'p' ||
            (*format == 'l' && (format[1] == 's' || format[1] == 'c' || format[1] == 'p'))) return true;
        if (*format) ++format;
    }
    return false;
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

#ifndef _WIN32
int HostFscanf(std::FILE* stream, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = HostVfscanf(stream, format, args);
    va_end(args);
    return result;
}
#endif

bool IsWideScan(const std::string& length, char conversion) {
    return conversion == 'S' || conversion == 'C' || (length == "l" && std::strchr("sc[", conversion) != nullptr);
}

bool InScanSet(const std::string& specifier, int character) {
    if (character == 0) return specifier.size() > 1 && specifier[1] == '^';
    const char text[2] = {static_cast<char>(character), '\0'};
    char probe[2];
    return HostSscanf(text, ("%1" + specifier).c_str(), probe) == 1;
}

void ClearFirst(void* target, bool wide) {
    if (wide) *static_cast<char16_t*>(target) = 0;
    else *static_cast<char*>(target) = '\0';
}

class BufferInput {
public:
    explicit BufferInput(const char* buffer) : start(buffer), cursor(buffer) {}

    int Peek() const { return *cursor == '\0' ? EOF : static_cast<unsigned char>(*cursor); }

    void Next() { ++cursor; }

    std::ptrdiff_t Offset() const { return cursor - start; }

    int Scan(const std::string& directive, void* target) {
        const std::string text = directive + "%n";
        int consumed = -1;
        const int matched = target ? HostSscanf(cursor, text.c_str(), target, &consumed) : HostSscanf(cursor, text.c_str(), &consumed);
        if (consumed < 0 || (target && matched != 1)) return matched == EOF ? EOF : 0;
        cursor += consumed;
        return 1;
    }

private:
    const char* start;
    const char* cursor;
};

#ifndef _WIN32
class StreamInput {
public:
    explicit StreamInput(FileStream* source) : stream(source->GetHandle()), owner(source) {}

    ~StreamInput() { owner->SyncStatus(); }

    int Peek() {
        const int character = std::getc(stream);
        if (character != EOF && std::ungetc(character, stream) == EOF) throw std::runtime_error("Cannot restore scanf lookahead");
        return character;
    }

    void Next() {
        if (std::getc(stream) != EOF) ++offset;
    }

    std::ptrdiff_t Offset() const { return offset; }

    int Scan(const std::string& directive, void* target) {
        const std::string text = directive + "%n";
        int consumed = -1;
        const int matched = target ? HostFscanf(stream, text.c_str(), target, &consumed) : HostFscanf(stream, text.c_str(), &consumed);
        if (consumed < 0 || (target && matched != 1)) return matched == EOF ? EOF : 0;
        offset += consumed;
        return 1;
    }

private:
    std::FILE* stream;
    FileStream* owner;
    std::ptrdiff_t offset = 0;
};
#endif

#ifndef _WIN32
bool AcceptsWideScan(const std::string& specifier, bool set, bool single, int character) {
    if (character == EOF) return false;
    if (set) return LibcDetail::InWideScanset(specifier, character < 0x80 ? character : 0x80);
    return single || character >= 0x80 || !std::isspace(character);
}

template <typename TInput>
std::size_t ReadWideScan(TInput& input, char32_t* point) {
    char text[4]{};
    const int first = input.Peek();
    if (first == EOF) return 0;
    text[0] = static_cast<char>(first);
    if (first >= 0x80 && (first < 0xc2 || first >= 0xf0))
        return LibcDetail::ReadWideScanCharacter(text, point);
    const std::size_t wanted = first < 0x80 ? 1 : first < 0xe0 ? 2 : 3;
    input.Next();
    for (std::size_t index = 1; index < wanted; ++index) {
        const int next = input.Peek();
        if (next == EOF) break;
        text[index] = static_cast<char>(next);
        if ((next & 0xc0) != 0x80) break;
        input.Next();
    }
    return LibcDetail::ReadWideScanCharacter(text, point);
}
#endif

template <typename TInput, typename NextPointer, typename NextCapacity>
int ScanGuest(TInput& input, const char* format, bool secure, NextPointer nextPointer, NextCapacity nextCapacity) {
    if (format == nullptr) return EOF;
#ifndef _WIN32
    if (HasGuestWideScan(format)) LibcDetail::RequireNoPositionalWideScan(format);
#endif
    int assigned = 0;
    bool converted = false;
    const char* cursor = format;
    const auto failure = [&] { return !converted && input.Peek() == EOF ? EOF : assigned; };
    while (*cursor != '\0') {
        if (std::isspace(static_cast<unsigned char>(*cursor))) {
            while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
            while (std::isspace(input.Peek())) input.Next();
            continue;
        }
        if (*cursor != '%' || cursor[1] == '%') {
            const char expected = *cursor == '%' ? '%' : *cursor;
            cursor += *cursor == '%' ? 2 : 1;
            if (expected == '%') while (std::isspace(input.Peek())) input.Next();
            if (input.Peek() != static_cast<unsigned char>(expected)) return failure();
            input.Next();
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
        if (conversion == '\0') return assigned;
        const bool wide = IsWideScan(length, conversion);
        if (!wide && std::strchr("diouxXaAeEfFgGcspn[", conversion) == nullptr)
            throw std::runtime_error(std::string("Unsupported scan conversion: ") + conversion);
        std::string specifier(1, conversion);
        if (conversion == '[') {
            const char* setStart = cursor++;
            if (*cursor == '^') ++cursor;
            if (*cursor == ']') ++cursor;
            while (*cursor != '\0' && *cursor != ']') ++cursor;
            if (*cursor != ']') return assigned;
            specifier.assign(setStart, cursor + 1);
        }
        ++cursor;
        if (conversion == 'n') {
            if (!suppress) {
                void* target = nextPointer();
                if (target == nullptr) return EOF;
                const auto count = input.Offset();
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
        const bool single = conversion == 'c' || conversion == 'C';
        if (!single && conversion != '[') {
            while (std::isspace(input.Peek())) input.Next();
            if (input.Peek() == EOF) return failure();
        }
        const bool sized = secure && (single || conversion == 's' || conversion == 'S' || conversion == '[');
        bool capped = false;
        void* target = nullptr;
        if (!suppress) {
            target = nextPointer();
            if (target == nullptr) return EOF;
        }
        if (sized && !suppress) {
            const auto capacity = static_cast<unsigned long>(nextCapacity());
            const unsigned long limit = single ? capacity : (capacity == 0 ? 0 : capacity - 1);
            const unsigned long wanted = single ? (width == 0 ? 1 : width) : width;
            if (limit == 0 || (single && wanted > limit)) {
                if (capacity != 0) ClearFirst(target, wide);
                return assigned;
            }
            capped = wanted == 0 || wanted > limit;
            width = wanted == 0 ? limit : std::min(wanted, limit);
        }
        const bool set = conversion == '[';
        if (wide) {
            const std::size_t maximum = width != 0 ? width : (single ? 1 : SIZE_MAX);
#ifndef _WIN32
            std::u16string text;
            while (text.size() < maximum) {
                char32_t character = 0;
                if (!AcceptsWideScan(specifier, set, single, input.Peek())) break;
                if (ReadWideScan(input, &character) == 0) break;
                text.push_back(static_cast<char16_t>(character));
            }
            if (text.empty()) return failure();
            if (!suppress) {
                std::memcpy(target, text.data(), text.size() * sizeof(char16_t));
                if (!single) static_cast<char16_t*>(target)[text.size()] = 0;
            }
#else
            std::size_t count = 0;
            while (count < maximum) {
                const int character = input.Peek();
                if (character == EOF || (set ? !InScanSet(specifier, character) : (!single && std::isspace(character)))) break;
                if (!suppress) static_cast<char16_t*>(target)[count] = static_cast<char16_t>(character);
                input.Next();
                ++count;
            }
            if (count == 0) return failure();
            if (!suppress && !single) static_cast<char16_t*>(target)[count] = 0;
#endif
        } else {
            std::string directive = "%";
            if (suppress) directive += '*';
            if (width != 0) directive += std::to_string(width);
            directive += HostLength(length, conversion);
            directive += specifier;
            const int result = input.Scan(directive, target);
            if (result != 1) return result == EOF && !converted ? EOF : assigned;
        }
        if (capped && !single) {
            const int next = input.Peek();
            bool continues = next != EOF && (set ? InScanSet(specifier, next) : !std::isspace(next));
#ifndef _WIN32
            if (wide) continues = AcceptsWideScan(specifier, set, false, next);
#endif
            if (continues) {
                ClearFirst(target, wide);
                return assigned;
            }
        }
        converted = true;
        if (!suppress) ++assigned;
    }
    return assigned;
}

template <typename NextPointer, typename NextCapacity>
int ScanGuestBuffer(const char* buffer, const char* format, bool secure, NextPointer nextPointer, NextCapacity nextCapacity) {
    if (buffer == nullptr) return EOF;
    BufferInput input(buffer);
    return ScanGuest(input, format, secure, nextPointer, nextCapacity);
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
    std::va_list args;
    va_start(args, format);
    StreamInput input(stream);
    const int result = HasGuestWideScan(format)
        ? ScanGuest(input, format, false, [&] { return va_arg(args, void*); }, [] { return 0u; })
        : HostVfscanf(native, format, args);
    va_end(args);
#endif
    stream->SyncStatus();
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
    const int result = RequiresGuestFormatting(format) ? LibcDetail::FormatWindows(buffer, size, format, args) : std::vsnprintf(buffer, size, format, args);
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
    if (HasGuestWideScan(format)) {
        LibcDetail::FormatArguments arguments(args);
        return ScanGuestBuffer(input, format, false, [&] { return arguments.Next<void*>(); }, [] { return 0u; });
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
    const int result = HasGuestWideScan(format)
        ? ScanGuestBuffer(input, format, false, [&] { return va_arg(args, void*); }, [] { return 0u; })
        : HostVsscanf(input, format, args);
    va_end(args);
    return result;
}
#endif

#ifdef _WIN32

int APS5_VABI sscanf_s_nid_postfix(const char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = ScanGuestBuffer(buffer, format, true, [&] { return __builtin_va_arg(args, void*); }, [&] { return __builtin_va_arg(args, unsigned int); });
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI sscanf_s_nid_postfix(const char* buffer, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = ScanGuestBuffer(buffer, format, true, [&] { return va_arg(args, void*); }, [&] { return va_arg(args, unsigned int); });
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
    const int result = std::vsnprintf(str, size, format, copy);
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

#ifdef _WIN32

int APS5_VABI vsscanf_s_nid_postfix(const char* buffer, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuestBuffer(buffer, format, true, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned int>(); });
}

int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = FormatSecure(buffer, size, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI printf_s_nid_postfix(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI vsscanf_s_nid_postfix(const char* buffer, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuestBuffer(buffer, format, true, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned int>(); });
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
#ifdef _WIN32
    const int result = LibcDetail::ScanFileWindows_nid_no_patch(native, format, args);
#else
    if (HasGuestWideScan(format)) {
        LibcDetail::FormatArguments arguments(args);
        StreamInput input(stream);
        const int result = ScanGuest(input, format, false, [&] { return arguments.Next<void*>(); }, [] { return 0u; });
        stream->SyncStatus();
        return result;
    }
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = HostVfscanf(native, format, copy);
    va_end(copy);
#endif
    stream->SyncStatus();
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

}
