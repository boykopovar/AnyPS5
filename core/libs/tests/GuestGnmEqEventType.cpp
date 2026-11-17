#include "prx/libSceGnmDriver/include/Api.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

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
    ExpectErr<std::runtime_error>([&] { sceGnmGetEqEventType(&other); });
    ExpectErr<std::runtime_error>([&] { sceGnmGetEqEventType(nullptr); });

    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceGnmGetEqEventType(reinterpret_cast<const KernelEvent*>(0x10)); });
    Require(errno == 71);
    return 0;
}
