#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {

void Format(const std::uint64_t error, char* line, const std::size_t size) {
    char text[256];
    ERR_error_string_n_nid_postfix(error, text, sizeof(text));
    std::snprintf(line, size, "0:%s::0:\n", text);
}

}

extern "C" {

void APS5_VABI ERR_print_errors_cb_nid_postfix(int (APS5_VABI *callback)(const char*, std::size_t, void*), void* userData) {
    if (!callback) throw std::invalid_argument("ERR_print_errors_cb requires a callback");
    char line[512];
    for (;;) {
        const std::uint64_t error = ERR_get_error_nid_postfix();
        if (!error) return;
        Format(error, line, sizeof(line));
        if (callback(line, std::strlen(line), userData) <= 0) return;
    }
}

void APS5_VABI ERR_print_errors_fp_nid_postfix(std::FILE* file) {
    if (!file) throw std::invalid_argument("ERR_print_errors_fp requires a stream");
    auto* stream = reinterpret_cast<FileStream*>(file);
    char line[512];
    for (;;) {
        const std::uint64_t error = ERR_get_error_nid_postfix();
        if (!error) return;
        Format(error, line, sizeof(line));
        if (fputs_nid_postfix(line, stream) < 0) return;
    }
}

}
