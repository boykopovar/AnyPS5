#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <initializer_list>
#include <limits>
#include <chrono>
#include <atomic>
#include <thread>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
extern "C" {
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
void APS5_VABI clearerr_unlocked_nid_postfix(FileStream*);
int APS5_VABI fileno_unlocked_nid_postfix(FileStream*);
char* APS5_VABI fgetln_nid_postfix(FileStream*, std::size_t*);
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
int APS5_VABI fputs_nid_postfix(const char*, FileStream*);
int APS5_VABI printf_nid_postfix(const char*, ...);
int APS5_VABI vprintf_nid_postfix(const char*, void*);
int APS5_VABI puts_nid_postfix(const char*);
int APS5_VABI vfprintf_nid_postfix(FileStream*, const char*, void*);
int APS5_VABI vsprintf_nid_postfix(char*, const char*, void*);
int APS5_VABI fgetc_nid_postfix(FileStream*);
int APS5_VABI fputc_nid_postfix(int, FileStream*);
int APS5_VABI getc_unlocked_nid_postfix(FileStream*);
int APS5_VABI putc_unlocked_nid_postfix(int, FileStream*);
int APS5_VABI fputwc_nid_postfix(char16_t, FileStream*);
int APS5_VABI fputws_nid_postfix(const char16_t*, FileStream*);
std::int32_t APS5_VABI getwc_nid_postfix(FileStream*);
std::int32_t APS5_VABI getwchar_nid_postfix();
std::int32_t APS5_VABI fgetwc_nid_postfix(FileStream*);
int APS5_VABI putwc_nid_postfix(char16_t, FileStream*);
int APS5_VABI putwchar_nid_postfix(char16_t);
char16_t* APS5_VABI fgetws_nid_postfix(char16_t*, int, FileStream*);
int APS5_VABI getchar_unlocked_nid_postfix();
int APS5_VABI putchar_unlocked_nid_postfix(int);
int APS5_VABI fscanf_nid_postfix(FileStream*, const char*, ...);
int APS5_VABI __srget_nid_postfix(FileStream*);
int APS5_VABI __swbuf_nid_postfix(int, FileStream*);
int APS5_VABI ungetc_nid_postfix(int, FileStream*);
char* APS5_VABI fgets_nid_postfix(char*, int, FileStream*);
int APS5_VABI feof_nid_postfix(FileStream*);
int APS5_VABI ferror_nid_postfix(FileStream*);
int APS5_VABI feof_unlocked_nid_postfix(FileStream*);
int APS5_VABI ferror_unlocked_nid_postfix(FileStream*);
int APS5_VABI fileno_nid_postfix(FileStream*);
void APS5_VABI clearerr_nid_postfix(FileStream*);
void APS5_VABI rewind_nid_postfix(FileStream*);
int APS5_VABI setvbuf_nid_postfix(FileStream*, char*, int, std::size_t);
void APS5_VABI setbuf_nid_postfix(FileStream*, char*);
FileStream* APS5_VABI fdopen_nid_postfix(int, const char*);
int APS5_VABI fclose_nid_postfix(FileStream*);
int APS5_VABI fflush_nid_postfix(FileStream*);
int APS5_VABI _Getmbcurmax_nid_postfix();
int APS5_VABI ___mb_cur_max_nid_postfix();
void APS5_VABI flockfile_nid_postfix(FileStream*);
void APS5_VABI funlockfile_nid_postfix(FileStream*);
int APS5_VABI ftrylockfile_nid_postfix(FileStream*);
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
static int APS5_VABI PrintStandard(const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vprintf_nid_postfix(format, args);
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

static void CheckUnlockedStreams() {
    FileStream stream(std::tmpfile());
    auto& guest = stream.GuestState();
    const auto originalFlags = guest.flags;
    for (const int value : {0, 0x7f, 0x80, 0xff, 0x1ab, -2}) {
        guest.readRemaining = 13;
        guest.writeRemaining = 19;
        Require(putc_unlocked_nid_postfix(value, &stream) == static_cast<unsigned char>(value));
        Require(guest.readRemaining == 0 && guest.writeRemaining == 0 && guest.flags == originalFlags);
    }
    std::rewind(stream.GetHandle());
    for (const int value : {0, 0x7f, 0x80, 0xff, 0x1ab, -2}) {
        guest.readRemaining = 13;
        guest.writeRemaining = 19;
        Require(getc_unlocked_nid_postfix(&stream) == static_cast<unsigned char>(value));
        Require(guest.readRemaining == 0 && guest.writeRemaining == 0 && guest.flags == originalFlags);
    }
    Require(getc_unlocked_nid_postfix(&stream) == EOF && (guest.flags & 0x20));
    std::clearerr(stream.GetHandle());
    Require(std::ungetc('Q', stream.GetHandle()) == 'Q');
    Require(getc_unlocked_nid_postfix(&stream) == 'Q' && !(guest.flags & 0x20));
    std::rewind(stream.GetHandle());
    stream.SetError();
    Require(putc_unlocked_nid_postfix('R', &stream) == 'R' && (guest.flags & 0x40));
    clearerr_nid_postfix(&stream);
    Require(!(guest.flags & 0x40));
    stream.Close();

    const auto filename = "anyps5-unlocked-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    { std::ofstream seed(filename, std::ios::binary); seed.put('A'); Require(seed.good()); }
    FileStream readOnly(std::fopen(filename.c_str(), "rb"));
    Require(putc_unlocked_nid_postfix('X', &readOnly) == EOF && (readOnly.GuestState().flags & 0x40));
    Require(ferror_unlocked_nid_postfix(&readOnly) == 1 && feof_unlocked_nid_postfix(&readOnly) == 0);
    clearerr_unlocked_nid_postfix(&readOnly);
    Require(ferror_unlocked_nid_postfix(&readOnly) == 0);
    readOnly.Close();
    Require(CheckFileBytes(filename, "A", "rb", false));
    Require(std::filesystem::remove(filename));
}

static void CheckUnlockedQueries() {
    FileStream stream(std::tmpfile());
    Require(feof_unlocked_nid_postfix(&stream) == 0 && ferror_unlocked_nid_postfix(&stream) == 0);
    Require(std::fgetc(stream.GetHandle()) == EOF);
    Require(feof_unlocked_nid_postfix(&stream) == 1 && ferror_unlocked_nid_postfix(&stream) == 0);
    Require((stream.GuestState().flags & 0x60) == 0x20);
    stream.SetError();
    Require(feof_unlocked_nid_postfix(&stream) == 1 && ferror_unlocked_nid_postfix(&stream) == 1);
    clearerr_unlocked_nid_postfix(&stream);
    Require(feof_unlocked_nid_postfix(&stream) == 0 && ferror_unlocked_nid_postfix(&stream) == 0);
    Require(std::fgetc(stream.GetHandle()) == EOF && std::ungetc('Q', stream.GetHandle()) == 'Q');
    Require(feof_unlocked_nid_postfix(&stream) == 0 && ferror_unlocked_nid_postfix(&stream) == 0);
    Require(std::fgetc(stream.GetHandle()) == 'Q');
    Require(fputc_nid_postfix(0x7f, &stream) == 0x7f);
    std::rewind(stream.GetHandle());
    Require(fgetc_nid_postfix(&stream) == 0x7f && feof_nid_postfix(&stream) == 0);
    Require(fgetc_nid_postfix(&stream) == -1 && feof_nid_postfix(&stream) != 0);
    clearerr_unlocked_nid_postfix(&stream);
    Require(feof_nid_postfix(&stream) == 0 && ferror_nid_postfix(&stream) == 0);
    Require(fileno_unlocked_nid_postfix(&stream) == fileno_nid_postfix(&stream) && fileno_unlocked_nid_postfix(&stream) >= 0);
    Require(fileno_unlocked_nid_postfix(nullptr) == -1);
    bool halted = false;
    std::size_t length = 0;
    try { fgetln_nid_postfix(&stream, &length); } catch (const std::runtime_error&) { halted = true; }
    Require(halted);
}

static void CheckTryLock() {
    FileStream stream(std::tmpfile());
    const auto tryFromOtherThread = [&] {
        int result = 0;
        std::thread other([&] {
            result = ftrylockfile_nid_postfix(&stream);
            if (result == 0) funlockfile_nid_postfix(&stream);
        });
        other.join();
        return result;
    };
    Require(ftrylockfile_nid_postfix(&stream) == 0);
    Require(ftrylockfile_nid_postfix(&stream) == 0);
    Require(tryFromOtherThread() == -1);
    funlockfile_nid_postfix(&stream);
    Require(tryFromOtherThread() == -1);
    std::atomic<bool> written = false;
    std::thread writer([&] {
        Require(fputc_nid_postfix('B', &stream) == 'B');
        written = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!written && fputc_nid_postfix('A', &stream) == 'A');
    funlockfile_nid_postfix(&stream);
    writer.join();
    Require(written && tryFromOtherThread() == 0);
    std::atomic<bool> held = false, release = false;
    std::thread owner([&] {
        flockfile_nid_postfix(&stream);
        held = true;
        while (!release) std::this_thread::yield();
        funlockfile_nid_postfix(&stream);
    });
    while (!held) std::this_thread::yield();
    const int contended = ftrylockfile_nid_postfix(&stream);
    release = true;
    owner.join();
    Require(contended == -1 && ftrylockfile_nid_postfix(&stream) == 0);
    funlockfile_nid_postfix(&stream);
    Require(tryFromOtherThread() == 0);
    Require(fseeko_nid_postfix(&stream, 0, SEEK_SET) == 0);
    Require(fgetc_nid_postfix(&stream) == 'A' && fgetc_nid_postfix(&stream) == 'B');
    stream.Close();
    bool threw = false;
    try { ftrylockfile_nid_postfix(&stream); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    threw = false;
    try { ftrylockfile_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

static void CheckUnlockedStreamLock(bool write, int (APS5_VABI *query)(FileStream*) = nullptr) {
    FileStream stream(std::tmpfile());
    Require(std::fputc('A', stream.GetHandle()) == 'A');
    std::rewind(stream.GetHandle());
    std::atomic<bool> held = false, release = false, entered = false, done = false;
    int result = EOF;
    std::thread owner([&] {
        flockfile_nid_postfix(&stream);
        held = true;
        while (!release) std::this_thread::yield();
        funlockfile_nid_postfix(&stream);
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!held && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    if (!held) {
        release = true;
        owner.join();
        Require(false);
    }
    std::thread caller([&] {
        entered = true;
        result = query ? query(&stream) : write ? putc_unlocked_nid_postfix('B', &stream) : getc_unlocked_nid_postfix(&stream);
        done = true;
    });
    const auto enteredDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!entered && std::chrono::steady_clock::now() < enteredDeadline) std::this_thread::yield();
    if (!entered) {
        release = true;
        owner.join();
        caller.join();
        Require(false);
    }
    const auto completionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!done && std::chrono::steady_clock::now() < completionDeadline) std::this_thread::yield();
    const bool completedWhileHeld = done;
    release = true;
    owner.join();
    caller.join();
    Require(completedWhileHeld && result == (query ? 0 : write ? 'B' : 'A'));
    stream.Close();
}

static void CheckFgetsSize() {
    FileStream stream(std::tmpfile());
    Require(fwrite_nid_postfix("line\n", 1, 5, &stream) == 5);
    rewind_nid_postfix(&stream);
    for (const int size : {0, -1, std::numeric_limits<int>::min()}) {
        char buffer[8];
        std::memset(buffer, '!', sizeof(buffer));
        *__error_nid_postfix() = 0;
        Require(fgets_nid_postfix(buffer, size, &stream) == nullptr);
        Require(*__error_nid_postfix() == 22 && ferror_nid_postfix(&stream) != 0 && feof_nid_postfix(&stream) == 0);
        Require(buffer[0] == '!' && ftello_nid_postfix(&stream) == 0);
        Require(ferror_nid_postfix(&stream) != 0);
        clearerr_nid_postfix(&stream);
        Require(ferror_nid_postfix(&stream) == 0);
    }
    char line[8]{};
    Require(fgets_nid_postfix(line, sizeof(line), &stream) == line && std::strcmp(line, "line\n") == 0);
    Require(ferror_nid_postfix(&stream) == 0);
    stream.Close();
}

static void CheckRewind() {
    FileStream stream(std::tmpfile());
    Require(fwrite_nid_postfix("ab", 1, 2, &stream) == 2);
    *__error_nid_postfix() = 7;
    rewind_nid_postfix(&stream);
    Require(*__error_nid_postfix() == 7 && ftello_nid_postfix(&stream) == 0);
    Require(fgetc_nid_postfix(&stream) == 'a' && ungetc_nid_postfix('z', &stream) == 'z');
    rewind_nid_postfix(&stream);
    Require(fgetc_nid_postfix(&stream) == 'a' && fgetc_nid_postfix(&stream) == 'b' && fgetc_nid_postfix(&stream) == EOF);
    Require(feof_nid_postfix(&stream) != 0);
    stream.SetError();
    Require(ferror_nid_postfix(&stream) != 0);
    rewind_nid_postfix(&stream);
    Require(feof_nid_postfix(&stream) == 0 && ferror_nid_postfix(&stream) == 0 && fgetc_nid_postfix(&stream) == 'a');
    stream.Close();
    int pipeEnds[2];
#ifdef _WIN32
    Require(_pipe(pipeEnds, 4096, _O_BINARY) == 0 && _write(pipeEnds[1], "pq", 2) == 2 && _close(pipeEnds[1]) == 0);
    FileStream pipe(_fdopen(pipeEnds[0], "rb"));
#else
    Require(::pipe(pipeEnds) == 0);
    Require(::write(pipeEnds[1], "pq", 2) == 2);
    ::close(pipeEnds[1]);
    FileStream pipe(::fdopen(pipeEnds[0], "r"));
#endif
    Require(fgetc_nid_postfix(&pipe) == 'p');
    for (const int origin : {SEEK_SET, SEEK_CUR, SEEK_END}) {
        *__error_nid_postfix() = 0;
        Require(fseeko_nid_postfix(&pipe, 0, origin) == -1 && *__error_nid_postfix() == 29);
    }
    *__error_nid_postfix() = 0;
    Require(ftello_nid_postfix(&pipe) == -1 && *__error_nid_postfix() == 29 && ferror_nid_postfix(&pipe) == 0);
    Require(fgetc_nid_postfix(&pipe) == 'q' && fgetc_nid_postfix(&pipe) == EOF && feof_nid_postfix(&pipe) != 0);
    *__error_nid_postfix() = 0;
    rewind_nid_postfix(&pipe);
    Require(*__error_nid_postfix() == 29 && feof_nid_postfix(&pipe) == 0 && ferror_nid_postfix(&pipe) == 0);
    pipe.Close();
}

static void CheckFlushWhileReading() {
    const auto filename = "anyps5-flush-read-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    { std::ofstream seed(filename, std::ios::binary); seed << "abcdef\n"; Require(seed.good()); }
    for (const char* mode : {"r", "r+"}) {
        FileStream* stream = fopen_nid_postfix(filename.c_str(), mode);
        Require(stream != nullptr);
        Require(fgetc_nid_postfix(stream) == 'a' && ungetc_nid_postfix('Z', stream) == 'Z');
        Require(fflush_nid_postfix(stream) == 0);
        char rest[16]{};
        const auto count = fread_nid_postfix(rest, 1, sizeof(rest) - 1, stream);
        if (count != 7 || std::strcmp(rest, "Zbcdef\n") != 0) {
            std::string shown;
            for (std::size_t index = 0; index < count; ++index) shown += rest[index] == '\n' ? std::string("\\n") : std::string(1, rest[index]);
            std::fprintf(stderr, "fflush of a \"%s\" stream that is reading changed the data read next: \"%s\" (%zu bytes), expected \"Zbcdef\\n\" (7 bytes)\n",
                mode, shown.c_str(), count);
            std::abort();
        }
        Require(fclose_nid_postfix(stream) == 0);
    }
    std::filesystem::remove(filename);
}

static bool CheckBinaryModes() {
    const auto directory = "anyps5-byte-stream-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Require(std::filesystem::create_directory(directory));
    const auto filename = directory + "/bytes";
    const std::string original("A\r\n\x1a" "B\0C", 7);
    const std::string written("D\n\x1a" "E\0F", 6);
    const char* modes[] = {"r", "r+", "w", "w+", "a", "a+", "rb", "rb+", "r+b", "wb", "wb+", "w+b", "ab", "ab+", "a+b", "rt", "r+t", "wt", "w+e", "ae", "rbv"};
    bool correct = true;
    for (const auto* mode : modes) {
        for (const bool reopen : {false, true}) {
            { std::ofstream seed(filename, std::ios::binary | std::ios::trunc); seed.write(original.data(), original.size()); Require(seed.good()); }
            FileStream redirected(std::tmpfile());
            FileStream* stream = reopen ? freopen_nid_postfix(filename.c_str(), mode, &redirected) : fopen_nid_postfix(filename.c_str(), mode);
            Require(stream != nullptr);
            const bool update = std::strchr(mode, '+') != nullptr;
            if (*mode == 'a') {
                const bool atEnd = ftello_nid_postfix(stream) == static_cast<std::int64_t>(original.size()) &&
                    (!update || fgetc_nid_postfix(stream) == EOF);
                if (!atEnd) std::fprintf(stderr, "%s %s: the append stream does not start at the end of the file\n", reopen ? "freopen" : "fopen", mode);
                correct &= atEnd;
            }
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
    for (const auto* mode : {"", "q", "tr", "rx", "rbx"}) {
        FileStream redirected(std::tmpfile());
        Require(fopen_nid_postfix(filename.c_str(), mode) == nullptr && *__error_nid_postfix() == 22);
        Require(freopen_nid_postfix(filename.c_str(), mode, &redirected) == nullptr && *__error_nid_postfix() == 22);
    }
    Require(fopen_nid_postfix(filename.c_str(), "wx") == nullptr && *__error_nid_postfix() == 17);
    FileStream* readOnly = fopen_nid_postfix(filename.c_str(), "rt+");
    Require(readOnly != nullptr && fputc_nid_postfix('X', readOnly) == EOF);
    Require(fclose_nid_postfix(readOnly) == 0);
    Require(std::filesystem::remove(filename));
    FileStream* created = fopen_nid_postfix(filename.c_str(), "wx");
    Require(created != nullptr && fputc_nid_postfix('Y', created) == 'Y');
    Require(fclose_nid_postfix(created) == 0);
    correct &= CheckFileBytes(filename, "Y", "wx", false);
    Require(std::filesystem::remove(filename));
    Require(std::filesystem::remove(directory));
    return correct;
}

static int DuplicateDescriptor(std::FILE* stream) {
#ifdef _WIN32
    return _dup(_fileno(stream));
#else
    return ::dup(::fileno(stream));
#endif
}

static void CheckDescriptorModes(std::FILE* original) {
    for (const char* mode : {"rt", "r+t", "re", "rbe", "rw", "r+x"}) {
        const int descriptor = DuplicateDescriptor(original);
        Require(descriptor >= 0);
        auto* wrapped = fdopen_nid_postfix(descriptor, mode);
        Require(wrapped != nullptr && fseek_nid_postfix(wrapped, 0, SEEK_SET) == 0);
        char contents[32]{};
        Require(fgets_nid_postfix(contents, sizeof(contents), wrapped) == contents && std::strcmp(contents, "retained") == 0);
        Require(fclose_nid_postfix(wrapped) == 0);
    }
    const int descriptor = DuplicateDescriptor(original);
    Require(descriptor >= 0);
    *__error_nid_postfix() = 0;
    Require(fdopen_nid_postfix(descriptor, "rx") == nullptr && *__error_nid_postfix() == 22);
#ifdef _WIN32
    Require(_close(descriptor) == 0);
#else
    Require(::close(descriptor) == 0);
#endif
}

static void CheckDescriptorAppend(std::FILE* original) {
    std::string expected = "retained";
    for (const char* mode : {"a", "ab", "a+", "ae"}) {
        const int descriptor = DuplicateDescriptor(original);
#ifdef _WIN32
        Require(descriptor >= 0 && _lseek(descriptor, 2, SEEK_SET) == 2);
#else
        Require(descriptor >= 0 && ::lseek(descriptor, 2, SEEK_SET) == 2);
#endif
        auto* wrapped = fdopen_nid_postfix(descriptor, mode);
        Require(wrapped != nullptr && fileno_nid_postfix(wrapped) == descriptor);
        Require(fputc_nid_postfix('+', wrapped) == '+' && fflush_nid_postfix(wrapped) == 0);
        Require(fseek_nid_postfix(wrapped, 0, SEEK_SET) == 0 && fputc_nid_postfix('+', wrapped) == '+');
        Require(fclose_nid_postfix(wrapped) == 0);
        expected += "++";
        const int reader = DuplicateDescriptor(original);
        Require(reader >= 0);
        auto* written = fdopen_nid_postfix(reader, "r");
        Require(written != nullptr && fseek_nid_postfix(written, 0, SEEK_SET) == 0);
        char contents[32]{};
        Require(fgets_nid_postfix(contents, sizeof(contents), written) == contents && expected == contents);
        Require(fclose_nid_postfix(written) == 0);
    }
}

static void CheckDescriptorAccessModes() {
    const auto filename = "anyps5-fdopen-modes-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    for (const int access : {0, 1, 2}) {
        for (const char* mode : {"r", "w", "a", "r+", "w+", "a+"}) {
            { std::ofstream output(filename); output << "retained"; }
#ifdef _WIN32
            const int descriptor = _open(filename.c_str(), access | _O_BINARY);
#else
            const int descriptor = ::open(filename.c_str(), access);
#endif
            Require(descriptor >= 0);
            const int requested = std::strchr(mode, '+') ? 2 : *mode == 'r' ? 0 : 1;
            const bool incompatible = access != 2 && access != requested;
#ifdef _WIN32
            const auto originalHandle = _get_osfhandle(descriptor);
            if (incompatible) Require(_lseek(descriptor, 2, SEEK_SET) == 2);
#else
            const int originalFlags = ::fcntl(descriptor, F_GETFL);
            Require(originalFlags >= 0);
            if (incompatible) Require(::lseek(descriptor, 2, SEEK_SET) == 2);
#endif
            *__error_nid_postfix() = 0;
            auto* stream = fdopen_nid_postfix(descriptor, mode);
            if (incompatible) {
                if (stream != nullptr || *__error_nid_postfix() != 22)
                    std::fprintf(stderr, "fdopen access=%d mode=%s returned %p with errno=%d\n", access, mode,
                        static_cast<void*>(stream), *__error_nid_postfix());
                Require(stream == nullptr && *__error_nid_postfix() == 22);
#ifdef _WIN32
                Require(_get_osfhandle(descriptor) == originalHandle && _lseek(descriptor, 0, SEEK_CUR) == 2);
#else
                Require(::fcntl(descriptor, F_GETFL) == originalFlags && ::lseek(descriptor, 0, SEEK_CUR) == 2);
#endif
                Require(CheckFileBytes(filename, "retained", mode, false));
                const bool probeAppend = access == 1 && *mode == 'a';
#ifdef _WIN32
                if (probeAppend) Require(_write(descriptor, "X", 1) == 1 && _lseek(descriptor, 0, SEEK_CUR) == 3);
                Require(_close(descriptor) == 0);
#else
                if (probeAppend) Require(::write(descriptor, "X", 1) == 1 && ::lseek(descriptor, 0, SEEK_CUR) == 3);
                Require(::close(descriptor) == 0);
#endif
                Require(CheckFileBytes(filename, probeAppend ? "reXained" : "retained", mode, false));
            } else {
                Require(stream != nullptr);
                if (requested == 0) Require(fgetc_nid_postfix(stream) == 'r');
                else Require(fputc_nid_postfix('X', stream) == 'X');
                Require(fclose_nid_postfix(stream) == 0);
            }
        }
    }
#ifdef _WIN32
    const int closed = _open(filename.c_str(), _O_RDONLY | _O_BINARY);
    Require(closed >= 0 && _close(closed) == 0);
#else
    const int closed = ::open(filename.c_str(), O_RDONLY);
    Require(closed >= 0 && ::close(closed) == 0);
#endif
    *__error_nid_postfix() = 0;
    Require(fdopen_nid_postfix(closed, "r") == nullptr && *__error_nid_postfix() == 9);
    Require(std::filesystem::remove(filename));
}

static void CheckClosedStreamWrites() {
    FileStream stream(std::tmpfile());
    Require(freopen_nid_postfix("missing-closed-stream-dir/out.log", "w", &stream) == nullptr);
    Require(fputs_nid_postfix("text", &stream) == -1 && *__error_nid_postfix() == 9);
    Require(fprintf_nid_postfix(&stream, "%d", 1) == -1 && *__error_nid_postfix() == 9);
    Require(fwrite_nid_postfix("text", 1, 4, &stream) == 0 && *__error_nid_postfix() == 9);
    Require(fputc_nid_postfix('t', &stream) == -1 && *__error_nid_postfix() == 9);
    Require(putc_unlocked_nid_postfix('t', &stream) == -1 && *__error_nid_postfix() == 9);
    Require(fflush_nid_postfix(&stream) == -1 && *__error_nid_postfix() == 9);
    char buffer[8] = {};
    Require(fread_nid_postfix(buffer, 1, sizeof(buffer), &stream) == 0 && *__error_nid_postfix() == 9);
    Require(fgetc_nid_postfix(&stream) == -1 && *__error_nid_postfix() == 9);
    Require(fgets_nid_postfix(buffer, sizeof(buffer), &stream) == nullptr && *__error_nid_postfix() == 9);
    Require(fseeko_nid_postfix(&stream, 0, SEEK_SET) == -1 && *__error_nid_postfix() == 9);
    Require(ftello_nid_postfix(&stream) == -1 && *__error_nid_postfix() == 9);
    Require(fileno_nid_postfix(&stream) == -1 && *__error_nid_postfix() == 9);
    Require(ferror_nid_postfix(&stream) == 0 && feof_nid_postfix(&stream) == 0);
    Require(fclose_nid_postfix(&stream) == -1 && *__error_nid_postfix() == 9);
}

static int CheckClosedStandardOutput() {
    Require(freopen_nid_postfix("missing-closed-stream-dir/out.log", "w", __stdoutp_nid_postfix) == nullptr);
    Require(printf_nid_postfix("%d", 1) == -1 && *__error_nid_postfix() == 9);
    Require(puts_nid_postfix("text") == -1 && *__error_nid_postfix() == 9);
    Require(PrintStandard("%d", 1) == -1 && *__error_nid_postfix() == 9);
    Require(fputs_nid_postfix("text", __stdoutp_nid_postfix) == -1 && *__error_nid_postfix() == 9);
    Require(std::fputs("host output\n", stdout) >= 0 && std::fflush(stdout) == 0 && !std::ferror(stdout));
    Require(freopen_nid_postfix("missing-closed-stream-dir/err.log", "a", __stderrp_nid_postfix) == nullptr);
    Require(std::fputs("host error\n", stderr) >= 0 && std::fflush(stderr) == 0 && !std::ferror(stderr));
    return 0;
}

static int CheckExclusiveStandardOutput() {
    const auto filename = "anyps5-exclusive-stdout-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Require(freopen_nid_postfix(filename.c_str(), "wx", __stdoutp_nid_postfix) == __stdoutp_nid_postfix);
    Require(fputs_nid_postfix("exclusive\n", __stdoutp_nid_postfix) >= 0 && fflush_nid_postfix(__stdoutp_nid_postfix) == 0);
    Require(fclose_nid_postfix(__stdoutp_nid_postfix) == 0);
    Require(CheckFileBytes(filename, "exclusive\n", "wx", true));
    Require(std::filesystem::remove(filename));
    return 0;
}

#ifdef _WIN32
static void CheckStandardStreamDeleteSharing() {
    int index = 0;
    for (auto* stream : {__stdinp_nid_postfix, __stderrp_nid_postfix}) {
        const std::string filename = "anyps5-standard-delete-" + std::to_string(index++) + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto* const native = stream->GetHandle();
        const int descriptor = ::_fileno(native);
        Require(freopen_nid_postfix(filename.c_str(), "w+b", stream) == stream);
        Require(stream->GetHandle() == native && ::_fileno(stream->GetHandle()) == descriptor);
        Require(fputc_nid_postfix('A', stream) == 'A' && fflush_nid_postfix(stream) == 0);
        const std::string renamed = filename + "-moved";
        std::filesystem::rename(filename, renamed);
        Require(fputc_nid_postfix('B', stream) == 'B' && fflush_nid_postfix(stream) == 0);
        Require(std::filesystem::remove(renamed));
        Require(fseeko_nid_postfix(stream, 0, SEEK_SET) == 0);
        Require(fgetc_nid_postfix(stream) == 'A' && fgetc_nid_postfix(stream) == 'B');
    }
}
#endif

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--closed-stdout") return CheckClosedStandardOutput();
    if (argc > 1 && std::string_view(argv[1]) == "--exclusive-stdout") return CheckExclusiveStandardOutput();
    CheckClosedStreamWrites();
    CheckDescriptorAccessModes();
    Require(_Getmbcurmax_nid_postfix() == 3 && ___mb_cur_max_nid_postfix() == 1);
    CheckFlushWhileReading();
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
    CheckDescriptorModes(original);
    CheckDescriptorAppend(original);
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
    auto* const savedInput = __stdinp_nid_postfix;
    auto* const savedOutput = __stdoutp_nid_postfix;
    FileStream customIn(std::tmpfile());
    Require(fputc_nid_postfix('T', &customIn) == 'T');
    std::rewind(customIn.GetHandle());
    __stdinp_nid_postfix = &customIn;
    Require(getchar_unlocked_nid_postfix() == 'T');
    Require(getchar_unlocked_nid_postfix() == EOF);
    Require(ungetc_nid_postfix('W', &customIn) == 'W');
    Require(getwchar_nid_postfix() == 'W');
    FileStream customOut(std::tmpfile());
    __stdoutp_nid_postfix = &customOut;
    Require(putchar_unlocked_nid_postfix('U') == 'U');
    Require(putwchar_nid_postfix(u'V') == u'V');
    std::rewind(customOut.GetHandle());
    Require(fgetc_nid_postfix(&customOut) == 'U');
    Require(fgetc_nid_postfix(&customOut) == 'V');
    __stdinp_nid_postfix = savedInput;
    __stdoutp_nid_postfix = savedOutput;
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
    Require(putc_unlocked_nid_postfix('C', &stream) == 'C');
    Require(--guest.writeRemaining < 0 && __swbuf_nid_postfix('\n', &stream) == '\n');
    std::rewind(stream.GetHandle());
    Require(--guest.readRemaining < 0 && __srget_nid_postfix(&stream) == 'A');
    Require(getc_unlocked_nid_postfix(&stream) == 'C');
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

    FileStream wideIo(std::tmpfile());
    Require(putwc_nid_postfix(u'H', &wideIo) == u'H');
    Require(putwc_nid_postfix(u'i', &wideIo) == u'i');
    Require(putwc_nid_postfix(u'\n', &wideIo) == u'\n');
    Require(fputws_nid_postfix(u"Next\n", &wideIo) == 5);
    Require(fputws_nid_postfix(u"End", &wideIo) == 3);
    std::rewind(wideIo.GetHandle());
    Require(getwc_nid_postfix(&wideIo) == 'H');
    std::rewind(wideIo.GetHandle());
    char16_t wideBuf[16]{};
    Require(fgetws_nid_postfix(wideBuf, 16, &wideIo) == wideBuf);
    Require(wideBuf[0] == u'H' && wideBuf[1] == u'i' && wideBuf[2] == u'\n' && wideBuf[3] == 0);
    Require(fgetws_nid_postfix(wideBuf, 16, &wideIo) == wideBuf);
    Require(wideBuf[0] == u'N' && wideBuf[1] == u'e' && wideBuf[2] == u'x' && wideBuf[3] == u't' && wideBuf[4] == u'\n' && wideBuf[5] == 0);
    Require(fgetws_nid_postfix(wideBuf, 16, &wideIo) == wideBuf);
    Require(wideBuf[0] == u'E' && wideBuf[1] == u'n' && wideBuf[2] == u'd' && wideBuf[3] == 0);
    Require(fgetws_nid_postfix(wideBuf, 16, &wideIo) == nullptr);
    Require(feof_nid_postfix(&wideIo));
    clearerr_nid_postfix(&wideIo);
    *__error_nid_postfix() = 0;
    Require(fgetws_nid_postfix(wideBuf, 0, &wideIo) == nullptr && *__error_nid_postfix() == 0);
    Require(fgetws_nid_postfix(wideBuf, -1, &wideIo) == nullptr && *__error_nid_postfix() == 0);
    Require(!ferror_nid_postfix(&wideIo));
    Require(fgetws_nid_postfix(nullptr, 16, &wideIo) == nullptr && *__error_nid_postfix() == 22);
    Require(ferror_nid_postfix(&wideIo));
    clearerr_nid_postfix(&wideIo);
    Require(fgetws_nid_postfix(wideBuf, 16, nullptr) == nullptr && *__error_nid_postfix() == 22);
    wideBuf[0] = u'X';
    wideBuf[1] = u'Y';
    std::rewind(wideIo.GetHandle());
    Require(fgetws_nid_postfix(wideBuf, 1, &wideIo) == nullptr && wideBuf[0] == u'X' && wideBuf[1] == u'Y');
    Require(fgetc_nid_postfix(&wideIo) == 'H');
    wideIo.Close();

    FileStream roundTrip(std::tmpfile());
    Require(putwc_nid_postfix(u'é', &roundTrip) == u'é');
    Require(putwc_nid_postfix(u'\u20ac', &roundTrip) == u'\u20ac');
    Require(putwc_nid_postfix(u'\n', &roundTrip) == u'\n');
    std::rewind(roundTrip.GetHandle());
    Require(getwc_nid_postfix(&roundTrip) == static_cast<std::int32_t>(u'é'));
    Require(getwc_nid_postfix(&roundTrip) == static_cast<std::int32_t>(u'\u20ac'));
    Require(getwc_nid_postfix(&roundTrip) == static_cast<std::int32_t>(u'\n'));
    Require(getwc_nid_postfix(&roundTrip) == -1);
    std::rewind(roundTrip.GetHandle());
    char16_t utfBuf[16]{};
    Require(fgetws_nid_postfix(utfBuf, 16, &roundTrip) == utfBuf);
    Require(utfBuf[0] == u'é' && utfBuf[1] == u'\u20ac' && utfBuf[2] == u'\n' && utfBuf[3] == 0);
    roundTrip.Close();

    FileStream wideIncomplete(std::tmpfile());
    Require(std::fwrite("\xE2\x82", 1, 2, wideIncomplete.GetHandle()) == 2);
    Require(fseeko_nid_postfix(&wideIncomplete, 0, SEEK_SET) == 0);
    *__error_nid_postfix() = 0;
    Require(fgetwc_nid_postfix(&wideIncomplete) == -1);
    Require(*__error_nid_postfix() == 86 && feof_nid_postfix(&wideIncomplete) != 0 && ferror_nid_postfix(&wideIncomplete) != 0);
    wideIncomplete.Close();

    FileStream wideInvalid(std::tmpfile());
    Require(std::fwrite("\xE2" "A", 1, 2, wideInvalid.GetHandle()) == 2);
    Require(fseeko_nid_postfix(&wideInvalid, 0, SEEK_SET) == 0);
    *__error_nid_postfix() = 0;
    Require(fgetwc_nid_postfix(&wideInvalid) == -1 && *__error_nid_postfix() == 86 && ferror_nid_postfix(&wideInvalid) != 0);
    clearerr_nid_postfix(&wideInvalid);
    Require(fseeko_nid_postfix(&wideInvalid, 1, SEEK_SET) == 0);
    Require(fgetwc_nid_postfix(&wideInvalid) == 'A' && ferror_nid_postfix(&wideInvalid) == 0);
    wideInvalid.Close();

    FileStream wideFourByte(std::tmpfile());
    Require(std::fwrite("\xF0\x9F\x98\x80", 1, 4, wideFourByte.GetHandle()) == 4);
    Require(fseeko_nid_postfix(&wideFourByte, 0, SEEK_SET) == 0);
    *__error_nid_postfix() = 0;
    Require(fgetwc_nid_postfix(&wideFourByte) == -1 && *__error_nid_postfix() == 86 && ferror_nid_postfix(&wideFourByte) != 0);
    wideFourByte.Close();

    FileStream wideRandom(std::tmpfile());
    wideRandom.MarkRandomDevice();
    *__error_nid_postfix() = 0;
    const std::int32_t randomCharacter = fgetwc_nid_postfix(&wideRandom);
    Require(randomCharacter == -1 ? *__error_nid_postfix() == 86 && ferror_nid_postfix(&wideRandom) != 0
                                  : randomCharacter >= 0 && randomCharacter <= 0xffff);
    wideRandom.Close();

    FileStream midErr(std::tmpfile());
    Require(fputc_nid_postfix('A', &midErr) == 'A');
    Require(fputc_nid_postfix(0xff, &midErr) == 0xff);
    std::rewind(midErr.GetHandle());
    char16_t midBuf[16]{};
    Require(fgetws_nid_postfix(midBuf, 16, &midErr) == midBuf && midBuf[0] == u'A' && midBuf[1] == 0);
    Require(ferror_nid_postfix(&midErr));
    midErr.Close();

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

    FileStream scanBinary(std::tmpfile());
    Require(std::fputs("0b101", scanBinary.GetHandle()) >= 0);
    std::rewind(scanBinary.GetHandle());
    int binaryValue = -1;
    Require(fscanf_nid_postfix(&scanBinary, "%i", &binaryValue) == 1 && binaryValue == 0);
    Require(fgetc_nid_postfix(&scanBinary) == 'b');
    scanBinary.Close();

#ifdef _WIN32
    FileStream scanSuppressed(std::tmpfile());
    Require(std::fputs("12", scanSuppressed.GetHandle()) >= 0);
    std::rewind(scanSuppressed.GetHandle());
    int suppressedTarget = 321;
    Require(fscanf_nid_postfix(&scanSuppressed, "%*d%d", &suppressedTarget) == 0 && suppressedTarget == 321);
    Require(fscanf_nid_postfix(&scanSuppressed, "%*d") == EOF);
    scanSuppressed.Close();
#endif

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
    FileStream locked(std::tmpfile());
    flockfile_nid_postfix(&locked);
    flockfile_nid_postfix(&locked);
    std::atomic<bool> lockedWrite = false;
    std::thread writer([&] {
        Require(fputc_nid_postfix('B', &locked) == 'B');
        lockedWrite = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!lockedWrite && fputc_nid_postfix('A', &locked) == 'A');
    funlockfile_nid_postfix(&locked);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!lockedWrite);
    funlockfile_nid_postfix(&locked);
    writer.join();
    Require(lockedWrite && fseeko_nid_postfix(&locked, 0, SEEK_SET) == 0);
    Require(fgetc_nid_postfix(&locked) == 'A' && fgetc_nid_postfix(&locked) == 'B');
    locked.Close();
    CheckRewind();
    CheckFgetsSize();
    CheckUnlockedStreams();
    CheckUnlockedStreamLock(false);
    CheckUnlockedStreamLock(true);
    CheckUnlockedStreamLock(false, feof_unlocked_nid_postfix);
    CheckUnlockedStreamLock(false, ferror_unlocked_nid_postfix);
    CheckTryLock();
    CheckUnlockedQueries();
    if (!CheckBinaryModes()) return 1;
#ifdef _WIN32
    CheckStandardStreamDeleteSharing();
    const std::string standardFilename = "anyps5-standard-reopen-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Require(freopen_nid_postfix(standardFilename.c_str(), "w", __stdoutp_nid_postfix) == __stdoutp_nid_postfix);
    const HANDLE hostOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
    Require(hostOutput != nullptr && hostOutput != INVALID_HANDLE_VALUE);
    Require(hostOutput != reinterpret_cast<HANDLE>(::_get_osfhandle(::_fileno(stdout))));
    DWORD handleFlags = 0;
    Require(::GetHandleInformation(hostOutput, &handleFlags) != 0);
    constexpr char hostMessage[] = "host stdout preserved\n";
    DWORD hostWritten = 0;
    Require(::WriteFile(hostOutput, hostMessage, sizeof(hostMessage) - 1, &hostWritten, nullptr) != 0 && hostWritten == sizeof(hostMessage) - 1);
    Require(printf_nid_postfix("standard output") == 15 && fflush_nid_postfix(__stdoutp_nid_postfix) == 0);
    { std::ifstream input(standardFilename, std::ios::binary); std::string contents; std::getline(input, contents);
      Require(contents == "standard output"); }
    const std::string movedFilename = standardFilename + "-moved";
    std::error_code renameError;
    std::filesystem::rename(standardFilename, movedFilename, renameError);
    Require(!renameError && !std::filesystem::exists(standardFilename) && std::filesystem::exists(movedFilename));
    Require(printf_nid_postfix(" continued") == 10 && fflush_nid_postfix(__stdoutp_nid_postfix) == 0);
    { std::ifstream input(movedFilename, std::ios::binary); std::string contents; std::getline(input, contents);
      Require(contents == "standard output continued"); }
    Require(std::filesystem::remove(movedFilename));
    Require(!std::filesystem::exists(movedFilename));
    Require(printf_nid_postfix(" after delete") == 13 && fflush_nid_postfix(__stdoutp_nid_postfix) == 0);
    const std::string exclusiveFilename = "anyps5-standard-exclusive-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    { std::ofstream out(exclusiveFilename, std::ios::binary); out << "existing"; }
    Require(freopen_nid_postfix(exclusiveFilename.c_str(), "wx", __stdoutp_nid_postfix) == nullptr && *__error_nid_postfix() == 17);
    Require(std::filesystem::remove(exclusiveFilename));
    Require(printf_nid_postfix("closed") == -1 && *__error_nid_postfix() == 9);
    Require(std::fputs("host stdout intact\n", stdout) >= 0 && std::fflush(stdout) == 0);
#endif
    return 0;
}
