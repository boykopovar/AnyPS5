#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
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
#include <limits>
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
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <chrono>
#include <future>

extern "C" {
extern FileStream* __stdinp_nid_postfix;
extern FileStream* __stdoutp_nid_postfix;
extern FileStream* __stderrp_nid_postfix;
#ifdef _WIN32
std::FILE* HostStandardErrorFile_nid_no_patch();
std::FILE* HostStandardOutputFile_nid_no_patch();
#endif
}

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
        auto* native = TemporaryFile::Open();
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
                    auto* native = TemporaryFile::Open();
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
                    auto* native = TemporaryFile::Open();
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
        auto* errorNative = TemporaryFile::Open();
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
        auto* native = TemporaryFile::Open();
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
                auto* native = TemporaryFile::Open();
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

static int FreopenAliasesChild([[maybe_unused]] const std::string& base) {
    std::promise<void> finished;
    std::thread watchdog([done = finished.get_future()] {
        if (done.wait_for(std::chrono::seconds(5)) == std::future_status::ready) return;
#ifdef _WIN32
        ::TerminateProcess(::GetCurrentProcess(), 116);
#else
        ::_exit(116);
#endif
    });
    watchdog.detach();
#ifdef _WIN32
    const std::string target = base + ".freopen.aliases";
    static std::FILE* replacement = nullptr;
    replacement = std::fopen(target.c_str(), "w+b");
    if (!replacement) return 110;
    FileStream source(std::tmpfile());
    FileStream alias(source.GetHandle());
    std::FILE* const original = source.GetHandle();
    alias.SetError();
    flockfile_nid_postfix(&source);
    const bool reopened = source.Reopen(target, "w+b", [](const std::filesystem::path&, const std::string&) {
        return std::exchange(replacement, nullptr);
    });
    const bool sameNative = reopened && source.RawHandle_nid_no_patch() == original;
    const bool aliasesMatch = reopened && alias.RawHandle_nid_no_patch() == source.RawHandle_nid_no_patch()
        && alias.GuestState().flags == source.GuestState().flags;
    if (!sameNative || !aliasesMatch) ::TerminateProcess(::GetCurrentProcess(), 111);
    auto writing = std::async(std::launch::async, [&] { return fputc_nid_postfix('R', &alias); });
    if (writing.wait_for(std::chrono::milliseconds(100)) != std::future_status::timeout)
        ::TerminateProcess(::GetCurrentProcess(), 113);
    funlockfile_nid_postfix(&source);
    if (writing.wait_for(std::chrono::seconds(2)) != std::future_status::ready)
        ::TerminateProcess(::GetCurrentProcess(), 114);
    const int written = writing.get();
    if (replacement) { std::fclose(replacement); replacement = nullptr; }
    if (source.IsOpen()) source.Close();
    std::FILE* const readback = std::fopen(target.c_str(), "rb");
    const bool preservedWrite = readback && std::fgetc(readback) == 'R';
    if (readback) std::fclose(readback);
    std::remove(target.c_str());
    if (written != 'R' || !preservedWrite) return 115;
#endif
    FileStream failing(std::tmpfile());
    FileStream failedAlias(failing.GetHandle());
    const bool unexpectedlyOpened = failing.Reopen(std::filesystem::current_path(), "wb");
    const bool bothClosed = !failing.IsOpen() && !failedAlias.IsOpen();
    if (unexpectedlyOpened) failing.Close();
    failedAlias.Detach();
    finished.set_value();
    return !unexpectedlyOpened && bothClosed ? 0 : 112;
}

