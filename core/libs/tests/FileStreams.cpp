#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <iterator>
#include <algorithm>
#include <barrier>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <atomic>
#include <exception>
#include <fcntl.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

static void Require(bool condition, const char* message = "File stream check failed") {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
static void ExpectException(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("Expected a stream exception");
}

static void ReplaceWithReadOnlyDescriptor(FileStream* stream, const char* path) {
#ifdef _WIN32
    const int descriptor = _open(path, _O_RDONLY | _O_BINARY);
    Require(descriptor >= 0 && _dup2(descriptor, _fileno(GetNativeStream(stream))) == 0);
    Require(_close(descriptor) == 0);
#else
    const int descriptor = open(path, O_RDONLY);
    Require(descriptor >= 0 && dup2(descriptor, fileno(GetNativeStream(stream))) >= 0);
    Require(close(descriptor) == 0);
#endif
}

static void CheckExclusiveCppOpen(const std::string& base) {
    constexpr int Out = 0x02, Binary = 0x80, Noreplace = 0x40;
    const std::string path = base + ".exclusive";
    std::remove(path.c_str());
    auto* stream = _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), Out | Binary | Noreplace, 0);
    Require(stream != nullptr);
    constexpr char original[] = "keep existing contents";
    Require(fwrite_nid_postfix(original, 1, sizeof(original), stream) == sizeof(original));
    Require(fclose_nid_postfix(stream) == 0);
#ifdef _WIN32
    const auto nativePath = std::filesystem::path(path).wstring();
    const auto writer = CreateFileW(nativePath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(writer != INVALID_HANDLE_VALUE);
    auto* refused = _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), Out | Binary | Noreplace, 0);
    if (refused) fclose_nid_postfix(refused);
    Require(CloseHandle(writer) != 0);
    Require(refused == nullptr, "C++ noreplace truncated an existing write-shared file");
#endif
    stream = fopen_nid_postfix(path.c_str(), "rb");
    Require(stream != nullptr);
    std::array<char, sizeof(original)> saved{};
    Require(fread_nid_postfix(saved.data(), 1, saved.size(), stream) == saved.size());
    Require(std::memcmp(saved.data(), original, saved.size()) == 0);
    Require(fclose_nid_postfix(stream) == 0 && std::remove(path.c_str()) == 0);
    for (const int mode : {0x42, 0x52, 0x53, 0xc2, 0xd2, 0xd3}) {
        stream = _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), mode | 0x04, 0);
        Require(stream != nullptr && ftell_nid_postfix(stream) == 0);
        Require(fwrite_nid_postfix(original, 1, sizeof(original), stream) == sizeof(original));
        Require(fclose_nid_postfix(stream) == 0);
        errno = 0;
        Require(_ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), mode, 0) == nullptr && errno == EEXIST);
        stream = fopen_nid_postfix(path.c_str(), "rb");
        Require(stream != nullptr && fread_nid_postfix(saved.data(), 1, saved.size(), stream) == saved.size());
        Require(std::memcmp(saved.data(), original, saved.size()) == 0);
        Require(fclose_nid_postfix(stream) == 0 && std::remove(path.c_str()) == 0);
    }
    for (int round = 0; round < 12; ++round) {
        std::array<FileStream*, 2> created{};
        std::barrier start(2);
        const auto create = [&](int index) {
            start.arrive_and_wait();
            created[index] = _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(path.c_str(), Out | Binary | Noreplace, 0);
        };
        std::thread first(create, 0), second(create, 1);
        first.join();
        second.join();
        const auto winners = static_cast<int>(created[0] != nullptr) + static_cast<int>(created[1] != nullptr);
        for (auto* opened : created) if (opened) Require(fclose_nid_postfix(opened) == 0);
        Require(winners == 1 && std::remove(path.c_str()) == 0);
    }
}

