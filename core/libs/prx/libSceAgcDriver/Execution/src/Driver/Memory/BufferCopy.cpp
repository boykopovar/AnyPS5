#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferCopy.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <cstdlib>
#include <cstring>
#include <functional>

namespace AgcDriver::DriverDetail {

std::atomic<std::uint64_t> knownValueEntries{0}, knownValueReads{0}, knownValueVerified{0}, knownValueMismatches{0};

bool Driver::matchesCopyKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData,
                               const ShaderRecompiler::ShaderComputeStageInfo& compute) {
    static const bool enabled = std::getenv("APS5_NO_COPY_HLE") == nullptr;
    if (!enabled || userData.size() != 12 || compute.numThreads[0] != 64 || compute.numThreads[1] != 1 ||
        compute.numThreads[2] != 1 || !compute.groupIdEnable[0])
        return false;
    static constexpr std::size_t copyKernelWords = 38;
    static constexpr std::uint64_t copyKernelHash = 0x6ec00fe8aa95f99aull;
    if (code.size() < copyKernelWords || code[0] != 0xbfa00002u || code[copyKernelWords - 1] != 0xbf810000u)
        return false;
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const auto word : code.first(copyKernelWords)) {
        hash ^= word;
        hash *= 0x100000001b3ull;
    }
    if (hash != copyKernelHash)
        return false;
    const auto stride = [&](std::size_t word) { return (userData[word] >> 16u) & 0x3fffu; };
    const auto swizzled = [&](std::size_t word) { return ((userData[word] >> 31u) & 1u) != 0; };
    return userData[3] == 0x00014004u && userData[7] == 0x00014004u && userData[11] == 0x0004dfacu && stride(1) == 4 &&
           stride(5) == 4 && stride(9) == 16 && !swizzled(1) && !swizzled(5) && !swizzled(9) && userData[10] >= 1;
}