static int FreopenStandardOwnerChild(const std::string& base) {
    const std::string target = base + ".freopen.standard-owner";
    std::promise<void> finished;
    std::thread watchdog([done = finished.get_future()] {
        if (done.wait_for(std::chrono::seconds(5)) == std::future_status::ready) return;
#ifdef _WIN32
        ::TerminateProcess(::GetCurrentProcess(), 125);
#else
        ::_exit(125);
#endif
    });
    watchdog.detach();
    FileStream* const stream = __stderrp_nid_postfix;
    flockfile_nid_postfix(stream);
    std::promise<void> entered;
    auto closing = std::async(std::launch::async, [&] {
        entered.set_value();
        fcloseall_nid_postfix();
    });
    entered.get_future().wait();
    if (closing.wait_for(std::chrono::milliseconds(100)) != std::future_status::timeout) return 120;
    if (freopen_nid_postfix(target.c_str(), "wb", stream) != stream) return 121;
    if (fputc_nid_postfix('S', stream) != 'S') return 122;
    funlockfile_nid_postfix(stream);
    if (closing.wait_for(std::chrono::seconds(2)) != std::future_status::ready) return 123;
    closing.get();
    std::FILE* const check = std::fopen(target.c_str(), "rb");
    const bool written = check && std::fgetc(check) == 'S';
    if (check) std::fclose(check);
    std::remove(target.c_str());
    finished.set_value();
    return written ? 0 : 124;
}

