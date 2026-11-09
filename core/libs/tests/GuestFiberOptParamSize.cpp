#include "prx/libSceFiber/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static void CheckedEntry(std::uint64_t, std::uint64_t) {
    sceFiberReturnToThread(0, nullptr);
}

int main() {
    Require(sizeof(FiberOptParam) == 4, "FiberOptParam size must be 4 bytes");
    
    Require(sceFiberOptParamInitialize(nullptr) == FiberErrorNull, "null opt param error");
    
    struct alignas(8) AlignedStorage {
        std::uint8_t bytes[0x80];
        std::uint32_t sentinel;
    } storage;
    
    std::memset(&storage, 0xA5, sizeof(storage));
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(storage.bytes)) == 0, "initialize opt param");
    
    for (std::size_t i = 0; i < sizeof(storage.bytes); ++i) {
        Require(storage.bytes[i] == 0, "opt param bytes must be cleared");
    }
    Require(storage.sentinel == 0xA5A5A5A5, "sentinel must remain intact");

    std::array<std::uint64_t, 64> context;
    FiberObject fiberObject{};
    
    Require(_sceFiberInitializeWithInternalOptionImpl(
        &fiberObject, "optParamTest", CheckedEntry, 0,
        context.data(), context.size() * sizeof(std::uint64_t),
        reinterpret_cast<const FiberOptParam*>(storage.bytes), 0, 0
    ) == 0, "initialize with valid optParam");
    
    Require(sceFiberFinalize(&fiberObject) == 0, "finalize fiber");
    return 0;
}
