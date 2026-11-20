#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main() {
    unsigned char cell[32];
    std::memset(cell, 0x5A, sizeof(cell));
    _ZN3JSC6JSCell7destroyEPS0_(cell);
    for (const unsigned char byte : cell) {
        if (byte != 0x5A) {
            std::fputs("JSCell::destroy changed the cell\n", stderr);
            std::abort();
        }
    }
    _ZN3JSC6JSCell7destroyEPS0_(nullptr);
    std::puts("JSCell destroy tests passed");
    return 0;
}
