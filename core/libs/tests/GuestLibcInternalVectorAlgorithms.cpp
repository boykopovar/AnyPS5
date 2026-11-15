#include "prx/libSceLibcInternal/include/VectorAlgorithms.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <xmmintrin.h>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal vector algorithms: %s\n", message);
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        const std::string suffix = " not implemented";
        return message.size() > suffix.size() && message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0 && message.compare(0, 6, "__std_") == 0;
    } catch (...) {
        return false;
    }
    return false;
}

template <typename T>
struct Api;

template <>
struct Api<float> {
    using Pair = MinMaxFloat;
    static float Min(const void* a, const void* b) { return __std_min_f_nid_postfix(a, b); }
    static float Max(const void* a, const void* b) { return __std_max_f_nid_postfix(a, b); }
    static Pair MinMax(const void* a, const void* b) { return __std_minmax_f_nid_postfix(a, b); }
    static const void* MinElement(const void* a, const void* b, bool flag = false) { return __std_min_element_f_nid_postfix(a, b, flag); }
    static const void* MaxElement(const void* a, const void* b, bool flag = false) { return __std_max_element_f_nid_postfix(a, b, flag); }
    static MinMaxPointers MinMaxElement(const void* a, const void* b, bool flag = false) { return __std_minmax_element_f_nid_postfix(a, b, flag); }
};

template <>
struct Api<double> {
    using Pair = MinMaxDouble;
    static double Min(const void* a, const void* b) { return __std_min_d_nid_postfix(a, b); }
    static double Max(const void* a, const void* b) { return __std_max_d_nid_postfix(a, b); }
    static Pair MinMax(const void* a, const void* b) { return __std_minmax_d_nid_postfix(a, b); }
    static const void* MinElement(const void* a, const void* b, bool flag = false) { return __std_min_element_d_nid_postfix(a, b, flag); }
    static const void* MaxElement(const void* a, const void* b, bool flag = false) { return __std_max_element_d_nid_postfix(a, b, flag); }
    static MinMaxPointers MinMaxElement(const void* a, const void* b, bool flag = false) { return __std_minmax_element_d_nid_postfix(a, b, flag); }
};

template <typename T>
bool SameBits(T left, T right) {
    return std::memcmp(&left, &right, sizeof(T)) == 0;
}

template <typename T>
void CheckAgainstStd(const std::vector<T>& values, const unsigned char* begin, const unsigned char* end, const char* message) {
    const auto minimum = std::min_element(values.begin(), values.end());
    const auto maximum = std::max_element(values.begin(), values.end());
    const auto pair = std::minmax_element(values.begin(), values.end());
    const auto offset = [&](typename std::vector<T>::const_iterator it) { return begin + (it - values.begin()) * sizeof(T); };
    Require(SameBits(Api<T>::Min(begin, end), *minimum), message);
    Require(SameBits(Api<T>::Max(begin, end), *maximum), message);
    const auto both = Api<T>::MinMax(begin, end);
    Require(SameBits(both.minimum, *pair.first) && SameBits(both.maximum, *pair.second), message);
    Require(Api<T>::MinElement(begin, end) == offset(minimum), message);
    Require(Api<T>::MaxElement(begin, end) == offset(maximum), message);
    const MinMaxPointers elements = Api<T>::MinMaxElement(begin, end);
    Require(elements.minimum == offset(pair.first) && elements.maximum == offset(pair.second), message);
}

template <typename T>
void CheckSequences() {
    std::vector<T> storage;
    for (std::size_t count = 1; count <= 40; ++count) {
        for (std::size_t pattern = 0; pattern < 6; ++pattern) {
            storage.assign(count, T(0));
            for (std::size_t i = 0; i < count; ++i) {
                const T index = static_cast<T>(i);
                switch (pattern) {
                case 0: storage[i] = index; break;
                case 1: storage[i] = static_cast<T>(count) - index; break;
                case 2: storage[i] = static_cast<T>((i * 7 + 3) % 11) - T(5); break;
                case 3: storage[i] = T(2.5); break;
                case 4: storage[i] = (i % 3 == 0) ? T(-1.5) : T(4.25); break;
                default: storage[i] = (i % 2 == 0) ? std::numeric_limits<T>::infinity() : -std::numeric_limits<T>::infinity(); break;
                }
            }
            const auto* begin = reinterpret_cast<const unsigned char*>(storage.data());
            CheckAgainstStd<T>(storage, begin, begin + count * sizeof(T), "sequence matches the standard algorithms");
        }
    }
}

