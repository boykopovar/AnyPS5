#include "prx/libScePlayGo/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <source_location>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

static void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "requirement failed at line %u\n", location.line());
    std::abort();
}

template <typename Operation>
static void RequireMissingChunkMetadata(Operation&& operation) {
    bool threw = false;
    try {
        operation();
    } catch (const std::runtime_error& error) {
        const std::string_view message(error.what());
        threw = message.find("playgo-chunk.dat") != std::string_view::npos &&
            message.find("playgo-scenario.json") != std::string_view::npos &&
            message.find("chunk ID") != std::string_view::npos &&
            message.find("APS5_PLAYGO_UNLISTED_CHUNKS") != std::string_view::npos;
    }
    Require(threw);
}

static int CheckPackageChunkTable(const char* root) {
    constexpr int badChunkId = static_cast<int>(0x80B2000C);
    std::filesystem::current_path(root);
    PlayGoInitParams init{};
    int handle = 0;
    Require(scePlayGoInitialize(&init) == 0);
    Require(scePlayGoOpen(&handle, nullptr) == 0);
    const std::uint16_t known[3] = {0, 1, 2};
    std::int8_t loci[3] = {0, 0, 0};
    Require(scePlayGoGetLocus(handle, known, 3, loci) == 0 && loci[0] == 3 && loci[1] == 3 && loci[2] == 3);
    const std::uint16_t outside[2] = {1, 3};
    Require(scePlayGoGetLocus(handle, outside, 2, loci) == badChunkId);
    std::uint32_t chunkCount = 0;
    Require(scePlayGoGetChunkId(handle, nullptr, 0, &chunkCount) == 0 && chunkCount == 3);
    Require(scePlayGoClose(handle) == 0);
    return 0;
}

static int CheckScenarioWithoutChunkTable(const char* root) {
    std::filesystem::current_path(root);
    PlayGoInitParams init{};
    int handle = 0;
    Require(scePlayGoInitialize(&init) == 0);
    Require(scePlayGoOpen(&handle, nullptr) == 0);
    const std::uint16_t anyIds[3] = {1, 10, 0xFFFF};
    std::int8_t loci[3] = {0, 0, 0};
    Require(scePlayGoGetLocus(handle, anyIds, 3, loci) == 0 && loci[0] == 3 && loci[1] == 3 && loci[2] == 3);
    const PlayGoToDo todo[2] = {{0, 3, 0}, {0xFFFF, 3, 0}};
    Require(scePlayGoSetToDoList(handle, todo, 2) == 0);
    std::uint32_t chunkCount = 0;
    Require(scePlayGoGetChunkId(handle, nullptr, 0, &chunkCount) == 0 && chunkCount == 1000);
    std::vector<std::uint16_t> ids(chunkCount + 1, 0xFFFF);
    Require(scePlayGoGetChunkId(handle, ids.data(), chunkCount, &chunkCount) == 0 && chunkCount == 1000);
    for (std::uint32_t index = 0; index < chunkCount; ++index) Require(ids[index] == index);
    Require(ids.back() == 0xFFFF);
    std::vector<std::int8_t> listedLoci(chunkCount, 0);
    Require(scePlayGoGetLocus(handle, ids.data(), chunkCount, listedLoci.data()) == 0);
    for (const auto locus : listedLoci) Require(locus == 3);
    std::uint16_t firstTwo[2] = {0xFFFF, 0xFFFF};
    Require(scePlayGoGetChunkId(handle, firstTwo, 2, &chunkCount) == 0 && chunkCount == 2 && firstTwo[0] == 0 && firstTwo[1] == 1);
    Require(scePlayGoClose(handle) == 0);
    return 0;
}

