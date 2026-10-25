#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <thread>

int main() {
    const unsigned result = _ZNSt6thread20hardware_concurrencyEv_nid_postfix();
    if (result != std::thread::hardware_concurrency()) {
        std::fprintf(stderr, "guest processor hint %u differs from std::thread::hardware_concurrency %u\n",
            result, std::thread::hardware_concurrency());
        return 1;
    }
    return 0;
}
