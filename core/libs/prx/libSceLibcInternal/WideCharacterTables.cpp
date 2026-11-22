#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include <cstddef>
#include <cstdint>

namespace {

struct TableEntry {
    const void* data;
    std::int64_t offset;
};

static_assert(sizeof(TableEntry) == 16);
static_assert(offsetof(TableEntry, offset) == 8);

constexpr std::uint16_t ClassificationRanges[] = {
    'A', 'Z', 1, 'a', 'z', 1, '0', '9', 1,
    'A', 'Z', 1, 'a', 'z', 1,
    0, 31, 1, 127, 127, 1,
    '0', '9', 1,
    '!', '~', 1,
    'a', 'z', 1,
    ' ', '~', 1,
    '!', '/', 1, ':', '@', 1, '[', '`', 1, '{', '~', 1,
    ' ', ' ', 1, '\t', '\r', 1,
    'A', 'Z', 1,
    '0', '9', 1, 'a', 'f', 1, 'A', 'F', 1,
    ' ', ' ', 1, '\t', '\t', 1,
};

constexpr TableEntry ClassificationTable[] = {
    {ClassificationRanges, 0},
    {"alnum", 0}, {"alpha", 9}, {"cntrl", 15}, {"digit", 21},
    {"graph", 24}, {"lower", 27}, {"print", 30}, {"punct", 33},
    {"space", 45}, {"upper", 51}, {"xdigit", 54}, {"blank", 63},
    {nullptr, 69},
};

constexpr std::uint16_t TranslationRanges[] = {
    'A', 'Z', 'a' - 'A',
    'a', 'z', static_cast<std::uint16_t>('A' - 'a'),
};

constexpr TableEntry TranslationTable[] = {
    {TranslationRanges, 0}, {"tolower", 0}, {"toupper", 3}, {nullptr, 6},
};

}

extern "C" {

const void* APS5_VABI _Getpwctrtab_nid_postfix() {
    return TranslationTable;
}

const void* APS5_VABI _Getpwctytab_nid_postfix() {
    return ClassificationTable;
}

}
