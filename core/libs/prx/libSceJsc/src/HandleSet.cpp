/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/JavaScriptCore/heap/HandleSet.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/HandleSetExports.hpp"

#include <stdexcept>

namespace {

using Jsc::HandleAbi::HandleNode;
using Jsc::HandleAbi::HandleNodeList;

constexpr std::uint64_t NotCellMask = 0xfffe000000000000ull | 0x2;

bool IsCell(std::uint64_t value) {
    return !(value & NotCellMask);
}

void Remove(HandleNode* node) {
    HandleNode* prev = node->prev;
    HandleNode* next = node->next;
    prev->next = next;
    next->prev = prev;
    node->prev = nullptr;
    node->next = nullptr;
}

void Push(HandleNodeList& list, HandleNode* node) {
    HandleNode* prev = &list.headSentinel;
    HandleNode* next = list.headSentinel.next;
    node->prev = prev;
    node->next = next;
    prev->next = node;
    next->prev = node;
}

} // namespace

extern "C" {

void APS5_VABI _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(Jsc::HandleAbi::HandleSet* self, std::uint64_t* slot, const std::uint64_t* value) {
    if (!self || !slot || !value) throw std::invalid_argument("JSC::HandleSet::writeBarrier requires a handle set, slot and value");
    const std::uint64_t newValue = *value;
    if (!newValue == !*slot && IsCell(*slot) == IsCell(newValue)) return;

    HandleNode* node = reinterpret_cast<HandleNode*>(slot);
    if (!node->prev || !node->next) throw std::invalid_argument("JSC::HandleSet::writeBarrier: handle is not in a list");
    Remove(node);
    Push(!newValue || !IsCell(newValue) ? self->immediateList : self->strongList, node);
}

}
