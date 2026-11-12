#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

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

    unsigned char sample = 7;
    Require(bsearch_nid_postfix(nullptr, nullptr, 0, 0, nullptr) == nullptr);
    Require(bsearch_nid_postfix(&sample, nullptr, 0, sizeof(sample), Compare) == nullptr);
    Require(bsearch_nid_postfix(&sample, keys.data(), 0, sizeof(unsigned char), Compare) == nullptr);

    auto expectInvalidArgument = [](const void* key, const void* base, std::size_t count, std::size_t size,
                                    int (APS5_VABI* compare)(const void*, const void*)) {
        try {
            bsearch_nid_postfix(key, base, count, size, compare);
            Require(false);
        } catch (const std::invalid_argument&) {
        }
    };
    expectInvalidArgument(nullptr, keys.data(), keys.size(), sizeof(unsigned char), Compare);
    expectInvalidArgument(&sample, nullptr, keys.size(), sizeof(unsigned char), Compare);
    expectInvalidArgument(&sample, keys.data(), keys.size(), 0, Compare);
    expectInvalidArgument(&sample, keys.data(), keys.size(), sizeof(unsigned char), nullptr);

    bool overflowed = false;
    try {
        bsearch_nid_postfix(&sample, keys.data(), (std::numeric_limits<std::size_t>::max() / 4) + 1, 4, Compare);
    } catch (const std::overflow_error&) {
        overflowed = true;
    }
    Require(overflowed);
}
