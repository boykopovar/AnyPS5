#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "Testing.hpp"
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

LibcCollationState* Read(Getter getter, int error) {
    errno = error;
    auto* state = getter();
    Require(errno == error);
    Require(state != nullptr);
    return state;
}

}

int main() {
    auto* narrow = Read(_Getpcostate_nid_postfix, 71);
    auto* wide = Read(_Getpwcostate_nid_postfix, 72);
    Require(narrow != wide);
    for (std::size_t index = 0; index < 16; ++index) {
        Require(narrow->tables[index] == nullptr);
        Require(wide->tables[index] == nullptr);
    }

    std::array<std::uint16_t, 16> narrowWeights {};
    std::array<std::uint16_t, 16> wideWeights {};
    for (std::size_t index = 0; index < 16; ++index) {
        narrowWeights[index] = static_cast<std::uint16_t>(0x100 + index);
        wideWeights[index] = static_cast<std::uint16_t>(0x200 + index);
        narrow->tables[index] = &narrowWeights[index];
        Require(wide->tables[index] == nullptr);
        wide->tables[index] = &wideWeights[index];
    }
    Require(Read(_Getpcostate_nid_postfix, 73) == narrow);
    Require(Read(_Getpwcostate_nid_postfix, 74) == wide);

    std::thread first([&] {
        auto* threadNarrow = Read(_Getpcostate_nid_postfix, 75);
        auto* threadWide = Read(_Getpwcostate_nid_postfix, 76);
        Require(threadNarrow == narrow && threadWide == wide);
        for (std::size_t index = 0; index < 16; ++index) {
            Require(threadNarrow->tables[index] == &narrowWeights[index]);
            Require(threadWide->tables[index] == &wideWeights[index]);
            Require(*threadNarrow->tables[index] == 0x100 + index);
            Require(*threadWide->tables[index] == 0x200 + index);
        }
        threadNarrow->tables[15] = nullptr;
    });
    first.join();
    Require(narrow->tables[15] == nullptr);
    Require(wide->tables[15] == &wideWeights[15]);

    std::thread second([&] {
        Require(Read(_Getpcostate_nid_postfix, 77) == narrow);
        Require(Read(_Getpcostate_nid_postfix, 78)->tables[15] == nullptr);
        Read(_Getpwcostate_nid_postfix, 79)->tables[15] = nullptr;
    });
    second.join();
    Require(wide->tables[15] == nullptr);
    for (std::size_t index = 0; index < 16; ++index) {
        narrow->tables[index] = nullptr;
        wide->tables[index] = nullptr;
    }
    std::puts("collation state checks passed");
}
