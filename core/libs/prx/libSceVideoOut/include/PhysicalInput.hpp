#pragma once

#include <cstdlib>

// Process-start diagnostic option. Scripted pad input is applied downstream.
inline bool PhysicalInputDisabled() {
    static const bool disabled = std::getenv("APS5_NO_PHYSICAL_INPUT") != nullptr;
    return disabled;
}
