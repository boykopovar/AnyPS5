#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int faultCalls = 0;
bool faultFails = false;
VkResult faultDataResult = VK_SUCCESS;
std::uint32_t faultDataAddressCount = 2;
std::uint64_t faultDataVendorBinarySize = 0;

VKAPI_ATTR VkResult VKAPI_CALL MockFaultInfo(VkDevice, VkDeviceFaultCountsEXT* counts, VkDeviceFaultInfoEXT* info) {
    ++faultCalls;
    if (faultFails) return VK_ERROR_DEVICE_LOST;
    if (info == nullptr) {
        counts->addressInfoCount = 2;
        counts->vendorInfoCount = 1;
        counts->vendorBinarySize = 4096;
        return VK_SUCCESS;
    }
    Require(counts->vendorBinarySize == 0, "the vendor binary was requested");
    Require(counts->addressInfoCount == 2 && info->pAddressInfos != nullptr, "the address array was not passed");
    Require(counts->vendorInfoCount == 1 && info->pVendorInfos != nullptr, "the vendor array was not passed");
    std::memset(info->description, 'd', sizeof(info->description));
    std::memcpy(info->description, "page fault", 10);
    info->description[10] = '\0';
    info->pAddressInfos[0] = {VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT, 0x1234000ull, 0x1000ull};
    info->pAddressInfos[1] = {VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT, 0xabcdull, 0x3ull};
    std::memset(info->pVendorInfos[0].description, 'v', sizeof(info->pVendorInfos[0].description));
    info->pVendorInfos[0].vendorFaultCode = 0x77ull;
    info->pVendorInfos[0].vendorFaultData = 0x88ull;
    counts->addressInfoCount = faultDataAddressCount;
    counts->vendorBinarySize = faultDataVendorBinarySize;
    return faultDataResult;
}

std::uint32_t checkpointCount = 0;

VKAPI_ATTR void VKAPI_CALL MockCheckpointData(VkQueue, std::uint32_t* count, VkCheckpointDataNV* data) {
    if (data == nullptr) {
        *count = checkpointCount;
        return;
    }
    Require(*count == checkpointCount, "the checkpoint array has the wrong size");
    for (std::uint32_t i = 0; i < *count; ++i) {
        Require(data[i].sType == VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV, "checkpoint entries were not initialised");
        data[i].stage = i == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        data[i].pCheckpointMarker = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10000ull + i * 0x100ull));
    }
}

VkDevice FakeDevice() {
    return reinterpret_cast<VkDevice>(static_cast<std::uintptr_t>(0x1000));
}

VkQueue FakeQueue() {
    return reinterpret_cast<VkQueue>(static_cast<std::uintptr_t>(0x2000));
}

void Diagnostics() {
    using AgcDriver::ChooseDeviceDiagnostics;
    constexpr VkDeviceDiagnosticsConfigFlagsNV all = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV;
    for (const bool configSupported : {false, true}) {
        for (const bool checkpointsSupported : {false, true}) {
            const auto off = ChooseDeviceDiagnostics(false, configSupported, checkpointsSupported);
            Require(!off.config && !off.checkpoints && off.flags == 0, "diagnostics were chosen without APS5_NV_DIAGNOSTICS");
        }
    }
    for (const bool checkpointsSupported : {false, true}) {
        const auto unsupported = ChooseDeviceDiagnostics(true, false, checkpointsSupported);
        Require(!unsupported.config && !unsupported.checkpoints && unsupported.flags == 0, "diagnostics were chosen on a device without VK_NV_device_diagnostics_config");
    }
    const auto alone = ChooseDeviceDiagnostics(true, true, false);
    Require(alone.config && !alone.checkpoints && alone.flags == all, "the diagnostics config without checkpoint support did not enable every crash-dump flag alone");
    const auto both = ChooseDeviceDiagnostics(true, true, true);
    Require(both.config && both.checkpoints && both.flags == all, "the diagnostics config did not add the diagnostic checkpoints the device supports");
}

