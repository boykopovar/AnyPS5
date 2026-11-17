#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr std::int32_t InitialUser = 0x10000000;

void WriteFile(const std::filesystem::path& path, std::size_t bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    const std::string content(bytes, 'x');
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

}

int main() {
    const std::filesystem::path root = "_sd";
    std::error_code error;
    std::filesystem::remove_all(root, error);

    std::uint32_t count = 7;
    std::uint64_t size = 7;
    Expect(sceSaveDataGetSaveDataCount(0, &count) == SaveDataErrorNotInitialized, "count before initialization returns not initialized");
    Expect(sceSaveDataGetAllSize(0, &size) == SaveDataErrorNotInitialized, "size before initialization returns not initialized");
    Expect(count == 7 && size == 7, "an uninitialized query writes nothing");

    Expect(sceSaveDataInitialize(nullptr) == 0, "initialize");
    Expect(sceSaveDataGetSaveDataCount(0, nullptr) == SaveDataErrorParameter, "count null output returns parameter error");
    Expect(sceSaveDataGetAllSize(0, nullptr) == SaveDataErrorParameter, "size null output returns parameter error");

    count = 7;
    size = 7;
    Expect(sceSaveDataGetSaveDataCount(0, &count) == 0 && count == 0, "an absent save root holds no saves");
    Expect(sceSaveDataGetAllSize(0, &size) == 0 && size == 0, "an absent save root holds no bytes");

    WriteFile(root / "SIZE_A" / "a.bin", 1024);
    WriteFile(root / "SIZE_B" / "b.bin", 1025);
    WriteFile(root / "SIZE_B" / "nested" / "c.bin", 512);
    WriteFile(root / "SIZE_FILE", 4096);

    for (const std::int32_t firstArgument : {0, -1, static_cast<std::int32_t>(InitialUser), 2147483647}) {
        count = 7;
        size = 7;
        Expect(sceSaveDataGetSaveDataCount(firstArgument, &count) == 0 && count == 2, "two save directories are counted");
        Expect(sceSaveDataGetAllSize(firstArgument, &size) == 0 && size == 3, "2561 bytes round up to 3 KiB");
    }

    auto* const inaccessibleCount = reinterpret_cast<std::uint32_t*>(0x10);
    auto* const inaccessibleSize = reinterpret_cast<std::uint64_t*>(0x10);
    Expect(sceSaveDataGetSaveDataCount(0, inaccessibleCount) == SaveDataErrorParameter, "an unwritable count returns parameter error");
    Expect(sceSaveDataGetAllSize(0, inaccessibleSize) == SaveDataErrorParameter, "an unwritable size returns parameter error");

    count = 7;
    size = 7;
    Expect(sceSaveDataGetSaveDataCount(0, &count) == 0 && count == 2, "a plain file in the save root is not a save");
    Expect(sceSaveDataGetAllSize(0, &size) == 0 && size == 3, "a plain file in the save root adds no bytes");

    std::filesystem::remove_all(root, error);
    return Failures() == 0 ? 0 : 1;
}
