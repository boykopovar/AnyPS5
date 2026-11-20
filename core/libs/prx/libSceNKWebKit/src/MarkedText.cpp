// Derived from Source/WebCore/rendering/MarkedText.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/MarkedTextExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::MarkedTextAbi::MarkedText;
using NKWebKit::MarkedTextAbi::MarkedTextVector;

constexpr unsigned MinimumCapacity = 16;
constexpr unsigned MaximumCapacity = std::numeric_limits<unsigned>::max() / sizeof(MarkedText);

struct Offset {
    enum Kind { Begin, End };
    Kind kind;
    unsigned value;
    const MarkedText* markedText;
};

struct Results {
    std::vector<MarkedText> items;
    unsigned capacity;

    void Append(const MarkedText& item) {
        if (items.size() == capacity) {
            const std::size_t current = capacity;
            const std::size_t grown = std::max(current + 1, std::max<std::size_t>(MinimumCapacity, current + current / 4 + 1));
            if (grown > MaximumCapacity) throw std::bad_alloc();
            capacity = static_cast<unsigned>(grown);
        }
        items.push_back(item);
    }
};

bool SameSource(const MarkedText& a, const MarkedText& b) {
    return a.marker == b.marker && a.highlightName == b.highlightName;
}

MarkedText Piece(unsigned start, unsigned end, const MarkedText& source) {
    MarkedText item{};
    item.startOffset = start;
    item.endOffset = end;
    item.type = source.type;
    item.marker = source.marker;
    item.highlightName = source.highlightName;
    return item;
}

}

extern "C" {

MarkedTextVector* APS5_VABI _ZN7WebCore9subdivideERKN3WTF6VectorINS_10MarkedTextELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEENS_15OverlapStrategyE(MarkedTextVector* result, const MarkedTextVector* markedTexts, std::uint32_t overlapStrategy) {
    if (!result) throw std::invalid_argument("WebCore::subdivide: null result");
    if (!markedTexts) throw std::invalid_argument("WebCore::subdivide: null vector");
    if (markedTexts->size && !markedTexts->buffer) throw std::invalid_argument("WebCore::subdivide: vector with a null buffer and a nonzero size");
    if (!markedTexts->size) {
        *result = MarkedTextVector{nullptr, 0, 0};
        return result;
    }
    const bool frontmostOnly = overlapStrategy == NKWebKit::MarkedTextAbi::OverlapStrategyFrontmost;
    if (!frontmostOnly && overlapStrategy != NKWebKit::MarkedTextAbi::OverlapStrategyNone) NotImplemented_nid_no_patch(__func__);
    const MarkedText* const base = markedTexts->buffer;
    const unsigned numberOfMarkedTexts = markedTexts->size;
    if (numberOfMarkedTexts > MaximumCapacity) throw std::bad_alloc();
    const unsigned numberOfOffsets = 2 * numberOfMarkedTexts;

    std::vector<Offset> offsets;
    offsets.reserve(numberOfOffsets);
    for (unsigned index = 0; index < numberOfMarkedTexts; ++index) {
        offsets.push_back({Offset::Begin, base[index].startOffset, &base[index]});
        offsets.push_back({Offset::End, base[index].endOffset, &base[index]});
    }
    std::stable_sort(offsets.begin(), offsets.end(), [](const Offset& a, const Offset& b) {
        if (a.value != b.value) return a.value < b.value;
        if (a.kind != b.kind) return a.kind == Offset::Begin;
        return a.kind == Offset::Begin ? a.markedText->type < b.markedText->type : a.markedText->type > b.markedText->type;
    });

    unsigned firstGroupEnd = 1;
    while (firstGroupEnd < numberOfOffsets && offsets[firstGroupEnd].value == offsets[0].value) ++firstGroupEnd;
    unsigned firstEnd = 0;
    while (firstEnd < firstGroupEnd && offsets[firstEnd].kind == Offset::Begin) ++firstEnd;
    if (firstEnd < firstGroupEnd && firstGroupEnd < numberOfOffsets) {
        if (firstEnd) NotImplemented_nid_no_patch(__func__);
        if (firstGroupEnd > 1 && offsets[1].markedText->type == offsets[0].markedText->type) NotImplemented_nid_no_patch(__func__);
    }

    Results results{{}, numberOfMarkedTexts};
    std::vector<bool> processed(numberOfMarkedTexts, false);
    const auto isProcessed = [&](const Offset& offset) { return processed[static_cast<std::size_t>(offset.markedText - base)]; };

    unsigned offsetSoFar = offsets[0].value;
    for (unsigned i = 1; i < numberOfOffsets; ++i) {
        if (offsets[i].value > offsets[i - 1].value) {
            if (frontmostOnly) {
                const Offset* frontmost = nullptr;
                for (unsigned j = 0; j < i; ++j) {
                    if (!isProcessed(offsets[j]) && (!frontmost || offsets[j].markedText->type > frontmost->markedText->type)) frontmost = &offsets[j];
                }
                for (unsigned j = 0; frontmost && j < i; ++j) {
                    const Offset& rival = offsets[j];
                    if (isProcessed(rival) || rival.value != frontmost->value || rival.kind != frontmost->kind) continue;
                    if (rival.markedText->type == frontmost->markedText->type && !SameSource(*rival.markedText, *frontmost->markedText)) NotImplemented_nid_no_patch(__func__);
                }
                if (frontmost) results.Append(Piece(offsetSoFar, offsets[i].value, *frontmost->markedText));
            } else {
                for (unsigned j = 0; j < i; ++j) {
                    if (!isProcessed(offsets[j])) results.Append(Piece(offsetSoFar, offsets[i].value, *offsets[j].markedText));
                }
            }
            offsetSoFar = offsets[i].value;
        }
        if (offsets[i].kind == Offset::End) processed[static_cast<std::size_t>(offsets[i].markedText - base)] = true;
    }

    if (!frontmostOnly) {
        std::stable_sort(results.items.begin(), results.items.end(),
                         [](const MarkedText& a, const MarkedText& b) { return a.startOffset < b.startOffset || (a.startOffset == b.startOffset && a.type < b.type); });
        for (std::size_t index = 1; index < results.items.size(); ++index) {
            const MarkedText& previous = results.items[index - 1];
            const MarkedText& current = results.items[index];
            if (previous.startOffset == current.startOffset && previous.type == current.type && !SameSource(previous, current)) NotImplemented_nid_no_patch(__func__);
        }
    }

    auto* buffer = static_cast<MarkedText*>(_ZN3WTF10fastMallocEm(static_cast<std::size_t>(results.capacity) * sizeof(MarkedText)));
    if (!results.items.empty()) std::memcpy(buffer, results.items.data(), results.items.size() * sizeof(MarkedText));
    for (const MarkedText& item : results.items) {
        if (item.highlightName) item.highlightName->refCount += 2;
    }
    *result = MarkedTextVector{buffer, results.capacity, static_cast<unsigned>(results.items.size())};
    return result;
}

}