void FaultInfo() {
    faultCalls = 0;
    faultFails = false;
    faultDataResult = VK_SUCCESS;
    faultDataAddressCount = 2;
    faultDataVendorBinarySize = 0;
    const auto lines = AgcDriver::DeviceFaultReport(FakeDevice(), MockFaultInfo, VK_NULL_HANDLE, nullptr);
    Require(faultCalls == 2, "the fault info was not queried for counts and then for data");
    Require(lines.size() == 4, "the report does not have a result, two addresses and a vendor line");
    Require(lines[0] == "[device-fault] result 0: page fault", "the description line changed");
    Require(lines[1] == "[device-fault] read invalid at 0x1234000 (precision 0x1000)", "the read fault line changed");
    Require(lines[2] == "[device-fault] instruction pointer fault at 0xabcd (precision 0x3)", "the instruction pointer line changed");
    Require(lines[3] == "[device-fault] vendor: " + std::string(VK_MAX_DESCRIPTION_SIZE, 'v') + " (code 0x77, data 0x88)", "an unterminated vendor description was not bounded");
}

void FaultInfoFails() {
    faultCalls = 0;
    faultFails = true;
    const auto lines = AgcDriver::DeviceFaultReport(FakeDevice(), MockFaultInfo, VK_NULL_HANDLE, nullptr);
    Require(faultCalls == 1, "a failed count query was followed by a data query");
    Require(lines.size() == 1 && lines[0] == "[device-fault] vkGetDeviceFaultInfoEXT reported nothing", "a failed query was not reported");
}

void Checkpoints() {
    checkpointCount = 2;
    auto lines = AgcDriver::DeviceFaultReport(VK_NULL_HANDLE, nullptr, FakeQueue(), MockCheckpointData);
    Require(lines.size() == 2, "each checkpoint gets one line");
    Require(lines[0] == "[device-fault] checkpoint: program 0x10000 at stage 0x1", "the first checkpoint line changed");
    Require(lines[1] == "[device-fault] checkpoint: program 0x10100 at stage 0x2000", "the second checkpoint line changed");
    checkpointCount = 0;
    lines = AgcDriver::DeviceFaultReport(VK_NULL_HANDLE, nullptr, FakeQueue(), MockCheckpointData);
    Require(lines.size() == 1 && lines[0] == "[device-fault] no checkpoint reached", "an empty checkpoint list was not reported");
}

void Nothing() {
    faultCalls = 0;
    Require(AgcDriver::DeviceFaultReport(FakeDevice(), nullptr, FakeQueue(), nullptr).empty(), "a device without either extension reported something");
    Require(faultCalls == 0, "the fault info was queried without the extension");
}

void BufferMarkers() {
    AgcDriver::BufferMarkerLog log;
    Require(log.Report(0, 0) == std::vector<std::string>{"[device-fault] no buffer marker reached"}, "an untouched marker buffer was not reported");
    Require(log.Next(0x10000) == 1 && log.Next(0x10100) == 2 && log.Next(0x10200) == 3, "markers are not numbered from 1");
    const auto lines = log.Report(3, 1);
    Require(lines.size() == 4, "a hang two markers past the last finished one does not get four lines");
    Require(lines[0] == "[device-fault] last dispatch reached: marker 3, program 0x10200", "the reached line changed");
    Require(lines[1] == "[device-fault] last dispatch finished: marker 1, program 0x10000", "the finished line changed");
    Require(lines[2] == "[device-fault] unfinished dispatch: marker 2, program 0x10100", "the first unfinished line changed");
    Require(lines[3] == "[device-fault] unfinished dispatch: marker 3, program 0x10200", "the second unfinished line changed");
    Require(log.Report(3, 3).size() == 2, "a finished queue listed unfinished dispatches");
    const auto first = log.Report(1, 0);
    Require(first.size() == 3 && first[1] == "[device-fault] no dispatch finished" && first[2] == "[device-fault] unfinished dispatch: marker 1, program 0x10000", "a hang in the first dispatch was not reported as nothing finished");
    Require(log.Report(2, 3).size() == 2, "markers finished past the reached one listed unfinished dispatches");
}