static int FcloseallChild(const std::string& base) {
    const std::string serialPath = base + ".fcloseall.serial";
    FileStream* serial = fopen_nid_postfix(serialPath.c_str(), "wb");
    if (!serial) return 60;
#ifdef _WIN32
    const HANDLE serialHandle = reinterpret_cast<HANDLE>(::_get_osfhandle(::_fileno(serial->GetHandle())));
#else
    const int serialHandle = ::fileno(serial->GetHandle());
#endif
    std::promise<void> entered;
    std::promise<int> completed;
    auto completion = completed.get_future();
    std::unique_lock registryLock(FileStream::RegistryMutex_nid_no_patch());
    std::thread closer([&] {
        entered.set_value();
        completed.set_value(fclose_nid_postfix(serial));
    });
    entered.get_future().wait();
    const bool closedEarly = completion.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready;
#ifdef _WIN32
    DWORD handleFlags = 0;
    const bool stillOpen = ::GetHandleInformation(serialHandle, &handleFlags) != 0;
#else
    const bool stillOpen = ::fcntl(serialHandle, F_GETFD) != -1;
#endif
    registryLock.unlock();
    closer.join();
    const int closeResult = completion.get();
    std::remove(serialPath.c_str());
    if (closedEarly || !stillOpen || closeResult != 0) return 61;
    const std::string pathFlush = base + ".fcloseall.flush";
    const std::string pathReopen = base + ".fcloseall.reopen";
    const std::string pathReopenSecond = base + ".fcloseall.reopen2";
    const std::string pathSentinel = base + ".fcloseall.sentinel";
    std::remove(pathFlush.c_str());
    std::remove(pathReopen.c_str());
    std::remove(pathReopenSecond.c_str());
    std::remove(pathSentinel.c_str());

    constexpr char payload[] = "fcloseall flush";
    constexpr std::size_t payloadLen = sizeof(payload) - 1;

    FileStream* flushStream = fopen_nid_postfix(pathFlush.c_str(), "wb");
    if (!flushStream) return 10;
    if (fwrite_nid_postfix(payload, 1, payloadLen, flushStream) != payloadLen) return 11;

    FileStream* reopenStream = fopen_nid_postfix(pathReopen.c_str(), "wb");
    if (!reopenStream) return 12;
    reopenStream = freopen_nid_postfix(pathReopenSecond.c_str(), "wb", reopenStream);
    if (!reopenStream) return 13;
    if (fwrite_nid_postfix(payload, 1, payloadLen, reopenStream) != payloadLen) return 14;

#ifdef _WIN32
    const int duplicate = ::_dup(::_fileno(stdout));
#else
    const int duplicate = ::dup(::fileno(stdout));
#endif
    if (duplicate < 0) return 15;
    FileStream* dupStream = fdopen_nid_postfix(duplicate, "w");
    if (!dupStream) return 16;
    if (!dupStream->IsOpen()) return 17;
#ifdef _WIN32
    const HANDLE duplicateHandle = reinterpret_cast<HANDLE>(::_get_osfhandle(duplicate));
#endif

    FileStream closedStatic(std::tmpfile());
    if (closedStatic.Close() != 0) return 18;
    if (closedStatic.IsOpen()) return 19;

    if (!__stdoutp_nid_postfix->IsOpen() || !__stderrp_nid_postfix->IsOpen() || !__stdinp_nid_postfix->IsOpen()) return 20;
    if (!_Stdout_nid_postfix.IsOpen() || !_Stderr_nid_postfix.IsOpen()) return 21;

#ifdef _WIN32
    std::FILE* const sentinelFile = std::fopen(pathSentinel.c_str(), "w");
    if (!sentinelFile) return 25;
    const int sentinelFd = ::_dup(::_fileno(sentinelFile));
    if (sentinelFd < 0) { std::fclose(sentinelFile); return 26; }
    if (::_dup2(sentinelFd, ::_fileno(stderr)) != 0) { ::_close(sentinelFd); std::fclose(sentinelFile); return 27; }
    ::_close(sentinelFd);
    std::fclose(sentinelFile);
#else
    std::FILE* const sentinelFile = std::freopen(pathSentinel.c_str(), "w", stderr);
    if (!sentinelFile) return 25;
#endif

#ifdef _WIN32
    std::vector<std::FILE*> exhaustedStreams;
    const int streamLimit = ::_getmaxstdio();
    exhaustedStreams.reserve(static_cast<std::size_t>(streamLimit));
    for (int i = 0; i < streamLimit; ++i) {
        std::FILE* extra = std::fopen("NUL", "w");
        if (!extra) break;
        exhaustedStreams.push_back(extra);
    }
    bool preservationFailed = false;
    try {
        fcloseall_nid_postfix();
    } catch (const std::runtime_error&) {
        preservationFailed = true;
    }
    for (std::FILE* extra : exhaustedStreams) std::fclose(extra);
    if (!preservationFailed) return 62;
    if (!flushStream->IsOpen() || !reopenStream->IsOpen() || !dupStream->IsOpen()) return 63;
    if (!__stdoutp_nid_postfix->IsOpen() || !__stderrp_nid_postfix->IsOpen() || !__stdinp_nid_postfix->IsOpen()) return 64;
#endif

    fcloseall_nid_postfix();

#ifdef _WIN32
    DWORD duplicateFlags = 0;
    if (::GetHandleInformation(duplicateHandle, &duplicateFlags) != 0 || ::GetLastError() != ERROR_INVALID_HANDLE) return 36;
#else
    errno = 0;
    if (::fcntl(duplicate, F_GETFD) != -1 || errno != EBADF) return 36;
#endif
    if (__stdoutp_nid_postfix->IsOpen() || __stderrp_nid_postfix->IsOpen() || __stdinp_nid_postfix->IsOpen()) return 30;
    if (_Stdout_nid_postfix.IsOpen() || _Stderr_nid_postfix.IsOpen()) return 31;
    errno = 0;
    if (fputc_nid_postfix('x', __stdoutp_nid_postfix) != -1 || errno != 9) return 32;
    errno = 0;
    if (fputc_nid_postfix('x', __stderrp_nid_postfix) != -1 || errno != 9) return 33;

    fcloseall_nid_postfix();
    if (__stdoutp_nid_postfix->IsOpen() || __stderrp_nid_postfix->IsOpen()) return 34;
    if (closedStatic.IsOpen()) return 35;

    std::FILE* const flushRead = std::fopen(pathFlush.c_str(), "rb");
    if (!flushRead) return 40;
    char buffer[payloadLen + 1]{};
    const std::size_t read = std::fread(buffer, 1, payloadLen, flushRead);
    std::fclose(flushRead);
    if (read != payloadLen || std::memcmp(buffer, payload, payloadLen) != 0) return 41;

    std::FILE* const reopenRead = std::fopen(pathReopenSecond.c_str(), "rb");
    if (!reopenRead) return 42;
    char reopenBuffer[payloadLen + 1]{};
    const std::size_t reopenReadBytes = std::fread(reopenBuffer, 1, payloadLen, reopenRead);
    std::fclose(reopenRead);
    if (reopenReadBytes != payloadLen || std::memcmp(reopenBuffer, payload, payloadLen) != 0) return 43;
    if (std::filesystem::exists(pathReopen)) {
        std::error_code status;
        if (std::filesystem::file_size(pathReopen, status) != 0) return 44;
    }

#ifdef _WIN32
    std::FILE* const hostErr = HostStandardErrorFile_nid_no_patch();
    if (!hostErr || std::fprintf(hostErr, "sentinel\n") < 0 || std::fflush(hostErr) != 0) return 50;
#else
    if (std::fprintf(stderr, "sentinel\n") < 0 || std::fflush(stderr) != 0) return 50;
#endif

    std::FILE* const sentinelCheck = std::fopen(pathSentinel.c_str(), "rb");
    if (!sentinelCheck) return 51;
    char sentinelBuffer[32]{};
    const std::size_t sentinelRead = std::fread(sentinelBuffer, 1, sizeof(sentinelBuffer) - 1, sentinelCheck);
    std::fclose(sentinelCheck);
    if (sentinelRead < 9 || std::strstr(sentinelBuffer, "sentinel") == nullptr) return 52;

    std::remove(pathFlush.c_str());
    std::remove(pathReopen.c_str());
    std::remove(pathReopenSecond.c_str());
    std::remove(pathSentinel.c_str());
    return 0;
}