static int APS5_VABI ScanShortCharacterList(FileStream* stream, const char* format, ...) {
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

static void CheckShortCharacterScanLocking() {
    constexpr std::size_t bytes = 1u << 20;
    using Scanner = int (APS5_VABI*)(FileStream*, const char*, ...);
    for (const Scanner scan : {fscanf_s_nid_postfix, ScanShortCharacterList}) {
        auto* native = std::tmpfile();
        Require(native != nullptr);
        const std::vector<char> input(bytes, 'a');
        Require(std::fwrite(input.data(), 1, input.size(), native) == input.size());
        std::rewind(native);
        FileStream stream(native);
        std::vector<char> output(bytes + 1, 'Z');
        int result = -1, consumed = -1;
        std::exception_ptr failure;
        std::atomic<bool> finished{false};
        std::barrier start(2);
        std::thread reader([&] {
            start.arrive_and_wait();
            try {
                result = scan(&stream, "%1048576c%n", output.data(), output.size(), &consumed);
            } catch (...) {
                failure = std::current_exception();
            }
            finished.store(true);
        });
        start.arrive_and_wait();
        bool releasedDuringField = false;
        while (!finished.load()) {
            if (ftrylockfile_nid_postfix(&stream) == 0) {
                const auto position = ftell_nid_postfix(&stream);
                releasedDuringField = releasedDuringField || (position > 0 && position < static_cast<long>(bytes));
                funlockfile_nid_postfix(&stream);
            }
            std::this_thread::yield();
        }
        reader.join();
        if (failure) std::rethrow_exception(failure);
        Require(ftrylockfile_nid_postfix(&stream) == 0, "Short stream character scan retained its file lock after returning");
        funlockfile_nid_postfix(&stream);
        Require(std::fclose(native) == 0);
        Require(!releasedDuringField, "Short stream character scan released its file lock during a field");
        Require(result == 1 && consumed == static_cast<int>(bytes) && output.back() == 'Z');
        Require(std::memcmp(output.data(), input.data(), bytes) == 0);
    }
}

static void CheckShortCharacterScans() {
    CheckShortCharacterScanLocking();
    using Scanner = int (APS5_VABI*)(FileStream*, const char*, ...);
    for (const Scanner scan : {fscanf_s_nid_postfix, ScanShortCharacterList}) {
        for (const std::string input : {std::string(), std::string("a"), std::string("ab"), std::string("a\0b", 3), std::string("a b")}) {
            for (const unsigned width : {1u, 2u, 8u}) {
                const auto taken = std::min<std::size_t>(input.size(), width);
                for (const bool suppress : {false, true}) {
                    auto* native = std::tmpfile();
                    Require(native != nullptr && std::fwrite(input.data(), 1, input.size(), native) == input.size());
                    std::rewind(native);
                    FileStream stream(native);
                    std::array<char, 32> output;
                    output.fill('Z');
                    int consumed = -1;
                    const std::string format = std::string("%") + (suppress ? "*" : "") + std::to_string(width) + "c%n";
                    const int result = suppress ? scan(&stream, format.c_str(), &consumed) : scan(&stream, format.c_str(), output.data(), 32u, &consumed);
                    Require(result == (taken == 0 ? EOF : (suppress ? 0 : 1)) && consumed == (taken == 0 ? -1 : static_cast<int>(taken)), "Short stream character scan result or consumption");
                    const auto written = suppress ? 0 : taken;
                    Require(std::memcmp(output.data(), input.data(), written) == 0);
                    for (std::size_t i = written; i < output.size(); ++i) Require(output[i] == 'Z', "Short stream character scan destination guard");
                    Require(ftell_nid_postfix(&stream) == static_cast<long>(taken));
                    Require((feof_nid_postfix(&stream) != 0) == (input.size() < width));
                    Require(ferror_nid_postfix(&stream) == 0);
                    Require(std::fclose(native) == 0);
                }
            }
        }
    }
}

static int APS5_VABI ScanList(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfscanf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

static void CheckScanInputFailureErrno(const std::string& path) {
    const int savedError = errno;
    for (const bool variadicList : {false, true}) {
        for (const int seed : {0, 22, 97}) {
            for (const char* input : {"", " "}) {
                for (const char* format : {"%d", "%*d"}) {
                    auto* native = std::tmpfile();
                    Require(native != nullptr);
                    Require(std::fwrite(input, 1, std::strlen(input), native) == std::strlen(input));
                    std::rewind(native);
                    FileStream stream(native);
                    int value = 123;
                    errno = seed;
                    const int result = variadicList ? ScanList(&stream, format, &value) : fscanf_nid_postfix(&stream, format, &value);
                    Require(result == EOF && errno == seed, "Stream scan input exhaustion changed errno");
                    Require(value == 123 && feof_nid_postfix(&stream) != 0 && ferror_nid_postfix(&stream) == 0);
                    Require(std::fclose(native) == 0);
                }
            }
        }
#if defined(__GLIBC__)
        auto* errorNative = std::tmpfile();
        Require(errorNative != nullptr);
        const int writeOnly = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        Require(writeOnly >= 0);
        Require(::dup2(writeOnly, ::fileno(errorNative)) >= 0 && ::close(writeOnly) == 0);
        FileStream readError(errorNative);
        int unchanged = 123;
        errno = 97;
        const int failed = variadicList ? ScanList(&readError, "%d", &unchanged) : fscanf_nid_postfix(&readError, "%d", &unchanged);
        Require(failed == EOF && errno != 97, "Stream scan lost its read error");
        Require(unchanged == 123 && ferror_nid_postfix(&readError) != 0, "Stream scan did not keep its read error indicator");
        Require(std::fclose(errorNative) == 0 && std::remove(path.c_str()) == 0);
#endif
        auto* native = std::tmpfile();
        Require(native != nullptr && std::fputs("1e99999", native) >= 0);
        std::rewind(native);
        FileStream stream(native);
        float value = 0;
        errno = 97;
        const int result = variadicList ? ScanList(&stream, "%f", &value) : fscanf_nid_postfix(&stream, "%f", &value);
        Require(result == 1 && errno == ERANGE, "Stream scan lost its conversion range error");
        Require(std::fclose(native) == 0);
    }
    errno = savedError;
}

static std::string ReadWhole(const std::string& path) {
    auto* stream = fopen_nid_postfix(path.c_str(), "rb");
    Require(stream != nullptr);
    std::array<char, 64> buffer{};
    const auto size = fread_nid_postfix(buffer.data(), 1, buffer.size(), stream);
    Require(fclose_nid_postfix(stream) == 0);
    return std::string(buffer.data(), size);
}

static int APS5_VABI ScanSafeList(FileStream* stream, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = vfscanf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
    return result;
}

static void CheckCharacterScanLengths() {
    using Scanner = int (APS5_VABI*)(FileStream*, const char*, ...);
    const Scanner scanners[]{fscanf_nid_postfix, ScanList, fscanf_s_nid_postfix, ScanSafeList};
    for (std::size_t api = 0; api < std::size(scanners); ++api) {
#ifdef _WIN32
        if (api < 2) continue;
#endif
        for (const char* length : {"hh", "h", "ll", "j", "z", "t", "L"}) {
            for (const char* conversion : {"s", "c", "[a-z]"}) {
                auto* native = std::tmpfile();
                Require(native != nullptr && std::fputs("ab", native) >= 0);
                std::rewind(native);
                FileStream stream(native);
                std::array<char, 32> output;
                output.fill('Z');
                int consumed = -1;
                const std::string format = std::string("%2") + length + conversion + "%n";
                const auto scan = scanners[api];
                const int result = api >= 2 ? scan(&stream, format.c_str(), output.data(), 32u, &consumed) : scan(&stream, format.c_str(), output.data(), &consumed);
                Require(result == 1 && consumed == 2, "Stream character scan ignored-length consumption");
                const bool single = conversion[0] == 'c';
                Require(output[0] == 'a' && output[1] == 'b' && (single || output[2] == 0), "Stream character scan ignored length stored wide data");
                Require(std::all_of(output.begin() + (single ? 2 : 3), output.end(), [](char value) { return value == 'Z'; }));
                Require(std::fclose(native) == 0);
            }
        }
    }
}

template<typename TRead>
static void CheckReadAfterWrite(const std::string& path, const char* mode, const char* expected, TRead read) {
    auto* stream = fopen_nid_postfix(path.c_str(), "wb");
    Require(stream != nullptr && fputs_nid_postfix("0123456789", stream) >= 0 && fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(path.c_str(), mode);
    Require(stream != nullptr && fwrite_nid_postfix("AB", 1, 2, stream) == 2);
    read(stream);
    Require(ferror_nid_postfix(stream) == 0, "Read after write on an update stream set the error indicator");
    Require(fclose_nid_postfix(stream) == 0);
    Require(ReadWhole(path) == expected, "Read after write on an update stream changed the file");
}

static void CheckUpdateStreamReadsAfterWrites(const std::string& base) {
    CheckScanInputFailureErrno(base + ".scan-error");
    const std::string path = base + ".update";
    constexpr char overwritten[] = "AB23456789";
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(fread_nid_postfix(buffer.data(), 1, 3, stream) == 3, "fread after fwrite on an r+ stream");
        Require(std::strcmp(buffer.data(), "234") == 0, "fread after fwrite on an r+ stream read other bytes");
        Require(ftell_nid_postfix(stream) == 5);
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::vector<char> buffer(65536);
        Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == 8, "fread of more than the stream buffer after fwrite on an r+ stream");
        Require(std::string_view(buffer.data(), 8) == "23456789", "fread of more than the stream buffer after fwrite on an r+ stream read other bytes");
        Require(ftell_nid_postfix(stream) == 10);
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        Require(fgetc_nid_postfix(stream) == '2', "fgetc after fwrite on an r+ stream");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        Require(getc_unlocked_nid_postfix(stream) == '2', "getc_unlocked after fwrite on an r+ stream");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(fgets_nid_postfix(buffer.data(), static_cast<int>(buffer.size()), stream) == buffer.data(), "fgets after fwrite on an r+ stream");
        Require(std::strcmp(buffer.data(), "234") == 0, "fgets after fwrite on an r+ stream read other bytes");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        Require(ungetc_nid_postfix('Z', stream) == 'Z', "ungetc after fwrite on an r+ stream");
        Require(fgetc_nid_postfix(stream) == 'Z' && fgetc_nid_postfix(stream) == '2', "ungetc after fwrite on an r+ stream pushed back other bytes");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(fscanf_nid_postfix(stream, "%3s", buffer.data()) == 1, "fscanf after fwrite on an r+ stream");
        Require(std::strcmp(buffer.data(), "234") == 0, "fscanf after fwrite on an r+ stream read other bytes");
    });
    CheckReadAfterWrite(path, "r+b", overwritten, [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(ScanList(stream, "%3s", buffer.data()) == 1, "vfscanf after fwrite on an r+ stream");
        Require(std::strcmp(buffer.data(), "234") == 0, "vfscanf after fwrite on an r+ stream read other bytes");
    });
    CheckReadAfterWrite(path, "w+b", "AB", [](FileStream* stream) {
        std::array<char, 4> buffer{};
        Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == 0 && feof_nid_postfix(stream) != 0, "fread after fwrite on a w+ stream");
    });
    CheckReadAfterWrite(path, "a+b", "0123456789AB", [](FileStream* stream) {
        Require(fgetc_nid_postfix(stream) == EOF && feof_nid_postfix(stream) != 0, "fgetc after fwrite on an a+ stream");
    });
    auto* stream = fopen_nid_postfix(path.c_str(), "r+b");
    Require(stream != nullptr);
    Require(ungetc_nid_postfix('Z', stream) == 'Z', "ungetc on a new r+ stream");
    Require(fgetc_nid_postfix(stream) == 'Z' && fgetc_nid_postfix(stream) == '0', "ungetc on a new r+ stream pushed back other bytes");
    Require(fseek_nid_postfix(stream, 2, SEEK_SET) == 0 && ungetc_nid_postfix('Y', stream) == 'Y', "ungetc after fseek on an r+ stream");
    Require(fgetc_nid_postfix(stream) == 'Y' && fgetc_nid_postfix(stream) == '2', "ungetc after fseek on an r+ stream pushed back other bytes");
    Require(fclose_nid_postfix(stream) == 0);
    Require(std::remove(path.c_str()) == 0);
}

