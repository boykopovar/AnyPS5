#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>
extern "C" {
int APS5_VABI sched_get_priority_max_nid_postfix(int policy);
int APS5_VABI sched_get_priority_min_nid_postfix(int policy);
}
static void Require(bool value) { if (!value) std::abort(); }
static bool Rejects(int policy) {
    try { sched_get_priority_max_nid_postfix(policy); } catch (const std::invalid_argument&) { return true; }
    return false;
}
int main() {
    for (int policy : {1, 2, 3}) {
        Require(sched_get_priority_max_nid_postfix(policy) == 256);
        Require(sched_get_priority_min_nid_postfix(policy) == 767);
    }
    Require(Rejects(0) && Rejects(4));
}
