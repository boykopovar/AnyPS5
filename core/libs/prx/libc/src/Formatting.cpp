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
#include <stdexcept>
#include <vector>

#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include "prx/libc/include/WindowsFormatting.hpp"

#ifdef _WIN32
#include "prx/libc/include/WindowsScanning.hpp"
#endif

#ifndef _WIN32
extern "C" std::size_t APS5_VABI mbrtowc_nid_postfix(std::uint16_t* destination, const char* source, std::size_t count, void* state);
#endif

#if defined(__GLIBC__)
extern "C" {
int __isoc99_vsscanf(const char*, const char*, std::va_list) noexcept;
int __isoc99_vfscanf(std::FILE*, const char*, std::va_list);
}
#endif

namespace {

#ifndef _WIN32
bool HasGuestWideFormat(const char* format) {
    if (format == nullptr) return false;
    while ((format = std::strchr(format, '%')) != nullptr) {
        ++format;
        if (*format == '%') { ++format; continue; }
        while (*format && std::strchr("-+ #0'0123456789.*$", *format)) ++format;
        if (*format == 'S' || *format == 'C' || *format == 'm' || (*format == 'l' && (format[1] == 's' || format[1] == 'c'))) return true;
        if (*format) ++format;
    }
    return false;
}

struct GuestWideScan {
    bool wide = false;
    bool extension = false;
};

GuestWideScan FindGuestWideScan(const char* format) {
    GuestWideScan found;
    if (format == nullptr) return found;
    while ((format = std::strchr(format, '%')) != nullptr) {
        ++format;
        if (*format == '%') { ++format; continue; }
        while (*format && std::strchr("-+ #0'0123456789.*$", *format)) {
            if (*format == '$' || *format == '\'') found.extension = true;
            ++format;
        }
        const char* modifier = format;
        while (*format && std::strchr("hljqztL", *format)) ++format;
        const bool longOnly = format - modifier == 1 && *modifier == 'l';
        if (*format == 'S' || *format == 'C' || (longOnly && *format != '\0' && std::strchr("sc[", *format)))
            found.wide = true;
        if (*format == '[') {
            ++format;
            if (*format == '^') ++format;
            if (*format == ']') ++format;
            while (*format && *format != ']') ++format;
        }
        if (*format) ++format;
    }
    return found;
}

void CheckGuestWideScan(const char* format) {
    const GuestWideScan found = FindGuestWideScan(format);
    if (found.wide && found.extension)
        throw std::runtime_error("scanf: 16-bit wide conversions combined with positional arguments or the ' flag are not supported");
}

bool HasGuestWideScan(const char* format) {
    CheckGuestWideScan(format);
    return FindGuestWideScan(format).wide;
}

void WidenGuestScan(const char* bytes, std::size_t count, bool terminate, void* target) {
    std::vector<std::uint16_t> units(count + (terminate ? 1 : 0), 0);
    for (std::size_t index = 0; index < count; ++index) mbrtowc_nid_postfix(&units[index], bytes + index, 1, nullptr);
    std::memcpy(target, units.data(), units.size() * sizeof(std::uint16_t));
}
#endif

std::string HostLength(const std::string& length, char conversion) {
    if (std::strchr("sc[", conversion) != nullptr) return length == "l" ? length : "";
#ifdef _WIN32
    if (length == "q") return std::strchr("diouxX", conversion) != nullptr ? "ll" : "";
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

#if defined(__GLIBC__)
int HostFscanf(std::FILE* stream, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = HostVfscanf(stream, format, args);
    va_end(args);
    return result;
}
#endif

bool IsScanSpace(int character) {
    return character != EOF && std::isspace(static_cast<unsigned char>(character));
}

class StringScanSource {
public:
    static constexpr bool allocatesWide = false;

    explicit StringScanSource(const char* buffer) : start(buffer), input(buffer) {}

    int Peek() const { return *input == '\0' ? EOF : static_cast<unsigned char>(*input); }
    void Advance() { ++input; }
    std::ptrdiff_t Count() const { return input - start; }
    std::size_t Remaining() const { return std::strlen(input); }

    int Scan(const std::string& directive, void* target, int& consumed) {
        const int matched = target ? HostSscanf(input, directive.c_str(), target, &consumed) : HostSscanf(input, directive.c_str(), &consumed);
        if (consumed > 0) input += consumed;
        return matched;
    }

private:
    const char* start;
    const char* input;
};

#if defined(__GLIBC__)
class StreamScanSource {
public:
    static constexpr bool allocatesWide = true;

    explicit StreamScanSource(std::FILE* stream) : stream(stream) { flockfile(stream); }
    ~StreamScanSource() { funlockfile(stream); }
    StreamScanSource(const StreamScanSource&) = delete;
    StreamScanSource& operator=(const StreamScanSource&) = delete;

    int Peek() const {
        const int character = std::getc(stream);
        if (character != EOF) std::ungetc(character, stream);
        return character;
    }
    void Advance() {
        if (std::getc(stream) != EOF) ++count;
    }
    std::ptrdiff_t Count() const { return count; }

    int Scan(const std::string& directive, void* target, int& consumed) {
        const int matched = target ? HostFscanf(stream, directive.c_str(), target, &consumed) : HostFscanf(stream, directive.c_str(), &consumed);
        if (consumed > 0) count += consumed;
        return matched;
    }

private:
    std::FILE* stream;
    std::ptrdiff_t count = 0;
};
#endif

template <typename TSource, typename NextPointer, typename NextCapacity>
int ScanGuestSource(TSource& source, const char* format, bool secure, NextPointer nextPointer, NextCapacity nextCapacity) {
    int assigned = 0;
    bool converted = false;
    const char* cursor = format;
    const auto finish = [](int result) { return result; };
    while (*cursor != '\0') {
        if (std::isspace(static_cast<unsigned char>(*cursor))) {
            while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
            while (IsScanSpace(source.Peek())) source.Advance();
            continue;
        }
        if (*cursor != '%' || cursor[1] == '%') {
            const char expected = *cursor == '%' ? '%' : *cursor;
            cursor += *cursor == '%' ? 2 : 1;
            if (expected == '%') while (IsScanSpace(source.Peek())) source.Advance();
            const int next = source.Peek();
            if (next != static_cast<unsigned char>(expected)) return finish(!converted && next == EOF ? EOF : assigned);
            source.Advance();
            continue;
        }
        ++cursor;
        const bool suppress = *cursor == '*';
        if (suppress) ++cursor;
        unsigned long width = 0;
        while (std::isdigit(static_cast<unsigned char>(*cursor))) width = width * 10 + static_cast<unsigned long>(*cursor++ - '0');
        const char* lengthStart = cursor;
        while (std::strchr("hljqztL", *cursor) != nullptr && *cursor != '\0') ++cursor;
        const std::string length(lengthStart, cursor);
        const char conversion = *cursor;
        if (conversion == '\0') return finish(assigned);
#ifdef _WIN32
        const bool wide = false;
        const char narrow = conversion;
#else
        const bool wide = conversion == 'S' || conversion == 'C' || (length == "l" && std::strchr("cs[", conversion) != nullptr);
        const char narrow = conversion == 'S' ? 's' : conversion == 'C' ? 'c' : conversion;
#endif
        std::string specifier(1, narrow);
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
                const auto count = source.Count();
                if (length == "hh") *static_cast<signed char*>(target) = static_cast<signed char>(count);
                else if (length == "h") *static_cast<short*>(target) = static_cast<short>(count);
                else if (length.empty()) *static_cast<int*>(target) = static_cast<int>(count);
                else if (length == "l" || length == "ll" || length == "q" || length == "j" || length == "z" || length == "t") {
                    const std::int64_t value = count;
                    std::memcpy(target, &value, sizeof(value));
                } else throw std::invalid_argument("Invalid scan count length");
            }
            continue;
        }
        if (narrow != 'c' && narrow != '[') {
            while (IsScanSpace(source.Peek())) source.Advance();
            if (source.Peek() == EOF) return finish(converted ? assigned : EOF);
        }
        const bool sized = secure && (narrow == 's' || narrow == 'c' || narrow == '[');
        bool capped = false;
        std::string directive = "%";
        void* target = nullptr;
        if (suppress) directive += '*';
        else target = nextPointer();
        const auto clearTarget = [&] {
            if (wide) {
                const std::uint16_t terminator = 0;
                std::memcpy(target, &terminator, sizeof(terminator));
            } else {
                static_cast<char*>(target)[0] = '\0';
            }
        };
        if (sized && !suppress) {
            const auto capacity = static_cast<unsigned long>(nextCapacity());
            const unsigned long limit = narrow == 'c' ? capacity : (capacity == 0 ? 0 : capacity - 1);
            const unsigned long wanted = narrow == 'c' ? (width == 0 ? 1 : width) : width;
            if (limit == 0 || (narrow == 'c' && wanted > limit)) {
                if (capacity != 0) clearTarget();
                return finish(assigned);
            }
            capped = wanted == 0 || wanted > limit;
            width = wanted == 0 ? limit : std::min(wanted, limit);
        }
        if (width != 0) directive += std::to_string(width);
        if (wide && !suppress && TSource::allocatesWide) directive += 'm';
        if (!wide) directive += HostLength(length, conversion);
        directive += specifier;
        directive += "%n";
        std::string wideBytes;
        char* allocatedBytes = nullptr;
        if constexpr (!TSource::allocatesWide) {
            if (wide && !suppress) {
                const std::size_t remaining = source.Remaining();
                const std::size_t wanted = narrow == 'c' && width == 0 ? 1 : width;
                wideBytes.assign((wanted == 0 ? remaining : std::min<std::size_t>(wanted, remaining)) + 1, '\0');
            }
        }
        void* hostTarget = target;
        if (wide && !suppress) hostTarget = TSource::allocatesWide ? static_cast<void*>(&allocatedBytes) : static_cast<void*>(wideBytes.data());
        const bool exhausted = source.Peek() == EOF;
        int consumed = -1;
        const int matched = source.Scan(directive, suppress ? nullptr : hostTarget, consumed);
        const std::unique_ptr<char, decltype(&std::free)> allocatedOwner(allocatedBytes, std::free);
        if (consumed < 0 || (!suppress && matched != 1)) return finish(!converted && exhausted ? EOF : assigned);
        if (capped && narrow != 'c') {
            const int next = source.Peek();
            bool overflow = false;
            if (narrow == 's') overflow = next != EOF && !IsScanSpace(next);
            else if (next != EOF) {
                const char text[2] = {static_cast<char>(next), '\0'};
                char probe[2];
                const std::string test = "%1" + specifier;
                overflow = std::sscanf(text, test.c_str(), probe) == 1;
            }
            if (overflow) {
                clearTarget();
                return finish(assigned);
            }
        }
#ifndef _WIN32
        if (wide && !suppress) {
            const char* bytes = TSource::allocatesWide ? allocatedBytes : wideBytes.data();
            const std::size_t stored = TSource::allocatesWide || narrow == 'c' ? static_cast<std::size_t>(consumed) : std::strlen(bytes);
            WidenGuestScan(bytes, stored, narrow != 'c', target);
        }
#endif
        converted = true;
        if (!suppress) ++assigned;
    }
    return finish(assigned);
}

