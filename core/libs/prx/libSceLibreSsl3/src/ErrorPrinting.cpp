#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdio>
#include <stdexcept>

namespace {

int APS5_VABI WriteToStream(const char* text, std::size_t length, void* userData) {
    auto* stream = static_cast<FileStream*>(userData);
    return static_cast<int>(fwrite_nid_postfix(text, 1, length, stream));
}

}

extern "C" {

void APS5_VABI ERR_print_errors_fp_nid_postfix(std::FILE* file) {
    if (!file) throw std::invalid_argument("ERR_print_errors_fp requires a stream");
    ERR_print_errors_cb_nid_postfix(WriteToStream, reinterpret_cast<FileStream*>(file));
}

}
