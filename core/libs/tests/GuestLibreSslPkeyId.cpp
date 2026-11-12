#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>

namespace {
void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::terminate();
}
}

int main() {
    alignas(8) std::array<std::byte, 64> pkey{};
    constexpr int ids[] = {-1, -6, 0, 1, 6, 408, 0x7fffffff, std::numeric_limits<int>::min()};
    for (const int expected : ids) {
        pkey.fill(std::byte{0xa5});
        std::memcpy(pkey.data(), &expected, sizeof(expected));
        const auto snapshot = pkey;
        errno = EIO;
        Require(EVP_PKEY_id_nid_postfix(pkey.data()) == expected, "returns the key's stored type unchanged");
        Require(errno == EIO, "leaves errno unchanged");
        Require(pkey == snapshot, "leaves the key buffer untouched");
    }
    return 0;
}
