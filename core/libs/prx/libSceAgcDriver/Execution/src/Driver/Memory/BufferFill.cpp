#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include <bit>
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

bool Driver::matchesFillKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute) {
    static const bool enabled = std::getenv("APS5_NO_FILL_HLE") == nullptr;
    if (!enabled || userData.size() < 8 || compute.numThreads[0] != 64 || compute.numThreads[1] != 1 || compute.numThreads[2] != 1) return false;
    static constexpr std::array<std::uint32_t, 9> fillKernel{0xd7460004u, 0x04010c08u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0xe01c2000u, 0x80000004u, 0xbf810000u};
    if (code.size() < fillKernel.size() || !std::equal(fillKernel.begin(), fillKernel.end(), code.begin())) return false;

    const auto stride = (userData[1] >> 16u) & 0x3fffu;
    const bool swizzled = ((userData[1] >> 31u) & 1u) != 0;
    const auto dstSel = userData[3] & 0xfffu;
    const bool addTid = ((userData[3] >> 23u) & 1u) != 0;
    const auto type = userData[3] >> 30u;
    const auto format = (userData[3] >> 12u) & 0x7fu;
    return type == 0 && stride == 16 && !swizzled && !addTid && dstSel == 0xfacu && format == 0x4bu;
}

bool Driver::fillClearEnabled() {
    static const bool enabled = std::getenv("APS5_NO_FILL_CLEAR") == nullptr;
    return enabled;
}

bool Driver::fillClearExactOnly() {
    static const bool exactOnly = std::getenv("APS5_FILL_CLEAR_EXACT_ONLY") != nullptr;
    return exactOnly;
}

void Driver::fillClearCount(const Graphics::StorageTexture::FillCoverage& coverage, std::size_t bytes, std::span<const std::uint32_t, 4> pattern, std::size_t discarded, bool cleared, const char* refusal) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    using FillCover = Graphics::StorageTexture::FillCover;
    static constexpr std::array<const char*, 8> names{"none", "exact", "inside", "around", "straddle", "several", "keys", "layer"};
    static std::mutex countsMutex;
    static std::array<std::uint64_t, 8> counts{}, coverBytes{};
    static std::map<std::uint32_t, std::uint64_t> coveredFormats;
    static std::map<std::string, std::uint64_t> refusals;
    static std::map<std::uint32_t, std::uint64_t> keyCodes;
    static std::uint64_t clears = 0, clearedBytes = 0, layerClears = 0, withOthers = 0, othersInside = 0, discards = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    std::lock_guard lock(countsMutex);
    const auto index = static_cast<std::size_t>(coverage.cover);
    ++counts[index];
    coverBytes[index] += bytes;
    if (coverage.image != nullptr) {
        ++coveredFormats[coverage.image->Descriptor().format];
        if (coverage.others != 0) ++withOthers;
        othersInside += coverage.inside;
    }
    if (coverage.cover == FillCover::Keys) {

        const auto word = pattern[0];
        const bool uniform = pattern[1] == word && pattern[2] == word && pattern[3] == word && word == (word & 0xffu) * 0x01010101u;
        ++keyCodes[uniform ? word & 0xffu : 0x100u];
    }
    discards += discarded;
    if (cleared) {
        ++clears;
        clearedBytes += bytes;
        if (coverage.cover == FillCover::Layer) ++layerClears;
    } else if (refusal != nullptr) {
        ++refusals[refusal];
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport < std::chrono::seconds(10)) return;
    lastReport = now;
    std::string line = "[fill-cover] fills by cover (count/MiB):";
    for (std::size_t i = 0; i < names.size(); ++i) line += " " + std::string(names[i]) + " " + std::to_string(counts[i]) + "/" + std::to_string(coverBytes[i] >> 20u);
    line += "; covers with other images over the range " + std::to_string(withOthers) + " (" + std::to_string(othersInside) + " images wholly inside), pending results discarded as dead " + std::to_string(discards) + "; covers by guest format:";
    for (const auto& [format, count] : coveredFormats) line += " " + std::to_string(format) + ":" + std::to_string(count);
    line += "; key fills by code:";
    for (const auto& [code, count] : keyCodes) {
        char text[32];
        std::snprintf(text, sizeof(text), " 0x%02x:%llu", code, static_cast<unsigned long long>(count));
        line += text;
    }
    std::fprintf(stderr, "%s\n", line.c_str());
    line = "[fill-clear] " + std::to_string(clears) + " fills cleared on resident images (" + std::to_string(clearedBytes >> 20u) + " MiB not stored, " + std::to_string(layerClears) + " one array layer); covers refused:";
    for (const auto& [reason, count] : refusals) line += " " + reason + " " + std::to_string(count);
    std::fprintf(stderr, "%s\n", line.c_str());
}