template<typename TAccess>
static void CheckWriteAfterRead(const std::string& path, const char* mode, const char* expected, TAccess access) {
    auto* stream = fopen_nid_postfix(path.c_str(), "wb");
    Require(stream != nullptr && fputs_nid_postfix("0123456789", stream) >= 0 && fclose_nid_postfix(stream) == 0, "Creating the written file");
    stream = fopen_nid_postfix(path.c_str(), mode);
    Require(stream != nullptr, "fopen of the written file");
    access(stream);
    Require(fclose_nid_postfix(stream) == 0, "fclose after a write after read");
    Require(ReadWhole(path) == expected, "Write after read wrote other bytes");
}

static void CheckStreamWritesAfterReads(const std::string& base) {
    CheckCharacterScanLengths();
    const std::string path = base + ".written";
    constexpr char appended[] = "0123456789XY";
#if defined(__GLIBC__)
    constexpr char afterReadAhead[] = "0XY3456789";
    constexpr char afterPushBack[] = "XY23456789";
#else
    constexpr char afterReadAhead[] = "0123456789XY";
    constexpr char afterPushBack[] = "0123456789XY";
#endif
    using Write = void (*)(FileStream*);
    for (const Write write : {
             +[](FileStream* stream) { Require(fwrite_nid_postfix("XY", 1, 2, stream) == 2, "fwrite after fgetc on an update stream"); },
             +[](FileStream* stream) { Require(fputs_nid_postfix("XY", stream) >= 0, "fputs after fgetc on an update stream"); },
             +[](FileStream* stream) { Require(fputc_nid_postfix('X', stream) == 'X' && fputc_nid_postfix('Y', stream) == 'Y', "fputc after fgetc on an update stream"); },
             +[](FileStream* stream) { Require(putc_unlocked_nid_postfix('X', stream) == 'X' && putc_unlocked_nid_postfix('Y', stream) == 'Y', "putc_unlocked after fgetc on an update stream"); },
             +[](FileStream* stream) { Require(fprintf_nid_postfix(stream, "%s", "XY") == 2, "fprintf after fgetc on an update stream"); },
             +[](FileStream* stream) { Require(fprintf_s_nid_postfix(stream, "%s", "XY") == 2, "fprintf_s after fgetc on an update stream"); },
             +[](FileStream* stream) { Require(fputwc_nid_postfix(u'X', stream) == u'X' && fputwc_nid_postfix(u'Y', stream) == u'Y', "fputwc after fgetc on an update stream"); },
             +[](FileStream* stream) { Require(fputws_nid_postfix(u"XY", stream) >= 0, "fputws after fgetc on an update stream"); },
             +[](FileStream* stream) { Require(fwprintf_nid_postfix(stream, u"XY") == 2, "fwprintf after fgetc on an update stream"); },
         }) {
        for (const char* mode : {"r+b", "a+b"}) {
            CheckWriteAfterRead(path, mode, std::string_view(mode) == "a+b" ? appended : afterReadAhead, [write](FileStream* stream) {
                Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0 && fgetc_nid_postfix(stream) == '0', "fgetc on an update stream");
                write(stream);
                Require(ferror_nid_postfix(stream) == 0, "Write after fgetc on an update stream set the error indicator");
            });
        }
    }
    CheckWriteAfterRead(path, "r+b", appended, [](FileStream* stream) {
        std::array<char, 10> buffer{};
        Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == buffer.size() && feof_nid_postfix(stream) == 0, "fread of every byte of an r+ stream");
        Require(fwrite_nid_postfix("XY", 1, 2, stream) == 2 && ferror_nid_postfix(stream) == 0, "fwrite after reading every byte of an r+ stream");
    });
    CheckWriteAfterRead(path, "r+b", afterReadAhead, [](FileStream* stream) {
        Require(fgetc_nid_postfix(stream) == '0' && fflush_nid_postfix(stream) == 0, "fflush after fgetc on an r+ stream");
        Require(fputs_nid_postfix("XY", stream) >= 0 && ferror_nid_postfix(stream) == 0, "fputs after fgetc and fflush on an r+ stream");
    });
    CheckWriteAfterRead(path, "r+b", afterPushBack, [](FileStream* stream) {
        Require(fgetc_nid_postfix(stream) == '0' && ungetc_nid_postfix('Z', stream) == 'Z', "ungetc after fgetc on an r+ stream");
        Require(fputs_nid_postfix("XY", stream) >= 0 && ferror_nid_postfix(stream) == 0, "fputs after ungetc on an r+ stream");
    });
    CheckWriteAfterRead(path, "rb", "0123456789", [](FileStream* stream) {
        Require(fgetc_nid_postfix(stream) == '0', "fgetc on a read-only stream");
        errno = 0;
        Require(fputc_nid_postfix('X', stream) == EOF && errno == EBADF && ferror_nid_postfix(stream) != 0, "fputc after fgetc on a read-only stream");
        clearerr_nid_postfix(stream);
        errno = 0;
        Require(fwrite_nid_postfix("XY", 1, 2, stream) == 0 && errno == EBADF && ferror_nid_postfix(stream) != 0, "fwrite after fgetc on a read-only stream");
        clearerr_nid_postfix(stream);
        errno = 0;
        Require(fprintf_nid_postfix(stream, "%s", "XY") == -1 && errno == EBADF && ferror_nid_postfix(stream) != 0, "fprintf after fgetc on a read-only stream");
    });
    Require(std::remove(path.c_str()) == 0);
}

