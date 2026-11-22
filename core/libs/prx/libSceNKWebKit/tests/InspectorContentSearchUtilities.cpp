#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "textPositionFromOffset check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

WTF::TextPosition Position(std::vector<std::size_t>& endings, std::size_t offset) {
    NKWebKit::WtfAbi::Vector<std::size_t> vector;
    vector.buffer = endings.data();
    vector.capacity = static_cast<unsigned>(endings.size());
    vector.size = static_cast<unsigned>(endings.size());
    const auto result = _ZN9Inspector22ContentSearchUtilities22textPositionFromOffsetEmRKN3WTF6VectorImLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(offset, vector);
    vector.buffer = nullptr;
    return result;
}

bool Is(WTF::TextPosition position, int line, int column) {
    return position.line == line && position.column == column;
}

}

int main() {
    std::vector<std::size_t> text{5, 11, 12, 20};
    Require(Is(Position(text, 0), 0, 0));
    Require(Is(Position(text, 4), 0, 4));
    Require(Is(Position(text, 5), 1, 0));
    Require(Is(Position(text, 6), 1, 1));
    Require(Is(Position(text, 11), 2, 0));
    Require(Is(Position(text, 12), 3, 0));
    Require(Is(Position(text, 19), 3, 7));
    Require(Is(Position(text, 20), 4, 0));
    Require(Is(Position(text, 25), 4, 5));

    std::vector<std::size_t> single{7};
    Require(Is(Position(single, 3), 0, 3));
    Require(Is(Position(single, 7), 1, 0));
    Require(Is(Position(single, 9), 1, 2));

    for (std::size_t count = 1; count <= 40; ++count) {
        std::vector<std::size_t> endings;
        for (std::size_t i = 0; i < count; ++i) endings.push_back(i * 3 + (i % 2));
        for (std::size_t offset = 0; offset <= endings.back() + 2; ++offset) {
            const auto line = static_cast<std::size_t>(std::upper_bound(endings.begin(), endings.end(), offset) - endings.begin());
            const std::size_t start = line ? endings[line - 1] : 0;
            Require(Is(Position(endings, offset), static_cast<int>(line), static_cast<int>(offset - start)));
        }
    }

    std::vector<std::size_t> empty;
    bool threw = false;
    try {
        Position(empty, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    Require(threw);

    std::puts("textPositionFromOffset tests passed");
    return 0;
}