void Driver::countCopy(int path, std::size_t bytes, std::chrono::steady_clock::time_point started,
                       std::chrono::steady_clock::time_point locked, const VulkanDevice::CopyOutcome* outcome) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile || path < 0 || path >= CopyPaths)
        return;
    using Outcome = VulkanDevice::CopyOutcome;
    static std::mutex countsMutex;
    static std::uint64_t counts[CopyPaths] = {};
    static std::uint64_t refused[Outcome::Refusals] = {};
    static std::uint64_t cpuBytes = 0, gpuBytes = 0, aliasedBytes = 0, syncs = 0, settledBySignal = 0, sourceWaits = 0;
    static double syncedMs = 0, totalMs = 0, lockWaitMs = 0, sourceWaitMs = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(countsMutex);
    ++counts[path];
    if (path == CopyCpu)
        cpuBytes += bytes;
    if (path == CopyGpu)
        gpuBytes += bytes;
    if (path == CopyAliased)
        aliasedBytes += bytes;
    if (outcome != nullptr) {
        if (outcome->synced) {
            ++syncs;
            syncedMs += outcome->syncMs;
        }
        if (outcome->reason > Outcome::None && outcome->reason < Outcome::Refusals)
            ++refused[outcome->reason];
        if (outcome->sourceSettledBySignal)
            ++settledBySignal;
        if (outcome->waitedForSource) {
            ++sourceWaits;
            sourceWaitMs += outcome->waitMs;
        }
    }
    totalMs += std::chrono::duration<double, std::milli>(now - started).count();
    lockWaitMs += std::chrono::duration<double, std::milli>(locked - started).count();
    if (now - lastReport < std::chrono::seconds(10))
        return;
    lastReport = now;
    std::uint64_t matched = 0;
    for (const auto count : counts)
        matched += count;
    const auto verify = VulkanDevice::CopyVerifyCounts();
    const auto aliasing = VulkanDevice::CopyAliasCounts();

    const auto known = ShaderMemory::KnownValues();
    const auto load = [](const std::atomic<std::uint64_t>& counter) {
        return static_cast<unsigned long long>(counter.load(std::memory_order_relaxed));
    };
    std::fprintf(
        stderr,
        "[copy] %llu copy kernels matched: cpu %llu (%.1f MiB; source settled by signal %llu), gpu %llu (%.0f MiB; "
        "%llu waited %.0f ms), aliased %llu (%.0f MiB; %llu destination images created; transfers instead: no source "
        "image %llu, shape %llu, refused %llu); refused cpu: size %llu, image %llu, shadow %llu, source pending %llu, "
        "source unsettled %llu, destination pending %llu, destination unsettled %llu, label %llu, reader %llu; kernel "
        "run instead: count record pending %llu, shape %llu, not imported %llu, overlapping %llu, inaccessible %llu, "
        "over gpu max %llu; waited for the source %llu / %.0f ms; verify: %llu verified, source changed %llu, "
        "destination changed %llu, reader missed %llu; known values: %llu entries, %llu reads served, %llu verified, "
        "%llu mismatches; took %.0f ms (%.0f ms waiting for the mutex)\n",
        static_cast<unsigned long long>(matched), static_cast<unsigned long long>(counts[CopyCpu]),
        cpuBytes / 1048576.0, static_cast<unsigned long long>(settledBySignal),
        static_cast<unsigned long long>(counts[CopyGpu]), gpuBytes / 1048576.0, static_cast<unsigned long long>(syncs),
        syncedMs, static_cast<unsigned long long>(counts[CopyAliased]), aliasedBytes / 1048576.0,
        static_cast<unsigned long long>(aliasing.created), static_cast<unsigned long long>(aliasing.noSource),
        static_cast<unsigned long long>(aliasing.shape), static_cast<unsigned long long>(aliasing.refused),
        static_cast<unsigned long long>(refused[Outcome::Size]),
        static_cast<unsigned long long>(refused[Outcome::Image]),
        static_cast<unsigned long long>(refused[Outcome::Shadow]),
        static_cast<unsigned long long>(refused[Outcome::SourcePending]),
        static_cast<unsigned long long>(refused[Outcome::SourceUnsettled]),
        static_cast<unsigned long long>(refused[Outcome::DestinationPending]),
        static_cast<unsigned long long>(refused[Outcome::DestinationUnsettled]),
        static_cast<unsigned long long>(refused[Outcome::Label]),
        static_cast<unsigned long long>(refused[Outcome::Reader]),
        static_cast<unsigned long long>(counts[CopyRecordPending]), static_cast<unsigned long long>(counts[CopyShape]),
        static_cast<unsigned long long>(counts[CopyNotImported]),
        static_cast<unsigned long long>(counts[CopyOverlapping]),
        static_cast<unsigned long long>(counts[CopyInaccessible]),
        static_cast<unsigned long long>(counts[CopyOverGpuMax]), static_cast<unsigned long long>(sourceWaits),
        sourceWaitMs, static_cast<unsigned long long>(verify.verified),
        static_cast<unsigned long long>(verify.sourceChanged),
        static_cast<unsigned long long>(verify.destinationChanged),
        static_cast<unsigned long long>(verify.readerMissed), load(knownValueEntries), load(knownValueReads),
        static_cast<unsigned long long>(known.verified) + load(knownValueVerified),
        static_cast<unsigned long long>(known.mismatches) + load(knownValueMismatches), totalMs, lockWaitMs);
}

void Driver::noteCopyWriter(std::uint64_t program, std::uint64_t begin, std::uint64_t end, std::uint32_t queue,
                            std::span<const std::byte> value, std::uint64_t generation) {
    if (!(writeEvidenceEnabled() || traceCapSync()))
        return;
    std::shared_ptr<const std::vector<std::byte>> known;
    if (!value.empty() && value.size() == end - begin && generation != 0) {
        known = std::make_shared<const std::vector<std::byte>>(value.begin(), value.end());
        knownValueEntries.fetch_add(1, std::memory_order_relaxed);
    }
    std::lock_guard lock(writtenBuffersMutex);
    writtenBuffers.push_back({program, begin, end, ++writtenBufferSerial, queue, false, std::move(known), generation});
    while (writtenBuffers.size() > WrittenBufferRing)
        writtenBuffers.pop_front();
}

bool Driver::copyKnownValues() {
    static const bool enabled = std::getenv("APS5_NO_COPY_KNOWN_VALUES") == nullptr;
    return enabled;
}

bool Driver::knownValueVerify() {
    static const bool verify = std::getenv("APS5_VERIFY_KNOWN_VALUES") != nullptr;
    return verify;
}

