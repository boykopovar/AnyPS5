#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <type_traits>

namespace {

using Getter = LibcCollationState* (APS5_VABI *)();

static_assert(sizeof(LibcCollationState) == 128);
static_assert(offsetof(LibcCollationState, tables) == 0);
static_assert(std::is_standard_layout_v<LibcCollationState>);
static_assert(std::is_same_v<decltype(&_Getpcostate_nid_postfix), Getter>);
static_assert(std::is_same_v<decltype(&_Getpwcostate_nid_postfix), Getter>);

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "collation state check failed at line %d\n", line);
    std::abort();
}
#define REQUIRE(value) Require((value), __LINE__)

LibcCollationState* Read(Getter getter, int error) {
    errno = error;
    auto* state = getter();
    REQUIRE(errno == error);
    REQUIRE(state != nullptr);
    return state;
}

}

int main() {
    auto* narrow = Read(_Getpcostate_nid_postfix, 71);
    auto* wide = Read(_Getpwcostate_nid_postfix, 72);
    REQUIRE(narrow != wide);
    for (std::size_t index = 0; index < 16; ++index) {
        REQUIRE(narrow->tables[index] == nullptr);
        REQUIRE(wide->tables[index] == nullptr);
    }

    std::array<std::uint16_t, 16> narrowWeights {};
    std::array<std::uint16_t, 16> wideWeights {};
    for (std::size_t index = 0; index < 16; ++index) {
        narrowWeights[index] = static_cast<std::uint16_t>(0x100 + index);
        wideWeights[index] = static_cast<std::uint16_t>(0x200 + index);
        narrow->tables[index] = &narrowWeights[index];
        REQUIRE(wide->tables[index] == nullptr);
        wide->tables[index] = &wideWeights[index];
    }
    REQUIRE(Read(_Getpcostate_nid_postfix, 73) == narrow);
    REQUIRE(Read(_Getpwcostate_nid_postfix, 74) == wide);

    std::thread first([&] {
        auto* threadNarrow = Read(_Getpcostate_nid_postfix, 75);
        auto* threadWide = Read(_Getpwcostate_nid_postfix, 76);
        REQUIRE(threadNarrow == narrow && threadWide == wide);
        for (std::size_t index = 0; index < 16; ++index) {
            REQUIRE(threadNarrow->tables[index] == &narrowWeights[index]);
            REQUIRE(threadWide->tables[index] == &wideWeights[index]);
            REQUIRE(*threadNarrow->tables[index] == 0x100 + index);
            REQUIRE(*threadWide->tables[index] == 0x200 + index);
        }
        threadNarrow->tables[15] = nullptr;
    });
    first.join();
    REQUIRE(narrow->tables[15] == nullptr);
    REQUIRE(wide->tables[15] == &wideWeights[15]);

    std::thread second([&] {
        REQUIRE(Read(_Getpcostate_nid_postfix, 77) == narrow);
        REQUIRE(Read(_Getpcostate_nid_postfix, 78)->tables[15] == nullptr);
        Read(_Getpwcostate_nid_postfix, 79)->tables[15] = nullptr;
    });
    second.join();
    REQUIRE(wide->tables[15] == nullptr);
    for (std::size_t index = 0; index < 16; ++index) {
        narrow->tables[index] = nullptr;
        wide->tables[index] = nullptr;
    }
    std::puts("collation state checks passed");
}
