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
#include <stdexcept>


namespace {
void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::terminate();
}
}

int main() {
    alignas(8) std::array<std::byte, 64> pkey{};
    constexpr int ids[] = {0, 1, 6, 408, 0x7fffffff, std::numeric_limits<int>::min()};
    for (const int expected : ids) {
        std::memcpy(pkey.data(), &expected, sizeof(expected));
        Require(EVP_PKEY_id_nid_postfix(pkey.data()) == expected, "returns the key's stored type unchanged");
    }
    errno = 71;
    bool rejected = false;
    try { EVP_PKEY_id_nid_postfix(reinterpret_cast<const void*>(0x10)); }
    catch (const std::runtime_error&) { rejected = errno == 71; }
    Require(rejected, "an unreadable key is rejected with errno preserved");
    return 0;
}
