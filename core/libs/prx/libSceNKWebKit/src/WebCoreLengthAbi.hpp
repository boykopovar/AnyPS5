#pragma once

#include <cstddef>

namespace NKWebKit::CalculationAbi {

struct CalculationValue {
    unsigned m_refCount;
    void* m_expression;
    bool m_shouldClampToNonNegative;

    void ref();
    void deref();
};

inline constexpr unsigned NodeDestroySlot = 1;
inline constexpr unsigned NodeEvaluateSlot = 2;

extern unsigned nextAvailableHandle;

static_assert(sizeof(CalculationValue) == 24);
static_assert(offsetof(CalculationValue, m_refCount) == 0);
static_assert(offsetof(CalculationValue, m_expression) == 8);
static_assert(offsetof(CalculationValue, m_shouldClampToNonNegative) == 16);

}
