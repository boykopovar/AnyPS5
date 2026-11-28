#pragma once

#include <cstddef>
#include <cstdint>

namespace Asan {

void CheckSmallRegion(std::uintptr_t address, std::size_t size, bool isWrite, std::uintptr_t pc, std::uintptr_t bp, std::uintptr_t sp);

}
