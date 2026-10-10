#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <system_error>
#include <vector>
#ifndef _WIN32
#include <csignal>
#include <sys/resource.h>
#endif

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataTerminate();
int APS5_VABI sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int internalError = static_cast<int>(0x809F000Bu);
constexpr int parameterError = static_cast<int>(0x809F0000u);

const std::filesystem::path paramPath("_sd_mem/u7531/slot0.param");
const std::filesystem::path memoryPath("_sd_mem/u7531/slot0.bin");

std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "open " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::vector<char> MemoryOriginal() {
    return std::vector<char>(32, 's');
}

class CurrentDirectoryScope {
public:
    explicit CurrentDirectoryScope(const std::filesystem::path& directory) : previous(std::filesystem::current_path()) {
        std::filesystem::current_path(directory);
    }

    ~CurrentDirectoryScope() {
        std::error_code ignored;
        std::filesystem::current_path(previous, ignored);
    }

    CurrentDirectoryScope(const CurrentDirectoryScope&) = delete;
    CurrentDirectoryScope& operator=(const CurrentDirectoryScope&) = delete;

private:
    std::filesystem::path previous;
};

class SaveDataMemoryFixture {
public:
    SaveDataMemoryFixture() {
        std::filesystem::create_directories(paramPath.parent_path());
        const auto memory = MemoryOriginal();
        {
            std::ofstream file(memoryPath, std::ios::binary);
            file.write(memory.data(), static_cast<std::streamsize>(memory.size()));
            Require(static_cast<bool>(file), "write the initial memory blob");
        }
        RequireEqual(sceSaveDataInitialize3(nullptr), 0, "sceSaveDataInitialize3");
    }

    ~SaveDataMemoryFixture() { sceSaveDataTerminate(); }

    SaveDataMemoryFixture(const SaveDataMemoryFixture&) = delete;
    SaveDataMemoryFixture& operator=(const SaveDataMemoryFixture&) = delete;

private:
    Testing::TemporaryDirectory directory;
    CurrentDirectoryScope scope{directory.Path()};
};

class MemoryRequest {
public:
    MemoryRequest() {
        set.user_id = 7531;
        set.data = data.data();
        set.data_num = static_cast<std::uint32_t>(data.size());
    }

    MemoryRequest(const MemoryRequest&) = delete;
    MemoryRequest& operator=(const MemoryRequest&) = delete;

    std::array<char, 8> first{'a', '\0', 'b', 'c', 'd', 'e', 'f', 'g'};
    std::array<char, 8> second{'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o'};
    std::array<SaveDataMemoryData, 3> data{{
        {first.data(), first.size(), 4},
        {second.data(), second.size(), 24},
        {nullptr, 0, std::numeric_limits<std::size_t>::max()}
    }};
    SaveDataMemorySet2 set{};
};

std::vector<char> Bytes(const SaveDataParam& param) {
    const auto* bytes = reinterpret_cast<const char*>(&param);
    return std::vector<char>(bytes, bytes + sizeof(param));
}

std::string Temporary(const std::filesystem::path& path) {
    return path.string() + ".tmp";
}

#ifndef _WIN32
class FileSizeLimit {
public:
    explicit FileSizeLimit(rlim_t bytes) {
        Require(getrlimit(RLIMIT_FSIZE, &previousLimit) == 0, "read the file size limit");
        previousHandler = std::signal(SIGXFSZ, SIG_IGN);
        Require(previousHandler != SIG_ERR, "ignore SIGXFSZ");
        auto limit = previousLimit;
        limit.rlim_cur = bytes;
        active = setrlimit(RLIMIT_FSIZE, &limit) == 0;
        if (!active) std::signal(SIGXFSZ, previousHandler);
        Require(active, "lower the file size limit");
    }

    ~FileSizeLimit() { Restore(); }

    FileSizeLimit(const FileSizeLimit&) = delete;
    FileSizeLimit& operator=(const FileSizeLimit&) = delete;

    bool Restore() {
        if (!active) return true;
        active = false;
        const bool limitRestored = setrlimit(RLIMIT_FSIZE, &previousLimit) == 0;
        const bool handlerRestored = std::signal(SIGXFSZ, previousHandler) != SIG_ERR;
        return limitRestored && handlerRestored;
    }

private:
    rlimit previousLimit{};
    void (*previousHandler)(int) = SIG_DFL;
    bool active = false;
};
#endif

const Case initialize{"Initialize_CalledTwice_SucceedsAndBalancedTerminatesSucceed", [] {
    RequireEqual(sceSaveDataInitialize3(nullptr), 0, "first initialize");
    RequireEqual(sceSaveDataInitialize3(nullptr), 0, "second initialize");
    RequireEqual(sceSaveDataTerminate(), 0, "first terminate");
    RequireEqual(sceSaveDataTerminate(), 0, "second terminate");
}};

const Case writeParam{"SetMemory_Param_WritesParamSidecar", [] {
    const SaveDataMemoryFixture fixture;
    SaveDataParam param{};
    SaveDataMemorySet2 set{};
    set.user_id = 7531;
    set.param = &param;
    RequireEqual(sceSaveDataSetSaveDataMemory2(&set), 0, "set param");
    RequireEqual(Read(paramPath).size(), sizeof(param), "param file size");
}};