template <typename NextPointer, typename NextCapacity>
int ScanGuest(const char* buffer, const char* format, bool secure, NextPointer nextPointer, NextCapacity nextCapacity) {
    if (buffer == nullptr || format == nullptr) return EOF;
    StringScanSource source(buffer);
    return ScanGuestSource(source, format, secure, nextPointer, nextCapacity);
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
        const bool guestWide = HasGuestWideFormat(format);
        char* text = nullptr;
        std::va_list copy;
        va_copy(copy, *reinterpret_cast<std::va_list*>(args));
        const int count = guestWide ? LibcDetail::FormatWindows(nullptr, 0, format, args, &guestText) : ::vasprintf(&text, format, copy);
        va_end(copy);
        std::unique_ptr<char, decltype(&std::free)> owner(text, std::free);
        if (count < 0) return -1;
        const char* source = guestWide ? guestText.c_str() : text;
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
    if (HasGuestWideFormat(format)) {
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
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::ScanFileWindows_nid_no_patch(native, format, args);
    __builtin_sysv_va_end(args);
#else
    const bool wide = HasGuestWideScan(format);
#if !defined(__GLIBC__)
    if (wide) throw std::runtime_error("fscanf: 16-bit wide conversions need the m modifier of glibc");
#endif
    std::va_list args;
    va_start(args, format);
    int result = EOF;
#if defined(__GLIBC__)
    if (wide) {
        StreamScanSource source(native);
        result = ScanGuestSource(source, format, false, [&] { return va_arg(args, void*); }, [] { return 0u; });
    } else {
        result = HostVfscanf(native, format, args);
    }
#else
    result = HostVfscanf(native, format, args);
#endif
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
    const int result = HasGuestWideFormat(format) ? LibcDetail::PrintWindows(format, args) : std::vprintf(format, args);
    va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    std::va_list args;
    va_start(args, format);
    const int result = HasGuestWideFormat(format) ? LibcDetail::PrintWindows(format, args) : std::vprintf(format, args);
    va_end(args);
    return result;
}

int APS5_VABI snprintf_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = HasGuestWideFormat(format) ? LibcDetail::FormatWindows(buffer, size, format, args) : std::vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int APS5_VABI sprintf_nid_postfix(char* buffer, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = HasGuestWideFormat(format) ? LibcDetail::FormatWindows(buffer, SIZE_MAX, format, args) : std::vsprintf(buffer, format, args);
    va_end(args);
    return result;
}

#endif

int APS5_VABI vsscanf_nid_postfix(const char* input, const char* format, VaList* args) {
#ifdef _WIN32
    return LibcDetail::ScanWindows(input, format, args);
#else
    const bool wide = HasGuestWideScan(format);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = wide ? ScanGuest(input, format, false, [&] { return va_arg(copy, void*); }, [] { return 0u; })
                            : HostVsscanf(input, format, copy);
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
    const bool wide = HasGuestWideScan(format);
    std::va_list args;
    va_start(args, format);
    const int result = wide ? ScanGuest(input, format, false, [&] { return va_arg(args, void*); }, [] { return 0u; })
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
    CheckGuestWideScan(format);
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
    if (HasGuestWideFormat(str)) return LibcDetail::PrintWindows(str, c);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(c));
    const int result = std::vprintf(str, copy);
    va_end(copy);
    return result;
#endif
}