void BufferMarkersWrap() {
    AgcDriver::BufferMarkerLog log;
    for (std::uint32_t i = 0; i <= AgcDriver::BufferMarkerLog::Capacity; ++i) static_cast<void>(log.Next(0x20000 + i));
    const auto lines = log.Report(AgcDriver::BufferMarkerLog::Capacity + 1, 1);
    Require(lines[0] == "[device-fault] last dispatch reached: marker 4097, program 0x21000", "the newest marker was not kept");
    Require(lines[1] == "[device-fault] last dispatch finished: marker 1, program not in the log", "an overwritten marker kept a program");
    Require(lines.size() == 2 + AgcDriver::BufferMarkerLog::Listed + 1 && lines.back() == "[device-fault] 4088 more unfinished dispatches", "a full log of unfinished markers was not listed and counted");
    Require(log.Report(AgcDriver::BufferMarkerLog::Capacity + 1, 0).size() == 2, "more unfinished markers than the log holds were listed");
    const auto pending = log.Report(AgcDriver::BufferMarkerLog::Capacity + 1, AgcDriver::BufferMarkerLog::Capacity - 19);
    Require(pending.size() == 2 + AgcDriver::BufferMarkerLog::Listed + 1, "a long unfinished run was not cut to the listed count");
    Require(pending.back() == "[device-fault] 12 more unfinished dispatches", "the count of unlisted unfinished dispatches changed");
}

std::vector<ShaderRecompiler::BdaAbi::Range> SnapshotRanges() {
    return {
        {0x10000ull, 0x11000ull, 0x10000ull, ShaderRecompiler::BdaAbi::Read, 0},
        {0x12000ull, 0x13000ull, 0x12000ull, ShaderRecompiler::BdaAbi::Read | ShaderRecompiler::BdaAbi::Write, 0},
    };
}

void SubmissionSnapshots() {
    AgcDriver::SubmissionSnapshots snapshots;
    Require(!snapshots.Find(0).has_value(), "serial zero found a snapshot");
    Require(!snapshots.Find(7).has_value(), "a missing serial found a snapshot");
    const auto ranges = std::make_shared<const std::vector<ShaderRecompiler::BdaAbi::Range>>(SnapshotRanges());
    snapshots.NoteSubmit(0, 9, ranges);
    Require(!snapshots.Find(0).has_value(), "serial zero kept a snapshot");
    snapshots.NoteSubmit(7, 3, ranges);
    const auto found = snapshots.Find(7);
    Require(found.has_value() && found->submitSerial == 7 && found->spaceSerial == 3 && found->ranges == ranges, "the noted snapshot was not found");
    snapshots.NoteSubmit(7 + AgcDriver::SubmissionSnapshots::Capacity, 4, ranges);
    Require(!snapshots.Find(7).has_value(), "an evicted serial kept its snapshot");
    const auto evictor = snapshots.Find(7 + AgcDriver::SubmissionSnapshots::Capacity);
    Require(evictor.has_value() && evictor->spaceSerial == 4 && evictor->ranges == ranges, "the evicting snapshot was not found");
    snapshots.NoteSubmit(0x100000005ull, 5, ranges);
    Require(!snapshots.Find(5).has_value(), "a truncated marker matched a wider serial");
    Require(snapshots.Find(0x100000005ull).has_value(), "a wide serial was not found exactly");
}

