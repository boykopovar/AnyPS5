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

int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param) {
    if (init_param == nullptr) APS5_INVALID_ARG_EX;
    if (init_param->malloc_func == nullptr || init_param->free_func == nullptr) throw std::invalid_argument(std::string(__func__) + ": missing allocator functions");
    if (init_param->reserved0 != 0 || init_param->reserved1 != 0) throw std::invalid_argument(std::string(__func__) + ": reserved fields are not zero");
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentExportFinish(void) {
    throw std::runtime_error("sceContentExportFinish: unknown signature");
}

int APS5_VABI sceContentExportFromData(void) {
    throw std::runtime_error("sceContentExportFromData: unknown signature");
}

int APS5_VABI sceContentExportFromFile(void) {
    throw std::runtime_error("sceContentExportFromFile: unknown signature");
}

int APS5_VABI sceContentExportFromFileWithThumbnail(void) {
    throw std::runtime_error("sceContentExportFromFileWithThumbnail: unknown signature");
}

int APS5_VABI sceContentExportStart(void) {
    throw std::runtime_error("sceContentExportStart: unknown signature");
}

int APS5_VABI sceContentExportTerm(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    return 0;
}

}