static int FcloseallFcloseRaceChild(const std::string& base) {
    const std::string racePath = base + ".fcloseall.race";
    FileStream* raced = fopen_nid_postfix(racePath.c_str(), "wb");
    if (!raced) return 80;
    std::promise<void> finished;
    std::thread watchdog([done = finished.get_future()] {
        if (done.wait_for(std::chrono::seconds(5)) == std::future_status::ready) return;
#ifdef _WIN32
        ::TerminateProcess(::GetCurrentProcess(), 85);
#else
        ::_exit(85);
#endif
    });
    watchdog.detach();
    flockfile_nid_postfix(raced);
    std::promise<void> fcloseEntered;
    std::promise<void> fcloseallEntered;
    std::promise<std::pair<int, int>> fcloseResult;
    std::thread fcloseThread([&] {
        fcloseEntered.set_value();
        const int result = fclose_nid_postfix(raced);
        fcloseResult.set_value({result, errno});
    });
    std::thread fcloseallThread([&] {
        fcloseallEntered.set_value();
        fcloseall_nid_postfix();
    });
    fcloseEntered.get_future().wait();
    fcloseallEntered.get_future().wait();
    auto fcloseFuture = fcloseResult.get_future();
    if (fcloseFuture.wait_for(std::chrono::milliseconds(100)) != std::future_status::timeout) return 81;
    funlockfile_nid_postfix(raced);
    if (fcloseFuture.wait_for(std::chrono::seconds(3)) != std::future_status::ready) return 82;
    fcloseThread.join();
    fcloseallThread.join();
    finished.set_value();
    const auto [closeRc, closeError] = fcloseFuture.get();
    if (closeRc != 0 && !(closeRc == -1 && closeError == 9)) return 83;
    std::remove(racePath.c_str());
    return 0;
}

static int FreopenLockChild(const std::string& base) {
    const std::string lockedPath = base + ".freopen.locked";
    const std::string reopenPath = base + ".freopen.target";
    FileStream* locked = fopen_nid_postfix(lockedPath.c_str(), "wb");
    if (!locked) return 90;
    std::promise<void> finished;
    std::thread watchdog([done = finished.get_future()] {
        if (done.wait_for(std::chrono::seconds(5)) == std::future_status::ready) return;
#ifdef _WIN32
        ::TerminateProcess(::GetCurrentProcess(), 95);
#else
        ::_exit(95);
#endif
    });
    watchdog.detach();
    flockfile_nid_postfix(locked);
    std::promise<void> entered;
    auto reopened = std::async(std::launch::async, [&] {
        entered.set_value();
        return freopen_nid_postfix(reopenPath.c_str(), "wb", locked);
    });
    entered.get_future().wait();
    if (reopened.wait_for(std::chrono::milliseconds(100)) != std::future_status::timeout) return 91;
    funlockfile_nid_postfix(locked);
    if (reopened.wait_for(std::chrono::seconds(3)) != std::future_status::ready) return 92;
    FileStream* const result = reopened.get();
    if (!result) return 93;
    if (fclose_nid_postfix(result) != 0) return 94;
    finished.set_value();
    std::remove(lockedPath.c_str());
    std::remove(reopenPath.c_str());
    return 0;
}