template <typename T>
void CheckTies() {
    const T values[] = {T(3), T(1), T(5), T(1), T(5), T(2)};
    const auto* base = reinterpret_cast<const unsigned char*>(values);
    const auto* end = base + sizeof(values);
    Require(Api<T>::MinElement(base, end) == base + 1 * sizeof(T), "the first smallest element wins ties");
    Require(Api<T>::MaxElement(base, end) == base + 2 * sizeof(T), "the first largest element wins ties");
    const MinMaxPointers pair = Api<T>::MinMaxElement(base, end);
    Require(pair.minimum == base + 1 * sizeof(T), "minmax reports the first smallest element");
    Require(pair.maximum == base + 4 * sizeof(T), "minmax reports the last largest element");
    Require(Api<T>::MinElement(base, end, true) == base + 1 * sizeof(T), "the trailing flag does not change the minimum element");
    Require(Api<T>::MaxElement(base, end, true) == base + 2 * sizeof(T), "the trailing flag does not change the maximum element");
    const MinMaxPointers flagged = Api<T>::MinMaxElement(base, end, true);
    Require(flagged.minimum == pair.minimum && flagged.maximum == pair.maximum, "the trailing flag does not change minmax element");
}

template <typename T>
void CheckExtremes() {
    const T values[] = {T(0), std::numeric_limits<T>::denorm_min(), std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest(), -std::numeric_limits<T>::denorm_min(), std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity()};
    const auto* base = reinterpret_cast<const unsigned char*>(values);
    const auto* end = base + sizeof(values);
    Require(Api<T>::Min(base, end) == -std::numeric_limits<T>::infinity(), "minimum is negative infinity");
    Require(Api<T>::Max(base, end) == std::numeric_limits<T>::infinity(), "maximum is positive infinity");
    const T finite[] = {T(0), std::numeric_limits<T>::denorm_min(), std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest(), -std::numeric_limits<T>::denorm_min()};
    const auto* finiteBase = reinterpret_cast<const unsigned char*>(finite);
    Require(Api<T>::Min(finiteBase, finiteBase + sizeof(finite)) == std::numeric_limits<T>::lowest(), "minimum is the lowest finite value");
    Require(Api<T>::Max(finiteBase, finiteBase + sizeof(finite)) == std::numeric_limits<T>::max(), "maximum is the largest finite value");
    const T tiny[] = {std::numeric_limits<T>::denorm_min(), T(0), -std::numeric_limits<T>::denorm_min()};
    const auto* tinyBase = reinterpret_cast<const unsigned char*>(tiny);
    Require(Api<T>::MinElement(tinyBase, tinyBase + sizeof(tiny)) == tinyBase + 2 * sizeof(T), "denormals order below and above zero");
    Require(Api<T>::MaxElement(tinyBase, tinyBase + sizeof(tiny)) == tinyBase, "denormals order below and above zero");
}

template <typename T>
void CheckSignedZero() {
    const T values[] = {T(0), -T(0), T(0), -T(0)};
    const auto* base = reinterpret_cast<const unsigned char*>(values);
    const auto* end = base + sizeof(values);
    Require(SameBits(Api<T>::Min(base, end), -T(0)), "the vector reduction selects the last lane's zero for minimum");
    Require(SameBits(Api<T>::Max(base, end), -T(0)), "the vector reduction selects the last lane's zero for maximum");
    Require(Api<T>::MinElement(base, end) == base, "the first zero is the smallest element");
    Require(Api<T>::MaxElement(base, end) == base, "the first zero is the largest element");
    const MinMaxPointers pair = Api<T>::MinMaxElement(base, end);
    Require(pair.minimum == base && pair.maximum == base + 3 * sizeof(T), "minmax follows operator< on equal zeros");
    const auto both = Api<T>::MinMax(base, end);
    Require(SameBits(both.minimum, T(0)), "the value form keeps the first zero as the minimum");
    Require(SameBits(both.maximum, T(0)), "the value form keeps the first equal zero as the maximum");
    const T negativeFirst[] = {-T(0), T(0)};
    const auto* negativeBase = reinterpret_cast<const unsigned char*>(negativeFirst);
    const T expectedZero = sizeof(T) == 4 ? -T(0) : T(0);
    Require(SameBits(Api<T>::Min(negativeBase, negativeBase + sizeof(negativeFirst)), expectedZero), "the scalar and packed paths retain their zero selection order");
}

