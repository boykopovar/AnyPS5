#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr UCharIteratorOrigin Start = UITER_START;
constexpr UCharIteratorOrigin Current = UITER_CURRENT;
constexpr UCharIteratorOrigin Limit = UITER_LIMIT;
constexpr UCharIteratorOrigin Zero = UITER_ZERO;
constexpr UCharIteratorOrigin Length = UITER_LENGTH;
constexpr auto UnknownOrigin = static_cast<UCharIteratorOrigin>(9);
constexpr std::int32_t Sentinel = -1;
constexpr std::int32_t UnknownIndex = -2;
constexpr std::uint32_t NoState = 0xffffffff;
constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;

using Units = std::vector<std::int32_t>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU iterator: %s\n", message);
    std::abort();
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void Append(Units& units, std::int32_t codePoint) {
    if (codePoint <= 0xffff) {
        units.push_back(codePoint);
        return;
    }
    units.push_back(0xd800 + ((codePoint - 0x10000) >> 10));
    units.push_back(0xdc00 + ((codePoint - 0x10000) & 0x3ff));
}

Units Decode(const std::string& bytes) {
    Units units;
    std::size_t position = 0;
    while (position < bytes.size()) {
        const auto lead = static_cast<std::uint8_t>(bytes[position]);
        std::size_t needed = 0;
        std::uint8_t low = 0x80, high = 0xbf;
        std::int32_t codePoint = lead;
        if (lead >= 0xc2 && lead <= 0xdf) { needed = 1; codePoint = lead & 0x1f; }
        else if (lead >= 0xe0 && lead <= 0xef) { needed = 2; codePoint = lead & 0x0f; low = lead == 0xe0 ? 0xa0 : 0x80; high = lead == 0xed ? 0x9f : 0xbf; }
        else if (lead >= 0xf0 && lead <= 0xf4) { needed = 3; codePoint = lead & 0x07; low = lead == 0xf0 ? 0x90 : 0x80; high = lead == 0xf4 ? 0x8f : 0xbf; }
        else if (lead >= 0x80) { units.push_back(0xfffd); ++position; continue; }
        std::size_t consumed = 1;
        bool valid = true;
        for (std::size_t trail = 0; trail < needed; ++trail) {
            const std::size_t at = position + consumed;
            const std::uint8_t byte = at < bytes.size() ? static_cast<std::uint8_t>(bytes[at]) : 0;
            const std::uint8_t minimum = trail == 0 ? low : 0x80, maximum = trail == 0 ? high : 0xbf;
            if (at >= bytes.size() || byte < minimum || byte > maximum) {
                valid = false;
                break;
            }
            codePoint = (codePoint << 6) | (byte & 0x3f);
            ++consumed;
        }
        if (valid) Append(units, codePoint);
        else units.push_back(0xfffd);
        position += consumed;
    }
    return units;
}

Units Forward(UCharIterator& iterator) {
    Units units;
    for (std::int32_t unit; (unit = iterator.next(&iterator)) != Sentinel;) units.push_back(unit);
    return units;
}

Units Backward(UCharIterator& iterator) {
    Units units;
    iterator.move(&iterator, 0, Limit);
    for (std::int32_t unit; (unit = iterator.previous(&iterator)) != Sentinel;) units.insert(units.begin(), unit);
    return units;
}