static int FcloseallLockChild(const std::string& base) {
    const std::string lockedPath = base + ".fcloseall.locked";
    const std::string otherPath = base + ".fcloseall.other";
    FileStream* locked = fopen_nid_postfix(lockedPath.c_str(), "wb");
    if (!locked) return 70;
    std::promise<void> finished;
    std::thread watchdog([done = finished.get_future()] {
        if (done.wait_for(std::chrono::seconds(5)) == std::future_status::ready) return;
#ifdef _WIN32
        ::TerminateProcess(::GetCurrentProcess(), 75);
#else
        ::_exit(75);
#endif
    });
    watchdog.detach();
    flockfile_nid_postfix(locked);
    std::promise<void> entered;
    auto closer = std::async(std::launch::async, [&] {
        entered.set_value();
        fcloseall_nid_postfix();
    });
    entered.get_future().wait();
    if (closer.wait_for(std::chrono::milliseconds(100)) != std::future_status::timeout) return 71;
    FileStream* other = fopen_nid_postfix(otherPath.c_str(), "wb");
    if (!other) return 72;
    if (fclose_nid_postfix(other) != 0) return 73;
    funlockfile_nid_postfix(locked);
    if (closer.wait_for(std::chrono::seconds(2)) != std::future_status::ready) return 74;
    closer.get();
    finished.set_value();
    std::remove(lockedPath.c_str());
    std::remove(otherPath.c_str());
    return 0;
}

static int ClosedStreamReopenChild(const std::string& base) {
    const std::string path = base + ".closed-reopen";
    const std::string missing = base + ".closed-reopen-missing";
    std::remove(path.c_str());
    std::remove(missing.c_str());
    FileStream stream(std::tmpfile());
    if (stream.Close() != 0 || stream.IsOpen()) return 130;
    errno = 0;
    if (freopen_nid_postfix(nullptr, "wb", &stream) != nullptr || errno != 9) return 131;
    if (freopen_nid_postfix(path.c_str(), "wb", &stream) != &stream) return 132;
    if (fputc_nid_postfix('A', &stream) != 'A') return 133;
    if (fclose_nid_postfix(&stream) != 0 || stream.IsOpen()) return 134;
    errno = 0;
    if (freopen_nid_postfix(nullptr, "wb", &stream) != nullptr || errno != 9) return 135;
    if (freopen_nid_postfix(missing.c_str(), "rb", &stream) != nullptr || stream.IsOpen()) return 136;
    if (freopen_nid_postfix(path.c_str(), "ab", &stream) != &stream) return 137;
    if (fputc_nid_postfix('B', &stream) != 'B') return 138;
    fcloseall_nid_postfix();
    if (stream.IsOpen()) return 139;
    if (freopen_nid_postfix(path.c_str(), "ab", &stream) != &stream) return 140;
    if (fputc_nid_postfix('C', &stream) != 'C') return 141;
#ifdef _WIN32
    const HANDLE native = reinterpret_cast<HANDLE>(::_get_osfhandle(::_fileno(stream.GetHandle())));
#else
    const int native = ::fileno(stream.GetHandle());
#endif
    fcloseall_nid_postfix();
    if (stream.IsOpen()) return 142;
#ifdef _WIN32
    DWORD flags = 0;
    if (::GetHandleInformation(native, &flags) != 0 || ::GetLastError() != ERROR_INVALID_HANDLE) return 143;
#else
    errno = 0;
    if (::fcntl(native, F_GETFD) != -1 || errno != EBADF) return 143;
#endif
    std::FILE* const read = std::fopen(path.c_str(), "rb");
    if (!read) return 144;
    char payload[4]{};
    const bool contents = std::fread(payload, 1, sizeof(payload), read) == 3
        && std::memcmp(payload, "ABC", 3) == 0;
    std::fclose(read);
    std::remove(path.c_str());
    return contents ? 0 : 145;
}

