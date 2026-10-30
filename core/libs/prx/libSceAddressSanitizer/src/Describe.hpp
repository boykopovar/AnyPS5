#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace AsanRuntime {

struct RegionInfo {
    const char* kind;
    std::uintptr_t begin;
    std::size_t size;
    std::string name;
};

RegionInfo LocateRegion_nid_no_patch(std::uintptr_t address);
std::string DescribeAddress_nid_no_patch(std::uintptr_t address, std::size_t accessSize);

}  // namespace AsanRuntime
