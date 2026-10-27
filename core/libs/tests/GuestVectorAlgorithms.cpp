#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>


static void Require(bool value) { if (!value) std::abort(); }

static void CheckMinValue() {
    const std::uint64_t edges[] = {0, 1, 0xffffffff, 0x100000000, 0x7fffffffffffffff, 0x8000000000000000, 0xfffffffffffffffe, 0xffffffffffffffff};
    for (const auto& left : edges) {
        Require(__std_min_8u_nid_postfix(&left, &left + 1) == left);
        for (const auto& right : edges) {
            const std::uint64_t pair[] = {left, right};
            Require(__std_min_8u_nid_postfix(pair, pair + 2) == std::min(left, right));
        }
    }
    std::uint64_t state = 0x521cadf08e47396b;
    for (const std::size_t count : {1, 2, 3, 4, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 257}) {
        std::vector<std::uint64_t> values(count);
        for (auto& value : values) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            value = state;
        }
        const auto expected = *std::min_element(values.begin(), values.end());
        for (std::size_t offset = 0; offset < 32; ++offset) {
            std::vector<unsigned char> storage(count * sizeof(std::uint64_t) + 64, 0xff);
            std::memcpy(storage.data() + offset, values.data(), count * sizeof(std::uint64_t));
            const auto original = storage;
            Require(__std_min_8u_nid_postfix(storage.data() + offset, storage.data() + offset + count * sizeof(std::uint64_t)) == expected);
            Require(storage == original);
        }
        std::fill(values.begin(), values.end(), UINT64_MAX);
        Require(__std_min_8u_nid_postfix(values.data(), values.data() + count) == UINT64_MAX);
        for (std::size_t position = 0; position < count; ++position) {
            values[position] = 0x100000005;
            Require(__std_min_8u_nid_postfix(values.data(), values.data() + count) == 0x100000005);
            values[position] = UINT64_MAX;
        }
    }
    const std::uint64_t repeated[] = {9, 9, 9, 9};
    Require(__std_min_8u_nid_postfix(repeated, repeated + 4) == 9);
}

static void CheckSizedHelpers() {
    const std::uint8_t bytes[] = {3, 0xff, 3, 9, 0xff};
    Require(__std_find_trivial_1_nid_postfix(bytes, bytes + 5, 0xff) == bytes + 1);
    Require(__std_find_trivial_1_nid_postfix(bytes, bytes + 5, 4) == bytes + 5);
    Require(__std_find_trivial_1_nid_postfix(bytes, bytes, 3) == bytes);
    Require(__std_count_trivial_1_nid_postfix(bytes, bytes + 5, 3) == 2 && __std_count_trivial_1_nid_postfix(bytes, bytes + 5, 7) == 0);
    Require(__std_max_element_1_nid_postfix(bytes, bytes + 5, false) == bytes + 1);
    Require(__std_max_element_1_nid_postfix(bytes, bytes + 5, true) == bytes + 3);

    const std::uint16_t halves[] = {0x8000, 5, 0x7fff, 5, 0x8000};
    Require(__std_count_trivial_2_nid_postfix(halves, halves + 5, 5) == 2 && __std_count_trivial_2_nid_postfix(halves, halves, 5) == 0);
    Require(__std_min_element_2_nid_postfix(halves, halves + 5, false) == halves + 1);
    Require(__std_min_element_2_nid_postfix(halves, halves + 5, true) == halves);
    Require(__std_max_element_2_nid_postfix(halves, halves + 5, false) == halves);
    Require(__std_max_element_2_nid_postfix(halves, halves + 5, true) == halves + 2);

    const std::uint32_t words[] = {7, 0xfffffff0u, 1, 1, 0xfffffff0u};
    Require(__std_find_trivial_4_nid_postfix(words, words + 5, 1) == words + 2);
    Require(__std_find_trivial_4_nid_postfix(words, words + 5, 2) == words + 5);
    Require(__std_count_trivial_4_nid_postfix(words, words + 5, 0xfffffff0u) == 2);
    Require(__std_min_element_4_nid_postfix(words, words + 5, false) == words + 2);
    Require(__std_min_element_4_nid_postfix(words, words + 5, true) == words + 1);
    Require(__std_min_element_4_nid_postfix(words, words, true) == words);

    const std::uint64_t wides[] = {0x8000000000000000ull, 2, 0x7fffffffffffffffull, 0x8000000000000000ull};
    Require(__std_find_trivial_8_nid_postfix(wides, wides + 4, 0x8000000000000000ull) == wides);
    Require(__std_find_trivial_8_nid_postfix(wides + 1, wides + 4, 0x8000000000000000ull) == wides + 3);
    Require(__std_min_element_8_nid_postfix(wides, wides + 4, true) == wides);
    Require(__std_min_element_8_nid_postfix(wides, wides + 4, false) == wides + 1);
    Require(__std_max_element_8_nid_postfix(wides, wides + 4, false) == wides);
    Require(__std_max_element_8_nid_postfix(wides, wides + 4, true) == wides + 2);
    Require(__std_max_element_8_nid_postfix(wides, wides, false) == wides);
}

