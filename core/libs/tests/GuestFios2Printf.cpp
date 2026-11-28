#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "tests/TemporaryFile.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(__clang__) && !defined(_WIN32)
#define __builtin_sysv_va_list va_list
#define __builtin_sysv_va_start va_start
#define __builtin_sysv_va_end va_end
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Fios2 printf: %s\n", message);
    std::abort();
}

class StandardOutputCapture {
public:
    StandardOutputCapture() : file_(TemporaryFile::Open()) {
        Require(file_ != nullptr, "the capture file could not be opened");
        std::fflush(stdout);
#ifdef _WIN32
        saved_ = ::_dup(::_fileno(stdout));
        Require(saved_ >= 0 && ::_dup2(::_fileno(file_), ::_fileno(stdout)) == 0, "stdout could not be redirected");
#else
        saved_ = ::dup(::fileno(stdout));
        Require(saved_ >= 0 && ::dup2(::fileno(file_), ::fileno(stdout)) == ::fileno(stdout), "stdout could not be redirected");
#endif
    }

    ~StandardOutputCapture() {
        Restore();
        if (file_ != nullptr) std::fclose(file_);
    }

    std::string Text() {
        Restore();
        std::rewind(file_);
        std::string text;
        char buffer[256];
        while (const auto count = std::fread(buffer, 1, sizeof(buffer), file_)) text.append(buffer, count);
        return text;
    }

private:
    void Restore() {
        if (saved_ < 0) return;
        std::fflush(stdout);
#ifdef _WIN32
        ::_dup2(saved_, ::_fileno(stdout));
        ::_close(saved_);
#else
        ::dup2(saved_, ::fileno(stdout));
        ::close(saved_);
#endif
        saved_ = -1;
    }

    std::FILE* file_;
    int saved_ = -1;
};

int APS5_VABI PrintList(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = sceFiosVprintf(format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

void Expect(const std::string& output, const std::string& expected, const char* message) {
    if (output == expected) return;
    std::fprintf(stderr, "output was:\n[%s]\nexpected:\n[%s]\n", output.c_str(), expected.c_str());
    Require(false, message);
}

}

int main() {
    try {
        int written = -1;
        std::string output;
        {
            StandardOutputCapture capture;
            written = sceFiosPrintf("%d %s %lx %c %5.2f|\n", -7, "fios", 0x123456789abcdefLL, 'z', 2.5);
            output = capture.Text();
        }
        Expect(output, "-7 fios 123456789abcdef z  2.50|\n", "sceFiosPrintf formats its variadic arguments");
        Require(written == static_cast<int>(output.size()), "sceFiosPrintf returns the number of characters written");

        {
            StandardOutputCapture capture;
            written = PrintList("%s=%u %%\n", "count", 42u);
            output = capture.Text();
        }
        Expect(output, "count=42 %\n", "sceFiosVprintf formats a va_list");
        Require(written == static_cast<int>(output.size()), "sceFiosVprintf returns the number of characters written");

        {
            StandardOutputCapture capture;
            Require(sceFiosPrintf(nullptr) == -1, "a null format is refused");
            Require(sceFiosVprintf(nullptr, nullptr) == -1, "a null format and list are refused");
            Require(sceFiosVprintf("text\n", nullptr) == -1, "a null list is refused");
            Require(PrintList(nullptr) == -1, "a null format with a list is refused");
            Require(sceFiosPrintf(reinterpret_cast<const char*>(0x10)) == -1, "an unreadable format is refused");
            output = capture.Text();
        }
        Expect(output, "", "a refused format writes nothing");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Fios2 printf: %s\n", error.what());
        return 1;
    }
    std::puts("Fios2 printf tests passed");
    return 0;
}