static int ReopenSnapshotChild(const std::string& base) {
    const std::string path = base + ".reopen-snapshot";
    std::remove(path.c_str());
    FileStream stream(std::tmpfile());
    std::uint64_t cursor = std::numeric_limits<std::uint64_t>::max();
    auto [selected, gate] = PinNextFileStream_nid_no_patch(cursor);
    if (selected != &stream || gate != stream.GatePtr_nid_no_patch()) return 150;
    FileStreamLease_nid_no_patch selection(selected, gate);
    std::lock_guard<std::recursive_mutex> gateLock(*gate);
    const std::uint64_t originalSnapshot = cursor + 1;
    if (!IsFileStreamInSnapshot_nid_no_patch(selected, originalSnapshot)) return 151;
    if (fclose_nid_postfix(&stream) != 0) return 152;
    if (AcquireFileStreamLease_nid_no_patch(&stream).Valid()) return 153;
    auto reopenLease = AcquireFileStreamLease_nid_no_patch(&stream, true);
    if (!reopenLease.Valid() || reopenLease.Gate() != gate) return 154;
    const auto reservation = ReserveFileStreamReopen_nid_no_patch(&stream);
    if (!reservation || *reservation == 0) return 155;
    struct ReservationCleanup {
        std::uint64_t reservation;
        ~ReservationCleanup() { CancelFileStreamReopenReservation_nid_no_patch(reservation); }
    } cleanup{*reservation};
    FileStream intervening(std::tmpfile());
    std::uint64_t interveningCursor = std::numeric_limits<std::uint64_t>::max();
    auto [interveningSelection, interveningGate] = PinNextFileStream_nid_no_patch(interveningCursor);
    if (interveningSelection != &intervening) return 156;
    FileStreamLease_nid_no_patch interveningLease(interveningSelection, interveningGate);
    const std::uint64_t reservedSnapshot = interveningCursor + 1;
    if (IsFileStreamInSnapshot_nid_no_patch(selected, reservedSnapshot)) return 157;
    if (!stream.Reopen(std::filesystem::path(path), "wb",
        [](const std::filesystem::path& filename, const std::string& mode) {
            return std::fopen(filename.string().c_str(), mode.c_str());
        })) return 158;
    PublishFileStreamReopen_nid_no_patch(&stream, *reservation);
    cleanup.reservation = 0;
    if (IsFileStreamInSnapshot_nid_no_patch(selected, originalSnapshot)) return 159;
    if (IsFileStreamInSnapshot_nid_no_patch(selected, reservedSnapshot)) return 160;
    if (!IsFileStreamInSnapshot_nid_no_patch(selected, std::numeric_limits<std::uint64_t>::max())) return 161;
    if (fputc_nid_postfix('R', &stream) != 'R') return 162;
    fcloseall_nid_postfix();
    if (stream.IsOpen() || intervening.IsOpen()) return 163;
    std::FILE* const read = std::fopen(path.c_str(), "rb");
    const bool contents = read && std::fgetc(read) == 'R';
    if (read) std::fclose(read);
    std::remove(path.c_str());
    return contents ? 0 : 164;
}

static int RunFcloseallChild(const char* self, const char* base, const char* mode = "--fcloseall-child") {
#ifdef _WIN32
    static_cast<void>(self);
    wchar_t executable[32768];
    Require(::GetModuleFileNameW(nullptr, executable, 32768) != 0);
    std::wstring widenedBase;
    for (const char character : std::string_view(base)) widenedBase.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
    std::wstring widenedMode;
    for (const char character : std::string_view(mode)) widenedMode.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
    std::wstring command = std::wstring(L"\"") + executable + L"\" \"" + widenedBase + L"\" " + widenedMode;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    Require(::CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != 0);
    ::CloseHandle(process.hThread);
    if (::WaitForSingleObject(process.hProcess, 30000) != WAIT_OBJECT_0) {
        ::TerminateProcess(process.hProcess, 99);
        ::CloseHandle(process.hProcess);
        return 99;
    }
    DWORD code = 99;
    ::GetExitCodeProcess(process.hProcess, &code);
    ::CloseHandle(process.hProcess);
    return static_cast<int>(code);
#else
    const pid_t pid = ::fork();
    if (pid < 0) return 97;
    if (pid == 0) {
        char* const arguments[] = {const_cast<char*>(self), const_cast<char*>(base), const_cast<char*>(mode), nullptr};
        ::execv(self, arguments);
        ::_exit(98);
    }
    int status = 0;
    if (::waitpid(pid, &status, 0) != pid) return 99;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
}

