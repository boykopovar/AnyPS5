#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using NKWebKit::MarkedTextAbi::MarkedText;
using NKWebKit::MarkedTextAbi::MarkedTextVector;
using NKWebKit::WtfAbi::StringImpl;

const auto Subdivide = &_ZN7WebCore9subdivideERKN3WTF6VectorINS_10MarkedTextELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEENS_15OverlapStrategyE;

constexpr std::uint32_t None = 0;
constexpr std::uint32_t Frontmost = 1;
constexpr std::uint32_t GrammarError = 1;
constexpr std::uint32_t SpellingError = 3;
constexpr std::uint32_t Selection = 7;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "MarkedText check failed at line %u\n", location.line());
    std::abort();
}

struct Item {
    unsigned start;
    unsigned end;
    std::uint32_t type;
    std::uintptr_t marker = 0;

    bool operator==(const Item&) const = default;
};

struct Output {
    std::vector<Item> items;
    unsigned capacity = 0;
    bool hasBuffer = false;
};

std::vector<MarkedText> ToAbi(const std::vector<Item>& items, StringImpl* highlightName = nullptr) {
    std::vector<MarkedText> texts;
    for (const Item& item : items) {
        MarkedText text{};
        text.startOffset = item.start;
        text.endOffset = item.end;
        text.type = item.type;
        text.marker = reinterpret_cast<const void*>(item.marker);
        text.highlightName = highlightName;
        texts.push_back(text);
    }
    return texts;
}

Output Run(const std::vector<Item>& items, std::uint32_t strategy, StringImpl* highlightName = nullptr) {
    std::vector<MarkedText> texts = ToAbi(items, highlightName);
    const MarkedTextVector input{texts.data(), static_cast<unsigned>(texts.size()), static_cast<unsigned>(texts.size())};
    MarkedTextVector result{reinterpret_cast<MarkedText*>(std::uintptr_t{0x1234}), 77, 99};
    Require(Subdivide(&result, &input, strategy) == &result);
    Output output;
    output.capacity = result.capacity;
    output.hasBuffer = result.buffer != nullptr;
    for (unsigned index = 0; index < result.size; ++index) {
        const MarkedText& text = result.buffer[index];
        Require(text.highlightName == highlightName);
        output.items.push_back({text.startOffset, text.endOffset, text.type, reinterpret_cast<std::uintptr_t>(text.marker)});
    }
    _ZN3WTF8fastFreeEPv(result.buffer);
    return output;
}

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

void TestEmpty() {
    for (const std::uint32_t strategy : {None, Frontmost}) {
        MarkedText unused{};
        for (MarkedText* buffer : {static_cast<MarkedText*>(nullptr), &unused}) {
            const MarkedTextVector input{buffer, buffer ? 1u : 0u, 0};
            MarkedTextVector result{reinterpret_cast<MarkedText*>(std::uintptr_t{0x1234}), 77, 99};
            Require(Subdivide(&result, &input, strategy) == &result);
            Require(result.buffer == nullptr && result.capacity == 0 && result.size == 0);
        }
    }
}

