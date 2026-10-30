#pragma once

#include <cstdint>

namespace AsanRuntime::Coverage {

void InitTracePcGuard_nid_no_patch(std::uint32_t* start, std::uint32_t* end);
void TracePcGuard_nid_no_patch(std::uint32_t* guard, std::uintptr_t pc);
void DumpTracePcGuard_nid_no_patch();
void DumpCoverage_nid_no_patch(const std::uintptr_t* pcs, std::uintptr_t length);

}  // namespace AsanRuntime::Coverage
