#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, int line = __builtin_LINE()) {
    if (condition) return;
    std::fprintf(stderr, "error printing check failed at line %d\n", line);
    std::abort();
}

char callbackLine[512];

int APS5_VABI Capture(const char* text, std::size_t length, void*) {
    Require(length < sizeof(callbackLine));
    std::memcpy(callbackLine, text, length);
    callbackLine[length] = '\0';
    return 1;
}

}

int main() {
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
    LibreSslPutError_nid_no_patch(20, 0xfff, 67);
    ERR_print_errors_cb_nid_postfix(Capture, nullptr);
    Require(std::strcmp(line, callbackLine) == 0);
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
    try { ERR_print_errors_fp_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);

    return 0;
}