void TestWebKitCases() {
    const std::vector<Item> simple{{0, 9, SpellingError}};
    Require(Run(simple, None).items == simple);

    const std::vector<Item> separate{{7, 8, GrammarError, 0x1000}, {22, 32, SpellingError}};
    Require(Run(separate, None).items == separate);

    const std::vector<Item> spellingAndGrammar{{0, 40, GrammarError}, {2, 17, SpellingError}, {20, 40, SpellingError}, {41, 45, SpellingError}};
    Require(Run(spellingAndGrammar, None).items == (std::vector<Item>{{0, 2, GrammarError}, {2, 17, GrammarError}, {2, 17, SpellingError}, {17, 20, GrammarError},
                                                       {20, 40, GrammarError}, {20, 40, SpellingError}, {41, 45, SpellingError}}));
    Require(Run(spellingAndGrammar, Frontmost).items ==
            (std::vector<Item>{{0, 2, GrammarError}, {2, 17, SpellingError}, {17, 20, GrammarError}, {20, 40, SpellingError}, {41, 45, SpellingError}}));

    const std::vector<Item> complicated{{0, 6, SpellingError},   {0, 46, GrammarError},   {7, 16, SpellingError},  {22, 27, SpellingError},
                                        {34, 44, SpellingError}, {46, 50, SpellingError}, {51, 58, SpellingError}, {59, 63, GrammarError}};
    Require(Run(complicated, Frontmost).items ==
            (std::vector<Item>{{0, 6, SpellingError},  {6, 7, GrammarError},    {7, 16, SpellingError},  {16, 22, GrammarError},
                               {22, 27, SpellingError}, {27, 34, GrammarError},  {34, 44, SpellingError}, {44, 46, GrammarError},
                               {46, 50, SpellingError}, {51, 58, SpellingError}, {59, 63, GrammarError}}));

    const std::vector<Item> grammarAndSelection{{0, 40, GrammarError}, {2, 60, Selection}, {50, 60, GrammarError}};
    Require(Run(grammarAndSelection, None).items ==
            (std::vector<Item>{{0, 2, GrammarError}, {2, 40, GrammarError}, {2, 40, Selection}, {40, 50, Selection}, {50, 60, GrammarError}, {50, 60, Selection}}));
    Require(Run(grammarAndSelection, Frontmost).items == (std::vector<Item>{{0, 2, GrammarError}, {2, 40, Selection}, {40, 50, Selection}, {50, 60, Selection}}));
}

struct OrderCase {
    std::uint32_t strategy;
    std::vector<Item> input;
    std::vector<Item> expected;
};

