#include "prx/libSceIcu/ClassIds.hpp"

namespace {

char normalizerClassId;
char ruleBasedBreakIteratorClassId;

}

extern "C" {

void* APS5_VABI _ZN6icu_6810Normalizer16getStaticClassIDEv_nid_postfix() {
    return &normalizerClassId;
}

void* APS5_VABI _ZNK6icu_6810Normalizer17getDynamicClassIDEv_nid_postfix(const void*) {
    return _ZN6icu_6810Normalizer16getStaticClassIDEv_nid_postfix();
}

void* APS5_VABI _ZN6icu_6822RuleBasedBreakIterator16getStaticClassIDEv_nid_postfix() {
    return &ruleBasedBreakIteratorClassId;
}

}