static void CheckSignedMin64Value() {
    const std::int64_t edges[] = {INT64_MIN, INT64_MIN + 1, -0x100000001ll, -0x100000000ll, -1, 0, 1, 0x7fffffff, 0xffffffff, 0x100000000, INT64_MAX - 1, INT64_MAX};
    for (const auto& left : edges) {
        Require(__std_min_8i_nid_postfix(&left, &left + 1) == left);
        for (const auto& right : edges) {
            const std::int64_t pair[] = {left, right};
            Require(__std_min_8i_nid_postfix(pair, pair + 2) == std::min(left, right));
        }
    }
    std::uint64_t state = 0xf3c82d51479ab6e1;
    for (const std::size_t count : {1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 257}) {
        std::vector<std::int64_t> values(count);
        for (auto& value : values) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            std::memcpy(&value, &state, sizeof(value));
        }
        const auto expected = *std::min_element(values.begin(), values.end());
        for (std::size_t offset = 0; offset < 32; ++offset) {
            std::vector<unsigned char> storage(count * sizeof(std::int64_t) + 64, 0xff);
            std::memcpy(storage.data() + offset, values.data(), count * sizeof(std::int64_t));
            const auto original = storage;
            Require(__std_min_8i_nid_postfix(storage.data() + offset, storage.data() + offset + count * sizeof(std::int64_t)) == expected);
            Require(storage == original);
        }
        std::fill(values.begin(), values.end(), INT64_MAX);
        Require(__std_min_8i_nid_postfix(values.data(), values.data() + count) == INT64_MAX);
        for (std::size_t position = 0; position < count; ++position) {
            values[position] = -0x100000005ll;
            Require(__std_min_8i_nid_postfix(values.data(), values.data() + count) == -0x100000005ll);
            values[position] = INT64_MAX;
        }
    }
    const std::int64_t repeated[] = {-9, -9, -9, -9};
    Require(__std_min_8i_nid_postfix(repeated, repeated + 4) == -9);
}

static void CheckFindAndCount() {
    const std::uint16_t halves[] = {5, 7, 9, 7};
    Require(__std_find_trivial_2_nid_postfix(halves, halves + 4, 7) == halves + 1);
    Require(__std_find_trivial_2_nid_postfix(halves, halves + 4, 8) == halves + 4);
    Require(__std_find_trivial_2_nid_postfix(halves, halves, 5) == halves);
    const std::uint64_t wides[] = {1, 2, 1, 1};
    Require(__std_count_trivial_8_nid_postfix(wides, wides + 4, 1) == 3 && __std_count_trivial_8_nid_postfix(wides, wides, 1) == 0);
    alignas(32) std::uint32_t words[16] = {};
    words[3] = 42;
    words[12] = 42;
    Require(__std_find_trivial_unsized_4_nid_postfix(words + 8, 42) == words + 12);
}

static void CheckSignedMax64Value() {
    const std::int64_t edges[] = {INT64_MIN, INT64_MIN + 1, -0x100000001ll, -0x100000000ll, -1, 0, 1, 0x7fffffff, 0xffffffff, 0x100000000, INT64_MAX - 1, INT64_MAX};
    for (const auto& left : edges) {
        Require(__std_max_8i_nid_postfix(&left, &left + 1) == left);
        for (const auto& right : edges) {
            const std::int64_t pair[] = {left, right};
            Require(__std_max_8i_nid_postfix(pair, pair + 2) == std::max(left, right));
        }
    }
    std::uint64_t state = 0x2e791fa84d6c530b;
    for (const std::size_t count : {1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 257}) {
        std::vector<std::int64_t> values(count);
        for (auto& value : values) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            std::memcpy(&value, &state, sizeof(value));
        }
        const auto expected = *std::max_element(values.begin(), values.end());
        for (std::size_t offset = 0; offset < 32; ++offset) {
            std::vector<unsigned char> storage(count * sizeof(std::int64_t) + 64, 0xff);
            std::memcpy(storage.data() + offset, values.data(), count * sizeof(std::int64_t));
            const auto original = storage;
            Require(__std_max_8i_nid_postfix(storage.data() + offset, storage.data() + offset + count * sizeof(std::int64_t)) == expected);
            Require(storage == original);
        }
        std::fill(values.begin(), values.end(), INT64_MIN);
        Require(__std_max_8i_nid_postfix(values.data(), values.data() + count) == INT64_MIN);
        for (std::size_t position = 0; position < count; ++position) {
            values[position] = -0x100000005ll;
            Require(__std_max_8i_nid_postfix(values.data(), values.data() + count) == -0x100000005ll);
            values[position] = INT64_MIN;
        }
    }
    const std::int64_t repeated[] = {-9, -9, -9, -9};
    Require(__std_max_8i_nid_postfix(repeated, repeated + 4) == -9);
}

