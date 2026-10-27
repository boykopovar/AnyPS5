#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotFound = -2137063416;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata convert check failed: %s\n", what.c_str());
    ++failures;
}

bool Throws(const SaveDataConvert* convert) {
    try {
        sceSaveDataConvert(convert);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int Convert(const char* data, std::size_t size) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, data, size);
    SaveDataConvert convert{};
    convert.user_id = 0x10000000;
    convert.dir_name = &name;
    return sceSaveDataConvert(&convert);
}

std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-convert-" + std::to_string(std::random_device{}()));
    const auto work = root / "work";
    const auto kept = work / "_sd" / "kept" / "data.bin";
    std::filesystem::create_directories(kept.parent_path());
    std::ofstream(kept) << "data";
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    float progress = 0.0f;
    Check(sceSaveDataGetConvertProgress(&progress) == SaveDataErrorNotInitialized, "query before initialize fails with not initialized");
    Check(Throws(nullptr), "null convert pointer throws");
    SaveDataConvert nullDir{};
    Check(Throws(&nullDir), "null dir_name throws");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    Check(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds");
    Check(progress == 0.0f, "convert progress reports 0.0f initially");
    Check(sceSaveDataGetConvertProgress(nullptr) == SaveDataErrorParameter, "null progress pointer is rejected");
    Check(Convert("missing", sizeof("missing")) == SaveDataErrorNotFound, "a missing save is not found");
    Check(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after failed convert");
    Check(progress < 1.0f, "convert progress is below 1.0f after failed convert");
    Check(progress == 0.0f, "convert progress reports 0.0f after failed convert");
    Check(Convert("kept", sizeof("kept")) == 0, "an existing save converts");
    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, "successful conversion queues completion");
    Check(event.type == 5 && event.error_code == 0, "conversion event reports successful conversion");
    Check(event.user_id == 0x10000000 && std::strcmp(event.dir_name.data, "kept") == 0, "conversion event identifies the user and save");
    Check(sceSaveDataGetEventResult(nullptr, &event) == SaveDataErrorNotFound, "conversion event is delivered once");
    Check(Read(kept) == std::vector<char>{'d', 'a', 't', 'a'}, "the converted save keeps its files");
    Check(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after successful convert");
    Check(progress == 1.0f, "convert progress reports 1.0f after successful convert");
    Check(Convert("missing", sizeof("missing")) == SaveDataErrorNotFound, "a missing save is not found");
    Check(sceSaveDataGetConvertProgress(&progress) == 0, "get convert progress succeeds after subsequent failed convert");
    Check(progress < 1.0f, "convert progress drops below 1.0f after subsequent failed convert");
    Check(progress == 0.0f, "convert progress reports 0.0f after subsequent failed convert");
    for (const char* invalid : {"../kept", "", ".", "..", "a\\b", "c:d"}) {
        Check(Convert(invalid, std::strlen(invalid) + 1) == SaveDataErrorParameter, std::string("\"") + invalid + "\" is rejected");
    }
    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    Check(Convert(unterminated, sizeof(unterminated)) == SaveDataErrorParameter, "an unterminated name is rejected");
    Check(sceSaveDataTerminate() == 0, "SaveData terminates");
    Check(sceSaveDataGetConvertProgress(&progress) == SaveDataErrorNotInitialized, "query after terminate fails with not initialized");

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata convert tests passed\n");
    return 0;
}
