#pragma once

#include <cstdint>

#include "SceTypes.hpp"

extern "C" {

std::uint64_t APS5_VABI __std_atomic_wait_get_deadline_nid_postfix(std::uint64_t timeout);
std::uint64_t APS5_VABI __std_atomic_wait_get_remaining_timeout_nid_postfix(std::uint64_t deadline);

}
