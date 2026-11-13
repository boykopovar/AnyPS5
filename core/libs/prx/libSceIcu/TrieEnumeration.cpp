// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <stdexcept>

namespace {
constexpr std::int32_t UTRIE2_SHIFT_1 = 11;
constexpr std::int32_t UTRIE2_SHIFT_2 = 5;
constexpr std::int32_t UTRIE2_OMITTED_BMP_INDEX_1_LENGTH = 32;
constexpr std::int32_t UTRIE2_CP_PER_INDEX_1_ENTRY = 2048;
constexpr std::int32_t UTRIE2_INDEX_2_BLOCK_LENGTH = 64;
constexpr std::int32_t UTRIE2_INDEX_2_MASK = 63;
constexpr std::int32_t UTRIE2_DATA_BLOCK_LENGTH = 32;
constexpr std::int32_t UTRIE2_INDEX_SHIFT = 2;
constexpr std::int32_t UTRIE2_DATA_GRANULARITY = 4;
constexpr std::int32_t UTRIE2_LSCP_INDEX_2_OFFSET = 2048;
constexpr std::int32_t UTRIE2_INDEX_1_OFFSET = 2112;

std::uint32_t APS5_VABI EnumSameValue(const void*, std::uint32_t value) {
    return value;
}

void EnumEitherTrie(const Icu::Trie2* trie,
                    std::int32_t start, std::int32_t limit,
                    Icu::Trie2EnumValue enumValue, Icu::Trie2EnumRange enumRange, const void* context) {
    if (!trie) throw std::invalid_argument("ICU trie2: null trie");
    if (!enumRange) return;
    if (!enumValue) enumValue = EnumSameValue;

    const std::uint32_t* data32 = nullptr;
    const std::uint16_t* idx = nullptr;
    std::int32_t index2NullOffset = 0;
    std::int32_t nullBlock = 0;

    if (!trie->builder) {
        idx = trie->index;
        data32 = trie->data32;
        index2NullOffset = trie->index2NullOffset;
        nullBlock = trie->dataNullOffset;
    } else {
        idx = nullptr;
        data32 = trie->builder->data;
        index2NullOffset = trie->builder->index2NullOffset;
        nullBlock = trie->builder->dataNullOffset;
    }

    const std::int32_t highStart = trie->highStart;
    const std::uint32_t initialValue = enumValue(context, trie->initialValue);

    std::int32_t prevI2Block = -1;
    std::int32_t prevBlock = -1;
    std::int32_t prev = start;
    std::uint32_t prevValue = 0;
    std::int32_t c = start;

    for (; c < limit && c < highStart;) {
        std::int32_t tempLimit = c + UTRIE2_CP_PER_INDEX_1_ENTRY;
        if (limit < tempLimit) tempLimit = limit;
        std::int32_t i2Block = 0;
        if (c <= 0xffff) {
            if (c < 0xd800 || c > 0xdfff) {
                i2Block = c >> UTRIE2_SHIFT_2;
            } else if (c <= 0xdbff) {
                i2Block = UTRIE2_LSCP_INDEX_2_OFFSET;
                tempLimit = std::min(0xdc00, limit);
            } else {
                i2Block = 0xd800 >> UTRIE2_SHIFT_2;
                tempLimit = std::min(0xe000, limit);
            }
        } else {
            if (idx) {
                i2Block = idx[(UTRIE2_INDEX_1_OFFSET - UTRIE2_OMITTED_BMP_INDEX_1_LENGTH) + (c >> UTRIE2_SHIFT_1)];
            } else {
                i2Block = trie->builder->index1[c >> UTRIE2_SHIFT_1];
            }
            if (i2Block == prevI2Block && (c - prev) >= UTRIE2_CP_PER_INDEX_1_ENTRY) {
                c += UTRIE2_CP_PER_INDEX_1_ENTRY;
                continue;
            }
        }
        prevI2Block = i2Block;
        if (i2Block == index2NullOffset) {
            if (prevValue != initialValue) {
                if (prev < c && !enumRange(context, prev, c - 1, prevValue)) return;
                prevBlock = nullBlock;
                prev = c;
                prevValue = initialValue;
            }
            c += UTRIE2_CP_PER_INDEX_1_ENTRY;
        } else {
            std::int32_t i2 = (c >> UTRIE2_SHIFT_2) & UTRIE2_INDEX_2_MASK;
            std::int32_t i2Limit = 0;
            if ((c >> UTRIE2_SHIFT_1) == (tempLimit >> UTRIE2_SHIFT_1)) {
                i2Limit = (tempLimit >> UTRIE2_SHIFT_2) & UTRIE2_INDEX_2_MASK;
            } else {
                i2Limit = UTRIE2_INDEX_2_BLOCK_LENGTH;
            }
            for (; i2 < i2Limit; ++i2) {
                std::int32_t block = 0;
                if (idx) {
                    block = static_cast<std::int32_t>(idx[i2Block + i2]) << UTRIE2_INDEX_SHIFT;
                } else {
                    block = trie->builder->index2[i2Block + i2];
                }
                if (block == prevBlock && (c - prev) >= UTRIE2_DATA_BLOCK_LENGTH) {
                    c += UTRIE2_DATA_BLOCK_LENGTH;
                    continue;
                }
                prevBlock = block;
                if (block == nullBlock) {
                    if (prevValue != initialValue) {
                        if (prev < c && !enumRange(context, prev, c - 1, prevValue)) return;
                        prev = c;
                        prevValue = initialValue;
                    }
                    c += UTRIE2_DATA_BLOCK_LENGTH;
                } else {
                    for (std::int32_t j = 0; j < UTRIE2_DATA_BLOCK_LENGTH; ++j) {
                        const std::uint32_t value = enumValue(context, data32 ? data32[block + j] : idx[block + j]);
                        if (value != prevValue) {
                            if (prev < c && !enumRange(context, prev, c - 1, prevValue)) return;
                            prev = c;
                            prevValue = value;
                        }
                        ++c;
                    }
                }
            }
        }
    }

    if (c > limit) {
        c = limit;
    } else if (c < limit) {
        std::uint32_t highValue = 0;
        if (idx) {
            highValue = data32 ? data32[trie->highValueIndex] : idx[trie->highValueIndex];
        } else {
            highValue = trie->builder->data[trie->builder->dataLength - UTRIE2_DATA_GRANULARITY];
        }
        const std::uint32_t value = enumValue(context, highValue);
        if (value != prevValue) {
            if (prev < c && !enumRange(context, prev, c - 1, prevValue)) return;
            prev = c;
            prevValue = value;
        }
        c = limit;
    }

    enumRange(context, prev, c - 1, prevValue);
}
}

extern "C" {
void APS5_VABI utrie2_enum_nid_postfix(const Icu::Trie2* trie, Icu::Trie2EnumValue enumValue, Icu::Trie2EnumRange enumRange, const void* context) {
    EnumEitherTrie(trie, 0, 0x110000, enumValue, enumRange, context);
}

void APS5_VABI utrie2_enumForLeadSurrogate_nid_postfix(const Icu::Trie2* trie, std::int32_t lead, Icu::Trie2EnumValue enumValue, Icu::Trie2EnumRange enumRange, const void* context) {
    if (lead < 0xd800 || lead > 0xdbff) return;
    lead = (lead - 0xd7c0) << 10;
    EnumEitherTrie(trie, lead, lead + 0x400, enumValue, enumRange, context);
}
}
