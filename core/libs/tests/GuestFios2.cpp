#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceFiosFileGetDateSync(int, const char*, std::int64_t*);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int ret = sceFiosFileGetDateSync(0, nullptr, nullptr);
    Require(ret != 0, "sceFiosFileGetDateSync accepted null path and null date pointers");
    std::fprintf(stderr, "GuestFios2 tests passed\n");
    return 0;
}