bool Driver::copyBuffer(QueueState& queue, std::uint32_t queueId, std::span<const std::uint32_t> packet,
                        std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData,
                        const ShaderRecompiler::ShaderComputeStageInfo& compute,
                        const std::shared_ptr<VulkanDevice>& localDevice, std::uint64_t programAddress) {
    if (!matchesCopyKernel(code, userData, compute))
        return false;
    const auto started = std::chrono::steady_clock::now();
    static const std::size_t cpuMax = [] {
        if (std::getenv("APS5_NO_CPU_COPY") != nullptr)
            return std::size_t{0};
        const char* text = std::getenv("APS5_COPY_HLE_CPU_MAX");
        return text ? static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) : std::size_t{262144};
    }();
    static const std::size_t gpuMax = [] {
        const char* text = std::getenv("APS5_COPY_HLE_GPU_MAX");
        return text ? static_cast<std::size_t>(std::strtoull(text, nullptr, 10))
                    : std::numeric_limits<std::size_t>::max();
    }();
    const auto base = [&](std::size_t word) {
        return userData[word] | (static_cast<std::uint64_t>(userData[word + 1] & 0xffffu) << 32u);
    };
    const auto source = base(0);
    const auto destination = base(4);
    const auto record = base(8);
    std::array<std::uint32_t, 3> groups{packet[1], packet[2], packet[3]};
    if ((packet[4] & 0x20u) != 0) {
        for (std::uint32_t axis = 0; axis < 3; ++axis) {
            const auto threads = std::max(readRegister(queue.shader, 0x207 + axis) & 0xffffu, 1u);
            groups[axis] = (groups[axis] + threads - 1) / threads;
        }
    }
    if (groups[1] != 1 || groups[2] != 1) {
        countCopy(CopyShape, 0, started, started);
        return false;
    }
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Copy);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto locked = std::chrono::steady_clock::now();

    recordLabelsForPacket(localDevice.get(), queueId);

    if (queueId == 0)
        localDevice->ReapRecorded();

    const auto labelPending = [&](std::uint64_t address, std::size_t bytes) {
        for (auto dword = address & ~std::uint64_t{3}; dword < address + bytes; dword += 4) {
            if (localDevice->PendingLabel(dword, 4, 0).has_value())
                return true;
        }
        return false;
    };
    const bool recordAccessible = GuestMemory::Accessible(reinterpret_cast<const void*>(record), 8);

    bool published = false;
    if (!recordAccessible || Graphics::Recorder::SnapshotWriteOverlaps(record, 8) ||
        Graphics::StorageTexture::FlushPending(record, 8, nullptr, "buffer copy count", Graphics::PublishScope::Whole,
                                               &published) ||
        published || labelPending(record, 8)) {
        if (traceCopy())
            traceCopyPending(recordAccessible ? "count record" : "inaccessible count record", record, 8, source,
                             destination, 0, localDevice);
        countCopy(CopyRecordPending, 0, started, locked);
        return false;
    }
    std::uint32_t count = 0, period = 0;
    std::memcpy(&count, reinterpret_cast<const void*>(record), 4);
    std::memcpy(&period, reinterpret_cast<const void*>(record + 4), 4);
    const auto bytes = static_cast<std::size_t>(count) * 4u;

    if (count == 0 || period < count || groups[0] != (count + 63u) / 64u || count > userData[2] ||
        count > userData[6] || bytes > (16u << 20u) || source % 4 != 0 || destination % 4 != 0 ||
        (record < destination + bytes && destination < record + 8)) {
        countCopy(CopyShape, bytes, started, locked);
        return false;
    }
    if (source < destination + bytes && destination < source + bytes) {
        countCopy(CopyOverlapping, bytes, started, locked);
        return false;
    }
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(source), bytes) ||
        !GuestMemory::Accessible(reinterpret_cast<const void*>(destination), bytes, true)) {
        countCopy(CopyInaccessible, bytes, started, locked);
        return false;
    }

    const auto outcome = localDevice->CopyBuffer(
        destination, source, bytes, cpuMax, gpuMax, copyKnownValues() ? KnownValueMaxBytes : 0, programAddress, queueId,
        [&](std::span<const std::byte> value, std::uint64_t generation) {
            noteCopyWriter(programAddress, destination, destination + bytes, queueId, value, generation);
        });
    countCopy(outcome.path, bytes, started, locked, &outcome);
    if (traceCopy() && bytes <= cpuMax && outcome.path != CopyCpu)
        traceCopyRefused(outcome, source, destination, bytes, localDevice);
    if (outcome.path == CopyNotImported || outcome.path == CopyOverGpuMax)
        return false;

    if (outcome.path == CopyCpu)
        noteForeignWriter(destination, destination + bytes, queueId);
    return true;
}

