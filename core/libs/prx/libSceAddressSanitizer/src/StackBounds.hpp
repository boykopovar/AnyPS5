#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace AsanRuntime {

bool CurrentStackBounds_nid_no_patch(std::uintptr_t& bottom, std::uintptr_t& top);

[[noreturn]] inline void StackUnavailable_nid_no_patch(const char* function) {
    throw std::runtime_error(std::string(function) + ": the bounds of the current thread stack are unavailable");
}

}  // namespace AsanRuntime
