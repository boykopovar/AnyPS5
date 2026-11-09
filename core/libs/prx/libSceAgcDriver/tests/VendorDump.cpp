#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver {
bool ChooseDeviceFaultVendorBinary(bool requested, bool faultSupported, bool vendorBinarySupported) noexcept;
const char* DeviceFaultVendorDumpPath() noexcept;
bool WriteDeviceFaultVendorBinary(const char* path, const void* data, std::size_t size) noexcept;
void DumpDeviceFaultVendorBinary(VkDevice device, PFN_vkGetDeviceFaultInfoEXT faultInfo, const char* path) noexcept;
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void SetEnv(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, "cannot set the test environment");
#else
    Require(setenv(name, value, 1) == 0, "cannot set the test environment");
#endif
}

void UnsetEnv(const char* name) {
#ifdef _WIN32
    Require(_putenv_s(name, "") == 0, "cannot clear the test environment");
#else
    Require(unsetenv(name) == 0, "cannot clear the test environment");
#endif
}

VkDevice FakeDevice() {
    return reinterpret_cast<VkDevice>(static_cast<std::uintptr_t>(0x1000));
}

std::string TempFile() {
    return (std::filesystem::temp_directory_path() / "aps5-vendor-dump-test.bin").string();
}

std::vector<std::byte> ReadFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    Require(static_cast<bool>(input), "the dump file was not created");
    const std::vector<char> chars{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    if (!chars.empty()) std::memcpy(bytes.data(), chars.data(), chars.size());
    return bytes;
}

void Choose() {
    Require(AgcDriver::ChooseDeviceFaultVendorBinary(true, true, true), "the vendor binary was not chosen when requested and supported");
    Require(!AgcDriver::ChooseDeviceFaultVendorBinary(true, true, false), "the vendor binary was chosen without vendor binary support");
    Require(!AgcDriver::ChooseDeviceFaultVendorBinary(true, false, true), "the vendor binary was chosen without device fault support");
    Require(!AgcDriver::ChooseDeviceFaultVendorBinary(true, false, false), "the vendor binary was chosen on a device without fault support");
    Require(!AgcDriver::ChooseDeviceFaultVendorBinary(false, true, true), "the vendor binary was chosen without APS5_DUMP_DEVICE_FAULT");
    Require(!AgcDriver::ChooseDeviceFaultVendorBinary(false, false, false), "the vendor binary was chosen without a request or support");
}

void DumpPath() {
    UnsetEnv("APS5_DUMP_DEVICE_FAULT");
    Require(AgcDriver::DeviceFaultVendorDumpPath() == nullptr, "an unset dump path was reported");
    SetEnv("APS5_DUMP_DEVICE_FAULT", "");
    Require(AgcDriver::DeviceFaultVendorDumpPath() == nullptr, "an empty dump path was reported");
    SetEnv("APS5_DUMP_DEVICE_FAULT", "fault.bin");
    const char* path = AgcDriver::DeviceFaultVendorDumpPath();
    Require(path != nullptr && std::strcmp(path, "fault.bin") == 0, "the dump path was not reported");
    UnsetEnv("APS5_DUMP_DEVICE_FAULT");
    Require(AgcDriver::DeviceFaultVendorDumpPath() == nullptr, "the dump path survived its unset");
}

void Write() {
    const std::string path = TempFile();
    std::filesystem::remove(path);
    Require(!AgcDriver::WriteDeviceFaultVendorBinary(nullptr, "x", 1), "a null path was accepted");
    Require(!AgcDriver::WriteDeviceFaultVendorBinary("", "x", 1), "an empty path was accepted");
    Require(!AgcDriver::WriteDeviceFaultVendorBinary(path.c_str(), nullptr, 1), "a null dump was accepted");
    Require(!std::filesystem::exists(path), "a rejected dump created its file");
    const std::byte bytes[] = {std::byte{0xde}, std::byte{0xad}, std::byte{0xbe}, std::byte{0xef}};
    Require(AgcDriver::WriteDeviceFaultVendorBinary(path.c_str(), bytes, sizeof(bytes)), "the vendor binary was not written");
    const auto read = ReadFile(path);
    Require(read.size() == sizeof(bytes) && std::memcmp(read.data(), bytes, sizeof(bytes)) == 0, "the dump file does not hold the vendor binary");
    Require(AgcDriver::WriteDeviceFaultVendorBinary(path.c_str(), nullptr, 0), "an empty dump was rejected");
    Require(std::filesystem::file_size(path) == 0, "an empty dump did not truncate its file");
    std::filesystem::remove(path);
}

