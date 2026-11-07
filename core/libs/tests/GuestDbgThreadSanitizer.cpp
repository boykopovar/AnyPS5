#include "prx/libSceDbgThreadSanitizer/include/Api.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    __tsan_acquire(nullptr);
    __tsan_release(nullptr);

    int scalar = 42;
    __tsan_acquire(&scalar);
    __tsan_release(&scalar);
    Require(scalar == 42);

    std::uint8_t buffer[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    __tsan_acquire(buffer);
    __tsan_release(buffer);
    Require(buffer[0] == 1);
    Require(buffer[7] == 8);

    std::uint64_t value64 = 0xdeadbeefcafebabeULL;
    __tsan_acquire(&value64);
    __tsan_release(&value64);
    Require(value64 == 0xdeadbeefcafebabeULL);

    return 0;
}
