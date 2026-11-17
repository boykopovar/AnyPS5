#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "Testing.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;

int ReadFromFile(int handle, char* data, int length) {
    return _ZN3WTF14FileSystemImpl12readFromFileEiPci(handle, data, length);
}

void TestReadsInOrder(const std::string& path) {
    const int handle = open_nid_postfix(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(handle >= 0);
    char buffer[16];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(ReadFromFile(handle, buffer, 4) == 4);
    Require(std::memcmp(buffer, "0123x", 5) == 0);
    Require(ReadFromFile(handle, buffer, sizeof(buffer)) == 6);
    Require(std::memcmp(buffer, "456789", 6) == 0);
    Require(ReadFromFile(handle, buffer, sizeof(buffer)) == 0);
    Require(ReadFromFile(handle, buffer, 0) == 0);
    Require(lseek_nid_postfix(handle, 2, 0) == 2);
    std::memset(buffer, 'x', sizeof(buffer));
    Require(ReadFromFile(handle, buffer, 3) == 3);
    Require(std::memcmp(buffer, "234x", 4) == 0);
    Require(close_nid_postfix(handle) == 0);
}

void TestFailures(const std::string& path) {
    char buffer[4] = {'x', 'x', 'x', 'x'};
    *__error_nid_postfix() = 0;
    Require(ReadFromFile(-1, buffer, sizeof(buffer)) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);
    const int handle = open_nid_postfix(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(handle >= 0);
    *__error_nid_postfix() = 0;
    Require(ReadFromFile(handle, buffer, -1) == -1);
    Require(*__error_nid_postfix() == GuestEinval);
    Require(std::memcmp(buffer, "xxxx", 4) == 0);
    Require(close_nid_postfix(handle) == 0);
    *__error_nid_postfix() = 0;
    Require(ReadFromFile(handle, buffer, sizeof(buffer)) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);
    const int writer = open_nid_postfix(path.c_str(), SCE_KERNEL_O_WRONLY, 0);
    Require(writer >= 0);
    *__error_nid_postfix() = 0;
    Require(ReadFromFile(writer, buffer, sizeof(buffer)) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);
    Require(std::memcmp(buffer, "xxxx", 4) == 0);
    Require(close_nid_postfix(writer) == 0);
}

struct TestCString {
    Jsc::WtfAbi::CStringBuffer header;
    char data[256];
};

Jsc::WtfAbi::CString MakeCString(TestCString& storage, const std::string& text) {
    Require(text.size() < sizeof(storage.data));
    storage.header = {1, text.size()};
    std::memcpy(storage.data, text.c_str(), text.size() + 1);
    return {&storage.header};
}

Jsc::WtfAbi::OptionalInt32 DeviceId(const std::string& path) {
    TestCString storage{};
    const Jsc::WtfAbi::CString string = MakeCString(storage, path);
    return _ZN3WTF14FileSystemImpl15getFileDeviceIdERKNS_7CStringE(&string);
}

std::int32_t GuestDevice(const std::string& path) {
    FileStat status{};
    Require(stat_nid_postfix(path.c_str(), &status) == 0);
    return static_cast<std::int32_t>(status.st_dev);
}

void TestDeviceId(const std::string& directory) {
    const std::string file = directory + "/digits.bin";
    const std::string nested = directory + "/nested/other.bin";
    const Jsc::WtfAbi::OptionalInt32 fileDevice = DeviceId(file);
    Require(fileDevice.hasValue);
    Require(fileDevice.value == GuestDevice(file));
    const Jsc::WtfAbi::OptionalInt32 nestedDevice = DeviceId(nested);
    Require(nestedDevice.hasValue);
    Require(nestedDevice.value == GuestDevice(nested));
    Require(nestedDevice.value == fileDevice.value);
    const Jsc::WtfAbi::OptionalInt32 directoryDevice = DeviceId(directory);
    Require(directoryDevice.hasValue);
    Require(directoryDevice.value == GuestDevice(directory));
    Require(!DeviceId(directory + "/missing.bin").hasValue);
    Require(!DeviceId("").hasValue);
    const Jsc::WtfAbi::CString nullString{nullptr};
    Require(!_ZN3WTF14FileSystemImpl15getFileDeviceIdERKNS_7CStringE(&nullString).hasValue);
}

} // namespace

int main() {
    const auto root = std::filesystem::current_path() / ("jsc-read-from-file-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    {
        std::ofstream file(root / "digits.bin", std::ios::binary);
        file << "0123456789";
    }
    Require(std::filesystem::create_directory(root / "nested"));
    {
        std::ofstream file(root / "nested" / "other.bin", std::ios::binary);
        file << "x";
    }
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/jsc-read-from-file", reinterpret_cast<const char*>(hostPath.c_str()));
    const std::string path = "/jsc-read-from-file/digits.bin";
    TestReadsInOrder(path);
    TestFailures(path);
    TestDeviceId("/jsc-read-from-file");
    std::filesystem::remove_all(root);
    return 0;
}
