#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "SceTypes.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

#ifdef _WIN32
#define TEST_ARGS_START(last) __builtin_sysv_va_list args; __builtin_sysv_va_start(args, last)
#define TEST_ARGS_END() __builtin_sysv_va_end(args)
#else
#define TEST_ARGS_START(last) std::va_list args; va_start(args, last)
#define TEST_ARGS_END() va_end(args)
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "curl printf check failed at line %u\n", location.line());
    std::abort();
}

static char* MvAprintf(const char* format, ...) {
    TEST_ARGS_START(format);
    char* text = curl_mvaprintf_nid_postfix(format, reinterpret_cast<VaList*>(&args));
    TEST_ARGS_END();
    return text;
}

static int APS5_VABI MvPrintf(const char* format, ...) {
    TEST_ARGS_START(format);
    const int result = curl_mvprintf_nid_postfix(format, reinterpret_cast<VaList*>(&args));
    TEST_ARGS_END();
    return result;
}

static int APS5_VABI MvSprintf(char* buffer, const char* format, ...) {
    TEST_ARGS_START(format);
    const int result = curl_mvsprintf_nid_postfix(buffer, format, reinterpret_cast<VaList*>(&args));
    TEST_ARGS_END();
    return result;
}

static int APS5_VABI MvSnprintf(char* buffer, std::size_t maximum, const char* format, ...) {
    TEST_ARGS_START(format);
    const int result = curl_mvsnprintf_nid_postfix(buffer, maximum, format, reinterpret_cast<VaList*>(&args));
    TEST_ARGS_END();
    return result;
}

static int APS5_VABI MvFprintf(FileStream* stream, const char* format, ...) {
    TEST_ARGS_START(format);
    const int result = curl_mvfprintf_nid_postfix(reinterpret_cast<std::FILE*>(stream), format, reinterpret_cast<VaList*>(&args));
    TEST_ARGS_END();
    return result;
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());

    char buffer[128] {};
    Require(curl_msprintf_nid_postfix(buffer, "%s %d %ld", "abc", 7, 1234567890123L) == 19);
    Require(std::strcmp(buffer, "abc 7 1234567890123") == 0);
    Require(MvSprintf(buffer, "%s-%d", "xyz", 42) == 6);
    Require(std::strcmp(buffer, "xyz-42") == 0);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(curl_msnprintf_nid_postfix(buffer, 5, "%s", "abcdef") == 4);
    Require(std::strcmp(buffer, "abcd") == 0);
    std::memset(buffer, 'x', sizeof(buffer));
    Require(MvSnprintf(buffer, 8, "%s", "abcdefghij") == 7);
    Require(std::strcmp(buffer, "abcdefg") == 0);

    char untouched[4] = {'x', 'x', 'x', 'x'};
    Require(curl_msnprintf_nid_postfix(untouched, 0, "%s", "abcdef") == 0);
    Require(untouched[0] == 'x' && untouched[1] == 'x' && untouched[2] == 'x' && untouched[3] == 'x');
    buffer[0] = 'x';
    Require(curl_msnprintf_nid_postfix(buffer, 1, "%s", "abcdef") == 0);
    Require(buffer[0] == '\0');

    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%.2f", 1.5) == 4);
    Require(std::strcmp(buffer, "1.50") == 0);
#ifndef _WIN32
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%s", static_cast<const char*>(nullptr)) == 5);
    Require(std::strcmp(buffer, "(nil)") == 0);
#endif

    char* text = curl_maprintf_nid_postfix("%s/%d", "x", 9);
    Require(text != nullptr && std::strcmp(text, "x/9") == 0);
    curl_free(text);
    text = MvAprintf("%s-%s", "left", "right");
    Require(text != nullptr && std::strcmp(text, "left-right") == 0);
    curl_free(text);

    Require(curl_mprintf_nid_postfix("%s", "") == 0);
    Require(MvPrintf("%s", "") == 0);

    const char* path = "curl_printf_test.txt";
    FileStream* stream = fopen_nid_postfix(path, "w+");
    Require(stream != nullptr);
    Require(curl_mfprintf_nid_postfix(reinterpret_cast<std::FILE*>(stream), "%s %d", "abc", 7) == 5);
    Require(MvFprintf(stream, " %s", "def") == 4);
    Require(fflush_nid_postfix(stream) == 0);
    rewind_nid_postfix(stream);
    char line[64] {};
    Require(fgets_nid_postfix(line, static_cast<int>(sizeof(line)), stream) != nullptr);
    Require(std::strcmp(line, "abc 7 def") == 0);
    Require(fclose_nid_postfix(stream) == 0);
    Require(remove_nid_postfix(path) == 0);

    return 0;
}
