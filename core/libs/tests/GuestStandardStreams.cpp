#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <initializer_list>
#include <chrono>
#ifndef _WIN32
#include <unistd.h>
#endif
extern "C" {
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
std::size_t APS5_VABI fread_nid_postfix(void*, std::size_t, std::size_t, FileStream*);
std::size_t APS5_VABI fwrite_nid_postfix(const void*, std::size_t, std::size_t, FileStream*);
FileStream* APS5_VABI freopen_nid_postfix(const char*, const char*, FileStream*);
int APS5_VABI fseeko_nid_postfix(FileStream*, std::int64_t, int);
std::int64_t APS5_VABI ftello_nid_postfix(FileStream*);
int APS5_VABI fseek_nid_postfix(FileStream*, std::int64_t, int);
std::int64_t APS5_VABI ftell_nid_postfix(FileStream*);
int* APS5_VABI __error_nid_postfix();
extern FileStream* __stdinp_nid_postfix;
extern FileStream* __stdoutp_nid_postfix;
extern FileStream* __stderrp_nid_postfix;
extern int __isthreaded_nid_postfix;
int APS5_VABI fprintf_nid_postfix(FileStream*, const char*, ...);
int APS5_VABI vfprintf_nid_postfix(FileStream*, const char*, void*);
int APS5_VABI vsprintf_nid_postfix(char*, const char*, void*);
int APS5_VABI fgetc_nid_postfix(FileStream*);
int APS5_VABI fputc_nid_postfix(int, FileStream*);
int APS5_VABI fputwc_nid_postfix(char16_t, FileStream*);
int APS5_VABI fputws_nid_postfix(const char16_t*, FileStream*);
int APS5_VABI fscanf_nid_postfix(FileStream*, const char*, ...);
int APS5_VABI __srget_nid_postfix(FileStream*);
int APS5_VABI __swbuf_nid_postfix(int, FileStream*);
int APS5_VABI ungetc_nid_postfix(int, FileStream*);
char* APS5_VABI fgets_nid_postfix(char*, int, FileStream*);
int APS5_VABI feof_nid_postfix(FileStream*);
int APS5_VABI fileno_nid_postfix(FileStream*);
void APS5_VABI clearerr_nid_postfix(FileStream*);
int APS5_VABI setvbuf_nid_postfix(FileStream*, char*, int, std::size_t);
void APS5_VABI setbuf_nid_postfix(FileStream*, char*);
FileStream* APS5_VABI fdopen_nid_postfix(int, const char*);
int APS5_VABI fclose_nid_postfix(FileStream*);
int APS5_VABI _Getmbcurmax_nid_postfix();
int APS5_VABI ___mb_cur_max_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
static int APS5_VABI WriteFormatted(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfprintf_nid_postfix(stream, format, args);
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}
static int APS5_VABI FormatString(char* buffer, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vsprintf_nid_postfix(buffer, format, args);
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}
static bool CheckFileBytes(const std::string& filename, const std::string& expected, const char* mode, bool reopen) {
    std::ifstream file(filename, std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (actual == expected) return true;
    std::fprintf(stderr, "%s %s: expected %zu raw bytes, received %zu\n", reopen ? "freopen" : "fopen", mode, expected.size(), actual.size());
    return false;
}

static bool CheckBinaryModes() {
    const auto directory = "anyps5-byte-stream-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Require(std::filesystem::create_directory(directory));
    const auto filename = directory + "/bytes";
    const std::string original("A\r\n\x1a" "B\0C", 7);
    const std::string written("D\n\x1a" "E\0F", 6);
    const char* modes[] = {"r", "r+", "w", "w+", "a", "a+", "rb", "rb+", "r+b", "wb", "wb+", "w+b", "ab", "ab+", "a+b"};
    bool correct = true;
    for (const auto* mode : modes) {
        for (const bool reopen : {false, true}) {
            { std::ofstream seed(filename, std::ios::binary | std::ios::trunc); seed.write(original.data(), original.size()); Require(seed.good()); }
            FileStream redirected(std::tmpfile());
            FileStream* stream = reopen ? freopen_nid_postfix(filename.c_str(), mode, &redirected) : fopen_nid_postfix(filename.c_str(), mode);
            Require(stream != nullptr);
            const bool update = std::strchr(mode, '+') != nullptr;
            if (*mode == 'r' || (*mode == 'a' && update)) {
                Require(fseeko_nid_postfix(stream, 0, SEEK_SET) == 0);
                char bytes[16]{};
                const auto count = fread_nid_postfix(bytes, 1, sizeof(bytes), stream);
                const bool matches = count == original.size() && std::memcmp(bytes, original.data(), original.size()) == 0;
                if (!matches) std::fprintf(stderr, "%s %s: expected 7 input bytes, received %zu\n", reopen ? "freopen" : "fopen", mode, count);
                correct &= matches;
                Require(fseeko_nid_postfix(stream, 3, SEEK_SET) == 0);
                correct &= fgetc_nid_postfix(stream) == 0x1a;
            }
            std::string expected = original;
            if (*mode == 'w' || *mode == 'a' || update) {
                Require(fseeko_nid_postfix(stream, 0, SEEK_SET) == 0);
                Require(fwrite_nid_postfix(written.data(), 1, written.size(), stream) == written.size());
                expected = *mode == 'w' ? written : *mode == 'a' ? original + written : written + original.substr(written.size());
            }
            Require(fclose_nid_postfix(stream) == 0);
            correct &= CheckFileBytes(filename, expected, mode, reopen);
        }
    }
    Require(std::filesystem::remove(filename));
    Require(std::filesystem::remove(directory));
    return correct;
}

int main() {
    Require(_Getmbcurmax_nid_postfix() == 1 && _Getmbcurmax_nid_postfix() == ___mb_cur_max_nid_postfix());
    Require(fdopen_nid_postfix(-1, "rb") == nullptr && *__error_nid_postfix() == 9);
    Require(fdopen_nid_postfix(0, nullptr) == nullptr && *__error_nid_postfix() == 22);
    Require(fdopen_nid_postfix(0, "invalid") == nullptr && *__error_nid_postfix() == 22);
    std::FILE* original = std::tmpfile();
    Require(original != nullptr);
    std::fputs("retained", original);
    std::fflush(original);
#ifdef _WIN32
    const int descriptor = _dup(_fileno(original));
#else
    const int descriptor = ::dup(::fileno(original));
#endif
    Require(descriptor >= 0);
    auto* wrapped = fdopen_nid_postfix(descriptor, "r+b");
    Require(wrapped != nullptr && fileno_nid_postfix(wrapped) == descriptor);
    setbuf_nid_postfix(wrapped, nullptr);
    Require(fseek_nid_postfix(wrapped, 0, SEEK_SET) == 0);
    char contents[32]{};
    Require(fgets_nid_postfix(contents, sizeof(contents), wrapped) == contents && std::strcmp(contents, "retained") == 0);
    Require(fclose_nid_postfix(wrapped) == 0);
#ifdef _WIN32
    Require(_close(descriptor) == -1);
#else
    Require(::close(descriptor) == -1);
#endif
    Require(std::fclose(original) == 0);
    char stringOutput[256];
    std::memset(stringOutput, '!', sizeof(stringOutput));
    std::int64_t count = -1;
    const char expectedString[] = "guest:4294967297:  3.50:1,2,3,4,5,6,7,8:%";
    const int written = FormatString(stringOutput, "%s:%ld:%*.*f:%d,%d,%d,%d,%d,%d,%d,%d:%%%ln",
        "guest", std::int64_t{4294967297}, 6, 2, 3.5, 1, 2, 3, 4, 5, 6, 7, 8, &count);
    Require(written == sizeof(expectedString) - 1 && count == written);
    Require(std::strcmp(stringOutput, expectedString) == 0 && stringOutput[written + 1] == '!');
    Require(FormatString(stringOutput, "%.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.2Lf",
        1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 1.25L) == 25);
    Require(std::strcmp(stringOutput, "1 2 3 4 5 6 7 8 9 10 1.25") == 0);
    Require(FormatString(stringOutput, "") == 0 && stringOutput[0] == '\0');
    Require(__isthreaded_nid_postfix == 1);
    Require(__stdoutp_nid_postfix == &_Stdout_nid_postfix);
    Require(__stderrp_nid_postfix == &_Stderr_nid_postfix);
    Require(fileno_nid_postfix(__stdinp_nid_postfix) == 0);
    Require(fileno_nid_postfix(__stdoutp_nid_postfix) == 1);
    Require(fileno_nid_postfix(__stderrp_nid_postfix) == 2);
    FileStream stream(std::tmpfile());
    auto& guest = *reinterpret_cast<GuestFilePrefix*>(&stream);
    Require(&guest == &stream.GuestState());
    Require(guest.position == nullptr && guest.readRemaining == 0 && guest.writeRemaining == 0);
    Require(guest.descriptor == fileno_nid_postfix(&stream));
    FileStream lineBuffered(std::tmpfile());
    Require(setvbuf_nid_postfix(&lineBuffered, nullptr, 1, 0) == 0);
    FileStream fullyBuffered(std::tmpfile());
    Require(setvbuf_nid_postfix(&fullyBuffered, nullptr, 0, 0) == 0);
    Require(setvbuf_nid_postfix(&stream, nullptr, 2, 0) == 0);
    Require(fputc_nid_postfix('A', &stream) == 'A');
    Require(--guest.writeRemaining < 0 && __swbuf_nid_postfix('\n', &stream) == '\n');
    std::rewind(stream.GetHandle());
    Require(--guest.readRemaining < 0 && __srget_nid_postfix(&stream) == 'A');
    Require(ungetc_nid_postfix('B', &stream) == 'B');
    char text[8]{};
    Require(fgets_nid_postfix(text, sizeof(text), &stream) == text);
    Require(std::strcmp(text, "B\n") == 0);
    Require(fgetc_nid_postfix(&stream) == EOF);
    Require(feof_nid_postfix(&stream) && (guest.flags & 0x20));
    clearerr_nid_postfix(&stream);
    Require(!feof_nid_postfix(&stream) && !(guest.flags & 0x20));
    stream.Close();
    Require(guest.flags == 0 && guest.descriptor == -1);

    FileStream wide(std::tmpfile());
    Require(fputwc_nid_postfix(u'A', &wide) == u'A');
    Require(fputws_nid_postfix(u"B\x00E9", &wide) == 2);
    std::rewind(wide.GetHandle());
    char wideBytes[8]{};
    Require(std::fread(wideBytes, 1, 4, wide.GetHandle()) == 4);
    Require(std::memcmp(wideBytes, "AB\xC3\xA9", 4) == 0);
    wide.Close();

    FileStream formatted(std::tmpfile());
    const char expected[] = "guest 4294967297 1.25 1 2 3 4 5 6 7 8\n";
    Require(fprintf_nid_postfix(&formatted, "%s %ld %.2f %d %d %d %d %d %d %d %d\n",
        "guest", std::int64_t{4294967297}, 1.25, 1, 2, 3, 4, 5, 6, 7, 8) == sizeof(expected) - 1);
    Require(WriteFormatted(&formatted, "%*.*f:%s", 6, 2, 3.5, "end") == 10);
    std::rewind(formatted.GetHandle());
    char output[128]{};
    Require(fgets_nid_postfix(output, sizeof(output), &formatted) == output);
    Require(std::strcmp(output, expected) == 0);
    Require(fgets_nid_postfix(output, sizeof(output), &formatted) == output);
    Require(std::strcmp(output, "  3.50:end") == 0);
    formatted.Close();

    FileStream scanned(std::tmpfile());
    Require(fprintf_nid_postfix(&scanned, "%d %s", 42, "answer") == 9);
    std::rewind(scanned.GetHandle());
    Require(fscanf_nid_postfix(&scanned, "%*d") == 0);
    Require(ftello_nid_postfix(&scanned) == 2);
    std::rewind(scanned.GetHandle());
    int scannedNumber = 0;
    char scannedWord[16]{};
    std::fputs("fscanf: suppressed conversion passed; assigning register arguments\n", stderr);
    std::fflush(stderr);
    Require(fscanf_nid_postfix(&scanned, "%d %15s", &scannedNumber, scannedWord) == 2);
    Require(scannedNumber == 42 && std::strcmp(scannedWord, "answer") == 0);
    Require(fscanf_nid_postfix(&scanned, "%d", &scannedNumber) == EOF);
    Require(feof_nid_postfix(&scanned) && (scanned.GuestState().flags & 0x20));
    scanned.Close();

    FileStream scanMany(std::tmpfile());
    Require(std::fputs("7 1 2 3 4 5 6 7 8 4294967297 -4294967298 4294967299 abc %!", scanMany.GetHandle()) >= 0);
    std::rewind(scanMany.GetHandle());
    int numbers[8]{};
    std::int64_t large = 0, negative = 0;
    std::uint64_t sized = 0;
    char letters[4]{};
    std::int64_t consumed = -1;
    Require(fscanf_nid_postfix(&scanMany, "%*d %d %d %d %d %d %d %d %d %ld %jd %zu %3[a-z] %%%ln",
        &numbers[0], &numbers[1], &numbers[2], &numbers[3], &numbers[4], &numbers[5], &numbers[6], &numbers[7],
        &large, &negative, &sized, letters, &consumed) == 12);
    for (int i = 0; i < 8; ++i) Require(numbers[i] == i + 1);
    Require(large == INT64_C(4294967297) && negative == -INT64_C(4294967298) && sized == UINT64_C(4294967299));
    Require(std::strcmp(letters, "abc") == 0 && consumed == ftello_nid_postfix(&scanMany));
    Require(fgetc_nid_postfix(&scanMany) == '!');
    int unmatched = 123;
    Require(fseeko_nid_postfix(&scanMany, -1, SEEK_CUR) == 0);
    Require(fscanf_nid_postfix(&scanMany, "%d", &unmatched) == 0 && unmatched == 123);
    Require(fgetc_nid_postfix(&scanMany) == '!');
    Require(fscanf_nid_postfix(&scanMany, "%d", &unmatched) == EOF && unmatched == 123);
    scanMany.Close();

    FileStream scanWrapped(std::tmpfile());
    Require(std::fputs("3735928559 4294967296 -2147483649 4294967297 65536 300 99999999999999999999", scanWrapped.GetHandle()) >= 0);
    std::rewind(scanWrapped.GetHandle());
    int identifier = 0, wrappedZero = 1, wrappedNegative = 0, saturated = 0;
    unsigned wrappedUnsigned = 0;
    short wrappedShort = 1;
    unsigned char wrappedByte = 0;
    Require(fscanf_nid_postfix(&scanWrapped, "%d %i %d %u %hd %hhu %d", &identifier, &wrappedZero, &wrappedNegative,
        &wrappedUnsigned, &wrappedShort, &wrappedByte, &saturated) == 7);
    Require(static_cast<unsigned>(identifier) == 0xdeadbeefu && wrappedZero == 0 && wrappedNegative == INT32_MAX);
    Require(wrappedUnsigned == 1 && wrappedShort == 0 && wrappedByte == 44 && saturated == -1);
    scanWrapped.Close();

    FileStream positioned(std::tmpfile());
    constexpr std::int64_t largeOffset = INT64_C(4294967313);
    Require(fseeko_nid_postfix(&positioned, largeOffset, SEEK_SET) == 0);
    Require(ftello_nid_postfix(&positioned) == largeOffset);
    Require(ftell_nid_postfix(&positioned) == largeOffset);
    Require(fseek_nid_postfix(&positioned, -9, SEEK_CUR) == 0);
    Require(ftello_nid_postfix(&positioned) == largeOffset - 9);
    Require(fseek_nid_postfix(&positioned, largeOffset, SEEK_SET) == 0);
    Require(ftell_nid_postfix(&positioned) == largeOffset);
    Require(fseeko_nid_postfix(&positioned, 0, 12345) == -1 && *__error_nid_postfix() == 22);
    Require(ftello_nid_postfix(&positioned) == largeOffset);
    Require(fseeko_nid_postfix(&positioned, 0, SEEK_END) == 0);
    Require(ftello_nid_postfix(&positioned) == 0); // Seeking alone did not extend the file.
    Require(fgetc_nid_postfix(&positioned) == EOF && feof_nid_postfix(&positioned));
    Require(fseeko_nid_postfix(&positioned, 0, SEEK_SET) == 0);
    Require(!feof_nid_postfix(&positioned));
    positioned.Close();

    const auto filename = "anyps5-reopen-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    FileStream redirected(std::tmpfile());
    Require(fgetc_nid_postfix(&redirected) == EOF && feof_nid_postfix(&redirected));
    Require(freopen_nid_postfix(filename.c_str(), "w+b", &redirected) == &redirected);
    Require(!feof_nid_postfix(&redirected));
    Require(fputc_nid_postfix('R', &redirected) == 'R');
    Require(fseeko_nid_postfix(&redirected, 0, SEEK_SET) == 0);
    Require(fgetc_nid_postfix(&redirected) == 'R');
    Require(freopen_nid_postfix(filename.c_str(), "ab", &redirected) == &redirected);
    Require(fputc_nid_postfix('S', &redirected) == 'S');
    Require(freopen_nid_postfix(nullptr, "r", &redirected) == nullptr && *__error_nid_postfix() == 45);
    redirected.Close();
    { std::ifstream input(filename, std::ios::binary); std::string contents; std::getline(input, contents);
      Require(contents == "RS"); }
    Require(std::filesystem::remove(filename));
    FileStream failed(std::tmpfile());
    Require(freopen_nid_postfix(filename.c_str(), "rb", &failed) == nullptr);
    Require(*__error_nid_postfix() == 2);
    Require(failed.GuestState().flags == 0 && failed.GuestState().descriptor == -1);
    return CheckBinaryModes() ? 0 : 1;
}