void Strings() {
    const std::u16string text = u"aé\U0001F600z";
    UCharIterator iterator{};
    uiter_setString_nid_postfix(&iterator, text.c_str(), -1);
    Require(iterator.context == text.c_str() && iterator.length == 5 && iterator.limit == 5 && iterator.start == 0 && iterator.index == 0, "string fields");
    Require(iterator.reservedFn == nullptr && iterator.reservedField == 0, "string reserved members");
    Require(iterator.getIndex(&iterator, Length) == 5 && iterator.getIndex(&iterator, Limit) == 5 && iterator.getIndex(&iterator, Start) == 0, "string bounds");
    Require(iterator.getIndex(&iterator, Zero) == 0 && iterator.getIndex(&iterator, UnknownOrigin) == -1, "string zero and invalid origins");
    Require(iterator.hasPrevious(&iterator) == 0 && iterator.hasNext(&iterator) == 1 && iterator.current(&iterator) == 0x61, "string start");
    Require(Forward(iterator) == Units{0x61, 0xe9, 0xd83d, 0xde00, 0x7a}, "string units");
    Require(iterator.hasNext(&iterator) == 0 && iterator.current(&iterator) == Sentinel && iterator.getIndex(&iterator, Current) == 5, "string end");
    Require(Backward(iterator) == Units{0x61, 0xe9, 0xd83d, 0xde00, 0x7a}, "string units backward");
    Require(iterator.previous(&iterator) == Sentinel, "string before start");
    Require(iterator.move(&iterator, 2, Zero) == 2 && iterator.current(&iterator) == 0xd83d, "string move from zero");
    Require(iterator.move(&iterator, 1, Current) == 3 && iterator.move(&iterator, -1, Limit) == 4 && iterator.move(&iterator, -3, Length) == 2, "string relative moves");
    Require(iterator.move(&iterator, 1, Start) == 1 && iterator.move(&iterator, 9, Current) == 5 && iterator.move(&iterator, -9, Current) == 0, "string moves are clamped");
    Require(iterator.move(&iterator, 0x7fffffff, Limit) == 0 && iterator.move(&iterator, 3, UnknownOrigin) == -1 && iterator.index == 0, "string moves wrap and reject unknown origins");
    iterator.move(&iterator, 3, Zero);
    const std::uint32_t state = iterator.getState(&iterator);
    Require(state == 3, "string state is the index");
    iterator.move(&iterator, 0, Zero);
    std::int32_t error = Success;
    iterator.setState(&iterator, state, &error);
    Require(error == Success && iterator.next(&iterator) == 0xde00, "string state round trip");
    iterator.setState(&iterator, 6, &error);
    Require(error == IndexOutOfBounds, "string state past the limit");
    iterator.setState(&iterator, 1, &error);
    Require(iterator.index == 4, "string state with an error set changes nothing");
    iterator.setState(&iterator, 1, nullptr);
    error = Success;
    iterator.setState(nullptr, 1, &error);
    Require(error == IllegalArgument, "string state without an iterator");
    const char16_t embedded[] = {0x41, 0, 0x42};
    uiter_setString_nid_postfix(&iterator, embedded, 3);
    Require(iterator.length == 3 && Forward(iterator) == Units{0x41, 0, 0x42}, "explicit length counts embedded NULs");
    uiter_setString_nid_postfix(&iterator, embedded, 0);
    Require(iterator.length == 0 && iterator.next(&iterator) == Sentinel, "empty string");
    Require(Rejects([&] { iterator.next(nullptr); }), "string callback without an iterator");
}

void Noop() {
    const char16_t text[] = u"abc";
    for (int variant = 0; variant < 4; ++variant) {
        UCharIterator iterator{};
        iterator.index = 99;
        if (variant == 0) uiter_setString_nid_postfix(&iterator, nullptr, 3);
        else if (variant == 1) uiter_setString_nid_postfix(&iterator, text, -2);
        else if (variant == 2) uiter_setUTF8_nid_postfix(&iterator, nullptr, 0);
        else uiter_setUTF8_nid_postfix(&iterator, "abc", -5);
        Require(iterator.context == nullptr && iterator.length == 0 && iterator.index == 0 && iterator.limit == 0, "noop fields");
        Require(iterator.getIndex(&iterator, Length) == 0 && iterator.move(&iterator, 5, Zero) == 0, "noop index and move");
        Require(iterator.hasNext(&iterator) == 0 && iterator.hasPrevious(&iterator) == 0, "noop has next and previous");
        Require(iterator.current(&iterator) == Sentinel && iterator.next(&iterator) == Sentinel && iterator.previous(&iterator) == Sentinel, "noop code units");
        Require(iterator.getState(&iterator) == NoState && iterator.reservedFn == nullptr, "noop state");
        std::int32_t error = Success;
        iterator.setState(&iterator, 0, &error);
        Require(error == Unsupported, "noop set state");
        Require(Rejects([&] { iterator.setState(&iterator, 0, nullptr); }), "noop set state with a null error pointer");
    }
    uiter_setString_nid_postfix(nullptr, text, 3);
    uiter_setUTF8_nid_postfix(nullptr, "abc", 3);
}

