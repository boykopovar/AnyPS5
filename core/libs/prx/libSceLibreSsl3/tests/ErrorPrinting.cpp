#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, int line = __builtin_LINE()) {
    if (condition) return;
    std::fprintf(stderr, "error printing check failed at line %d\n", line);
    std::abort();
}

int collected;

int APS5_VABI Collect(const char* text, std::size_t length, void* data) {
    static_cast<std::string*>(data)->append(text, length);
    ++collected;
    return 1;
}

int APS5_VABI Stop(const char*, std::size_t, void*) {
    ++collected;
    return 0;
}

std::size_t Lines(const std::string& text) {
    std::size_t lines = 0;
    for (const char character : text)
        if (character == '\n') ++lines;
    return lines;
}

}

int main() {
    ERR_clear_error_nid_postfix();
    collected = 0;
    std::string empty;
    ERR_print_errors_cb_nid_postfix(Collect, &empty);
    Require(collected == 0 && empty.empty());

    LibreSslPutError_nid_no_patch(20, 0xfff, 67);
    LibreSslPutError_nid_no_patch(13, 0xfff, 0xa8);
    collected = 0;
    std::string text;
    ERR_print_errors_cb_nid_postfix(Collect, &text);
    Require(collected == 2 && Lines(text) == 2);
    Require(text.find("error:") != std::string::npos);
    Require(ERR_peek_error_nid_postfix() == 0);

    LibreSslPutError_nid_no_patch(20, 0xfff, 67);
    LibreSslPutError_nid_no_patch(13, 0xfff, 0xa8);
    collected = 0;
    ERR_print_errors_cb_nid_postfix(Stop, nullptr);
    Require(collected == 1 && ERR_peek_error_nid_postfix() != 0);
    ERR_clear_error_nid_postfix();

    const char* path = "err_print_test.txt";
    FileStream* stream = fopen_nid_postfix(path, "w+");
    Require(stream != nullptr);
    LibreSslPutError_nid_no_patch(20, 0xfff, 67);
    ERR_print_errors_fp_nid_postfix(reinterpret_cast<std::FILE*>(stream));
    Require(fflush_nid_postfix(stream) == 0);
    rewind_nid_postfix(stream);
    char line[512] {};
    Require(fgets_nid_postfix(line, static_cast<int>(sizeof(line)), stream) != nullptr);
    Require(std::strstr(line, "error:") != nullptr);
    Require(ERR_peek_error_nid_postfix() == 0);
    Require(fclose_nid_postfix(stream) == 0);

    stream = fopen_nid_postfix(path, "w+");
    Require(stream != nullptr);
    ERR_print_errors_fp_nid_postfix(reinterpret_cast<std::FILE*>(stream));
    Require(fflush_nid_postfix(stream) == 0);
    rewind_nid_postfix(stream);
    Require(fgets_nid_postfix(line, static_cast<int>(sizeof(line)), stream) == nullptr);
    Require(fclose_nid_postfix(stream) == 0);
    Require(remove_nid_postfix(path) == 0);

    bool threw = false;
    try { ERR_print_errors_cb_nid_postfix(nullptr, nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    try { ERR_print_errors_fp_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);

    return 0;
}
