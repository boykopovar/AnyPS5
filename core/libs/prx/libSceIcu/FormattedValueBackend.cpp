#include "prx/libSceIcu/FormattedValueBackend.hpp"
#include "unicode/uformattedvalue.h"
#include <cstdint>
#include <mutex>
#include <unordered_set>

namespace {

std::mutex g_formattedValueMutex;
std::unordered_set<const void*> g_formattedValues;

}

namespace Icu::FormattedValueBackend {

void Register(const void* value) {
    if (!value) return;
    std::lock_guard lock(g_formattedValueMutex);
    g_formattedValues.insert(value);
}

bool IsRegistered(const void* value) {
    if (!value) return false;
    std::lock_guard lock(g_formattedValueMutex);
    return g_formattedValues.contains(value);
}

const char16_t* GetString(const void* value, std::int32_t* length, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    std::int32_t localLength = 0;
    const char16_t* text = ufmtval_getString(static_cast<const UFormattedValue*>(value), &localLength, &local);
    *status = static_cast<std::int32_t>(local);
    if (length) *length = localLength;
    return text;
}

}
