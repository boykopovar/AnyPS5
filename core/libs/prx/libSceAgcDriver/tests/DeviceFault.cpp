#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int faultCalls = 0;
bool faultFails = false;

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
    info->pAddressInfos[0] = {VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT, 0x1234000ull, 0xfffull};
    info->pAddressInfos[1] = {VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT, 0xabcdull, 0x3ull};
    std::memset(info->pVendorInfos[0].description, 'v', sizeof(info->pVendorInfos[0].description));
    info->pVendorInfos[0].vendorFaultCode = 0x77ull;
    info->pVendorInfos[0].vendorFaultData = 0x88ull;
    return VK_SUCCESS;
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

void FaultInfo() {
    faultCalls = 0;
    faultFails = false;
    const auto lines = AgcDriver::DeviceFaultReport(FakeDevice(), MockFaultInfo, VK_NULL_HANDLE, nullptr);
    Require(faultCalls == 2, "the fault info was not queried for counts and then for data");
    Require(lines.size() == 4, "the report does not have a result, two addresses and a vendor line");
    Require(lines[0] == "[device-fault] result 0: page fault", "the description line changed");
    Require(lines[1] == "[device-fault] read invalid at 0x1234000 (precision 0xfff)", "the read fault line changed");
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
        FaultInfo();
        FaultInfoFails();
        Checkpoints();
        Nothing();
        AddressKinds();
        BufferMarkers();
        BufferMarkersWrap();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "agc_device_fault: %s\n", error.what());
        return 1;
    }
    std::puts("agc_device_fault: passed");
    return 0;
}
