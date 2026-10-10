#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataTerminate();
int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2*, SaveDataMemorySetupResult*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Save-data memory growth check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main(int argc, char**) {
    const auto previousDirectory = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-save-growth-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::current_path(root);
    const auto path = std::filesystem::path("_sd_mem/u7531/slot0.bin");
    std::filesystem::create_directories(path.parent_path());
    const std::vector<char> original{'s', 'a', 'v', 'e', '\0', '\x7f'};
    {
        std::ofstream file(path, std::ios::binary);
        file.write(original.data(), static_cast<std::streamsize>(original.size()));
        Require(static_cast<bool>(file));
    }
    Require(sceSaveDataInitialize3(nullptr) == 0);
    SaveDataMemorySetup2 setup{};
    setup.user_id = 7531;
    setup.memory_size = original.size() + 5;
    SaveDataMemorySetupResult result{};
    int exitCode = 0;
    if (argc > 1) {
        SaveDataParam param{};
        setup.option = 1;
        setup.init_param = &param;
        result.existed_memory_size = 123;
#ifdef _WIN32
        HANDLE lock = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(lock != INVALID_HANDLE_VALUE);
#else
        const auto permissions = std::filesystem::status(path).permissions();
        std::filesystem::permissions(path, std::filesystem::perms::owner_write);
#endif
        const bool readable = static_cast<bool>(std::ifstream(path, std::ios::binary));
        int status = 0;
        if (!readable) {
            Require(std::filesystem::is_regular_file(path));
            Require(std::filesystem::file_size(path) == original.size());
            status = sceSaveDataSetupSaveDataMemory2(&setup, &result);
        }
#ifdef _WIN32
        Require(CloseHandle(lock) != 0);
        Require(!readable);
#else
        std::filesystem::permissions(path, permissions);
#endif
        if (readable) {
            std::fprintf(stderr, "Read denial unavailable for this user\n");
            exitCode = 77;
        } else {
            Require(Read(path) == original);
            Require(status == static_cast<int>(0x809F000Bu));
            Require(result.existed_memory_size == 123);
            Require(!std::filesystem::exists("_sd_mem/u7531/slot0.param"));
            Require(!std::filesystem::exists(path.string() + ".tmp"));
        }
    } else {
        Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
        Require(result.existed_memory_size == original.size());
        auto expected = original;
        expected.resize(setup.memory_size, 0);
        Require(Read(path) == expected);
        Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
        Require(result.existed_memory_size == expected.size());
        Require(Read(path) == expected);
        setup.memory_size = 2;
        Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
        Require(Read(path) == expected);
        setup.slot_id = 1;
        Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
        Require(result.existed_memory_size == 0);
        Require(Read("_sd_mem/u7531/slot1.bin") == std::vector<char>(2, 0));
        setup.slot_id = 0;
        setup.memory_size = 32u * 1024u * 1024u;
        Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
        Require(result.existed_memory_size == expected.size());
        expected.resize(setup.memory_size, 0);
        Require(Read(path) == expected);
        ++setup.memory_size;
        Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == static_cast<int>(0x809F0000u));
        Require(Read(path) == expected);
    }
    Require(sceSaveDataTerminate() == 0);
    std::filesystem::current_path(previousDirectory);
    std::filesystem::remove_all(root);
    return exitCode;
}
