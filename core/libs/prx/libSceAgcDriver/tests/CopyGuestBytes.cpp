#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

int main() {
    std::array<unsigned char, 96> source{};
    for (std::size_t index = 0; index < source.size(); ++index) source[index] = static_cast<unsigned char>(index * 37 + 11);
    for (std::size_t offset = 0; offset < 8; ++offset) {
        for (std::size_t bytes : {0, 1, 7, 8, 9, 15, 16, 17, 64, 80}) {
            std::array<unsigned char, 96> destination{};
            destination.fill(0xa5);
            AgcDriver::GuestMemory::CopyGuestBytes(destination.data() + 1, source.data() + offset, bytes);
            Require(destination[0] == 0xa5, "the byte before the destination is kept");
            Require(std::memcmp(destination.data() + 1, source.data() + offset, bytes) == 0, "the bytes are copied from every source offset");
            Require(destination[1 + bytes] == 0xa5, "the byte after the destination is kept");
        }
    }
    std::puts("copy guest bytes: ok");
}