static void CheckWideStateAfterAliasReopen(const std::string& base) {
    const std::string path = base + ".wide-reopen";
    std::FILE* replacement = std::fopen(path.c_str(), "wb");
    Require(replacement != nullptr && std::fputs("Q", replacement) >= 0 && std::fclose(replacement) == 0, "Seeding the wide reopen replacement");
    FileStream stream(std::tmpfile());
    FileStream alias(stream.GetHandle());
    Require(fputws_nid_postfix(u"\U0001f600", &stream) == 2, "Seeding supplementary wide input");
    rewind_nid_postfix(&stream);
    Require(getwc_nid_postfix(&stream) == 0xd83d, "Leaving a pending low surrogate on the source");
    rewind_nid_postfix(&alias);
    Require(getwc_nid_postfix(&alias) == 0xd83d, "Leaving a pending low surrogate on the alias");
    Require(freopen_nid_postfix(path.c_str(), "rb", &stream) == &stream, "Reopening a source with pending wide state");
    Require(getwc_nid_postfix(&alias) == u'Q', "Reopen must discard the alias's old low surrogate");
    Require(getwc_nid_postfix(&stream) == -1, "Reopen must discard the source's old low surrogate");
    Require(fclose_nid_postfix(&stream) == 0 && !alias.IsOpen(), "Closing the reopened source must detach its alias");
    Require(std::remove(path.c_str()) == 0, "Removing the wide reopen replacement");
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
    if (argc == 3 && std::strcmp(argv[2], "--freopen-standard-owner-child") == 0) return FreopenStandardOwnerChild(argv[1]);
    if (argc == 3 && std::strcmp(argv[2], "--freopen-aliases-child") == 0) return FreopenAliasesChild(argv[1]);
    if (argc == 3 && std::strcmp(argv[2], "--fcloseall-child") == 0) return FcloseallChild(argv[1]);
    if (argc == 3 && std::strcmp(argv[2], "--fcloseall-lock-child") == 0) return FcloseallLockChild(argv[1]);
    if (argc == 3 && std::strcmp(argv[2], "--fcloseall-fclose-race-child") == 0) return FcloseallFcloseRaceChild(argv[1]);
    if (argc == 3 && std::strcmp(argv[2], "--freopen-lock-child") == 0) return FreopenLockChild(argv[1]);
    if (argc == 3 && std::strcmp(argv[2], "--closed-reopen-child") == 0) return ClosedStreamReopenChild(argv[1]);
    if (argc == 3 && std::strcmp(argv[2], "--reopen-snapshot-child") == 0) return ReopenSnapshotChild(argv[1]);
    Require(argc == 2);
    CheckExclusiveCppOpen(argv[1]);
    CheckShortCharacterScans();
    CheckZeroSizedTransfers(argv[1]);
    CheckWideStateAfterAliasReopen(argv[1]);
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
    FileStream closed(TemporaryFile::Open());
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
    const int reopenSnapshot = RunFcloseallChild(argv[0], argv[1], "--reopen-snapshot-child");
    Require(reopenSnapshot == 0, "reopen snapshot publication child failed");
    const int closedReopen = RunFcloseallChild(argv[0], argv[1], "--closed-reopen-child");
    Require(closedReopen == 0, "closed-stream reopen and close-all child failed");
    const int childResult = RunFcloseallChild(argv[0], argv[1]);
    Require(childResult == 0, "fcloseall child failed");
    const int lockResult = RunFcloseallChild(argv[0], argv[1], "--fcloseall-lock-child");
    Require(lockResult == 0, "fcloseall locked-stream child failed");
    const int raceResult = RunFcloseallChild(argv[0], argv[1], "--fcloseall-fclose-race-child");
    Require(raceResult == 0, "fcloseall racing fclose child failed");
    const int freopenLock = RunFcloseallChild(argv[0], argv[1], "--freopen-lock-child");
    Require(freopenLock == 0, "freopen locked-stream child failed");
    const int freopenAliases = RunFcloseallChild(argv[0], argv[1], "--freopen-aliases-child");
    Require(freopenAliases == 0, "freopen alias-state child failed");
    const int freopenStandard = RunFcloseallChild(argv[0], argv[1], "--freopen-standard-owner-child");
    Require(freopenStandard == 0, "freopen standard-stream owner child failed");
    std::cout << "PASS: stream objects, file operations, EOF and error handling\n";
}