template <typename T>
void CheckUnaligned() {
    std::vector<unsigned char> bytes(sizeof(T) * 9 + 8);
    const T values[] = {T(4), T(-2), T(9), T(0.5), T(-7), T(3), T(8), T(-7), T(1)};
    for (std::size_t offset = 0; offset < 8; ++offset) {
        std::memcpy(bytes.data() + offset, values, sizeof(values));
        const auto* begin = bytes.data() + offset;
        const auto* end = begin + sizeof(values);
        Require(Api<T>::Min(begin, end) == T(-7), "unaligned minimum");
        Require(Api<T>::Max(begin, end) == T(9), "unaligned maximum");
        Require(Api<T>::MinElement(begin, end) == begin + 4 * sizeof(T), "unaligned minimum element");
        Require(Api<T>::MaxElement(begin, end) == begin + 2 * sizeof(T), "unaligned maximum element");
        const MinMaxPointers pair = Api<T>::MinMaxElement(begin, end);
        Require(pair.minimum == begin + 4 * sizeof(T) && pair.maximum == begin + 2 * sizeof(T), "unaligned minmax element");
    }
}

template <typename T>
void CheckNaN() {
    const T quiet = std::numeric_limits<T>::quiet_NaN();
    for (std::size_t position = 0; position < 7; position += 3) {
        T values[7] = {T(1), T(2), T(3), T(4), T(5), T(6), T(7)};
        values[position] = quiet;
        const auto* base = reinterpret_cast<const unsigned char*>(values);
        const auto* end = base + sizeof(values);
        Require(Throws([&] { Api<T>::MinMax(base, end); }), "minmax rejects a NaN");
        Require(Throws([&] { Api<T>::MinElement(base, end); }), "minimum element rejects a NaN");
        Require(Throws([&] { Api<T>::MaxElement(base, end); }), "maximum element rejects a NaN");
        Require(Throws([&] { Api<T>::MinMaxElement(base, end); }), "minmax element rejects a NaN");
    }
    T negativeNaN[2] = {T(1), -std::numeric_limits<T>::quiet_NaN()};
    const auto* negativeBase = reinterpret_cast<const unsigned char*>(negativeNaN);
    Require(Throws([&] { Api<T>::MinMax(negativeBase, negativeBase + sizeof(negativeNaN)); }), "a negative NaN is rejected by minmax");
    T signaling[2] = {T(1), std::numeric_limits<T>::signaling_NaN()};
    const auto* signalingBase = reinterpret_cast<const unsigned char*>(signaling);
    Require(Throws([&] { Api<T>::MinMax(signalingBase, signalingBase + sizeof(signaling)); }), "a signaling NaN is rejected by minmax");
}

template <typename T>
void CheckInvalidRanges() {
    const T values[4] = {T(1), T(2), T(3), T(4)};
    const auto* base = reinterpret_cast<const unsigned char*>(values);
    Require(Throws([&] { Api<T>::Min(base, base); }), "an empty range is rejected for minimum");
    Require(Throws([&] { Api<T>::Max(base, base); }), "an empty range is rejected for maximum");
    Require(Throws([&] { Api<T>::MinMax(base, base); }), "an empty range is rejected for minmax");
    Require(Throws([&] { Api<T>::Min(base + sizeof(values), base); }), "a reversed range is rejected");
    Require(Throws([&] { Api<T>::Max(base, base + sizeof(T) + 1); }), "a partial element range is rejected");
    Require(Throws([&] { Api<T>::MinMax(nullptr, base); }), "a null first pointer is rejected");
    Require(Throws([&] { Api<T>::MinMax(base, nullptr); }), "a null last pointer is rejected");
    Require(Throws([&] { Api<T>::MinElement(base + sizeof(values), base); }), "a reversed element range is rejected");
    Require(Throws([&] { Api<T>::MaxElement(base, base + sizeof(T) + 1); }), "a partial element range is rejected for elements");
    const void* const wrapped = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0) - sizeof(T) * 16);
    Require(Throws([&] { Api<T>::Min(wrapped, base); }), "a reversed range of whole elements is rejected before any read");
    Require(Throws([&] { Api<T>::MinMax(wrapped, base); }), "a reversed minmax range of whole elements is rejected before any read");
    Require(Throws([&] { Api<T>::MaxElement(wrapped, base); }), "a reversed element range of whole elements is rejected before any read");
    Require(Api<T>::MinElement(base, base) == base, "an empty range returns first for the minimum element");
    Require(Api<T>::MaxElement(base, base) == base, "an empty range returns first for the maximum element");
    const MinMaxPointers empty = Api<T>::MinMaxElement(base, base);
    Require(empty.minimum == base && empty.maximum == base, "an empty range returns first for minmax element");
    Require(Api<T>::MinElement(nullptr, nullptr) == nullptr, "an empty null range returns first for the minimum element");
    Require(Api<T>::MaxElement(nullptr, nullptr) == nullptr, "an empty null range returns first for the maximum element");
    const MinMaxPointers emptyNull = Api<T>::MinMaxElement(nullptr, nullptr);
    Require(emptyNull.minimum == nullptr && emptyNull.maximum == nullptr, "an empty null range returns first for minmax element");
    Require(Throws([&] { Api<T>::Min(nullptr, nullptr); }), "an empty null range is rejected for minimum");
}