void TestSortIndependentCases() {
    const std::vector<OrderCase> cases{
        {0, {{0, 5, 1, 7}, {0, 5, 1, 7}},
         {{0, 5, 1, 7}, {0, 5, 1, 7}}},
        {0, {{0, 9, 3, 1}, {2, 6, 4, 2}, {4, 12, 5, 3}},
         {{0, 2, 3, 1}, {2, 4, 3, 1}, {2, 4, 4, 2}, {4, 6, 3, 1}, {4, 6, 4, 2}, {4, 6, 5, 3}, {6, 9, 3, 1}, {6, 9, 5, 3}, {9, 12, 5, 3}}},
        {0, {{5, 0, 2, 1}, {1, 4, 1, 2}, {3, 8, 3, 3}},
         {{0, 1, 2, 1}, {1, 3, 1, 2}, {1, 3, 2, 1}, {3, 4, 1, 2}, {3, 4, 2, 1}, {3, 4, 3, 3}, {4, 5, 2, 1}, {4, 5, 3, 3}, {5, 8, 2, 1}, {5, 8, 2, 1}, {5, 8, 3, 3}}},
        {0, {{0, 5, 1, 1}, {5, 9, 1, 1}, {3, 5, 2, 2}, {5, 5, 2, 2}},
         {{0, 3, 1, 1}, {3, 5, 1, 1}, {3, 5, 2, 2}, {5, 9, 1, 1}}},
        {0, {{4, 2, 3, 1}, {1, 6, 2, 2}, {0, 3, 1, 3}, {3, 3, 1, 3}, {2, 6, 4, 4}},
         {{0, 1, 1, 3}, {1, 2, 1, 3}, {1, 2, 2, 2}, {2, 3, 1, 3}, {2, 3, 2, 2}, {2, 3, 4, 4}, {3, 4, 2, 2}, {3, 4, 4, 4}, {4, 6, 2, 2}, {4, 6, 4, 4}}},
        {1, {{0, 5, 1, 7}, {0, 5, 1, 7}},
         {{0, 5, 1, 7}}},
        {1, {{0, 9, 3, 1}, {2, 6, 3, 2}, {4, 12, 3, 3}},
         {{0, 2, 3, 1}, {2, 4, 3, 1}, {4, 6, 3, 1}, {6, 9, 3, 1}, {9, 12, 3, 3}}},
        {1, {{5, 0, 2, 1}, {1, 4, 1, 2}, {3, 8, 3, 3}},
         {{0, 1, 2, 1}, {1, 3, 2, 1}, {3, 4, 3, 3}, {4, 5, 3, 3}, {5, 8, 3, 3}}},
        {1, {{0, 5, 1, 1}, {5, 9, 1, 1}, {3, 5, 2, 2}, {5, 5, 2, 2}},
         {{0, 3, 1, 1}, {3, 5, 2, 2}, {5, 9, 1, 1}}},
        {1, {{4, 2, 3, 1}, {1, 6, 2, 2}, {0, 3, 1, 3}, {3, 3, 1, 3}, {2, 6, 4, 4}},
         {{0, 1, 1, 3}, {1, 2, 2, 2}, {2, 3, 4, 4}, {3, 4, 4, 4}, {4, 6, 4, 4}}},
        {0, {{1, 3, 3, 2}, {4, 4, 8, 1}, {5, 5, 8, 0}, {1, 2, 7, 5}, {3, 3, 6, 0}, {1, 3, 0, 1}, {5, 8, 4, 4}, {6, 9, 3, 2}, {2, 4, 8, 4}},
         {{1, 2, 0, 1}, {1, 2, 3, 2}, {1, 2, 7, 5}, {2, 3, 0, 1}, {2, 3, 3, 2}, {2, 3, 8, 4}, {3, 4, 8, 4}, {5, 6, 4, 4}, {6, 8, 3, 2}, {6, 8, 4, 4}, {8, 9, 3, 2}}},
        {1, {{1, 4, 1, 1}, {0, 2, 0, 2}, {0, 3, 1, 3}, {6, 6, 0, 4}, {6, 8, 2, 5}, {0, 3, 2, 6}, {1, 2, 2, 7}, {5, 8, 2, 8}, {4, 7, 0, 9}},
         {{0, 1, 2, 6}, {1, 2, 2, 6}, {2, 3, 2, 6}, {3, 4, 1, 1}, {4, 5, 0, 9}, {5, 6, 2, 8}, {6, 7, 2, 8}, {7, 8, 2, 8}}},
    };
    for (const OrderCase& test : cases) Require(Run(test.input, test.strategy).items == test.expected);
}

bool ThrowsNotImplemented(const std::vector<Item>& items, std::uint32_t strategy) {
    std::vector<MarkedText> texts = ToAbi(items);
    const MarkedTextVector input{texts.data(), static_cast<unsigned>(texts.size()), static_cast<unsigned>(texts.size())};
    MarkedText* const untouched = reinterpret_cast<MarkedText*>(std::uintptr_t{0x1234});
    MarkedTextVector result{untouched, 77, 99};
    try {
        Subdivide(&result, &input, strategy);
    } catch (const std::runtime_error&) {
        return result.buffer == untouched && result.capacity == 77 && result.size == 99;
    } catch (...) {
        return false;
    }
    _ZN3WTF8fastFreeEPv(result.buffer);
    return false;
}