const Case blockedParam{"SetMemory_ParamTemporaryPathBlocked_FailsAndKeepsPreviousParam", [] {
    const SaveDataMemoryFixture fixture;
    SaveDataParam param{};
    SaveDataMemorySet2 set{};
    set.user_id = 7531;
    set.param = &param;
    RequireEqual(sceSaveDataSetSaveDataMemory2(&set), 0, "set the initial param");
    const auto original = Read(paramPath);
    param.user_param = 42;
    Require(std::filesystem::create_directory(Temporary(paramPath)), "block the temporary path");
    const int status = sceSaveDataSetSaveDataMemory2(&set);
    Require(Read(paramPath) == original, "param file unchanged");
    RequireEqual(status, internalError, "blocked set status");
    Require(!std::filesystem::exists(Temporary(paramPath)), "temporary path removed");
}};

const Case retryParam{"SetMemory_ParamRetriedAfterBlockedWrite_WritesUpdatedParam", [] {
    const SaveDataMemoryFixture fixture;
    SaveDataParam param{};
    SaveDataMemorySet2 set{};
    set.user_id = 7531;
    set.param = &param;
    RequireEqual(sceSaveDataSetSaveDataMemory2(&set), 0, "set the initial param");
    param.user_param = 42;
    Require(std::filesystem::create_directory(Temporary(paramPath)), "block the temporary path");
    RequireEqual(sceSaveDataSetSaveDataMemory2(&set), internalError, "blocked set status");
    RequireEqual(sceSaveDataSetSaveDataMemory2(&set), 0, "retried set status");
    Require(Read(paramPath) == Bytes(param), "param file holds the updated param");
}};

const Case noParamNoData{"SetMemory_NoParamAndNoData_Succeeds", [] {
    const SaveDataMemoryFixture fixture;
    SaveDataMemorySet2 set{};
    set.user_id = 7531;
    RequireEqual(sceSaveDataSetSaveDataMemory2(&set), 0, "set status");
}};

const Case dataPastEnd{"SetMemory_DataPastEnd_FailsWithParameterErrorAndKeepsMemory", [] {
    const SaveDataMemoryFixture fixture;
    MemoryRequest request;
    request.data[1].offset = MemoryOriginal().size();
    RequireEqual(sceSaveDataSetSaveDataMemory2(&request.set), parameterError, "set status");
    Require(Read(memoryPath) == MemoryOriginal(), "memory blob unchanged");
}};

const Case blockedData{"SetMemory_DataTemporaryPathBlocked_FailsAndKeepsMemory", [] {
    const SaveDataMemoryFixture fixture;
    MemoryRequest request;
    Require(std::filesystem::create_directory(Temporary(memoryPath)), "block the temporary path");
    RequireEqual(sceSaveDataSetSaveDataMemory2(&request.set), internalError, "set status");
    Require(Read(memoryPath) == MemoryOriginal(), "memory blob unchanged");
    Require(!std::filesystem::exists(Temporary(memoryPath)), "temporary path removed");
}};

#ifndef _WIN32
const Case fileSizeLimit{"SetMemory_FileSizeLimitExceeded_FailsAndKeepsMemory", [] {
    const SaveDataMemoryFixture fixture;
    MemoryRequest request;
    FileSizeLimit limit(16);
    const int writeStatus = sceSaveDataSetSaveDataMemory2(&request.set);
    Require(limit.Restore(), "restore the file size limit and SIGXFSZ handler");
    RequireEqual(writeStatus, internalError, "set status");
    Require(Read(memoryPath) == MemoryOriginal(), "memory blob unchanged");
    Require(!std::filesystem::exists(Temporary(memoryPath)), "temporary path removed");
}};
#endif

const Case validData{"SetMemory_ValidData_WritesEachDescriptorAtItsOffset", [] {
    const SaveDataMemoryFixture fixture;
    MemoryRequest request;
    RequireEqual(sceSaveDataSetSaveDataMemory2(&request.set), 0, "set status");
    auto expected = MemoryOriginal();
    std::copy(request.first.begin(), request.first.end(), expected.begin() + 4);
    std::copy(request.second.begin(), request.second.end(), expected.begin() + 24);
    Require(Read(memoryPath) == expected, "memory blob contents");
}};

const Case zeroDataNum{"SetMemory_ZeroDataNumWithData_WritesFirstDescriptorOnly", [] {
    const SaveDataMemoryFixture fixture;
    MemoryRequest request;
    RequireEqual(sceSaveDataSetSaveDataMemory2(&request.set), 0, "initial set status");
    auto expected = MemoryOriginal();
    std::copy(request.first.begin(), request.first.end(), expected.begin() + 4);
    std::copy(request.second.begin(), request.second.end(), expected.begin() + 24);
    request.first.fill('z');
    request.set.data_num = 0;
    RequireEqual(sceSaveDataSetSaveDataMemory2(&request.set), 0, "set status with data_num 0");
    std::copy(request.first.begin(), request.first.end(), expected.begin() + 4);
    Require(Read(memoryPath) == expected, "memory blob contents");
}};

} // namespace