static int CheckNoPlayGoFile(const char* root) {
    std::filesystem::current_path(root);
    constexpr int badHandle = static_cast<int>(0x80B20009);
    constexpr int badPointer = static_cast<int>(0x80B2000A);
    constexpr int badSize = static_cast<int>(0x80B2000B);
    constexpr int badLocus = static_cast<int>(0x80B20010);

    PlayGoInitParams init{};
    int handle = 0;
    Require(scePlayGoInitialize(nullptr) == badPointer);
    Require(scePlayGoInitialize(&init) == 0);
    Require(scePlayGoOpen(nullptr, nullptr) == badPointer);
    Require(scePlayGoOpen(&handle, nullptr) == 0);

    std::uint64_t mask = 0;
    Require(scePlayGoGetLanguageMask(handle + 1, &mask) == badHandle);
    Require(scePlayGoGetLanguageMask(handle, nullptr) == badPointer);
    Require(scePlayGoGetLanguageMask(handle, &mask) == 0 && mask == ~0ull);
    Require(scePlayGoSetLanguageMask(handle + 1, 0x5) == badHandle);
    Require(scePlayGoGetLanguageMask(handle, &mask) == 0 && mask == ~0ull);
    Require(scePlayGoSetLanguageMask(handle, 0x5) == 0);
    Require(scePlayGoGetLanguageMask(handle, &mask) == 0 && mask == 0x5);
    Require(scePlayGoSetLanguageMask(handle, ~0ull) == 0);
    Require(scePlayGoGetLanguageMask(handle, &mask) == 0 && mask == ~0ull);

    for (const auto getOptionalChunk : {scePlayGoGetSupportedOptionalChunk, scePlayGoGetOptionalChunk}) {
        PlayGoOptionalChunk option{};
        Require(getOptionalChunk(handle + 1, 0, &option) == badHandle);
        Require(getOptionalChunk(handle, 0, nullptr) == badPointer);
        Require(getOptionalChunk(handle, 0, &option) == 0 && option.bitmask == ~0ull);
        Require(getOptionalChunk(handle, 3, &option) == static_cast<int>(0x80B20024));
        Require(getOptionalChunk(handle, 0xFFFFFFFF, &option) == static_cast<int>(0x80B20024));
        Require(getOptionalChunk(handle, 3, nullptr) == badPointer);
    }

    PlayGoToDo todo[2] = {{0, 3, 0}, {0, 0, 0}};
    Require(scePlayGoSetToDoList(handle + 1, todo, 2) == badHandle);
    Require(scePlayGoSetToDoList(handle, nullptr, 2) == badPointer);
    Require(scePlayGoSetToDoList(handle, todo, 0) == badSize);
    Require(scePlayGoSetToDoList(handle, todo, 2) == 0);
    todo[1].locus = 1;
    Require(scePlayGoSetToDoList(handle, todo, 2) == badLocus);
    todo[1] = {1, 3, 0};
    RequireMissingChunkMetadata([&] { scePlayGoSetToDoList(handle, todo, 2); });
    todo[1] = {0xFFFF, 3, 0};
    RequireMissingChunkMetadata([&] { scePlayGoSetToDoList(handle, todo, 2); });

    std::uint32_t entries = 1;
    Require(scePlayGoGetToDoList(handle, todo, 2, &entries) == 0 && entries == 0);
    const std::uint16_t chunk = 0;
    std::int8_t locus = 0;
    Require(scePlayGoGetLocus(handle, &chunk, 0, &locus) == badSize);
    Require(scePlayGoGetLocus(handle, &chunk, 1001, &locus) == badSize);
    std::uint32_t invalidListCount = 42;
    std::uint16_t invalidListOutput = 0;
    Require(scePlayGoGetChunkId(handle, &invalidListOutput, 0, &invalidListCount) == badSize);
    Require(scePlayGoGetChunkId(handle, &invalidListOutput, 1001, &invalidListCount) == badSize);
    Require(invalidListCount == 42);
    Require(scePlayGoGetLocus(handle, &chunk, 1, &locus) == 0 && locus == 3);
    const std::uint16_t undeclared[2] = {1, 10};
    std::int8_t loci[2] = {0, 0};
    RequireMissingChunkMetadata([&] { scePlayGoGetLocus(handle, undeclared, 2, loci); });
    const std::uint16_t onlyDeclared[2] = {0, 0};
    Require(scePlayGoGetLocus(handle, onlyDeclared, 2, loci) == 0 && loci[0] == 3 && loci[1] == 3);
    std::uint32_t chunkCount = 0;
    Require(scePlayGoGetChunkId(handle, nullptr, 0, &chunkCount) == 0 && chunkCount == 1);
    Require(scePlayGoClose(handle + 1) == badHandle);
    Require(scePlayGoClose(handle) == 0);
    return 0;
}

static int CheckRejectedLayout(const char* root, const char* fileName) {
    std::filesystem::current_path(root);
    PlayGoInitParams init{};
    int handle = 0;
    Require(scePlayGoInitialize(&init) == 0);
    Require(scePlayGoOpen(&handle, nullptr) == 0);
    const std::uint16_t chunk = 0;
    std::int8_t locus = 0;
    bool threw = false;
    try {
        scePlayGoGetLocus(handle, &chunk, 1, &locus);

    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()).find(fileName) != std::string_view::npos;
    }
    Require(threw);
    return 0;
}

static void SetUnlistedChunks(const char* value) {
#ifdef _WIN32
    Require(_putenv_s("APS5_PLAYGO_UNLISTED_CHUNKS", value) == 0);
#else
    Require(setenv("APS5_PLAYGO_UNLISTED_CHUNKS", value, 1) == 0);
#endif
}

