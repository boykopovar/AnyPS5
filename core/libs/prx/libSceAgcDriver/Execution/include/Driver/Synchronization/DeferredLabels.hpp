#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DEFERREDLABELS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DEFERREDLABELS_HPP

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace AgcDriver::DriverDetail {

inline constexpr std::chrono::microseconds ReapInterval{250};

struct DeferredLabel {
    static constexpr std::size_t Capacity = 32;
    std::uint64_t address;
    std::size_t size;
    std::array<std::byte, Capacity> bytes;
};

struct DeferredLabels {
    std::vector<DeferredLabel> labels;

    std::chrono::steady_clock::time_point since;
};

DeferredLabels& deferredLabels();

bool DeferLabels();

bool QueuedLabelTable();

bool LabelBatchSubmit();

std::size_t& queuedLabelsNoted();

bool NeedsRecordedLabels(std::uint32_t header);

bool PacketLocksItself(std::uint32_t header);

}

#endif