void Utf8Known() {
    const std::string text = "a\xc3\xa9\xf0\x9f\x98\x80z";
    UCharIterator iterator{};
    uiter_setUTF8_nid_postfix(&iterator, text.c_str(), -1);
    Require(iterator.limit == 8 && iterator.length == -1 && iterator.index == 0 && iterator.start == 0, "UTF-8 fields");
    Require(iterator.getIndex(&iterator, Length) == 5 && iterator.length == 5, "UTF-8 length is computed lazily");
    Require(iterator.next(&iterator) == 0x61 && iterator.next(&iterator) == 0xe9 && iterator.next(&iterator) == 0xd83d, "UTF-8 lead surrogate");
    Require(iterator.getIndex(&iterator, Current) == 3 && iterator.reservedField == 0x1f600 && iterator.current(&iterator) == 0xde00, "UTF-8 between surrogates");
    const std::uint32_t inside = iterator.getState(&iterator);
    Require(inside == ((7u << 1) | 1u), "UTF-8 state inside a supplementary character");
    Require(iterator.previous(&iterator) == 0xd83d && iterator.previous(&iterator) == 0xe9, "UTF-8 previous across the pair");
    std::int32_t error = Success;
    iterator.setState(&iterator, inside, &error);
    Require(error == Success && iterator.index == -1 && iterator.next(&iterator) == 0xde00 && iterator.getIndex(&iterator, Current) == 4, "UTF-8 state round trip inside the pair");
    Require(iterator.next(&iterator) == 0x7a && iterator.next(&iterator) == Sentinel && iterator.hasNext(&iterator) == 0, "UTF-8 end");
    iterator.setState(&iterator, (2u << 1) | 1u, &error);
    Require(error == IndexOutOfBounds, "UTF-8 state inside below byte 4");
    error = Success;
    iterator.setState(&iterator, 9u << 1, &error);
    Require(error == IndexOutOfBounds, "UTF-8 state past the limit");
    error = Success;
    iterator.setState(&iterator, (8u << 1) | 1u, &error);
    Require(error == IndexOutOfBounds && iterator.start == 8 && iterator.index == -1, "UTF-8 inside state after a BMP character");
    error = Success;
    iterator.setState(nullptr, 0, &error);
    Require(error == IllegalArgument, "UTF-8 state without an iterator");
    uiter_setUTF8_nid_postfix(&iterator, "x", -1);
    Require(iterator.length == 1 && iterator.limit == 1, "short UTF-8 strings know their length");
    Require(Rejects([&] { iterator.current(nullptr); }), "UTF-8 callback without an iterator");
}

void Utf8Text(const std::string& bytes) {
    const Units expected = Decode(bytes);
    const auto count = static_cast<std::int32_t>(expected.size());
    UCharIterator iterator{};
    uiter_setUTF8_nid_postfix(&iterator, bytes.data(), static_cast<std::int32_t>(bytes.size()));
    Require(Forward(iterator) == expected, "UTF-8 forward units match the reference decoder");
    Require(iterator.getIndex(&iterator, Current) == count && iterator.getIndex(&iterator, Length) == count, "UTF-8 index at the end");
    Require(Backward(iterator) == expected, "UTF-8 backward units match");
    for (std::int32_t index = 0; index <= count; ++index) {
        uiter_setUTF8_nid_postfix(&iterator, bytes.data(), static_cast<std::int32_t>(bytes.size()));
        Require(iterator.move(&iterator, index, Zero) == index, "UTF-8 move from zero");
        Require(iterator.current(&iterator) == (index < count ? expected[static_cast<std::size_t>(index)] : Sentinel), "UTF-8 current after a move");
        const std::uint32_t state = iterator.getState(&iterator);
        iterator.move(&iterator, 0, Start);
        std::int32_t error = Success;
        iterator.setState(&iterator, state, &error);
        Require(error == Success && iterator.getIndex(&iterator, Current) == index, "UTF-8 state round trip");
        Require(iterator.next(&iterator) == (index < count ? expected[static_cast<std::size_t>(index)] : Sentinel), "UTF-8 next after a restored state");
        uiter_setUTF8_nid_postfix(&iterator, bytes.data(), static_cast<std::int32_t>(bytes.size()));
        const std::int32_t moved = iterator.move(&iterator, index - count, Limit);
        Require(moved == UnknownIndex || moved == index, "UTF-8 move from the limit");
        Require(iterator.current(&iterator) == (index < count ? expected[static_cast<std::size_t>(index)] : Sentinel), "UTF-8 current after a move from the limit");
        Require(iterator.getIndex(&iterator, Current) == index, "UTF-8 index computed after a move from the limit");
    }
}

void Utf8Samples() {
    for (const char* sample : {"", "plain ASCII", "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\xf4\x8f\xbf\xbf", "\x80\xbf\xc0\xc1\xf5\xff",
                               "\xe0\x80\x80\xed\xa0\x80\xf0\x80\x80\x80\xf4\x90\x80\x80", "\xe2\x82\xf0\x9f\x98\xc3", "a\xf0\x9f\x98", "\xed\x9f\xbf\xee\x80\x80"})
        Utf8Text(sample);
    std::uint64_t state = 0x2545f4914f6cdd1dull;
    const auto next = [&state] {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    };
    constexpr std::uint8_t interesting[] = {0x00, 0x41, 0x7f, 0x80, 0x8f, 0x90, 0x9f, 0xa0, 0xbf, 0xc0, 0xc2, 0xdf, 0xe0, 0xe1, 0xed, 0xef, 0xf0, 0xf1, 0xf4, 0xf5, 0xff};
    for (int sample = 0; sample < 3000; ++sample) {
        std::string bytes(next() % 24, '\0');
        for (auto& byte : bytes) {
            const std::uint64_t value = next();
            byte = static_cast<char>(value % 3 == 0 ? static_cast<std::uint8_t>(value >> 8) : interesting[(value >> 8) % sizeof(interesting)]);
        }
        Utf8Text(bytes);
    }
}

}

int main() {
    Strings();
    Noop();
    Utf8Known();
    Utf8Samples();
    return 0;
}
