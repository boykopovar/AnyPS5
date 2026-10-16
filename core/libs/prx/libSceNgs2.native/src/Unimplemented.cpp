#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#include "prx/libc/include/General.hpp"
#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"

#pragma GCC visibility push(default)

namespace {

struct ReportHandler {
    std::uint32_t report_type;
    void* handler;
    std::uintptr_t user_data;
};

std::mutex g_report_mutex;
std::unordered_map<Ngs2Handle, ReportHandler> g_report_handlers;
std::atomic<Ngs2Handle> g_next_report_handle{1};

}

extern "C" {

int APS5_VABI sceNgs2ReportRegisterHandler(uint32_t report_type, void* handler, uintptr_t user_data, Ngs2Handle* out_handle) {
    if (handler == nullptr || out_handle == nullptr) {
        throw std::runtime_error("sceNgs2ReportRegisterHandler: invalid argument");
    }
    const Ngs2Handle handle = g_next_report_handle.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard lock(g_report_mutex);
        g_report_handlers.emplace(handle, ReportHandler{report_type, handler, user_data});
    }
    *out_handle = handle;
    return 0;
}

int APS5_VABI sceNgs2ReportUnregisterHandler(Ngs2Handle report_handle) {
    std::lock_guard lock(g_report_mutex);
    if (g_report_handlers.erase(report_handle) == 0) {
        throw std::runtime_error("sceNgs2ReportUnregisterHandler: unknown report handle");
    }
    return 0;
}

}

#pragma GCC visibility pop