void SubmissionSnapshotsConcurrent() {
    AgcDriver::SubmissionSnapshots snapshots;
    std::atomic<bool> stop{false};
    std::atomic<bool> readersReady{false};
    std::atomic<std::uint64_t> published{0};
    std::thread writer([&] {
        while (!readersReady.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::uint64_t serial = 1; serial <= 500; ++serial) {
            auto owned = std::make_shared<std::vector<ShaderRecompiler::BdaAbi::Range>>();
            owned->push_back({serial * 0x1000ull, serial * 0x1000ull + 0x100ull, serial * 0x1000ull, ShaderRecompiler::BdaAbi::Read, 0});
            snapshots.NoteSubmit(serial, serial + 1, owned);
            published.store(serial, std::memory_order_release);
        }
        stop.store(true, std::memory_order_release);
    });
    auto reader = [&] {
        readersReady.store(true, std::memory_order_release);
        while (!stop.load(std::memory_order_acquire)) {
            const std::uint64_t serial = published.load(std::memory_order_acquire);
            if (serial == 0) continue;
            const auto found = snapshots.Find(serial);
            if (!found.has_value()) continue;
            Require(found->submitSerial == serial && found->spaceSerial == serial + 1 && found->ranges != nullptr && found->ranges->size() == 1 && (*found->ranges)[0].begin == serial * 0x1000ull, "a concurrent snapshot read mixed serials or ranges");
        }
    };
    std::thread first(reader);
    std::thread second(reader);
    writer.join();
    first.join();
    second.join();
    for (std::uint64_t serial = 500 - AgcDriver::SubmissionSnapshots::Capacity + 1; serial <= 500; ++serial) {
        const auto found = snapshots.Find(serial);
        Require(found.has_value() && found->submitSerial == serial && found->ranges != nullptr && (*found->ranges)[0].begin == serial * 0x1000ull, "a published snapshot was lost or mixed");
    }
    Require(!snapshots.Find(500 - AgcDriver::SubmissionSnapshots::Capacity).has_value(), "an evicted snapshot was still found");
}

void FaultRangeHit() {
    const auto snapshot = SnapshotRanges();
    const auto lines = AgcDriver::DeviceFaultRangeLines(VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT, 0x12abcull, 0x100ull, snapshot, 7, 3);
    Require(lines.size() == 1, "a contained fault range reports more than its owner");
    Require(lines[0] == "[device-fault] write invalid at 0x12abc (precision 0x100) in guest range [0x12000, 0x13000) read write (batch 7, space 3)", "the owner line changed");
}

void FaultRangeStraddle() {
    const auto snapshot = SnapshotRanges();
    const auto lines = AgcDriver::DeviceFaultRangeLines(VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT, 0x10fffull, 0x2000ull, snapshot, 7, 3);
    Require(lines.size() == 2, "a fault range escaping its point owner does not report the miss and the next range");
    Require(lines[0] == "[device-fault] write invalid at 0x10fff (precision 0x2000) has no guest owner in batch 7 (internal or unmapped)", "the miss line changed");
    Require(lines[1] == "[device-fault] next guest range [0x12000, 0x13000) read write starts 0x1 after the fault range", "the next range line changed");
}

void FaultRangeMiss() {
    const auto snapshot = SnapshotRanges();
    auto gap = AgcDriver::DeviceFaultRangeLines(VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT, 0x11800ull, 0, snapshot, 7, 3);
    Require(gap.size() == 3, "a fault in a gap does not report its neighbours");
    Require(gap[0] == "[device-fault] read invalid at 0x11800 (precision 0x0) has no guest owner in batch 7 (internal or unmapped)", "the gap miss line changed");
    Require(gap[1] == "[device-fault] previous guest range [0x10000, 0x11000) read ends 0x800 before the fault range", "the previous range line changed");
    Require(gap[2] == "[device-fault] next guest range [0x12000, 0x13000) read write starts 0x800 after the fault range", "the gap next line changed");
    auto low = AgcDriver::DeviceFaultRangeLines(VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT, 0x500ull, 0x100ull, snapshot, 7, 3);
    Require(low.size() == 2 && low[1] == "[device-fault] next guest range [0x10000, 0x11000) read starts 0xfa01 after the fault range", "a fault below every range misses its next range");
    auto high = AgcDriver::DeviceFaultRangeLines(VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT, 0x20000ull, 0, snapshot, 7, 3);
    Require(high.size() == 2 && high[1] == "[device-fault] previous guest range [0x12000, 0x13000) read write ends 0xd000 before the fault range", "a fault above every range misses its previous range");
    const std::vector<ShaderRecompiler::BdaAbi::Range> empty;
    auto none = AgcDriver::DeviceFaultRangeLines(VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT, 0x12000ull, 0, empty, 7, 3);
    Require(none.size() == 1 && none[0] == "[device-fault] read invalid at 0x12000 (precision 0x0) has no guest owner in batch 7 (internal or unmapped)", "an empty snapshot reports a neighbour");
}

