#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>

extern "C" int APS5_VABI EVP_PKEY_id_nid_postfix(const void* pkey);

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
    try {
        static_cast<void>(EVP_PKEY_id_nid_postfix(nullptr));
    } catch (const std::exception&) {
        return 0;
    }
    Require(false, "null key is rejected");
    return 1;
}
