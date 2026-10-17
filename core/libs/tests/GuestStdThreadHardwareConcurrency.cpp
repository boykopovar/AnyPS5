#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" unsigned APS5_VABI _ZNSt6thread20hardware_concurrencyEv_nid_postfix();

int main() {
    unsigned expected = 0;
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    expected = info.dwNumberOfProcessors;
#else
    const long processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (processors > 0) expected = static_cast<unsigned>(processors);
#endif
    const unsigned result = _ZNSt6thread20hardware_concurrencyEv_nid_postfix();
    if (expected != 0 && result != expected) {
        std::fprintf(stderr, "guest processor hint %u differs from OS count %u\n", result, expected);
        return 1;
    }
    if (result != std::thread::hardware_concurrency()) {
        std::fprintf(stderr, "guest processor hint %u differs from std::thread::hardware_concurrency %u\n",
            result, std::thread::hardware_concurrency());
        return 1;
    }
    return 0;
}
