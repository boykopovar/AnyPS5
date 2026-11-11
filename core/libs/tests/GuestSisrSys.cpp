#include "prx/libSceSisrSys/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

template<typename TCall>
bool Throws(TCall call) {
    try {
        static_cast<void>(call());
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(sceSisrSysSetup(nullptr) == 0);
    SisrSysSetupParams params{};
    Require(sceSisrSysSetup(&params) == 0);

    Require(Throws([] { return sceSisrSysProcessUmdEvent(nullptr); }));
    SisrSysUmdEvent bad{1, 0};
    Require(Throws([&] { return sceSisrSysProcessUmdEvent(&bad); }));

    const uint32_t expected[] = {0, 1, 2, 0, 4, 0, 6, 7};
    for (uint32_t type = 0; type < 8; ++type) {
        SisrSysUmdEvent event{0, type};
        if (type == 0 || type == 3 || type == 5) {
            Require(Throws([&] { return sceSisrSysProcessUmdEvent(&event); }));
        } else {
            Require(sceSisrSysProcessUmdEvent(&event) == static_cast<int>(expected[type]));
        }
    }
    return 0;
}
