#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU iterator comparison check failed at line %u\n", location.line());
    std::abort();
}

struct Text {
    std::vector<std::int32_t> units;
    std::string calls;
};

Text& Of(UCharIterator* iterator) {
    return *static_cast<Text*>(const_cast<void*>(iterator->context));
}

std::int32_t APS5_VABI Move_nid_no_patch(UCharIterator* iterator, std::int32_t delta, UCharIteratorOrigin origin) {
    auto& text = Of(iterator);
    Require(origin == UITER_START);
    text.calls += 'm';
    iterator->index = iterator->start + delta;
    return iterator->index;
}

std::int32_t APS5_VABI Next_nid_no_patch(UCharIterator* iterator) {
    auto& text = Of(iterator);
    text.calls += 'n';
    return iterator->index < iterator->limit ? text.units[iterator->index++] : -1;
}

std::int32_t APS5_VABI Current_nid_no_patch(UCharIterator* iterator) {
    auto& text = Of(iterator);
    text.calls += 'c';
    return iterator->index < iterator->limit ? text.units[iterator->index] : -1;
}

std::int32_t APS5_VABI Previous_nid_no_patch(UCharIterator* iterator) {
    auto& text = Of(iterator);
    text.calls += 'p';
    return iterator->index > iterator->start ? text.units[--iterator->index] : -1;
}

struct Iterator {
    Text text;
    UCharIterator iterator{};

    explicit Iterator(std::vector<std::int32_t> units, std::int32_t index = 0) : text{std::move(units), {}} {
        iterator.context = &text;
        iterator.length = static_cast<std::int32_t>(text.units.size());
        iterator.start = 0;
        iterator.index = index;
        iterator.limit = iterator.length;
        iterator.move = Move_nid_no_patch;
        iterator.next = Next_nid_no_patch;
        iterator.current = Current_nid_no_patch;
        iterator.previous = Previous_nid_no_patch;
    }
};

struct Result {
    std::int32_t value;
    std::string first;
    std::string second;
};

Result Compare(std::vector<std::int32_t> left, std::vector<std::int32_t> right, std::uint8_t codePointOrder) {
    Iterator first(std::move(left), 1);
    Iterator second(std::move(right), 1);
    const auto value = u_strCompareIter_nid_postfix(&first.iterator, &second.iterator, codePointOrder);
    return {value, first.text.calls, second.text.calls};
}

void Expect(std::vector<std::int32_t> left, std::vector<std::int32_t> right, std::uint8_t codePointOrder,
    std::int32_t value, const char* first, const char* second, std::source_location location = std::source_location::current()) {
    const auto result = Compare(std::move(left), std::move(right), codePointOrder);
    Require(result.value == value, location);
    Require(result.first == first, location);
    Require(result.second == second, location);
}

template<typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false, location);
}

void Arguments() {
    Iterator text({u'a'});
    Require(u_strCompareIter_nid_postfix(nullptr, &text.iterator, 1) == 0);
    Require(u_strCompareIter_nid_postfix(&text.iterator, nullptr, 1) == 0);
    Require(u_strCompareIter_nid_postfix(nullptr, nullptr, 0) == 0);
    Require(u_strCompareIter_nid_postfix(&text.iterator, &text.iterator, 1) == 0);
    Require(text.text.calls.empty());
}

void Units() {
    Expect({}, {}, 0, 0, "mn", "mn");
    Expect({u'a', u'b'}, {u'a', u'b'}, 1, 0, "mnnn", "mnnn");
    Expect({u'a', u'b', u'c'}, {u'a', u'b', u'd'}, 0, -1, "mnnn", "mnnn");
    Expect({u'a', u'b'}, {u'a', u'b', u'c'}, 1, -1 - u'c', "mnnn", "mnnn");
    Expect({u'b'}, {u'a', u'z'}, 1, 1, "mn", "mn");
    Expect({0xff61}, {0xd800, 0xdc00}, 0, 0xff61 - 0xd800, "mn", "mn");
    Expect({0xdc00}, {0xd800, 0xdc00}, 0, 0x400, "mn", "mn");
}

