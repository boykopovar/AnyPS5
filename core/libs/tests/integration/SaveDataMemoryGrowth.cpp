#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataTerminate();
int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2*, SaveDataMemorySetupResult*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::int32_t userId = 7531;
constexpr int saveDataParameterError = static_cast<int>(0x809F0000u);
constexpr int saveDataInternalError = static_cast<int>(0x809F000Bu);
const std::vector<char> original{'s', 'a', 'v', 'e', '\0', '\x7f'};

std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "open " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void Write(const std::filesystem::path& path, const std::vector<char>& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    Require(static_cast<bool>(file), "write " + path.string());
}

std::vector<char> Grown(std::vector<char> data, std::size_t size) {
    data.resize(size, 0);
    return data;
}

class SaveDataMemory {
public:
    explicit SaveDataMemory(const std::vector<char>& existing) : previous(std::filesystem::current_path()) {
        std::filesystem::current_path(directory.Path());
        Write(path, existing);
        RequireEqual(sceSaveDataInitialize3(nullptr), 0, "initialize");
        initialized = true;
        setup.user_id = userId;
    }

    ~SaveDataMemory() {
        if (initialized) sceSaveDataTerminate();
        std::error_code ignored;
        std::filesystem::current_path(previous, ignored);
    }

    SaveDataMemory(const SaveDataMemory&) = delete;
    SaveDataMemory& operator=(const SaveDataMemory&) = delete;

    int Setup(std::size_t size) {
        setup.memory_size = size;
        return sceSaveDataSetupSaveDataMemory2(&setup, &result);
    }

    void Terminate() {
        initialized = false;
        RequireEqual(sceSaveDataTerminate(), 0, "terminate");
    }

    const std::filesystem::path path{"_sd_mem/u7531/slot0.bin"};
    SaveDataMemorySetup2 setup{};
    SaveDataMemorySetupResult result{};

private:
    Testing::TemporaryDirectory directory;
    std::filesystem::path previous;
    bool initialized = false;
};

const Case grows{"SetupSaveDataMemory2_SmallerExistingFile_GrowsWithZeros", [] {
    SaveDataMemory memory(original);
    RequireEqual(memory.Setup(original.size() + 5), 0, "setup status");
    RequireEqual(memory.result.existed_memory_size, static_cast<std::uint64_t>(original.size()), "existing size");
    Require(Read(memory.path) == Grown(original, original.size() + 5), "file keeps its bytes and grows with zeros");
    memory.Terminate();
}};

const Case sameSize{"SetupSaveDataMemory2_SameSizeExistingFile_KeepsContents", [] {
    const auto existing = Grown(original, original.size() + 5);
    SaveDataMemory memory(existing);
    RequireEqual(memory.Setup(existing.size()), 0, "setup status");
    RequireEqual(memory.result.existed_memory_size, static_cast<std::uint64_t>(existing.size()), "existing size");
    Require(Read(memory.path) == existing, "file is unchanged");
    memory.Terminate();
}};

const Case smallerRequest{"SetupSaveDataMemory2_SmallerRequestedSize_KeepsLargerFile", [] {
    const auto existing = Grown(original, original.size() + 5);
    SaveDataMemory memory(existing);
    RequireEqual(memory.Setup(2), 0, "setup status");
    Require(Read(memory.path) == existing, "file is not truncated");
    memory.Terminate();
}};

const Case newSlot{"SetupSaveDataMemory2_NewSlot_CreatesZeroFilledFile", [] {
    SaveDataMemory memory(original);
    memory.setup.slot_id = 1;
    RequireEqual(memory.Setup(2), 0, "setup status");
    RequireEqual(memory.result.existed_memory_size, std::uint64_t{0}, "existing size");
    Require(Read("_sd_mem/u7531/slot1.bin") == std::vector<char>(2, 0), "new slot is zero filled");
    Require(Read(memory.path) == original, "other slot is unchanged");
    memory.Terminate();
}};

const Case maximumSize{"SetupSaveDataMemory2_MaximumSize_GrowsTo32MiB", [] {
    const auto existing = Grown(original, original.size() + 5);
    SaveDataMemory memory(existing);
    constexpr std::size_t maximum = 32u * 1024u * 1024u;
    RequireEqual(memory.Setup(maximum), 0, "setup status");
    RequireEqual(memory.result.existed_memory_size, static_cast<std::uint64_t>(existing.size()), "existing size");
    Require(Read(memory.path) == Grown(existing, maximum), "file grows to the maximum size");
    memory.Terminate();
}};

const Case aboveMaximum{"SetupSaveDataMemory2_AboveMaximumSize_FailsWithoutChangingFile", [] {
    constexpr std::size_t maximum = 32u * 1024u * 1024u;
    const auto existing = Grown(original, maximum);
    SaveDataMemory memory(existing);
    RequireEqual(memory.Setup(maximum + 1), saveDataParameterError, "setup status");
    Require(Read(memory.path) == existing, "file is unchanged");
    memory.Terminate();
}};

const Case readFailure{"SetupSaveDataMemory2_UnreadableExistingFile_FailsWithoutChangingAnything", [] {
    SaveDataMemory memory(original);
    SaveDataParam param{};
    memory.setup.option = 1;
    memory.setup.init_param = &param;
    memory.result.existed_memory_size = 123;
#ifdef _WIN32
    HANDLE lock = CreateFileW(memory.path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(lock != INVALID_HANDLE_VALUE, "lock the save file against reading");
#else
    const auto permissions = std::filesystem::status(memory.path).permissions();
    std::filesystem::permissions(memory.path, std::filesystem::perms::owner_write);
#endif
    const bool readable = static_cast<bool>(std::ifstream(memory.path, std::ios::binary));
    bool isFile = false;
    std::uintmax_t size = 0;
    int status = 0;
    if (!readable) {
        isFile = std::filesystem::is_regular_file(memory.path);
        size = std::filesystem::file_size(memory.path);
        status = memory.Setup(original.size() + 5);
    }
#ifdef _WIN32
    const bool unlocked = CloseHandle(lock) != 0;
    Require(unlocked, "unlock the save file");
    Require(!readable, "an exclusive Windows handle denies reading");
#else
    std::filesystem::permissions(memory.path, permissions);
#endif
    if (readable) Testing::Skip("read denial unavailable for this user");
    Require(isFile, "save file exists while locked");
    RequireEqual(size, static_cast<std::uintmax_t>(original.size()), "save file size while locked");
    Require(Read(memory.path) == original, "file is unchanged");
    RequireEqual(status, saveDataInternalError, "setup status");
    RequireEqual(memory.result.existed_memory_size, std::uint64_t{123}, "existing size is untouched");
    Require(!std::filesystem::exists("_sd_mem/u7531/slot0.param"), "no parameter file is written");
    Require(!std::filesystem::exists(memory.path.string() + ".tmp"), "no temporary file is left");
    memory.Terminate();
}};

} // namespace
