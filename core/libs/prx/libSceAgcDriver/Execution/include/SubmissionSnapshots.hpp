#ifndef CORE_LIBS_PRX_LIBSCeAgcDriver_EXECUTION_INCLUDE_SUBMISSIONSNAPSHOTS_HPP
#define CORE_LIBS_PRX_LIBSCeAgcDriver_EXECUTION_INCLUDE_SUBMISSIONSNAPSHOTS_HPP

#include "../../../../../shader/recompiler/BdaAbi.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace AgcDriver {

struct SubmissionSnapshot {
    std::uint64_t submitSerial = 0;
    std::uint64_t spaceSerial = 0;
    std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>> ranges;
};

class SubmissionSnapshots {
public:
    static constexpr std::size_t Capacity = 64;

    static SubmissionSnapshots& DeviceSnapshots() {
        static SubmissionSnapshots store;
        return store;
    }

    SubmissionSnapshots() = default;
    SubmissionSnapshots(const SubmissionSnapshots&) = delete;
    SubmissionSnapshots& operator=(const SubmissionSnapshots&) = delete;

    void NoteSubmit(std::uint64_t submitSerial, std::uint64_t spaceSerial, std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>> ranges) noexcept {
        try {
            if (submitSerial == 0 || ranges == nullptr) return;
            std::lock_guard<std::mutex> lock(mutex);
            Slot& slot = slots[submitSerial % Capacity];
            slot.serial = submitSerial;
            slot.spaceSerial = spaceSerial;
            slot.ranges = std::move(ranges);
        } catch (...) {
        }
    }

    void Reset() noexcept {
        try {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto& slot : slots) {
                slot.serial = 0;
                slot.spaceSerial = 0;
                slot.ranges.reset();
            }
        } catch (...) {
        }
    }

    std::optional<SubmissionSnapshot> Find(std::uint64_t submitSerial) const noexcept {
        try {
            if (submitSerial == 0) return std::nullopt;
            std::lock_guard<std::mutex> lock(mutex);
            const Slot& slot = slots[submitSerial % Capacity];
            if (slot.serial != submitSerial || slot.ranges == nullptr) return std::nullopt;
            SubmissionSnapshot found;
            found.submitSerial = slot.serial;
            found.spaceSerial = slot.spaceSerial;
            found.ranges = slot.ranges;
            return found;
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    struct Slot {
        std::uint64_t serial = 0;
        std::uint64_t spaceSerial = 0;
        std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>> ranges;
    };

    mutable std::mutex mutex;
    std::array<Slot, Capacity> slots{};
};

}

#endif
