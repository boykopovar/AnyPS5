#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" const char* APS5_VABI OpenSSL_version_nid_postfix(int type);

#ifdef LIBRESSL3_STANDALONE_TEST
extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* function) {
    throw std::runtime_error(function);
}
#endif

int main() {
    constexpr std::array<const char*, 6> expected {
        "LibreSSL 4.0.0",
        "compiler: information not available",
        "built on: date not available",
        "platform: information not available",
        "OPENSSLDIR: \"/etc/ssl\"",
        "ENGINESDIR: N/A",
    };

    for (int type = 0; type < static_cast<int>(expected.size()); ++type) {
        const char* first = OpenSSL_version_nid_postfix(type);
        const char* second = OpenSSL_version_nid_postfix(type);
        if (!first || std::strcmp(first, expected[type]) != 0 || first != second) return EXIT_FAILURE;
    }

    constexpr std::array<int, 4> unsupported {-1, 6, 42, 0x7fffffff};
    for (int type : unsupported) {
        const char* value = OpenSSL_version_nid_postfix(type);
        if (!value || std::strcmp(value, "not available") != 0) return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
