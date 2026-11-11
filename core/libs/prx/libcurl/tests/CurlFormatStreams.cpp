#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <source_location>
#include <string>

#if defined(__clang__) && !defined(_WIN32)
#define __builtin_sysv_va_list va_list
#define __builtin_sysv_va_start va_start
#define __builtin_sysv_va_end va_end
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "failure at %u\n", location.line());
        std::exit(1);
    }
}

static int APS5_VABI FormatStream(FileStream* stream, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = curl_mvfprintf_nid_postfix(reinterpret_cast<std::FILE*>(stream), format, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
    return result;
}

static std::string ReadAll(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

static void TestFileStreams() {
    const auto path = (std::filesystem::temp_directory_path() / "anyps5_curl_format_streams.txt").string();
    auto* stream = fopen_nid_postfix(path.c_str(), "w");
    Require(stream != nullptr);
    Require(curl_mfprintf_nid_postfix(reinterpret_cast<std::FILE*>(stream), "%s %d|%-4s|%2$d;", "one", 1, "x") == 13);
    Require(FormatStream(stream, "%s %#x %p", "two", 255, static_cast<void*>(nullptr)) == 14);
    std::string tooMany;
    for (int index = 0; index < 129; ++index) tooMany += "%d";
    Require(curl_mfprintf_nid_postfix(reinterpret_cast<std::FILE*>(stream), tooMany.c_str()) == -1);
    Require(fclose_nid_postfix(stream) == 0);
    Require(ReadAll(path) == "one 1|x   |1;two 0xff (nil)");

    stream = fopen_nid_postfix(path.c_str(), "r");
    Require(stream != nullptr);
    Require(curl_mfprintf_nid_postfix(reinterpret_cast<std::FILE*>(stream), "abc") == 0);
    Require(fclose_nid_postfix(stream) == 0);
    std::filesystem::remove(path);
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestFileStreams();
    return 0;
}
