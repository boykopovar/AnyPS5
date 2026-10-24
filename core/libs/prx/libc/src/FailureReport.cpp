#include "prx/libc/include/FailureReport.hpp"
#include <array>
#include <atomic>
#include <stdexcept>

namespace {

using Report = void (*)() noexcept;

std::array<std::atomic<Report>, 8> reports{};
std::atomic<bool> ran{false};

}

extern "C" void LibcRegisterFailureReport_nid_no_patch(Report report) {
    if (report == nullptr) throw std::invalid_argument("libc: null failure report");
    for (auto& slot : reports) {
        if (slot.load(std::memory_order_acquire) == report) return;
    }
    for (auto& slot : reports) {
        Report empty = nullptr;
        if (slot.compare_exchange_strong(empty, report, std::memory_order_acq_rel)) return;
    }
    throw std::runtime_error("libc: more than 8 failure reports registered");
}

extern "C" void LibcUnregisterFailureReport_nid_no_patch(Report report) noexcept {
    for (auto& slot : reports) {
        Report expected = report;
        slot.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
    }
}

extern "C" void LibcRunFailureReports_nid_no_patch() noexcept {
    if (ran.exchange(true, std::memory_order_acq_rel)) return;
    for (auto& slot : reports) {
        if (const auto report = slot.load(std::memory_order_acquire)) report();
    }
}
