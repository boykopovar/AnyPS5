#include "prx/libSceNKWebKit/include/DisplayListSetStateExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::DisplayListAbi::ItemType;
using NKWebKit::DisplayListAbi::ItemVtable;
using NKWebKit::DisplayListSetStateAbi::SetState;
using NKWebKit::DisplayListSetStateAbi::State;
using NKWebKit::DisplayListSetStateAbi::StateChange;

void Require(const bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "DisplayList::SetState check failed at line %u\n", location.line());
        std::abort();
    }
}

State MakeState() {
    State state{};
    state.strokeThickness = 3.5f;
    state.shadowBlur = 4.5f;
    state.fillColor.m_colorAndFlags = 0x11223344ULL;
    state.strokeColor.m_colorAndFlags = 0x55667788ULL;
    state.alpha = 0.5f;
    state.compositeOperator = 6;
    state.blendMode = 2;
    return state;
}

void TestStateConstructor() {
    const State state = MakeState();
    alignas(8) unsigned char storage[sizeof(SetState)];
    auto* item = reinterpret_cast<SetState*>(storage);
    _ZN7WebCore11DisplayList8SetStateC1ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(item, &state, 0x1234);
    Require(item->item.type == static_cast<std::uint8_t>(ItemType::SetState));
    Require(item->item.refCount == 1);
    Require(item->stateChange.state.strokeThickness == 3.5f);
    Require(item->stateChange.state.shadowBlur == 4.5f);
    Require(item->stateChange.state.fillColor.m_colorAndFlags == 0x11223344ULL);
    Require(item->stateChange.state.alpha == 0.5f);
    Require(item->stateChange.state.compositeOperator == 6);
    Require(item->stateChange.changeFlags == 0x1234);

    const auto* table = static_cast<const ItemVtable*>(item->item.vptr);
    Require(!table->isDrawingItem(item));
    bool threw = false;
    try {
        table->apply(item, nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
    _ZN7WebCore11DisplayList8SetStateD1Ev(item);
}

void TestStateChangeConstructor() {
    StateChange change{};
    change.state = MakeState();
    change.changeFlags = 0x99;
    alignas(8) unsigned char storage[sizeof(SetState)];
    auto* item = reinterpret_cast<SetState*>(storage);
    _ZN7WebCore11DisplayList8SetStateC2ERKNS_26GraphicsContextStateChangeE(item, &change);
    Require(item->item.type == static_cast<std::uint8_t>(ItemType::SetState));
    Require(item->stateChange.state.strokeThickness == 3.5f);
    Require(item->stateChange.changeFlags == 0x99);
    _ZN7WebCore11DisplayList8SetStateD2Ev(item);

    auto* allocated = static_cast<SetState*>(_ZN3WTF10fastMallocEm(sizeof(SetState)));
    _ZN7WebCore11DisplayList8SetStateC1ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(allocated, &change.state, change.changeFlags);
    _ZN7WebCore11DisplayList8SetStateD0Ev(allocated);
}

void TestReferences() {
    const auto throws = [](const State& state, const bool throughChange) {
        alignas(8) unsigned char storage[sizeof(SetState)];
        auto* item = reinterpret_cast<SetState*>(storage);
        try {
            if (throughChange) {
                StateChange change{};
                change.state = state;
                _ZN7WebCore11DisplayList8SetStateC1ERKNS_26GraphicsContextStateChangeE(item, &change);
            } else {
                _ZN7WebCore11DisplayList8SetStateC1ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(item, &state, 0);
            }
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };

    for (const bool throughChange : {false, true}) {
        for (int index = 0; index < 4; ++index) {
            State state = MakeState();
            switch (index) {
            case 0: state.strokeGradient = &state; break;
            case 1: state.strokePattern = &state; break;
            case 2: state.fillGradient = &state; break;
            case 3: state.fillPattern = &state; break;
            }
            Require(throws(state, throughChange));
        }
        for (int index = 0; index < 3; ++index) {
            State state = MakeState();
            WebCore::Color& color = index == 0 ? state.strokeColor : index == 1 ? state.fillColor : state.shadowColor;
            color.m_colorAndFlags |= 1ULL << 51;
            Require(throws(state, throughChange));
        }
    }
}

void TestNull() {
    const State state = MakeState();
    bool threw = false;
    try {
        _ZN7WebCore11DisplayList8SetStateC1ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(nullptr, &state, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    alignas(8) unsigned char storage[sizeof(SetState)];
    try {
        _ZN7WebCore11DisplayList8SetStateC1ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(reinterpret_cast<SetState*>(storage), nullptr, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    try {
        TestStateConstructor();
        TestStateChangeConstructor();
        TestReferences();
        TestNull();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "DisplayList::SetState test failed: %s\n", error.what());
        return 1;
    }
    std::puts("DisplayList::SetState tests passed.");
    return 0;
}
