#include "prx/libSceNpTrophy/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int NotRegistered = static_cast<int>(0x8055160fu);
const std::filesystem::path StatePath("_trophy/trophy03.1");

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "NpTrophy state: %s\n", message);
    std::abort();
}

void Write(const std::filesystem::path& path, const std::string& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    Require(static_cast<bool>(file), "write the fixture");
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(static_cast<bool>(file), "read the fixture");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void WritePackage() {
    const std::vector<std::pair<std::string, std::string>> files{{"TROPCONF.ESFM", "conf"}, {"TROP000.PNG", "first"}, {"TROP001.PNG", "second"}};
    std::string bytes(0x60 + files.size() * 0x40, '\0');
    const auto put = [&](std::size_t at, std::uint64_t value, int width) {
        for (int i = 0; i < width; ++i) bytes[at + i] = static_cast<char>(value >> (8 * (width - 1 - i)));
    };
    put(0, 0xdca24d00u, 4);
    put(0x10, files.size(), 4);
    put(0x14, 0x40, 4);
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto at = 0x60 + i * 0x40;
        std::memcpy(bytes.data() + at, files[i].first.data(), files[i].first.size());
        put(at + 32, bytes.size(), 8);
        put(at + 40, files[i].second.size(), 8);
        bytes += files[i].second;
    }
    Write("app0/sce_sys/trophy/trophy03.trp", bytes);
}

int CreateContext() {
    int context = 0;
    Require(sceNpTrophyCreateContext(&context, 1, 3, 0) == 0, "create the context");
    return context;
}

void CheckUnlocks(int context, int handle, std::uint32_t expected) {
    TrophyFlagArray flags{};
    std::uint32_t count = 0;
    Require(sceNpTrophyGetTrophyUnlockState(context, handle, &flags, &count) == 0 && count == 2 && flags.bits[0] == expected &&
                flags.bits[1] == 0 && flags.bits[2] == 0 && flags.bits[3] == 0,
            "the loaded unlock set is incorrect");
}

void CheckFailedRead(int context, int handle) {
    bool failed = false;
    try {
        sceNpTrophyRegisterContext(context, handle, 0);
    } catch (const std::runtime_error& error) {
        failed = std::string(error.what()).find("trophy03.1") != std::string::npos;
    }
    Require(failed, "registration accepted an unreadable or malformed state record");
    TrophyFlagArray flags{};
    std::uint32_t count = 0;
    Require(sceNpTrophyGetTrophyUnlockState(context, handle, &flags, &count) == NotRegistered, "a failed read registered its context");
    int platinum = -1;
    Require(sceNpTrophyUnlockTrophy(context, handle, 0, &platinum) == NotRegistered, "a failed read allowed an unlock to rewrite the record");
}

void TestMalformedRecords(int handle) {
    for (const auto& bytes : std::vector<std::string>{"1 broken 0\n", "1 2147483648", "1 -2147483649", "1 9999999999999999999999999", "1 0x0\n", "1 1.0\n", std::string("1 \0 0", 5)}) {
        Write(StatePath, bytes);
        const auto context = CreateContext();
        CheckFailedRead(context, handle);
        Require(Read(StatePath) == bytes, "a failed read changed the damaged record");
        Write(StatePath, "0\n");
        Require(sceNpTrophyRegisterContext(context, handle, 0) == 0, "retry after repairing the record");
        CheckUnlocks(context, handle, 1);
        Require(Read(StatePath) == "0\n", "registration rewrote the repaired record");
        Require(sceNpTrophyDestroyContext(context) == 0, "destroy the repaired context");
    }
}

void TestUnreadableRecord(int handle) {
    std::filesystem::remove(StatePath);
    std::filesystem::create_directory(StatePath);
    const auto context = CreateContext();
    CheckFailedRead(context, handle);
    Require(std::filesystem::is_directory(StatePath), "a failed read replaced the unreadable path");
    std::filesystem::remove(StatePath);
    Write(StatePath, "1\n");
    Require(sceNpTrophyRegisterContext(context, handle, 0) == 0, "retry after replacing the unreadable path");
    CheckUnlocks(context, handle, 2);
    Require(sceNpTrophyDestroyContext(context) == 0, "destroy the unreadable record context");
}

void TestValidRecords(int handle) {
    for (const auto& [bytes, expected] : std::vector<std::pair<std::string, std::uint32_t>>{
             {"", 0}, {" \t\r\n", 0}, {"1", 2}, {" 1\t0\r\n", 3}, {"1 1 0 1", 3}, {"-2147483648 -1 2 127 2147483647 1", 2}, {"+0", 1}}) {
        Write(StatePath, bytes);
        const auto context = CreateContext();
        Require(sceNpTrophyRegisterContext(context, handle, 0) == 0, "register a valid record");
        CheckUnlocks(context, handle, expected);
        Require(Read(StatePath) == bytes, "registration changed a valid record");
        Require(sceNpTrophyDestroyContext(context) == 0, "destroy a valid record context");
    }
    std::filesystem::remove(StatePath);
    const auto context = CreateContext();
    Require(sceNpTrophyRegisterContext(context, handle, 0) == 0, "register without a previous state record");
    CheckUnlocks(context, handle, 0);
    int platinum = -1;
    Require(sceNpTrophyUnlockTrophy(context, handle, 0, &platinum) == 0 && Read(StatePath) == "0\n", "create the first state record");
    Require(sceNpTrophyDestroyContext(context) == 0, "destroy the new record context");
}

}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-np-trophy-state-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    WritePackage();
    int handle = 0;
    Require(sceNpTrophyCreateHandle(&handle) == 0, "create the handle");
    TestMalformedRecords(handle);
    TestUnreadableRecord(handle);
    TestValidRecords(handle);
    Require(sceNpTrophyDestroyHandle(handle) == 0, "destroy the handle");
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    return 0;
}