void TestSortDependentCases() {
    const std::vector<Item> twins{{0, 5, 1, 1}, {0, 5, 1, 2}};
    Require(ThrowsNotImplemented(twins, None));
    Require(ThrowsNotImplemented(twins, Frontmost));
    Require(ThrowsNotImplemented({{0, 9, 1, 1}, {4, 6, 1, 2}}, None));
    Require(Run({{0, 9, 1, 1}, {4, 6, 1, 2}}, Frontmost).items == (std::vector<Item>{{0, 4, 1, 1}, {4, 6, 1, 1}, {6, 9, 1, 1}}));
    Require(Run({{0, 9, 1, 1}, {4, 6, 2, 2}}, None).items == (std::vector<Item>{{0, 4, 1, 1}, {4, 6, 1, 1}, {4, 6, 2, 2}, {6, 9, 1, 1}}));
    Require(ThrowsNotImplemented({{2, 9, 1, 1}, {2, 6, 1, 2}, {0, 9, 0, 3}}, Frontmost));
    Require(Run({{2, 9, 1, 1}, {2, 6, 1, 2}, {0, 9, 5, 3}}, Frontmost).items == (std::vector<Item>{{0, 2, 5, 3}, {2, 6, 5, 3}, {6, 9, 5, 3}}));

    Require(ThrowsNotImplemented({{0, 0, 2, 1}, {0, 1, 8, 2}}, None));
    Require(ThrowsNotImplemented({{0, 0, 2, 1}, {0, 1, 8, 2}}, Frontmost));
    Require(Run({{1, 1, 2, 1}, {0, 2, 8, 2}}, None).items == (std::vector<Item>{{0, 1, 8, 2}, {1, 2, 8, 2}}));
    Require(ThrowsNotImplemented({{5, 0, 1, 1}, {6, 0, 1, 2}}, None));
    Require(Run({{5, 0, 1, 1}, {6, 0, 2, 2}}, Frontmost).items == (std::vector<Item>{{0, 5, 2, 2}, {5, 6, 2, 2}}));

    const std::vector<Item> plain{{0, 9, 3}};
    Require(ThrowsNotImplemented(plain, 2));
    Require(ThrowsNotImplemented(plain, 0xFFFFFFFFu));
}

void TestCapacity() {
    const Output single = Run({{0, 9, SpellingError}}, None);
    Require(single.items.size() == 1 && single.capacity == 1 && single.hasBuffer);

    const Output nothing = Run({{5, 5, 1}, {5, 5, 2}, {5, 5, 3}}, None);
    Require(nothing.items.empty() && nothing.capacity == 3 && nothing.hasBuffer);

    const Output four = Run({{0, 10, 1}, {1, 9, 2}, {2, 8, 3}, {3, 7, 4}}, None);
    Require(four.items.size() == 16 && four.capacity == 16);

    const Output five = Run({{0, 10, 1}, {1, 9, 2}, {2, 8, 3}, {3, 7, 4}, {4, 6, 5}}, None);
    Require(five.items.size() == 25 && five.capacity == 27);

    const Output frontmost = Run({{0, 10, 1}, {1, 9, 2}, {2, 8, 3}, {3, 7, 4}, {4, 6, 5}}, Frontmost);
    Require(frontmost.items.size() == 9 && frontmost.capacity == 16);
    Require(frontmost.items[4] == (Item{4, 6, 5}));
}

void TestHighlightName() {
    const std::string name = "highlight";
    StringImpl impl{2, static_cast<unsigned>(name.size()), name.data(), 4};
    const Output output = Run({{0, 10, 6}, {5, 15, 6}}, None, &impl);
    Require(output.items.size() == 4);
    Require(impl.refCount == 2 + 2 * 4);
    impl.refCount = 2;
    Require(Run({{0, 10, 6}, {5, 15, 6}}, Frontmost, &impl).items.size() == 3);
    Require(impl.refCount == 2 + 2 * 3);
}

void TestArguments() {
    MarkedText text{};
    const MarkedTextVector input{&text, 1, 1};
    const MarkedTextVector broken{nullptr, 0, 1};
    MarkedTextVector result{};
    Require(ThrowsInvalidArgument([&] { Subdivide(nullptr, &input, None); }));
    Require(ThrowsInvalidArgument([&] { Subdivide(&result, nullptr, None); }));
    Require(ThrowsInvalidArgument([&] { Subdivide(&result, &broken, None); }));
}

}

int main() {
    TestEmpty();
    TestWebKitCases();
    TestSortIndependentCases();
    TestSortDependentCases();
    TestCapacity();
    TestHighlightName();
    TestArguments();
    return 0;
}
