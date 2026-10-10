#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> g_initialized{false};
}

extern "C" {

int APS5_VABI sceContentSearchInit(const ContentSearchInitParam* init_param) {
    if (init_param == nullptr) APS5_INVALID_ARG_EX;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentSearchCloseMetadata(void) {
    throw std::runtime_error("sceContentSearchCloseMetadata: unknown signature");
}

int APS5_VABI sceContentSearchGetMetadataFieldInfo(void) {
    throw std::runtime_error("sceContentSearchGetMetadataFieldInfo: unknown signature");
}

int APS5_VABI sceContentSearchGetMetadataValue(void) {
    throw std::runtime_error("sceContentSearchGetMetadataValue: unknown signature");
}

int APS5_VABI sceContentSearchOpenMetadata(void) {
    throw std::runtime_error("sceContentSearchOpenMetadata: unknown signature");
}

int APS5_VABI sceContentSearchSearchContent(void) {
    throw std::runtime_error("sceContentSearchSearchContent: unknown signature");
}

int APS5_VABI sceContentSearchTerm(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    return 0;
}

}
