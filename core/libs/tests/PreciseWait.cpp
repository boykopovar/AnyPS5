#include "prx/libc/include/PreciseWait.hpp"
#include <chrono>
#include <iostream>

int main() {
    const auto start = std::chrono::steady_clock::now();
    PreciseSleepUs(50);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed < std::chrono::microseconds(50)) {
        std::cerr << "PreciseSleepUs returned before its requested duration\n";
        return 1;
    }
    return 0;
}