int APS5_VABI vsprintf_nid_postfix(char* str, const char* format, VaList* args) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, SIZE_MAX, format, args);
#else
    if (HasGuestWideFormat(format)) return LibcDetail::FormatWindows(str, SIZE_MAX, format, args);
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
    if (HasGuestWideFormat(format)) return LibcDetail::FormatWindows(str, size, format, c);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(c));
    const int result = std::vsnprintf(str, size, format, copy);
    va_end(copy);
    return result;
#endif
}

#ifdef _WIN32

int APS5_VABI vsscanf_s_nid_postfix(const char* buffer, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuest(buffer, format, true, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned int>(); });
}

int APS5_VABI vsnprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    return LibcDetail::FormatWindows(buffer, size, format, args);
}

int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::FormatWindows(buffer, size, format, args);
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
    CheckGuestWideScan(format);
    LibcDetail::FormatArguments arguments(args);
    return ScanGuest(buffer, format, true, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned int>(); });
}

int APS5_VABI vsnprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    if (HasGuestWideFormat(format)) return LibcDetail::FormatWindows(buffer, size, format, args);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = std::vsnprintf(buffer, size, format, copy);
    va_end(copy);
    return result;
}

int APS5_VABI vsprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    if (HasGuestWideFormat(format)) return LibcDetail::FormatWindows(buffer, size, format, args);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = std::vsnprintf(buffer, size, format, copy);
    va_end(copy);
    return result;
}

int APS5_VABI sprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = HasGuestWideFormat(format) ? LibcDetail::FormatWindows(buffer, size, format, args) : std::vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = HasGuestWideFormat(format) ? LibcDetail::FormatWindows(buffer, size, format, args) : std::vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int APS5_VABI printf_s_nid_postfix(const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = HasGuestWideFormat(format) ? LibcDetail::PrintWindows(format, args) : std::vprintf(format, args);
    va_end(args);
    return result;
}

#endif

int APS5_VABI puts_nid_postfix(const char* s) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    return std::puts(s);
}

}
