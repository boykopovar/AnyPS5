#include <cstdint>

extern "C" {

extern std::uint64_t CfiFixtureCalls_nid_no_patch;

unsigned char CfiDependentData_nid_no_patch[64] = {1};

std::uint64_t* CfiDependentCalls_nid_no_patch() {
    return &CfiFixtureCalls_nid_no_patch;
}

}