void BatchSerialMismatch() {
    AgcDriver::SubmissionSnapshots snapshots;
    snapshots.NoteSubmit(7, 3, std::make_shared<const std::vector<ShaderRecompiler::BdaAbi::Range>>(SnapshotRanges()));
    const auto missing = AgcDriver::DeviceFaultBatchLines(VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT, 0x12abcull, 0x100ull, 8, snapshots);
    Require(missing.size() == 1 && missing[0] == "[device-fault] no guest snapshot for batch 8", "a serial without a snapshot was correlated");
    const auto hit = AgcDriver::DeviceFaultBatchLines(VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT, 0x12abcull, 0x100ull, 7, snapshots);
    Require(hit.size() == 1 && hit[0] == "[device-fault] write invalid at 0x12abc (precision 0x100) in guest range [0x12000, 0x13000) read write (batch 7, space 3)", "the batch snapshot was not correlated");
}

void FaultIncomplete() {
    faultCalls = 0;
    faultFails = false;
    faultDataResult = VK_INCOMPLETE;
    faultDataAddressCount = 5;
    faultDataVendorBinarySize = 0;
    const auto lines = AgcDriver::DeviceFaultReport(FakeDevice(), MockFaultInfo, VK_NULL_HANDLE, nullptr);
    Require(faultCalls == 2, "an incomplete fault info was not queried for counts and then for data");
    Require(lines.size() == 4, "an incomplete fault info does not report the addresses it returned");
    Require(lines[0] == "[device-fault] result 5: page fault", "an incomplete result was not reported");
    faultDataResult = VK_ERROR_DEVICE_LOST;
    const auto failed = AgcDriver::DeviceFaultReport(FakeDevice(), MockFaultInfo, VK_NULL_HANDLE, nullptr);
    Require(failed.size() == 1 && failed[0] == "[device-fault] vkGetDeviceFaultInfoEXT reported nothing", "a failed data query was not reported");
    faultDataResult = VK_SUCCESS;
    faultDataAddressCount = 2;
}

void VendorBinaryUntouched() {
    faultCalls = 0;
    faultFails = false;
    faultDataResult = VK_SUCCESS;
    faultDataAddressCount = 2;
    faultDataVendorBinarySize = 4096;
    const auto lines = AgcDriver::DeviceFaultReport(FakeDevice(), MockFaultInfo, VK_NULL_HANDLE, nullptr);
    Require(faultCalls == 2, "the fault info was not queried for counts and then for data");
    Require(lines.size() == 4, "a reported vendor binary changed the address report");
    Require(lines[1] == "[device-fault] read invalid at 0x1234000 (precision 0x1000)", "the read fault line changed");
    faultDataVendorBinarySize = 0;
}

void AddressKinds() {
    Require(std::strcmp(AgcDriver::DeviceFaultAddressKind(VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT), "none") == 0, "none");
    Require(std::strcmp(AgcDriver::DeviceFaultAddressKind(VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT), "write invalid") == 0, "write invalid");
    Require(std::strcmp(AgcDriver::DeviceFaultAddressKind(VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT), "execute invalid") == 0, "execute invalid");
    Require(std::strcmp(AgcDriver::DeviceFaultAddressKind(VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT), "instruction pointer unknown") == 0, "instruction pointer unknown");
    Require(std::strcmp(AgcDriver::DeviceFaultAddressKind(VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT), "instruction pointer invalid") == 0, "instruction pointer invalid");
    Require(std::strcmp(AgcDriver::DeviceFaultAddressKind(static_cast<VkDeviceFaultAddressTypeEXT>(99)), "unknown") == 0, "an unknown kind");
}

}

int main() {
    try {
        Diagnostics();
        FaultInfo();
        FaultInfoFails();
        FaultIncomplete();
        VendorBinaryUntouched();
        Checkpoints();
        Nothing();
        AddressKinds();
        SubmissionSnapshots();
        SubmissionSnapshotsConcurrent();
        FaultRangeHit();
        FaultRangeStraddle();
        FaultRangeMiss();
        BatchSerialMismatch();
        BufferMarkers();
        BufferMarkersWrap();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "agc_device_fault: %s\n", error.what());
        return 1;
    }
    std::puts("agc_device_fault: passed");
    return 0;
}
