#include "prx/libfontconfig/include/Config.hpp"

#include <atomic>
#include <new>
#include <stdexcept>

struct FcConfigState {
    std::atomic<unsigned> references{1};
};

namespace {

FcConfigState* State(FcConfig* config) {
    return reinterpret_cast<FcConfigState*>(config);
}

FcConfig* Handle(FcConfigState* state) {
    return reinterpret_cast<FcConfig*>(state);
}

[[noreturn, gnu::noinline]] void UnsupportedCurrentConfig() {
    throw std::invalid_argument("FcConfigReference: loading the global font configuration is not implemented");
}

}

extern "C" {

FcConfig* APS5_VABI FcConfigCreate() {
    return Handle(new (std::nothrow) FcConfigState);
}

FcConfig* APS5_VABI FcConfigReference(FcConfig* config) {
    if (!config) UnsupportedCurrentConfig();
    State(config)->references.fetch_add(1, std::memory_order_relaxed);
    return config;
}

void APS5_VABI FcConfigDestroy(FcConfig* config) {
    if (!config) return;
    if (State(config)->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete State(config);
}

}