static int CheckNoPlayGoFileAnswered(const char* root, const char* answer) {
    constexpr int badChunkId = static_cast<int>(0x80B2000C);
    SetUnlistedChunks(answer);
    std::filesystem::current_path(root);
    PlayGoInitParams init{};
    int handle = 0;
    Require(scePlayGoInitialize(&init) == 0);
    Require(scePlayGoOpen(&handle, nullptr) == 0);
    const std::uint16_t unlisted[3] = {0, 1, 999};
    std::int8_t loci[3] = {0, 0, 0};
    const int result = scePlayGoGetLocus(handle, unlisted, 3, loci);
    if (std::string_view(answer) == "installed") Require(result == 0 && loci[0] == 3 && loci[1] == 3 && loci[2] == 3);
    else Require(result == badChunkId);
    const std::uint16_t first = 0;
    Require(scePlayGoGetLocus(handle, &first, 1, loci) == 0 && loci[0] == 3);
    Require(scePlayGoClose(handle) == 0);
    return 0;
}

static int CheckUnknownUnlistedChunkAnswer(const char* root) {
    SetUnlistedChunks("maybe");
    std::filesystem::current_path(root);
    PlayGoInitParams init{};
    int handle = 0;
    Require(scePlayGoInitialize(&init) == 0);
    Require(scePlayGoOpen(&handle, nullptr) == 0);
    const std::uint16_t unlisted = 1;
    std::int8_t locus = 0;
    bool threw = false;
    try {
        scePlayGoGetLocus(handle, &unlisted, 1, &locus);

    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()).find("APS5_PLAYGO_UNLISTED_CHUNKS must be installed or absent") != std::string_view::npos;
    }
    Require(threw);
    return 0;
}

static void RunLayout(const char* self, const char* mode, std::initializer_list<std::pair<const char*, std::string>> files) {
    const auto root = std::filesystem::temp_directory_path() / ("guest_playgo_" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "app0" / "sce_sys");
    for (const auto& [name, content] : files) std::ofstream(root / "app0" / "sce_sys" / name, std::ios::binary).write(content.data(), static_cast<std::streamsize>(content.size()));
    const std::string arguments = "\"" + std::string(self) + "\" " + mode + " \"" + root.string() + "\"";
#ifdef _WIN32
    const std::string command = "\"" + arguments + "\"";
#else
    const std::string command = arguments;
#endif
    const int status = std::system(command.c_str());
    std::filesystem::remove_all(root);
    Require(status == 0);
}

static std::string ChunkTableFile(const char* magic = "plgx", char chunks = 3) {
    std::string header(0x100, '\0');
    header.replace(0, 4, magic);
    header[0x0A] = chunks;
    return header;
}

int main(int argc, char** argv) {
    if (argc == 3) {
        const std::string_view mode = argv[1];
        if (mode == "--no-file") return CheckNoPlayGoFile(argv[2]);
        if (mode == "--no-file-installed") return CheckNoPlayGoFileAnswered(argv[2], "installed");
        if (mode == "--no-file-absent") return CheckNoPlayGoFileAnswered(argv[2], "absent");
        if (mode == "--no-file-unknown-answer") return CheckUnknownUnlistedChunkAnswer(argv[2]);
        if (mode == "--package-chunk-table" || mode == "--scenario-with-chunk-table") return CheckPackageChunkTable(argv[2]);
        if (mode == "--scenario-only") return CheckScenarioWithoutChunkTable(argv[2]);
        if (mode == "--ficm-only") return CheckRejectedLayout(argv[2], "playgo-ficm.dat");
        if (mode == "--hash-table-only") return CheckRejectedLayout(argv[2], "playgo-hash-table.dat");
        if (mode == "--invalid-chunk-table") return CheckRejectedLayout(argv[2], "playgo-chunk.dat");
    }
    RunLayout(argv[0], "--no-file", {});
    RunLayout(argv[0], "--no-file-installed", {});
    RunLayout(argv[0], "--no-file-absent", {});
    RunLayout(argv[0], "--no-file-unknown-answer", {});
    RunLayout(argv[0], "--package-chunk-table", {{"playgo-chunk.dat", ChunkTableFile()}});
    RunLayout(argv[0], "--package-chunk-table", {{"playgo-chunk.dat", ChunkTableFile("plgo")}});
    RunLayout(argv[0], "--scenario-with-chunk-table", {{"playgo-chunk.dat", ChunkTableFile()}, {"playgo-scenario.json", "{}"}});
    RunLayout(argv[0], "--scenario-only", {{"playgo-scenario.json", "{}"}});
    RunLayout(argv[0], "--ficm-only", {{"playgo-ficm.dat", "x"}});
    RunLayout(argv[0], "--hash-table-only", {{"playgo-hash-table.dat", "x"}});
    RunLayout(argv[0], "--invalid-chunk-table", {{"playgo-chunk.dat", "not a chunk table"}});
    RunLayout(argv[0], "--invalid-chunk-table", {{"playgo-chunk.dat", ChunkTableFile("plgo", 0)}});
    return 0;
}
