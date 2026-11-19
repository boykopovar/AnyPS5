#include "prx/libSceGnmDriver/include/Api.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

template <typename Operation>
static void RequireThrows(Operation operation) {
    try {
        operation();
    } catch (const std::runtime_error&) {
        return;
    }
    std::abort();
}

int main() {
    constexpr std::int16_t graphicsFilter = -14;
    constexpr std::int16_t otherFilter = -1;
    KernelEvent graphics{};
    graphics.filter = graphicsFilter;
    graphics.ident = 0x2a;
    Require(sceGnmGetEqEventType(&graphics) == 0x2a);

    KernelEvent other{};
    other.filter = otherFilter;
    other.ident = 5;
    RequireThrows([&] { sceGnmGetEqEventType(&other); });
    RequireThrows([&] { sceGnmGetEqEventType(nullptr); });

    errno = 71;
    RequireThrows([&] { sceGnmGetEqEventType(reinterpret_cast<const KernelEvent*>(0x10)); });
    Require(errno == 71);
    return 0;
}