static void CheckZeroSizedTransfers(const std::string& base) {
    const std::string path = base + ".zero";
    auto* stream = fopen_nid_postfix(path.c_str(), "w+b");
    Require(stream != nullptr && fwrite_nid_postfix("abcd", 1, 4, stream) == 4, "Creating the zero-sized transfer file");
    Require(fflush_nid_postfix(stream) == 0 && fseek_nid_postfix(stream, 1, SEEK_SET) == 0, "Positioning the zero-sized transfer file");
    std::array<char, 4> buffer{};
    buffer.fill(0x5a);
    Require(fread_nid_postfix(buffer.data(), 0, buffer.size(), stream) == 0, "fread with size 0 must return 0");
    Require(fread_nid_postfix(buffer.data(), 1, 0, stream) == 0, "fread with count 0 must return 0");
    Require(std::memcmp(buffer.data(), "ZZZZ", buffer.size()) == 0, "fread of nothing changed the buffer");
    Require(ftell_nid_postfix(stream) == 1 && feof_nid_postfix(stream) == 0 && ferror_nid_postfix(stream) == 0, "fread of nothing changed the stream");
    Require(fwrite_nid_postfix("xy", 0, 2, stream) == 0, "fwrite with size 0 must return 0");
    Require(fwrite_nid_postfix("xy", 1, 0, stream) == 0, "fwrite with count 0 must return 0");
    Require(ftell_nid_postfix(stream) == 1 && ferror_nid_postfix(stream) == 0, "fwrite of nothing changed the stream");
    Require(fgetc_nid_postfix(stream) == 'b', "Zero-sized transfers moved the stream");
    Require(fclose_nid_postfix(stream) == 0, "fclose after zero-sized transfers");
    stream = fopen_nid_postfix(path.c_str(), "rb");
    Require(stream != nullptr, "fopen of the zero-sized transfer file for reading");
    Require(fwrite_nid_postfix("xy", 1, 0, stream) == 0 && fwrite_nid_postfix("xy", 0, 1, stream) == 0, "fwrite of nothing on a read-only stream must return 0");
    Require(ferror_nid_postfix(stream) == 0, "fwrite of nothing on a read-only stream set the error indicator");
    Require(fclose_nid_postfix(stream) == 0 && std::remove(path.c_str()) == 0, "Removing the zero-sized transfer file");
}

