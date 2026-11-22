#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

static std::vector<int> order;

static void APS5_VABI RecordA(void*) { order.push_back(1); }
static void APS5_VABI RecordB(void*) { order.push_back(2); }
static void APS5_VABI RecordC(void*) { order.push_back(3); }
static void APS5_VABI RecordD(void*) { order.push_back(4); }

int main() {
    void* const handleA = reinterpret_cast<void*>(0x10);
    void* const handleB = reinterpret_cast<void*>(0x20);

    Require(__cxa_atexit_nid_postfix(RecordA, nullptr, handleA) == 0);
    Require(__cxa_atexit_nid_postfix(RecordB, nullptr, handleB) == 0);
    Require(__cxa_atexit_nid_postfix(RecordC, nullptr, handleA) == 0);

    libc___cxa_finalize(handleA);
    Require(order.size() == 2);
    Require(order[0] == 3 && order[1] == 1);

    order.clear();
    libc___cxa_finalize(handleA);
    Require(order.empty());

    libc___cxa_finalize(handleB);
    Require(order.size() == 1 && order[0] == 2);

    order.clear();
    Require(__cxa_atexit_nid_postfix(RecordC, nullptr, handleA) == 0);
    Require(__cxa_atexit_nid_postfix(RecordD, nullptr, handleB) == 0);
    libc___cxa_finalize(nullptr);
    Require(order.size() == 2);
    Require(order[0] == 4 && order[1] == 3);
    return 0;
}
