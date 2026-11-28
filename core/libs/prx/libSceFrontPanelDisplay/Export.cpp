#include "prx/libSceFrontPanelDisplay/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<bool> g_initialized{false};
std::atomic<int> g_outputMode{0};

}

extern "C" {

int APS5_VABI sceFrontPanelDisplayInitialize(void) {
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceFrontPanelDisplaySetOutputMode(void) {
    g_outputMode.store(0);
    return 0;
}

int APS5_VABI sceFrontPanelDisplayStickReadState(void) {
    return g_outputMode.load();
}

}
