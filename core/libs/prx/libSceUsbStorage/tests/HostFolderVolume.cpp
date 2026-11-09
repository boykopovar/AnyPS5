#include "prx/libSceUsbStorage/include/HostVolumes.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool correct = true;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBFOLDER: %s\n", message);
        correct = false;
    }
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() / "anyps5-usb-folder-test";
    fs::remove_all(base);
    fs::create_directories(base / "folder");
    std::ofstream(base / "file") << "data";

    const auto folder = UsbStorage::HostFolderVolume((base / "folder").string());
    Check(folder.has_value(), "an existing folder is refused");
    Check(folder && folder->root == base / "folder", "a folder is not mounted at its own root");
    Check(folder && folder->fileSystem == UsbStorage::FileSystem::Fat, "a folder is not presented as a FAT volume");
    Check(folder && !folder->key.empty() && folder->key == UsbStorage::HostFolderVolume((base / "folder").string())->key, "a folder key is not stable across scans");
    Check(!UsbStorage::HostFolderVolume((base / "missing").string()).has_value(), "a missing folder is mounted");
    Check(!UsbStorage::HostFolderVolume((base / "file").string()).has_value(), "a regular file is mounted as a folder");
    Check(!UsbStorage::HostFolderVolume("").has_value(), "an empty path is mounted");

#ifndef _WIN32
    ::unsetenv("ANYPS5_STORAGE_FOLDER");
    Check(UsbStorage::HostFolderVolumes().empty(), "a folder is mounted without ANYPS5_STORAGE_FOLDER");
    ::setenv("ANYPS5_STORAGE_FOLDER", (base / "folder").string().c_str(), 1);
    Check(UsbStorage::HostFolderVolumes().size() == 1, "ANYPS5_STORAGE_FOLDER does not mount its folder");
    ::setenv("ANYPS5_STORAGE_FOLDER", (base / "missing").string().c_str(), 1);
    Check(UsbStorage::HostFolderVolumes().empty(), "ANYPS5_STORAGE_FOLDER mounts a missing folder");
    ::unsetenv("ANYPS5_STORAGE_FOLDER");
#endif

    fs::remove_all(base);
    return correct ? 0 : 1;
}
