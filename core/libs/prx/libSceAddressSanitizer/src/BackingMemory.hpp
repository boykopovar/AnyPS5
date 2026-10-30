#pragma once

#include <cstddef>

namespace AsanRuntime {

extern "C" void* AllocateBacking_nid_no_patch(std::size_t size, std::size_t alignment) noexcept;
extern "C" void FreeBacking_nid_no_patch(void* pointer, std::size_t alignment) noexcept;

}