void Driver::traceCopyRefused(const VulkanDevice::CopyOutcome& outcome, std::uint64_t source, std::uint64_t destination,
                              std::size_t bytes, const std::shared_ptr<VulkanDevice>& localDevice) {
    using Outcome = VulkanDevice::CopyOutcome;
    static constexpr const char* rules[Outcome::Refusals] = {"none",
                                                             "size",
                                                             "pending image",
                                                             "source pending",
                                                             "source unsettled",
                                                             "destination pending",
                                                             "destination unsettled",
                                                             "label",
                                                             "reader",
                                                             "shadowed"};
    static constexpr const char* readers[] = {"dispatch element", "gpu copy",       "address-based",
                                              "indirect",         "storage upload", "copy source"};
    if (outcome.reason == Outcome::SourcePending || outcome.reason == Outcome::SourceUnsettled) {
        traceCopyPending(rules[outcome.reason], source, bytes, source, destination, bytes, localDevice);
        return;
    }
    if (outcome.reason == Outcome::DestinationPending || outcome.reason == Outcome::DestinationUnsettled ||
        outcome.reason == Outcome::Label) {
        traceCopyPending(rules[outcome.reason], destination, bytes, source, destination, bytes, localDevice);
        return;
    }
    static std::atomic<int> shown{0};
    if (shown.fetch_add(1) >= 160 || outcome.reason <= Outcome::None || outcome.reason >= Outcome::Refusals)
        return;
    if (outcome.reason == Outcome::Reader) {
        const auto kind = outcome.readerKind >= 0 && outcome.readerKind < 6 ? readers[outcome.readerKind] : "?";
        std::fprintf(stderr,
                     "[copyhle] reader refused the CPU copy 0x%llx -> 0x%llx (0x%zx bytes): batch %llu%s of queue 0x%x "
                     "reads the destination in place (%s)\n",
                     static_cast<unsigned long long>(source), static_cast<unsigned long long>(destination), bytes,
                     static_cast<unsigned long long>(outcome.readerSerial), outcome.readerOpen ? " (open)" : "",
                     outcome.readerQueue, kind);
        return;
    }
    std::fprintf(stderr, "[copyhle] %s refused the CPU copy 0x%llx -> 0x%llx (0x%zx bytes)\n", rules[outcome.reason],
                 static_cast<unsigned long long>(source), static_cast<unsigned long long>(destination), bytes);
}

bool Driver::traceCopy() {
    static const bool enabled = std::getenv("APS5_TRACE_COPY_HLE") != nullptr;
    return enabled;
}

void Driver::traceCopyPending(const char* what, std::uint64_t address, std::size_t bytes, std::uint64_t source,
                              std::uint64_t destination, std::size_t copyBytes,
                              const std::shared_ptr<VulkanDevice>& localDevice) {
    static std::atomic<int> shown{0};
    if (shown.fetch_add(1) >= 160)
        return;
    char pending[160];
    std::snprintf(pending, sizeof(pending), " no recorded write");
    if (auto* recorder = Graphics::Recorder::Active()) {
        if (const auto info = recorder->DescribePendingWrite(address, bytes)) {
            std::snprintf(pending, sizeof(pending), " batch %llu%s%s range 0x%llx+0x%llx (%zu batches to finish)",
                          static_cast<unsigned long long>(info->serial), info->open ? " open" : "",
                          info->signaled ? " signaled" : " unsignaled",
                          static_cast<unsigned long long>(info->rangeBegin),
                          static_cast<unsigned long long>(info->rangeEnd - info->rangeBegin), info->batchesToFinish);
        }
    }
    std::uint32_t labels = 0;
    for (auto dword = address & ~std::uint64_t{3}; dword < address + bytes; dword += 4) {
        if (localDevice->PendingLabel(dword, 4, 0).has_value())
            ++labels;
    }
    std::fprintf(
        stderr,
        "[copyhle] %s 0x%llx+0x%zx pending (copy 0x%llx -> 0x%llx, 0x%zx bytes):%s; label dwords %u; writers:%s\n",
        what, static_cast<unsigned long long>(address), bytes, static_cast<unsigned long long>(source),
        static_cast<unsigned long long>(destination), copyBytes, pending, labels,
        describeWriters(address, address + bytes).c_str());
}

}