bool Driver::fillBuffer(QueueState& queue, std::uint32_t queueId, std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, const std::shared_ptr<VulkanDevice>& localDevice) {
    if (!matchesFillKernel(code, userData, compute)) return false;
    const auto numRecords = userData[2];
    std::array<std::uint32_t, 3> groups{packet[1], packet[2], packet[3]};
    if ((packet[4] & 0x20u) != 0) {
        for (std::uint32_t axis = 0; axis < 3; ++axis) {
            const auto threads = std::max(readRegister(queue.shader, 0x207 + axis) & 0xffffu, 1u);
            groups[axis] = (groups[axis] + threads - 1) / threads;
        }
    }
    if (groups[1] != 1 || groups[2] != 1) return false;
    const auto records = std::min<std::uint64_t>(static_cast<std::uint64_t>(groups[0]) * 64u, numRecords);
    const auto base = userData[0] | (static_cast<std::uint64_t>(userData[1] & 0xffffu) << 32u);
    const auto bytes = static_cast<std::size_t>(records * 16u);
    const std::array<std::uint32_t, 4> pattern{userData[4], userData[5], userData[6], userData[7]};
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    static std::atomic<std::uint64_t> fills{0}, filledBytes{0}, cpuFills{0};

    static std::uint64_t uniformFills = 0, uniformBytes = 0, patternFills = 0, patternBytes = 0, chainCopies = 0;

    enum FillPhase : std::size_t { FillLabels, FillCheck, FillClassify, FillDiscard, FillClearPhase, FillFlush, FillDevice, FillCpu, FillPhaseCount };
    static constexpr const char* fillPhaseNames[FillPhaseCount] = {"labels", "check", "classify", "discard", "clear", "flush", "device", "cpu"};
    static std::array<double, FillPhaseCount> fillPhaseMs{};
    static std::array<double, 8> holdByCoverMs{};
    static std::array<std::uint64_t, 8> holdByCover{};
    static std::uint64_t flushes = 0, flushed = 0;
    static auto fillReport = std::chrono::steady_clock::now();
    ++fills;
    filledBytes += bytes;
    if (bytes != 0) {
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Fill);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        const auto holdStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        auto phaseStart = holdStart;
        const auto phase = [&](FillPhase which) {
            if (!profile) return;
            const auto now = std::chrono::steady_clock::now();
            fillPhaseMs[which] += std::chrono::duration<double, std::milli>(now - phaseStart).count();
            phaseStart = now;
        };

        recordLabelsForPacket(localDevice.get(), queueId);
        phase(FillLabels);
        GuestMemory::CheckRange(reinterpret_cast<const void*>(base), bytes, 16, true);
        phase(FillCheck);
        const auto coverage = Graphics::StorageTexture::ClassifyFill(base, bytes);
        phase(FillClassify);

        static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
        if (traceKeys && coverage.cover == Graphics::StorageTexture::FillCover::Keys) std::fprintf(stderr, "[dcc-keys] title fills keys 0x%llx+0x%zx with %08x %08x %08x %08x (queue 0x%x)\n", static_cast<unsigned long long>(base), bytes, pattern[0], pattern[1], pattern[2], pattern[3], queueId);
        const bool uniformKeysFill = coverage.cover == Graphics::StorageTexture::FillCover::Keys && std::all_of(pattern.begin(), pattern.end(), [&](std::uint32_t word) { return word == (pattern[0] & 0xffu) * 0x01010101u; });
        if (uniformKeysFill) {
            Graphics::StorageTexture::NoteKeysFill(base, bytes, static_cast<std::uint8_t>(pattern[0]));
            Graphics::StorageTexture::ClearByKeysFill(base, bytes, static_cast<std::uint8_t>(pattern[0]));
        }

        const auto discarded = fillClearEnabled() ? Graphics::StorageTexture::DiscardPendingInside(base, bytes) : 0u;
        phase(FillDiscard);
        const char* refusal = nullptr;
        bool cleared = false;
        if (coverage.image != nullptr) {
            if (!fillClearEnabled()) {
                refusal = "disabled";
            } else if (fillClearExactOnly() && (coverage.cover != Graphics::StorageTexture::FillCover::Exact || coverage.others != 0)) {
                refusal = "exact only";
            } else {

                Graphics::StorageTexture::FlushPending(base, bytes, coverage.image.get(), "buffer fill", Graphics::PublishScope::PartialUnits);
                cleared = coverage.image->FillClear(pattern, coverage.layer, refusal);
            }
        }
        phase(FillClearPhase);
        fillClearCount(coverage, bytes, pattern, discarded, cleared, refusal);
        noteForeignWriter(base, base + bytes, queueId);

        if (!cleared) {
            ++flushes;

            if (Graphics::StorageTexture::FlushPending(base, bytes, nullptr, "buffer fill", Graphics::PublishScope::PartialUnits)) ++flushed;
        }
        phase(FillFlush);
        const bool stored = cleared || localDevice->FillBuffer(base, bytes, pattern);
        if (stored && pattern[0] == pattern[1] && pattern[1] == pattern[2] && pattern[2] == pattern[3]) Graphics::NoteDepthMetadataFill(base, bytes, pattern[0]);
        phase(FillDevice);
        if (uniformKeysFill && stored && !cleared) {
            Graphics::DccKeys filled = Graphics::DccKeys::Mixed;
            switch (pattern[0] & 0xffu) {
                case 0x00: filled = Graphics::DccKeys::Clear0000; break;
                case 0x40: filled = Graphics::DccKeys::Clear0001; break;
                case 0x80: filled = Graphics::DccKeys::Clear1110; break;
                case 0xc0: filled = Graphics::DccKeys::Clear1111; break;
                case 0x20: filled = Graphics::DccKeys::ClearRegister; break;
                case 0xff: filled = Graphics::DccKeys::Uncompressed; break;
                default: break;
            }
            if (filled != Graphics::DccKeys::Mixed) Graphics::NoteKeysFillOnGpu(base, bytes, filled);
        }
        if (profile && stored && !cleared) {
            const bool uniform = pattern[0] == pattern[1] && pattern[1] == pattern[2] && pattern[2] == pattern[3];
            ++(uniform ? uniformFills : patternFills);
            (uniform ? uniformBytes : patternBytes) += bytes;
            if (!uniform) chainCopies += static_cast<std::uint64_t>(std::bit_width((bytes / 16) - 1));
        }
        if (!stored) {
            ++cpuFills;

            localDevice->WaitIdle();
            static thread_local std::vector<std::byte>* blockSlot = nullptr;
            auto& block = ShaderRecompiler::ThreadOwned(blockSlot);
            const auto chunk = std::min<std::size_t>(bytes, 1u << 20u);
            block.resize(chunk);
            for (std::size_t at = 0; at < chunk; at += 16) std::memcpy(block.data() + at, pattern.data(), 16);
            for (std::size_t done = 0; done < bytes; done += chunk) GuestMemory::Write(base + done, std::span<const std::byte>(block).first(std::min(chunk, bytes - done)), 16);
            phase(FillCpu);
        }
        if (profile) {
            const auto now = std::chrono::steady_clock::now();
            const auto cover = static_cast<std::size_t>(coverage.cover);
            ++holdByCover[cover];
            holdByCoverMs[cover] += std::chrono::duration<double, std::milli>(now - holdStart).count();
            if (now - fillReport > std::chrono::seconds(10)) {
                fillReport = now;
                static constexpr std::array<const char*, 8> coverNames{"none", "exact", "inside", "around", "straddle", "several", "keys", "layer"};
                std::string line;
                char text[64];
                for (std::size_t i = 0; i < FillPhaseCount; ++i) {
                    std::snprintf(text, sizeof(text), " %s %.0f", fillPhaseNames[i], fillPhaseMs[i]);
                    line += text;
                }
                line += "; hold by cover (count/ms):";
                for (std::size_t i = 0; i < coverNames.size(); ++i) {
                    std::snprintf(text, sizeof(text), " %s %llu/%.0f", coverNames[i], static_cast<unsigned long long>(holdByCover[i]), holdByCoverMs[i]);
                    line += text;
                }
                std::fprintf(stderr, "[fill] %llu buffer fills (%.0f MiB), %llu stored by the CPU; hold phases ms (cumulative):%s; pre-store flushes %llu (%llu stored an image); stored on the GPU: uniform %llu (%.0f MiB), pattern %llu (%.0f MiB), chain copies %llu\n", static_cast<unsigned long long>(fills.load()), filledBytes.load() / 1048576.0, static_cast<unsigned long long>(cpuFills.load()), line.c_str(), static_cast<unsigned long long>(flushes), static_cast<unsigned long long>(flushed), static_cast<unsigned long long>(uniformFills), uniformBytes / 1048576.0, static_cast<unsigned long long>(patternFills), patternBytes / 1048576.0, static_cast<unsigned long long>(chainCopies));
            }
        }
    }
    return true;
}

}