int main(int argc, char** argv) {
    Require(argc == 2);
    CheckExclusiveCppOpen(argv[1]);
    CheckShortCharacterScans();
    CheckZeroSizedTransfers(argv[1]);
    CheckUpdateStreamReadsAfterWrites(argv[1]);
    CheckStreamWritesAfterReads(argv[1]);
    Require(_Stdout_nid_postfix.GetHandle() == stdout);
    Require(_Stderr_nid_postfix.GetHandle() == stderr);
    Require(fputs_nid_postfix("stdout object works\n", &_Stdout_nid_postfix) >= 0);
    Require(fputs_nid_postfix("stderr object works\n", &_Stderr_nid_postfix) >= 0);
    Require(fflush_nid_postfix(nullptr) == 0);
    auto* stream = fopen_nid_postfix(argv[1], "w+b");
    constexpr char payload[] = "stream round trip";
    Require(fputs_nid_postfix(payload, stream) >= 0);
    Require(fwrite_nid_postfix(payload, 1, sizeof(payload), stream) == sizeof(payload));
    const auto expectedSize = sizeof(payload) * 2 - 1;
    Require(ftell_nid_postfix(stream) == static_cast<long>(expectedSize));
    Require(fflush_nid_postfix(stream) == 0);
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    std::array<char, sizeof(payload)> buffer{};
    Require(fread_nid_postfix(buffer.data(), 1, sizeof(payload) - 1, stream) == sizeof(payload) - 1);
    Require(std::strcmp(buffer.data(), payload) == 0);
    Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == buffer.size());
    Require(std::strcmp(buffer.data(), payload) == 0);
    Require(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream) == 0);
    ExpectException([&] { fputs_nid_postfix(nullptr, stream); });
    ExpectException([&] { fwrite_nid_postfix(nullptr, 1, sizeof(payload), stream); });
    errno = 0;
    Require(fread_nid_postfix(buffer.data(), 2, SIZE_MAX / 2 + 1, stream) == 0 && errno == EINVAL && ferror_nid_postfix(stream) == 0);
    errno = 0;
    Require(fread_nid_postfix(buffer.data(), 2, SIZE_MAX / 2 + 1, nullptr) == 0 && errno == EINVAL);
    ExpectException([&] { fwrite_nid_postfix(buffer.data(), SIZE_MAX / 2 + 1, 2, stream); });
    errno = 0;
    Require(fseek_nid_postfix(stream, 0, -1) == -1);
    Require(errno == EINVAL);
    Require(fclose_nid_postfix(stream) == 0);
    ExpectException([] { fputs_nid_postfix("invalid stream", nullptr); });
    ExpectException([] { fopen_nid_postfix(nullptr, "r"); });
    FileStream closed(std::tmpfile());
    closed.Close();
    Require(fflush_nid_postfix(&closed) == -1 && errno == 9);
    Require(std::remove(argv[1]) == 0);
    errno = 0;
    Require(fopen_nid_postfix(argv[1], "rb") == nullptr);
    Require(errno == ENOENT);
    const std::string directoryName = std::string(argv[1]) + ".dir";
    Require(std::filesystem::create_directory(directoryName));
    errno = 0;
    Require(fopen_nid_postfix(directoryName.c_str(), "wb") == nullptr);
    Require(errno == EISDIR);
    Require(std::filesystem::remove(directoryName));
    const std::u8string unicodeName = u8"セーブé.tmp";
    const std::string guestName(unicodeName.begin(), unicodeName.end());
    const std::filesystem::path hostName(unicodeName);
    constexpr char unicodePayload[] = "unicode";
    stream = fopen_nid_postfix(guestName.c_str(), "wb");
    Require(stream != nullptr);
    Require(fwrite_nid_postfix(unicodePayload, 1, sizeof(unicodePayload), stream) == sizeof(unicodePayload));
    Require(fclose_nid_postfix(stream) == 0);
    Require(std::filesystem::exists(hostName));
    Require(std::filesystem::file_size(hostName) == sizeof(unicodePayload));
    stream = fopen_nid_postfix(guestName.c_str(), "rb");
    Require(stream != nullptr);
    std::array<char, sizeof(unicodePayload)> unicodeBuffer{};
    Require(fread_nid_postfix(unicodeBuffer.data(), 1, unicodeBuffer.size(), stream) == unicodeBuffer.size());
    Require(std::strcmp(unicodeBuffer.data(), unicodePayload) == 0);
    stream = freopen_nid_postfix(guestName.c_str(), "wb", stream);
    Require(stream != nullptr);
    Require(fclose_nid_postfix(stream) == 0);
    Require(std::filesystem::file_size(hostName) == 0);
    std::filesystem::remove(hostName);
    const std::string heldName = std::string(argv[1]) + ".held";
    const std::string movedName = std::string(argv[1]) + ".moved";
    stream = fopen_nid_postfix(heldName.c_str(), "w+b");
    Require(stream != nullptr && fwrite_nid_postfix(payload, 1, sizeof(payload), stream) == sizeof(payload));
    Require(fflush_nid_postfix(stream) == 0);
    Require(rename_nid_postfix(heldName.c_str(), movedName.c_str()) == 0 && !std::filesystem::exists(heldName));
    Require(remove_nid_postfix(movedName.c_str()) == 0 && !std::filesystem::exists(movedName));
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    std::array<char, sizeof(payload)> heldBuffer{};
    Require(fread_nid_postfix(heldBuffer.data(), 1, heldBuffer.size(), stream) == heldBuffer.size());
    Require(std::strcmp(heldBuffer.data(), payload) == 0 && fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(heldName.c_str(), "ab");
    Require(stream != nullptr && fputs_nid_postfix("ab", stream) >= 0 && fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(heldName.c_str(), "ab");
    Require(stream != nullptr && fputs_nid_postfix("cd", stream) >= 0 && fclose_nid_postfix(stream) == 0);
    errno = 0;
    Require(fopen_nid_postfix(heldName.c_str(), "wbx") == nullptr && errno == EEXIST);
    stream = fopen_nid_postfix(heldName.c_str(), "rb");
    std::array<char, 5> appended{};
    Require(stream != nullptr && fread_nid_postfix(appended.data(), 1, 4, stream) == 4 && std::strcmp(appended.data(), "abcd") == 0);
    Require(fclose_nid_postfix(stream) == 0 && remove_nid_postfix(heldName.c_str()) == 0);
    stream = fopen_nid_postfix(heldName.c_str(), "w+b");
    Require(stream != nullptr);
    stream = freopen_nid_postfix(movedName.c_str(), "w+b", stream);
    Require(stream != nullptr && fwrite_nid_postfix(payload, 1, sizeof(payload), stream) == sizeof(payload));
    Require(fflush_nid_postfix(stream) == 0);
    Require(rename_nid_postfix(movedName.c_str(), heldName.c_str()) == 0 && !std::filesystem::exists(movedName));
    Require(remove_nid_postfix(heldName.c_str()) == 0 && !std::filesystem::exists(heldName));
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    std::array<char, sizeof(payload)> reopenedBuffer{};
    Require(fread_nid_postfix(reopenedBuffer.data(), 1, reopenedBuffer.size(), stream) == reopenedBuffer.size());
    Require(std::strcmp(reopenedBuffer.data(), payload) == 0 && fclose_nid_postfix(stream) == 0);
    constexpr char marker[] = "marker";
    stream = fopen_nid_postfix(argv[1], "wb");
    Require(stream != nullptr);
    std::array<char, sizeof(marker)> markerBuffer{};
    Require(fread_nid_postfix(markerBuffer.data(), 1, markerBuffer.size(), stream) == 0);
    Require(ferror_nid_postfix(stream) != 0);
    Require(fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(argv[1], "rb");
    Require(stream != nullptr);
    Require(fwrite_nid_postfix(marker, 1, sizeof(marker), stream) == 0);
    Require(ferror_nid_postfix(stream) != 0);
    clearerr_nid_postfix(stream);
    Require(fputs_nid_postfix(marker, stream) == EOF);
    Require(ferror_nid_postfix(stream) != 0);
    Require(fclose_nid_postfix(stream) == 0);
    stream = fopen_nid_postfix(argv[1], "wb");
    Require(stream != nullptr && fputs_nid_postfix(marker, stream) >= 0);
    ReplaceWithReadOnlyDescriptor(stream, argv[1]);
    Require(fflush_nid_postfix(stream) == EOF);
    Require(ferror_nid_postfix(stream) != 0);
    static_cast<void>(fclose_nid_postfix(stream));
    Require(std::remove(argv[1]) == 0);
    std::cout << "PASS: stream objects, file operations, EOF and error handling\n";
}