static void CheckUnsizedRescan() {
    alignas(32) unsigned char bytes[96] = {};
    bytes[3] = 'x';
    bytes[70] = 'x';
    Require(__std_find_trivial_unsized_1_nid_postfix(bytes + 8, 'x') == bytes + 3);
    bytes[20] = 'x';
    Require(__std_find_trivial_unsized_1_nid_postfix(bytes + 8, 'x') == bytes + 20);
    bytes[3] = 0;
    bytes[20] = 0;
    Require(__std_find_trivial_unsized_1_nid_postfix(bytes + 8, 'x') == bytes + 70);

    alignas(32) std::uint16_t halves[32] = {};
    halves[0] = 17;
    halves[4] = 17;
    halves[20] = 17;
    Require(__std_find_trivial_unsized_2_nid_postfix(halves + 1, 17) == halves + 4);
    halves[4] = 0;
    Require(__std_find_trivial_unsized_2_nid_postfix(halves + 1, 17) == halves);
    halves[0] = 0;
    Require(__std_find_trivial_unsized_2_nid_postfix(halves + 1, 17) == halves + 20);

    alignas(32) std::uint32_t quads[16] = {};
    quads[0] = 42;
    quads[5] = 42;
    quads[12] = 42;
    Require(__std_find_trivial_unsized_4_nid_postfix(quads + 1, 42) == quads + 5);
    quads[5] = 0;
    Require(__std_find_trivial_unsized_4_nid_postfix(quads + 1, 42) == quads);
    quads[0] = 0;
    Require(__std_find_trivial_unsized_4_nid_postfix(quads + 1, 42) == quads + 12);

    alignas(32) std::uint64_t wides[8] = {};
    wides[0] = 99;
    wides[2] = 99;
    wides[6] = 99;
    Require(__std_find_trivial_unsized_8_nid_postfix(wides + 1, 99) == wides + 2);
    wides[2] = 0;
    Require(__std_find_trivial_unsized_8_nid_postfix(wides + 1, 99) == wides);
    wides[0] = 0;
    Require(__std_find_trivial_unsized_8_nid_postfix(wides + 1, 99) == wides + 6);
}

static void CheckMinMax() {
    const unsigned char bytes[] = {3, 0x80, 1, 0x80, 1, 0xff};
    Require(__std_min_element_1_nid_postfix(bytes, bytes + 6, false) == bytes + 2);
    Require(__std_min_element_1_nid_postfix(bytes, bytes + 6, true) == bytes + 1);
    Require(__std_min_element_1_nid_postfix(bytes, bytes, true) == bytes);
    const std::uint32_t words[] = {7, 0xffffffffu, 9, 0xffffffffu, 9};
    Require(__std_max_element_4_nid_postfix(words, words + 5, false) == words + 1);
    Require(__std_max_element_4_nid_postfix(words, words + 5, true) == words + 2);
    const std::int16_t halves[] = {4, -3, 8, -3, 8, 2};
    const MinMaxPointers signedHalves = __std_minmax_element_2_nid_postfix(halves, halves + 6, true);
    Require(signedHalves.minimum == halves + 1 && signedHalves.maximum == halves + 4);
    const MinMaxPointers unsignedHalves = __std_minmax_element_2_nid_postfix(halves, halves + 6, false);
    Require(unsignedHalves.minimum == halves + 5 && unsignedHalves.maximum == halves + 3);
    const std::uint64_t wides[] = {5};
    const MinMaxPointers single = __std_minmax_element_8_nid_postfix(wides, wides + 1, false);
    Require(single.minimum == wides && single.maximum == wides);
    const MinMaxPointers empty = __std_minmax_element_8_nid_postfix(wides, wides, false);
    Require(empty.minimum == wides && empty.maximum == wides);
}

