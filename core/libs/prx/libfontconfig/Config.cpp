#include "prx/libfontconfig/include/Config.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <new>

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

}

extern "C" {

FcConfig* APS5_VABI FcConfigCreate() {
    return Handle(new (std::nothrow) FcConfigState);
}

FcConfig* APS5_VABI FcConfigReference(FcConfig* config) {
    if (!config) NotImplemented_nid_no_patch(__func__);
    State(config)->references.fetch_add(1, std::memory_order_relaxed);
    return config;
}

void APS5_VABI FcConfigDestroy(FcConfig* config) {
    if (!config) return;
    if (State(config)->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete State(config);
}

}
