// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}

int main() {
    Icu::Abi::ParsePosition pos{};
    pos.index = 42;
    pos.errorIndex = -1;
    void* staticId = _ZN6icu_6813ParsePosition16getStaticClassIDEv_nid_postfix();
    void* dynamicId = _ZNK6icu_6813ParsePosition17getDynamicClassIDEv_nid_postfix(&pos);
    Check(staticId != nullptr && staticId == dynamicId);

    auto* clone = _ZNK6icu_6813ParsePosition5cloneEv_nid_postfix(&pos);
    Check(clone != nullptr);
    Check(clone->index == 42);
    Check(clone->errorIndex == -1);
    Check(_ZNK6icu_6813ParsePosition17getDynamicClassIDEv_nid_postfix(clone) == staticId);

    _ZN6icu_6813ParsePositionD0Ev_nid_postfix(clone);
    _ZN6icu_6813ParsePositionD1Ev_nid_postfix(&pos);

    try {
        _ZNK6icu_6813ParsePosition5cloneEv_nid_postfix(nullptr);
        std::abort();
    } catch (const std::invalid_argument&) {}

    std::puts("ICU ParsePosition tests passed");
    return 0;
}
