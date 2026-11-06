#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "tests/TemporaryFile.hpp"
#include <cstdarg>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest libc internal format check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static int APS5_VABI WriteFormatted(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = __vfprintf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int main() {
    const auto filePath = "guest_libc_internal_format_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    auto* stream = fopen_nid_postfix(filePath.c_str(), "w+b");
    Require(stream != nullptr);

    const int written = WriteFormatted(stream, "value: %d, string: %s", 12345, "testing");
    Require(written == 29);
    Require(fclose_nid_postfix(stream) == 0);

    std::ifstream in(filePath, std::ios::binary);
    Require(in.is_open());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    Require(contents == "value: 12345, string: testing");
    std::filesystem::remove(filePath);

    FileStream closedStream(TemporaryFile::Open());
    closedStream.Close();
    *__error_nid_postfix() = 0;
    Require(WriteFormatted(&closedStream, "test") == -1);
    Require(*__error_nid_postfix() == 9);

    *__error_nid_postfix() = 0;
    Require(WriteFormatted(nullptr, "test") == -1);
    Require(*__error_nid_postfix() == 9);

    return 0;
}
