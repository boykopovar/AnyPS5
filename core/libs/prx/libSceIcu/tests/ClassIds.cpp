#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

}

int main() {
    void* normalizer = _ZN6icu_6810Normalizer16getStaticClassIDEv_nid_postfix();
    void* breakIterator = _ZN6icu_6822RuleBasedBreakIterator16getStaticClassIDEv_nid_postfix();
    Expect(normalizer != nullptr, "Normalizer::getStaticClassID is not null");
    Expect(breakIterator != nullptr, "RuleBasedBreakIterator::getStaticClassID is not null");
    Expect(normalizer != breakIterator, "class IDs differ between classes");
    for (int i = 0; i < 4; ++i) {
        Expect(_ZN6icu_6810Normalizer16getStaticClassIDEv_nid_postfix() == normalizer, "Normalizer::getStaticClassID is stable");
        Expect(_ZN6icu_6822RuleBasedBreakIterator16getStaticClassIDEv_nid_postfix() == breakIterator,
               "RuleBasedBreakIterator::getStaticClassID is stable");
    }
    alignas(16) unsigned char object[256] = {};
    const void* objects[] = {object, object + 64, nullptr};
    for (const void* self : objects) {
        Expect(_ZNK6icu_6810Normalizer17getDynamicClassIDEv_nid_postfix(self) == normalizer,
               "Normalizer::getDynamicClassID returns the static class ID");
    }
    std::puts("class id tests passed");
    return 0;
}
