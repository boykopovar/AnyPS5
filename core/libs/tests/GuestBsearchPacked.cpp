#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

static int APS5_VABI Compare(const void* key, const void* record) {
    const auto left = *static_cast<const unsigned char*>(key);
    const auto right = *static_cast<const unsigned char*>(record);
    return (left > right) - (left < right);
}

int main() {
    const std::array<unsigned char, 9> keys{1, 3, 3, 7, 9, 11, 13, 17, 21};
    for (std::size_t stride : {1, 3, 5, 7, 17}) {
        for (std::size_t offset : {0, 1, 2, 5}) {
            std::vector<unsigned char> records(offset + keys.size() * stride + 7, 0xa5);
            auto* base = records.data() + offset;
            for (std::size_t i = 0; i < keys.size(); ++i) base[i * stride] = keys[i];
            const auto before = records;
            for (unsigned char key = 0; key < 24; ++key) {
                auto* found = static_cast<unsigned char*>(bsearch_nid_postfix(&key, base, keys.size(), stride, Compare));
                const bool present = std::find(keys.begin(), keys.end(), key) != keys.end();
                Require((found != nullptr) == present);
                if (found) Require(found >= base && found < base + keys.size() * stride &&
                                   (found - base) % stride == 0 && *found == key);
                Require(records == before);
            }
        }
    }
}