template<typename TValue>
using ValueBits = std::conditional_t<sizeof(TValue) == 4, std::uint32_t, std::uint64_t>;

template<typename TValue>
struct NativeValueCase {
    bool special;
    std::vector<ValueBits<TValue>> values;
    ValueBits<TValue> minimum;
    ValueBits<TValue> maximum;
    unsigned int minimumFlags;
    unsigned int maximumFlags;
    unsigned int control;
};

template<typename TValue>
std::vector<NativeValueCase<TValue>> NativeValueCases() {
    if constexpr (sizeof(TValue) == 4) {
        return {
            {false, {0x80000000ULL}, 0x80000000ULL, 0x80000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x00000000ULL, 0x80000000ULL}, 0x00000000ULL, 0x00000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x00000000ULL, 0x00000000ULL, 0x80000000ULL}, 0x00000000ULL, 0x00000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x80000000ULL}, 0x80000000ULL, 0x80000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x80000000ULL}, 0x00000000ULL, 0x00000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x80000000ULL}, 0x80000000ULL, 0x80000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x80000000ULL}, 0x00000000ULL, 0x00000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x00000000ULL, 0x80000000ULL}, 0x00000000ULL, 0x00000000ULL, 0u, 0u, 0x1f80u},
            {true, {0x7fc12345ULL}, 0x7fc12345ULL, 0x7fc12345ULL, 0u, 0u, 0x1f80u},
            {true, {0x7fc12345ULL, 0xc0400000ULL}, 0x7fc12345ULL, 0x7fc12345ULL, 1u, 1u, 0x1f80u},
            {true, {0x7fc12345ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL}, 0xc0400000ULL, 0xbf800000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7fc12345ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL, 0x00000000ULL}, 0xc0400000ULL, 0x00000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7f812345ULL}, 0x7f812345ULL, 0x7f812345ULL, 0u, 0u, 0x1f80u},
            {true, {0x7f812345ULL, 0xc0400000ULL}, 0x7f812345ULL, 0x7f812345ULL, 1u, 1u, 0x1f80u},
            {true, {0x7f812345ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL}, 0xc0400000ULL, 0xbf800000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7f812345ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL, 0x00000000ULL}, 0xc0400000ULL, 0x00000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x00000001ULL}, 0x00000001ULL, 0x00000001ULL, 0u, 0u, 0x1f80u},
            {true, {0x00000001ULL, 0xc0400000ULL}, 0xc0400000ULL, 0x00000001ULL, 2u, 2u, 0x1f80u},
            {true, {0x00000001ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL}, 0xc0400000ULL, 0x00000001ULL, 2u, 2u, 0x1f80u},
            {true, {0x00000001ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL, 0x00000000ULL}, 0xc0400000ULL, 0x00000001ULL, 2u, 2u, 0x1f80u},
            {true, {0xc0800000ULL, 0x7fc12345ULL}, 0xc0800000ULL, 0xc0800000ULL, 1u, 1u, 0x1f80u},
            {true, {0xc0800000ULL, 0xc0400000ULL, 0xc0000000ULL, 0x7fc12345ULL}, 0x7fc12345ULL, 0x7fc12345ULL, 1u, 1u, 0x1f80u},
            {true, {0xc0800000ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL, 0x7fc12345ULL}, 0xc0800000ULL, 0xbf800000ULL, 1u, 1u, 0x1f80u},
            {true, {0xc0800000ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL, 0x00000000ULL, 0x3f800000ULL, 0x40000000ULL, 0x40400000ULL, 0x7fc12345ULL}, 0xc0800000ULL, 0x40400000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7f800000ULL, 0xc0400000ULL, 0xc0000000ULL, 0xbf800000ULL, 0x00000000ULL, 0x3f800000ULL, 0x40000000ULL}, 0xc0400000ULL, 0x7f800000ULL, 0u, 0u, 0x1f80u},
            {true, {0xff854321ULL, 0x3f800000ULL, 0x7f812345ULL, 0x80000000ULL, 0x7fc12345ULL, 0xbf800000ULL, 0xff854321ULL}, 0xbf800000ULL, 0x3f800000ULL, 1u, 1u, 0x1fc0u},
            {true, {0xff854321ULL, 0x3f800000ULL, 0x7f812345ULL, 0x80000000ULL, 0x7fc12345ULL, 0xbf800000ULL, 0xff854321ULL}, 0xbf800000ULL, 0x3f800000ULL, 1u, 1u, 0x9f80u},
            {true, {0xff854321ULL, 0x3f800000ULL, 0x7f812345ULL, 0x80000000ULL, 0x7fc12345ULL, 0xbf800000ULL, 0xff854321ULL}, 0xbf800000ULL, 0x3f800000ULL, 1u, 1u, 0x9fc0u},
        };
    } else {
        return {
            {false, {0x8000000000000000ULL}, 0x8000000000000000ULL, 0x8000000000000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x0000000000000000ULL, 0x8000000000000000ULL}, 0x8000000000000000ULL, 0x8000000000000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x0000000000000000ULL, 0x0000000000000000ULL, 0x8000000000000000ULL}, 0x0000000000000000ULL, 0x0000000000000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x8000000000000000ULL}, 0x8000000000000000ULL, 0x8000000000000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x8000000000000000ULL}, 0x0000000000000000ULL, 0x0000000000000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x8000000000000000ULL}, 0x8000000000000000ULL, 0x8000000000000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x8000000000000000ULL}, 0x0000000000000000ULL, 0x0000000000000000ULL, 0u, 0u, 0x1f80u},
            {false, {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x8000000000000000ULL}, 0x0000000000000000ULL, 0x0000000000000000ULL, 0u, 0u, 0x1f80u},
            {true, {0x7ff8123456789abcULL}, 0x7ff8123456789abcULL, 0x7ff8123456789abcULL, 0u, 0u, 0x1f80u},
            {true, {0x7ff8123456789abcULL, 0xc008000000000000ULL}, 0xc008000000000000ULL, 0xc008000000000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7ff8123456789abcULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL}, 0xc008000000000000ULL, 0xbff0000000000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7ff8123456789abcULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL, 0x0000000000000000ULL}, 0xc008000000000000ULL, 0x0000000000000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7ff0123456789abcULL}, 0x7ff0123456789abcULL, 0x7ff0123456789abcULL, 0u, 0u, 0x1f80u},
            {true, {0x7ff0123456789abcULL, 0xc008000000000000ULL}, 0xc008000000000000ULL, 0xc008000000000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7ff0123456789abcULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL}, 0xc008000000000000ULL, 0xbff0000000000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7ff0123456789abcULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL, 0x0000000000000000ULL}, 0xc008000000000000ULL, 0x0000000000000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x0000000000000001ULL}, 0x0000000000000001ULL, 0x0000000000000001ULL, 0u, 0u, 0x1f80u},
            {true, {0x0000000000000001ULL, 0xc008000000000000ULL}, 0xc008000000000000ULL, 0x0000000000000001ULL, 2u, 2u, 0x1f80u},
            {true, {0x0000000000000001ULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL}, 0xc008000000000000ULL, 0x0000000000000001ULL, 2u, 2u, 0x1f80u},
            {true, {0x0000000000000001ULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL, 0x0000000000000000ULL}, 0xc008000000000000ULL, 0x0000000000000001ULL, 2u, 2u, 0x1f80u},
            {true, {0xc010000000000000ULL, 0x7ff8123456789abcULL}, 0x7ff8123456789abcULL, 0x7ff8123456789abcULL, 1u, 1u, 0x1f80u},
            {true, {0xc010000000000000ULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0x7ff8123456789abcULL}, 0x7ff8123456789abcULL, 0x7ff8123456789abcULL, 1u, 1u, 0x1f80u},
            {true, {0xc010000000000000ULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL, 0x7ff8123456789abcULL}, 0xc010000000000000ULL, 0xbff0000000000000ULL, 1u, 1u, 0x1f80u},
            {true, {0xc010000000000000ULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL, 0x0000000000000000ULL, 0x3ff0000000000000ULL, 0x4000000000000000ULL, 0x4008000000000000ULL, 0x7ff8123456789abcULL}, 0xc010000000000000ULL, 0x4008000000000000ULL, 1u, 1u, 0x1f80u},
            {true, {0x7ff0000000000000ULL, 0xc008000000000000ULL, 0xc000000000000000ULL, 0xbff0000000000000ULL, 0x0000000000000000ULL, 0x3ff0000000000000ULL, 0x4000000000000000ULL}, 0xc008000000000000ULL, 0x7ff0000000000000ULL, 0u, 0u, 0x1f80u},
            {true, {0xfff023456789abcdULL, 0x3ff0000000000000ULL, 0x7ff0123456789abcULL, 0x8000000000000000ULL, 0x7ff8123456789abcULL, 0xbff0000000000000ULL, 0xfff023456789abcdULL}, 0xbff0000000000000ULL, 0x3ff0000000000000ULL, 1u, 1u, 0x1fc0u},
            {true, {0xfff023456789abcdULL, 0x3ff0000000000000ULL, 0x7ff0123456789abcULL, 0x8000000000000000ULL, 0x7ff8123456789abcULL, 0xbff0000000000000ULL, 0xfff023456789abcdULL}, 0xbff0000000000000ULL, 0x3ff0000000000000ULL, 1u, 1u, 0x9f80u},
            {true, {0xfff023456789abcdULL, 0x3ff0000000000000ULL, 0x7ff0123456789abcULL, 0x8000000000000000ULL, 0x7ff8123456789abcULL, 0xbff0000000000000ULL, 0xfff023456789abcdULL}, 0xbff0000000000000ULL, 0x3ff0000000000000ULL, 1u, 1u, 0x9fc0u},
        };
    }
}

