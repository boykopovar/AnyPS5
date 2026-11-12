#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int InvalidParameter = static_cast<int>(0x80930005);
constexpr std::uint32_t Untouched = 0x5a5a5a5au;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Ajm initialize context pointer: %s\n", message);
    std::abort();
}

}

int main() {
    std::uint32_t first = Untouched;
    Require(sceAjmInitialize(0, &first) == 0 && first == 1, "a valid context pointer was not written with the first id");
    std::uint32_t second = Untouched;
    Require(sceAjmInitialize(0, &second) == 0 && second != Untouched && second != first, "two calls did not yield unique ids");

    Require(sceAjmInitialize(0, nullptr) == InvalidParameter, "a null context pointer was not rejected");
    Require(sceAjmInitialize(0, reinterpret_cast<std::uint32_t*>(0x10)) == InvalidParameter, "a dangling context pointer was not rejected");

    std::uint32_t third = Untouched;
    Require(sceAjmInitialize(0, &third) == 0 && third == 3, "an invalid call consumed a context id");

    std::puts("Ajm initialize context pointer tests passed");
    return 0;
}
