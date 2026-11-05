#pragma once

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

namespace NKWebKit {

bool ParkingFairness_nid_no_patch();
WtfAbi::ParkResult ParkLock_nid_no_patch(std::uint8_t* address, std::uint8_t value);
void UnparkCallback_nid_no_patch(const void* address, void* context, std::intptr_t (*callback)(void*, WtfAbi::UnparkResult));

}  // namespace NKWebKit
