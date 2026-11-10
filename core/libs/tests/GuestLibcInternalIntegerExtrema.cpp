#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal integer extrema: %s\n", message);
    std::abort();
}

template<typename TValue>
using Single = TValue (APS5_VABI*)(const void*, const void*);

template<typename TValue>
using Both = IntegerMinMax<TValue> (APS5_VABI*)(const void*, const void*);

template<typename TValue>
struct Exports {
    Single<TValue> minimum;
    Single<TValue> maximum;
    Both<TValue> both;
    const char* minimumName;
    const char* maximumName;
    const char* bothName;
};

std::uint64_t Next(std::uint64_t& state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

template<typename TValue>
std::vector<TValue> Edges() {
    using Limits = std::numeric_limits<TValue>;
    return {Limits::min(), static_cast<TValue>(Limits::min() + 1), static_cast<TValue>(-1), TValue{0}, TValue{1},
            static_cast<TValue>(Limits::max() - 1), Limits::max(), static_cast<TValue>(Limits::max() / 2 + 1)};
}

template<typename TValue>
void CheckRange(const Exports<TValue>& exports, const unsigned char* first, std::size_t count, TValue expectedMinimum, TValue expectedMaximum) {
    const unsigned char* last = first + count * sizeof(TValue);
    if (exports.minimum) Require(exports.minimum(first, last) == expectedMinimum, "the minimum differs from std::min_element");
    if (exports.maximum) Require(exports.maximum(first, last) == expectedMaximum, "the maximum differs from std::max_element");
    const IntegerMinMax<TValue> both = exports.both(first, last);
    Require(both.minimum == expectedMinimum && both.maximum == expectedMaximum, "minmax differs from std::minmax_element");
}

template<typename TValue>
void CheckPairs(const Exports<TValue>& exports) {
    const auto edges = Edges<TValue>();
    for (const TValue left : edges) {
        CheckRange(exports, reinterpret_cast<const unsigned char*>(&left), 1, left, left);
        for (const TValue right : edges) {
            const TValue pair[] = {left, right};
            CheckRange(exports, reinterpret_cast<const unsigned char*>(pair), 2, std::min(left, right), std::max(left, right));
        }
    }
}

template<typename TValue>
void CheckRanges(const Exports<TValue>& exports) {
    std::uint64_t state = 0x6a09e667f3bcc909;
    for (const std::size_t count : {1u, 2u, 3u, 7u, 8u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 257u}) {
        for (std::size_t offset = 0; offset < sizeof(TValue) + 1; ++offset) {
            std::vector<TValue> values(count);
            for (auto& value : values) value = static_cast<TValue>(Next(state));
            std::vector<unsigned char> storage(offset + count * sizeof(TValue) + sizeof(TValue), 0xa5);
            std::memcpy(storage.data() + offset, values.data(), count * sizeof(TValue));
            const auto original = storage;
            const auto [low, high] = std::minmax_element(values.begin(), values.end());
            CheckRange(exports, storage.data() + offset, count, *low, *high);
            Require(storage == original, "the input range was modified");
            for (const std::size_t position : {std::size_t{0}, count / 2, count - 1}) {
                for (const TValue edge : {std::numeric_limits<TValue>::min(), std::numeric_limits<TValue>::max()}) {
                    auto changed = values;
                    changed[position] = edge;
                    std::memcpy(storage.data() + offset, changed.data(), count * sizeof(TValue));
                    const auto [changedLow, changedHigh] = std::minmax_element(changed.begin(), changed.end());
                    CheckRange(exports, storage.data() + offset, count, *changedLow, *changedHigh);
                }
            }
        }
    }
}

template<typename TCall>
void RequireThrows(TCall call, const char* name) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(name) != std::string::npos, "the error does not name the export");
        return;
    }
    Require(false, "a null, empty, reversed or partial-element range must throw");
}

template<typename TValue>
void CheckInvalidRanges(const Exports<TValue>& exports) {
    TValue values[4]{};
    const auto* first = reinterpret_cast<const unsigned char*>(values);
    const auto* last = first + sizeof(values);
    const std::vector<std::pair<const void*, const void*>> ranges = [&] {
        std::vector<std::pair<const void*, const void*>> result{{nullptr, nullptr}, {nullptr, last}, {first, nullptr}, {first, first}, {last, first}};
        if (sizeof(TValue) > 1) result.emplace_back(first, last - 1);
        return result;
    }();
    for (const auto& [begin, end] : ranges) {
        if (exports.minimum) RequireThrows([&] { static_cast<void>(exports.minimum(begin, end)); }, exports.minimumName);
        if (exports.maximum) RequireThrows([&] { static_cast<void>(exports.maximum(begin, end)); }, exports.maximumName);
        RequireThrows([&] { static_cast<void>(exports.both(begin, end)); }, exports.bothName);
    }
}

template<typename TValue>
void Check(const Exports<TValue>& exports) {
    CheckPairs(exports);
    CheckRanges(exports);
    CheckInvalidRanges(exports);
}

} // namespace

int main() {
    Check(Exports<std::int8_t>{__std_min_1i_nid_postfix, __std_max_1i_nid_postfix, __std_minmax_1i_nid_postfix,
                               "__std_min_1i", "__std_max_1i", "__std_minmax_1i"});
    Check(Exports<std::uint8_t>{nullptr, nullptr, __std_minmax_1u_nid_postfix, "", "", "__std_minmax_1u"});
    Check(Exports<std::int16_t>{__std_min_2i_nid_postfix, __std_max_2i_nid_postfix, __std_minmax_2i_nid_postfix,
                                "__std_min_2i", "__std_max_2i", "__std_minmax_2i"});
    Check(Exports<std::uint16_t>{__std_min_2u_nid_postfix, __std_max_2u_nid_postfix, __std_minmax_2u_nid_postfix,
                                 "__std_min_2u", "__std_max_2u", "__std_minmax_2u"});
    Check(Exports<std::int32_t>{nullptr, nullptr, __std_minmax_4i_nid_postfix, "", "", "__std_minmax_4i"});
    Check(Exports<std::uint32_t>{nullptr, nullptr, __std_minmax_4u_nid_postfix, "", "", "__std_minmax_4u"});
    Check(Exports<std::int64_t>{nullptr, nullptr, __std_minmax_8i_nid_postfix, "", "", "__std_minmax_8i"});
    Check(Exports<std::uint64_t>{nullptr, nullptr, __std_minmax_8u_nid_postfix, "", "", "__std_minmax_8u"});
    return 0;
}
