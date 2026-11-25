#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdio>
#include <vector>

namespace {

int APS5_VABI CompareKeys(const void* left, const void* right) {
    const auto first = *static_cast<const unsigned char*>(left);
    const auto second = *static_cast<const unsigned char*>(right);
    return (first > second) - (first < second);
}

unsigned char RecordByte(std::size_t key, std::size_t position) {
    return position == 0 ? static_cast<unsigned char>(key)
                         : static_cast<unsigned char>((key * 29 + position * 17 + (position >> 8) * 53)
                                                      ^ ((key + 1) * (position % 13 + 1)));
}

bool ExpectCompleteRecords(std::size_t width, std::size_t count, std::size_t offset, int pattern) {
    constexpr unsigned char guard = 0xa5;
    const auto end = offset + count * width;
    std::vector<unsigned char> bytes(end + 11, guard);
    auto* base = bytes.data() + offset;
    for (std::size_t index = 0; index < count; ++index) {
        const auto key = pattern == 0 ? count - index - 1 : (index + count / 2) % count;
        for (std::size_t position = 0; position < width; ++position) {
            base[index * width + position] = RecordByte(key, position);
        }
    }

    qsort_nid_postfix(base, count, width, CompareKeys);

    for (std::size_t index = 0; index < count; ++index) {
        for (std::size_t position = 0; position < width; ++position) {
            const auto actual = base[index * width + position];
            const auto expected = RecordByte(index, position);
            if (actual != expected) {
                std::fprintf(stderr, "payload mismatch: width=%zu count=%zu offset=%zu pattern=%d record=%zu byte=%zu actual=%u expected=%u\n",
                             width, count, offset, pattern, index, position,
                             static_cast<unsigned>(actual), static_cast<unsigned>(expected));
                return false;
            }
        }
    }
    for (std::size_t position = 0; position < offset; ++position) {
        if (bytes[position] != guard) {
            std::fprintf(stderr, "prefix guard mismatch: width=%zu count=%zu offset=%zu pattern=%d byte=%zu actual=%u expected=%u\n",
                         width, count, offset, pattern, position,
                         static_cast<unsigned>(bytes[position]), static_cast<unsigned>(guard));
            return false;
        }
    }
    for (std::size_t position = end; position < bytes.size(); ++position) {
        if (bytes[position] != guard) {
            std::fprintf(stderr, "suffix guard mismatch: width=%zu count=%zu offset=%zu pattern=%d byte=%zu actual=%u expected=%u\n",
                         width, count, offset, pattern, position,
                         static_cast<unsigned>(bytes[position]), static_cast<unsigned>(guard));
            return false;
        }
    }
    return true;
}

}

int main() {
    std::size_t cases = 0;
    for (std::size_t width : {24, 255, 256, 257, 511, 512, 513}) {
        for (std::size_t count : {2, 7, 32, 33, 41, 65}) {
            for (std::size_t offset : {1, 5}) {
                for (int pattern : {0, 1}) {
                    if (!ExpectCompleteRecords(width, count, offset, pattern)) return 1;
                    ++cases;
                }
            }
        }
    }
    std::printf("PASS: %zu wide record cases; full payloads and guards match\n", cases);
    return 0;
}
