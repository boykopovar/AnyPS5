#include "prx/libSceDiscId/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
    Require(Throws([] { return sceDiscIdGet(nullptr); }));
    Require(Throws([] { return sceDiscIdGet2(nullptr, 0); }));
    Require(Throws([] { return sceDiscIdGet2(nullptr, 0xFFFFFFFFu); }));

    std::uint8_t buffer[64];
    std::memset(buffer, 0xAB, sizeof(buffer));
    auto* id = reinterpret_cast<DiscId*>(buffer);
    auto* id2 = reinterpret_cast<DiscId2*>(buffer);

    Require(Throws([id] { return sceDiscIdGet(id); }));
    Require(Throws([id2] { return sceDiscIdGet2(id2, 0); }));
    Require(Throws([id2] { return sceDiscIdGet2(id2, 1); }));
    Require(Throws([id2] { return sceDiscIdGet2(id2, 3); }));
    Require(Throws([id2] { return sceDiscIdGet2(id2, 0xFFFFFFFFu); }));
    for (std::uint8_t byte : buffer) Require(byte == 0xAB);

    return 0;
}
