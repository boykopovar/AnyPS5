#pragma once

#include <cstddef>

namespace WebRtcModuleStorage {

void* Allocate(std::size_t bytes);
void Free(void* storage, std::size_t capacityBytes);

}