int dumpCalls = 0;
bool dumpFails = false;
VkDeviceSize dumpSize = 0;
std::vector<std::byte> dumpContents;

VKAPI_ATTR VkResult VKAPI_CALL MockDumpInfo(VkDevice device, VkDeviceFaultCountsEXT* counts, VkDeviceFaultInfoEXT* info) {
    ++dumpCalls;
    if (device != FakeDevice()) return VK_ERROR_DEVICE_LOST;
    if (dumpFails) return VK_ERROR_DEVICE_LOST;
    if (info == nullptr) {
        counts->addressInfoCount = 1;
        counts->vendorInfoCount = 1;
        counts->vendorBinarySize = dumpSize;
        return VK_SUCCESS;
    }
    if (counts->addressInfoCount != 0 || counts->vendorInfoCount != 0) return VK_ERROR_UNKNOWN;
    if (counts->vendorBinarySize != dumpContents.size() || info->pVendorBinaryData == nullptr) return VK_INCOMPLETE;
    std::memcpy(info->pVendorBinaryData, dumpContents.data(), dumpContents.size());
    return VK_SUCCESS;
}

void DumpWrites() {
    dumpCalls = 0;
    dumpFails = false;
    dumpContents = {std::byte{0xde}, std::byte{0xad}, std::byte{0xbe}, std::byte{0xef}};
    dumpSize = static_cast<VkDeviceSize>(dumpContents.size());
    const std::string path = TempFile();
    std::filesystem::remove(path);
    AgcDriver::DumpDeviceFaultVendorBinary(FakeDevice(), MockDumpInfo, path.c_str());
    Require(dumpCalls == 2, "the vendor binary was not queried for size and then for data");
    const auto read = ReadFile(path);
    Require(read == dumpContents, "the dump file does not hold the queried vendor binary");
    std::filesystem::remove(path);
}

void DumpSkips() {
    const std::string path = TempFile();
    std::filesystem::remove(path);
    dumpCalls = 0;
    AgcDriver::DumpDeviceFaultVendorBinary(VK_NULL_HANDLE, MockDumpInfo, path.c_str());
    AgcDriver::DumpDeviceFaultVendorBinary(FakeDevice(), nullptr, path.c_str());
    AgcDriver::DumpDeviceFaultVendorBinary(FakeDevice(), MockDumpInfo, nullptr);
    AgcDriver::DumpDeviceFaultVendorBinary(FakeDevice(), MockDumpInfo, "");
    Require(dumpCalls == 0, "the vendor binary was queried without a device, entry point or path");
    dumpFails = true;
    AgcDriver::DumpDeviceFaultVendorBinary(FakeDevice(), MockDumpInfo, path.c_str());
    Require(dumpCalls == 1, "a failed size query was followed by a data query");
    dumpFails = false;
    dumpSize = 0;
    AgcDriver::DumpDeviceFaultVendorBinary(FakeDevice(), MockDumpInfo, path.c_str());
    Require(!std::filesystem::exists(path), "an empty vendor binary created its file");
    std::filesystem::remove(path);
}

}

int main() {
    try {
        Choose();
        DumpPath();
        Write();
        DumpWrites();
        DumpSkips();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "agc_vendor_dump: %s\n", error.what());
        return 1;
    }
    std::puts("agc_vendor_dump: passed");
    return 0;
}
