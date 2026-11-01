#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdio>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    Require(gettimeofday_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == 14);
    Require(sceKernelGettimeofday(nullptr) == static_cast<int>(0x8002000eu));
    return 0;
}
