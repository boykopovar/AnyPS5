#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType OpenType constants: %s\n", message);
    std::exit(1);
}

}

int main() {
    Require(FT_Trace_Get_Count_nid_no_patch() == 0, "FT_Trace_Get_Count returns zero");
    Require(FT_Trace_Get_Name_nid_no_patch(-1) == nullptr, "FT_Trace_Get_Name returns null for negative index");
    Require(FT_Trace_Get_Name_nid_no_patch(0) == nullptr, "FT_Trace_Get_Name returns null for zero index");
    Require(FT_Trace_Get_Name_nid_no_patch(128) == nullptr, "FT_Trace_Get_Name returns null for positive index");
    std::puts("Guest FreeType OpenType constant tests passed");
    return 0;
}
