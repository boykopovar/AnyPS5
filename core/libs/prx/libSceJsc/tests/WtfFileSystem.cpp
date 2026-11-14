#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

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

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF file system check failed at line %d\n", line);
        std::abort();
    }
}

int ReadFromFile(int handle, char* data, int length) {
    return _ZN3WTF14FileSystemImpl12readFromFileEiPci(handle, data, length);
}

void TestReadsInOrder(const std::string& path) {
    const int handle = open_nid_postfix(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(handle >= 0, __LINE__);
    char buffer[16];
    std::memset(buffer, 'x', sizeof(buffer));
    Check(ReadFromFile(handle, buffer, 4) == 4, __LINE__);
    Check(std::memcmp(buffer, "0123x", 5) == 0, __LINE__);
    Check(ReadFromFile(handle, buffer, sizeof(buffer)) == 6, __LINE__);
    Check(std::memcmp(buffer, "456789", 6) == 0, __LINE__);
    Check(ReadFromFile(handle, buffer, sizeof(buffer)) == 0, __LINE__);
    Check(ReadFromFile(handle, buffer, 0) == 0, __LINE__);
    Check(lseek_nid_postfix(handle, 2, 0) == 2, __LINE__);
    std::memset(buffer, 'x', sizeof(buffer));
    Check(ReadFromFile(handle, buffer, 3) == 3, __LINE__);
    Check(std::memcmp(buffer, "234x", 4) == 0, __LINE__);
    Check(close_nid_postfix(handle) == 0, __LINE__);
}

void TestFailures(const std::string& path) {
    char buffer[4] = {'x', 'x', 'x', 'x'};
    *__error_nid_postfix() = 0;
    Check(ReadFromFile(-1, buffer, sizeof(buffer)) == -1, __LINE__);
    Check(*__error_nid_postfix() == GuestEbadf, __LINE__);
    const int handle = open_nid_postfix(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(handle >= 0, __LINE__);
    *__error_nid_postfix() = 0;
    Check(ReadFromFile(handle, buffer, -1) == -1, __LINE__);
    Check(*__error_nid_postfix() == GuestEinval, __LINE__);
    Check(std::memcmp(buffer, "xxxx", 4) == 0, __LINE__);
    Check(close_nid_postfix(handle) == 0, __LINE__);
    *__error_nid_postfix() = 0;
    Check(ReadFromFile(handle, buffer, sizeof(buffer)) == -1, __LINE__);
    Check(*__error_nid_postfix() == GuestEbadf, __LINE__);
    const int writer = open_nid_postfix(path.c_str(), SCE_KERNEL_O_WRONLY, 0);
    Check(writer >= 0, __LINE__);
    *__error_nid_postfix() = 0;
    Check(ReadFromFile(writer, buffer, sizeof(buffer)) == -1, __LINE__);
    Check(*__error_nid_postfix() == GuestEbadf, __LINE__);
    Check(std::memcmp(buffer, "xxxx", 4) == 0, __LINE__);
    Check(close_nid_postfix(writer) == 0, __LINE__);
}

struct TestCString {
    Jsc::WtfAbi::CStringBuffer header;
    char data[256];
};

Jsc::WtfAbi::CString MakeCString(TestCString& storage, const std::string& text) {
    Check(text.size() < sizeof(storage.data), __LINE__);
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
    Check(stat_nid_postfix(path.c_str(), &status) == 0, __LINE__);
    return static_cast<std::int32_t>(status.st_dev);
}

void TestDeviceId(const std::string& directory) {
    const std::string file = directory + "/digits.bin";
    const std::string nested = directory + "/nested/other.bin";
    const Jsc::WtfAbi::OptionalInt32 fileDevice = DeviceId(file);
    Check(fileDevice.hasValue, __LINE__);
    Check(fileDevice.value == GuestDevice(file), __LINE__);
    const Jsc::WtfAbi::OptionalInt32 nestedDevice = DeviceId(nested);
    Check(nestedDevice.hasValue, __LINE__);
    Check(nestedDevice.value == GuestDevice(nested), __LINE__);
    Check(nestedDevice.value == fileDevice.value, __LINE__);
    const Jsc::WtfAbi::OptionalInt32 directoryDevice = DeviceId(directory);
    Check(directoryDevice.hasValue, __LINE__);
    Check(directoryDevice.value == GuestDevice(directory), __LINE__);
    Check(!DeviceId(directory + "/missing.bin").hasValue, __LINE__);
    Check(!DeviceId("").hasValue, __LINE__);
    const Jsc::WtfAbi::CString nullString{nullptr};
    Check(!_ZN3WTF14FileSystemImpl15getFileDeviceIdERKNS_7CStringE(&nullString).hasValue, __LINE__);
}

} // namespace

int main() {
    const auto root = std::filesystem::current_path() / ("jsc-read-from-file-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(root), __LINE__);
    {
        std::ofstream file(root / "digits.bin", std::ios::binary);
        file << "0123456789";
    }
    Check(std::filesystem::create_directory(root / "nested"), __LINE__);
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