template<typename TValue>
void CheckNativeValueCases(bool special) {
    const auto savedControl = _mm_getcsr();
    for (const auto& test : NativeValueCases<TValue>()) {
        if (test.special != special) continue;
        const auto byteCount = test.values.size() * sizeof(TValue);
        for (std::size_t sample = 0; sample < sizeof(TValue) * 2; ++sample) {
            const auto offset = sample % sizeof(TValue);
            const unsigned int stickyFlags = sample < sizeof(TValue) ? 0u : 0x20u;
            std::vector<unsigned char> storage(byteCount + sizeof(TValue));
            auto* first = storage.data() + offset;
            const auto* last = first + byteCount;
            std::memcpy(first, test.values.data(), byteCount);
            _mm_setcsr(test.control | stickyFlags);
            const auto minimum = Api<TValue>::Min(first, last);
            const auto minimumState = _mm_getcsr();
            Require(std::bit_cast<ValueBits<TValue>>(minimum) == test.minimum, "native minimum value bits");
            Require((minimumState & 0x3fu) == (test.minimumFlags | stickyFlags), "native minimum floating-point flags");
            Require((minimumState & ~0x3fu) == test.control, "minimum preserves floating-point controls");
            _mm_setcsr(test.control | stickyFlags);
            const auto maximum = Api<TValue>::Max(first, last);
            const auto maximumState = _mm_getcsr();
            Require(std::bit_cast<ValueBits<TValue>>(maximum) == test.maximum, "native maximum value bits");
            Require((maximumState & 0x3fu) == (test.maximumFlags | stickyFlags), "native maximum floating-point flags");
            Require((maximumState & ~0x3fu) == test.control, "maximum preserves floating-point controls");
            Require(std::memcmp(first, test.values.data(), byteCount) == 0, "native extrema preserve input");
        }
    }
    _mm_setcsr(savedControl);
}

template <typename T>
void CheckType() {
    CheckSequences<T>();
    CheckTies<T>();
    CheckExtremes<T>();
    CheckSignedZero<T>();
    CheckUnaligned<T>();
    CheckNaN<T>();
    CheckInvalidRanges<T>();
}

}

int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string mode = argv[1];
        if (mode != "native-zero" && mode != "native-special") return 2;
        CheckNativeValueCases<float>(mode == "native-special");
        CheckNativeValueCases<double>(mode == "native-special");
        return 0;
    }
    if (argc != 1) return 2;
    CheckType<float>();
    CheckType<double>();
    return 0;
}
