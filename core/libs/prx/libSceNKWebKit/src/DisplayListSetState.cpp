// Derived from Source/WebCore/platform/graphics/displaylists/DisplayListItems.h and DisplayListItems.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/DisplayListSetStateExports.hpp"

namespace {

using NKWebKit::DisplayListAbi::ApplyWithoutGraphicsContext;
using NKWebKit::DisplayListAbi::ConstructItem;
using NKWebKit::DisplayListAbi::DeleteItem;
using NKWebKit::DisplayListAbi::DestroyItem;
using NKWebKit::DisplayListAbi::IsNotDrawingItem;
using NKWebKit::DisplayListAbi::ItemType;
using NKWebKit::DisplayListAbi::ItemVtable;
using NKWebKit::DisplayListSetStateAbi::SetState;
using NKWebKit::DisplayListSetStateAbi::State;
using NKWebKit::DisplayListSetStateAbi::StateChange;

constexpr std::uint64_t OutOfLineFlag = 1ULL << 51;

bool IsOutOfLine(const WebCore::Color& color) {
    return (color.m_colorAndFlags & OutOfLineFlag) != 0;
}

bool HoldsReference(const State& state) {
    return state.strokeGradient != nullptr || state.strokePattern != nullptr ||
           state.fillGradient != nullptr || state.fillPattern != nullptr ||
           IsOutOfLine(state.strokeColor) || IsOutOfLine(state.fillColor) || IsOutOfLine(state.shadowColor);
}

void CheckRepresentable(const State& state) {
    if (HoldsReference(state))
        throw std::invalid_argument("WebCore::DisplayList::SetState cannot retain a gradient, a pattern or an out-of-line colour");
}

SetState& Checked(SetState* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires an object");
    return *self;
}

void APS5_VABI SetStateCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList8SetStateD1Ev(static_cast<SetState*>(self));
}

void APS5_VABI SetStateDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList8SetStateD0Ev(static_cast<SetState*>(self));
}

const ItemVtable SetStateTable{SetStateCompleteDestructor, SetStateDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructSetState(SetState& self, const State* state, const std::uint32_t changeFlags) {
    if (!state) throw std::invalid_argument("WebCore::DisplayList::SetState::SetState requires a state");
    CheckRepresentable(*state);
    ConstructItem(self.item, &SetStateTable, ItemType::SetState);
    self.stateChange.state = *state;
    self.stateChange.changeFlags = changeFlags;
}

void ConstructSetState(SetState& self, const StateChange* change) {
    if (!change) throw std::invalid_argument("WebCore::DisplayList::SetState::SetState requires a state change");
    CheckRepresentable(change->state);
    ConstructItem(self.item, &SetStateTable, ItemType::SetState);
    self.stateChange = *change;
}

void DestroySetState(SetState& self) {
    DestroyItem(self.item);
}

}

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList8SetStateC1ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(SetState* self, const State* state, std::uint32_t changeFlags) {
    ConstructSetState(Checked(self, "WebCore::DisplayList::SetState::SetState"), state, changeFlags);
}

void APS5_VABI _ZN7WebCore11DisplayList8SetStateC2ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(SetState* self, const State* state, std::uint32_t changeFlags) {
    ConstructSetState(Checked(self, "WebCore::DisplayList::SetState::SetState"), state, changeFlags);
}

void APS5_VABI _ZN7WebCore11DisplayList8SetStateC1ERKNS_26GraphicsContextStateChangeE(SetState* self, const StateChange* change) {
    ConstructSetState(Checked(self, "WebCore::DisplayList::SetState::SetState"), change);
}

void APS5_VABI _ZN7WebCore11DisplayList8SetStateC2ERKNS_26GraphicsContextStateChangeE(SetState* self, const StateChange* change) {
    ConstructSetState(Checked(self, "WebCore::DisplayList::SetState::SetState"), change);
}

void APS5_VABI _ZN7WebCore11DisplayList8SetStateD1Ev(SetState* self) {
    DestroySetState(Checked(self, "WebCore::DisplayList::SetState::~SetState"));
}

void APS5_VABI _ZN7WebCore11DisplayList8SetStateD2Ev(SetState* self) {
    DestroySetState(Checked(self, "WebCore::DisplayList::SetState::~SetState"));
}

void APS5_VABI _ZN7WebCore11DisplayList8SetStateD0Ev(SetState* self) {
    auto& item = Checked(self, "WebCore::DisplayList::SetState::~SetState");
    DestroySetState(item);
    DeleteItem(&item);
}

}