void CodePoints() {
    Expect({0xff61}, {0xd800, 0xdc00}, 1, 0xff61 - 0x2800 - 0xd800, "mn", "mnc");
    Expect({0xd800, 0xdc00}, {0xff61}, 0x80, 0xd800 - (0xff61 - 0x2800), "mnc", "mn");
    Expect({0xdc00}, {0xd800, 0xdc00}, 1, 0xdc00 - 0x2800 - 0xd800, "mnpp", "mnc");
    Expect({0xd800, 0xdc00}, {0xd800, 0xdc01}, 1, -1, "mnnpp", "mnnpp");
    Expect({u'x', 0xdc00}, {u'x', 0xe000}, 1, 0xdc00 - 0xe000, "mnnpp", "mnn");
    Expect({0xd800}, {0xe000}, 1, -0x800, "mnc", "mn");
    Expect({0xd800, u'a'}, {0xdbff, 0xdfff}, 1, 0xd800 - 0x2800 - 0xdbff, "mnc", "mnc");
    Expect({0xffff}, {0x10000}, 1, 0xffff - 0x2800 - (0x10000 - 0x2800), "mn", "mn");
    Expect({u'a', 0xd7ff}, {u'a', 0xd800}, 1, -1, "mnn", "mnn");
    Expect({0xd800}, {}, 1, 0xd801, "mn", "mn");
}

void Wrapping() {
    Expect({std::numeric_limits<std::int32_t>::min()}, {1}, 0, std::numeric_limits<std::int32_t>::max(), "mn", "mn");
    Expect({std::numeric_limits<std::int32_t>::max()}, {-1}, 1, std::numeric_limits<std::int32_t>::min(), "mn", "mn");
}

void Callbacks() {
    for (int missing = 0; missing < 4; ++missing) {
        Iterator first({0xdc00});
        Iterator second({0xd800, 0xdc00});
        auto& target = missing < 2 ? first.iterator : second.iterator;
        if (missing % 2 == 0) target.move = nullptr; else target.next = nullptr;
        RequireInvalidArgument([&] { u_strCompareIter_nid_postfix(&first.iterator, &second.iterator, 1); });
    }
    {
        Iterator first({0xdc00});
        Iterator second({0xd800, 0xdc00});
        first.iterator.previous = nullptr;
        RequireInvalidArgument([&] { u_strCompareIter_nid_postfix(&first.iterator, &second.iterator, 1); });
    }
    {
        Iterator first({0xdc00});
        Iterator second({0xd800, 0xdc00});
        second.iterator.current = nullptr;
        RequireInvalidArgument([&] { u_strCompareIter_nid_postfix(&first.iterator, &second.iterator, 1); });
    }
    {
        Iterator first({0xdc00});
        Iterator second({0xd800, 0xdc00});
        first.iterator.current = nullptr;
        second.iterator.previous = nullptr;
        Require(u_strCompareIter_nid_postfix(&first.iterator, &second.iterator, 1) == 0xdc00 - 0x2800 - 0xd800);
        Require(first.text.calls == "mnpp");
        Require(second.text.calls == "mnc");
    }
    {
        Iterator first({u'a'});
        Iterator second({u'b'});
        first.iterator.current = nullptr;
        first.iterator.previous = nullptr;
        second.iterator.current = nullptr;
        second.iterator.previous = nullptr;
        Require(u_strCompareIter_nid_postfix(&first.iterator, &second.iterator, 1) == -1);
    }
}

void UnicodeVersion() {
    std::array<std::uint8_t, 6> version{};
    version.fill(0xa5);
    u_getUnicodeVersion_nid_postfix(version.data());
    Require(version == std::array<std::uint8_t, 6>{17, 0, 0, 0, 0xa5, 0xa5});
    u_getUnicodeVersion_nid_postfix(nullptr);
}

}

int main() {
    Arguments();
    Units();
    CodePoints();
    Wrapping();
    Callbacks();
    UnicodeVersion();
    return 0;
}
