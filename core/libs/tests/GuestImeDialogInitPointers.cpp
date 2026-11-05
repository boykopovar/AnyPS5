#include "prx/libSceImeDialog/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int StatusNone = 0;
constexpr int ErrorInvalidAddress = static_cast<int>(0x80bc0031u);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

}

int main() {
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        const auto* badParam = reinterpret_cast<const Param*>(address);
        Require(sceImeDialogInit(badParam, nullptr) == ErrorInvalidAddress, "an unmapped param returns ErrorInvalidAddress");
        Require(sceImeDialogGetStatus() == StatusNone, "a rejected param leaves no dialog open");
    }
    return 0;
}