static void CheckUnsignedMin32Value() {
    const std::uint32_t edges[] = {0, 1, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff};
    for (const auto& left : edges) {
        Require(__std_min_4u_nid_postfix(&left, &left + 1) == left);
        for (const auto& right : edges) {
            const std::uint32_t pair[] = {left, right};
            Require(__std_min_4u_nid_postfix(pair, pair + 2) == std::min(left, right));
        }
    }
    std::uint32_t state = 0x8e47396b;
    for (const std::size_t count : {1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 257}) {
        std::vector<std::uint32_t> values(count);
        for (auto& value : values) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            value = state;
        }
        const auto expected = *std::min_element(values.begin(), values.end());
        for (std::size_t offset = 0; offset < 32; ++offset) {
            std::vector<unsigned char> storage(count * sizeof(std::uint32_t) + 64, 0xff);
            std::memcpy(storage.data() + offset, values.data(), count * sizeof(std::uint32_t));
            const auto original = storage;
            Require(__std_min_4u_nid_postfix(storage.data() + offset, storage.data() + offset + count * sizeof(std::uint32_t)) == expected);
            Require(storage == original);
        }
        std::fill(values.begin(), values.end(), UINT32_MAX);
        Require(__std_min_4u_nid_postfix(values.data(), values.data() + count) == UINT32_MAX);
        for (std::size_t position = 0; position < count; ++position) {
            values[position] = 0x10005;
            Require(__std_min_4u_nid_postfix(values.data(), values.data() + count) == 0x10005);
            values[position] = UINT32_MAX;
        }
    }
    const std::uint32_t repeated[] = {9, 9, 9, 9};
    Require(__std_min_4u_nid_postfix(repeated, repeated + 4) == 9);
}

static void CheckReverseAndSwap() {
    std::uint16_t halves[] = {1, 2, 3, 4, 5};
    __std_reverse_trivially_swappable_2_nid_postfix(halves, halves + 5);

    Require(halves[0] == 5 && halves[1] == 4 && halves[2] == 3 && halves[3] == 2 && halves[4] == 1);
    const std::uint64_t wides[] = {1, 2, 3};
    std::uint64_t reversed[4] = {9, 9, 9, 9};
    __std_reverse_copy_trivially_copyable_8_nid_postfix(wides, wides + 3, reversed);

    Require(reversed[0] == 3 && reversed[1] == 2 && reversed[2] == 1 && reversed[3] == 9);
    unsigned char left[37], right[37];
    for (int i = 0; i < 37; ++i) {
        left[i] = static_cast<unsigned char>(i);
        right[i] = static_cast<unsigned char>(100 + i);
    }
    __std_swap_ranges_trivially_swappable_noalias_nid_postfix(left, left + 37, right);

    for (int i = 0; i < 37; ++i) Require(left[i] == 100 + i && right[i] == i);
}

static void CheckMaxValue() {
    const std::uint64_t edges[] = {0, 1, 0xffffffff, 0x100000000, 0x7fffffffffffffff, 0x8000000000000000, 0xfffffffffffffffe, 0xffffffffffffffff};
    for (const auto& left : edges) {
        Require(__std_max_8u_nid_postfix(&left, &left + 1) == left);
        for (const auto& right : edges) {
            const std::uint64_t pair[] = {left, right};
            Require(__std_max_8u_nid_postfix(pair, pair + 2) == std::max(left, right));
        }
    }
    std::uint64_t state = 0x7698f1b33a25c04d;
    for (const std::size_t count : {1, 2, 3, 4, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 257}) {
        std::vector<std::uint64_t> values(count);
        for (auto& value : values) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            value = state;
        }
        const auto expected = *std::max_element(values.begin(), values.end());
        for (std::size_t offset = 0; offset < 16; ++offset) {
            std::vector<unsigned char> storage(count * sizeof(std::uint64_t) + 32, 0xff);
            std::memcpy(storage.data() + offset, values.data(), count * sizeof(std::uint64_t));
            const auto original = storage;
            Require(__std_max_8u_nid_postfix(storage.data() + offset, storage.data() + offset + count * sizeof(std::uint64_t)) == expected);
            Require(storage == original);
        }
    }
    const std::uint64_t repeated[] = {9, 9, 9, 9};
    Require(__std_max_8u_nid_postfix(repeated, repeated + 4) == 9);
}

int main() {
    CheckMinValue();
    CheckSizedHelpers();
    CheckFindAndCount();
    CheckUnsignedMin32Value();
    CheckUnsizedRescan();
    CheckSignedMax64Value();
    CheckMinMax();
    CheckMaxValue();
    CheckReverseAndSwap();
    CheckSignedMin64Value();
    return 0;
}
