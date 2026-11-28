#include "prx/libSceKbEmulate/libSceKbEmulate.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notOpen = static_cast<int>(0x80BC0002u);
    constexpr int nullParam = static_cast<int>(0x80BC0022u);
    Require(sceKeyboardPadEmulateClose() == notOpen);
    Require(sceKeyboardPadEmulateOpen(nullptr) == nullParam);
    Require(sceKeyboardPadEmulateClose() == notOpen);
    return 0;
}
