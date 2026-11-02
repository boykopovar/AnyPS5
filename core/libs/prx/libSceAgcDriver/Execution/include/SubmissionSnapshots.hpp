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

using FaultToken = std::uint32_t;

struct FaultTokenEntry {
    alignas(std::atomic_ref<FaultToken>::required_alignment) mutable FaultToken token = 0;
    std::uint64_t submitSerial = 0;
    std::uint64_t programAddress = 0;
    std::uint64_t variantId = 0;
    char desc[48]{};
};

struct SubmitBracket {
    std::uint64_t submitSerial = 0;
    std::uint64_t lastCompleted = 0;
    FaultToken firstToken = 0;
    FaultToken lastToken = 0;
};

class FaultTokenRing {
public:
    static constexpr std::size_t Capacity = 4096;
    static FaultTokenRing& Instance() noexcept {
        static FaultTokenRing store;
        return store;
    }
    FaultTokenRing() = default;
    FaultTokenRing(const FaultTokenRing&) = delete;
    FaultTokenRing& operator=(const FaultTokenRing&) = delete;
    FaultToken Claim() noexcept {
        for (;;) {
            FaultToken candidate = next.fetch_add(1, std::memory_order_relaxed);
            if (candidate != 0) return candidate;
        }
    }
    FaultTokenEntry* Mutable(FaultToken token) noexcept {
        if (token == 0) return nullptr;
        return &entries[static_cast<std::size_t>(token) % Capacity];
    }
    const FaultTokenEntry* Find(FaultToken token) const noexcept {
        if (token == 0) return nullptr;
        const FaultTokenEntry& slot = entries[static_cast<std::size_t>(token) % Capacity];
        if (std::atomic_ref<FaultToken>(slot.token).load(std::memory_order_acquire) != token) return nullptr;
        return &slot;
    }
private:
    std::atomic<FaultToken> next{1};
    std::array<FaultTokenEntry, Capacity> entries{};
};

struct SubmissionSnapshot {
    std::uint64_t submitSerial = 0;
    std::uint64_t spaceSerial = 0;
    std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>> ranges;
    SubmitBracket bracket{};
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

    void NoteSubmit(std::uint64_t submitSerial, std::uint64_t spaceSerial, std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>> ranges, SubmitBracket bracket = {}) noexcept {
        try {
            if (submitSerial == 0 || ranges == nullptr) return;
            std::lock_guard<std::mutex> lock(mutex);
            Slot& slot = slots[submitSerial % Capacity];
            slot.serial = submitSerial;
            slot.spaceSerial = spaceSerial;
            slot.ranges = std::move(ranges);
            slot.bracket = bracket;
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
                slot.bracket = {};
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
            found.bracket = slot.bracket;
            return found;
        } catch (...) {
            return std::nullopt;
        }
    }

    std::optional<SubmitBracket> FindBracket(std::uint64_t submitSerial) const noexcept {
        try {
            if (submitSerial == 0) return std::nullopt;
            std::lock_guard<std::mutex> lock(mutex);
            const Slot& slot = slots[submitSerial % Capacity];
            if (slot.serial != submitSerial) return std::nullopt;
            return slot.bracket;
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    struct Slot {
        std::uint64_t serial = 0;
        std::uint64_t spaceSerial = 0;
        std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>> ranges;
        SubmitBracket bracket{};
    };

    mutable std::mutex mutex;
    std::array<Slot, Capacity> slots{};
};

}

#endif
