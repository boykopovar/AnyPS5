#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>

// The game is fully installed on the host, so every chunk is local and nothing is pending.
static constexpr int SCE_PLAYGO_ERROR_BAD_POINTER = static_cast<int>(0x80B2000A);
static constexpr int SCE_PLAYGO_ERROR_BAD_HANDLE = static_cast<int>(0x80B20009);
static constexpr int PLAYGO_HANDLE = 1;
static constexpr int8_t PLAYGO_LOCUS_NOT_DOWNLOADED = 0;
static constexpr int8_t PLAYGO_LOCUS_LOCAL_SLOW = 2;
static constexpr int8_t PLAYGO_LOCUS_LOCAL_FAST = 3;
static constexpr uint64_t PLAYGO_LANGUAGE_MASK_ALL = ~0ull;
static constexpr uint64_t PLAYGO_OPTIONAL_CHUNK_MASK_ALL = ~0ull;
static constexpr int32_t PLAYGO_INSTALL_SPEED_FULL = 2;
static constexpr uint32_t PLAYGO_SCENARIO_CHUNK_COUNT = 1000;

static int32_t g_installSpeed = PLAYGO_INSTALL_SPEED_FULL;
static uint64_t g_languageMask = PLAYGO_LANGUAGE_MASK_ALL;

static constexpr int SCE_PLAYGO_ERROR_BAD_CHUNK_ID = static_cast<int>(0x80B2000C);
static constexpr int SCE_PLAYGO_ERROR_BAD_SIZE = static_cast<int>(0x80B2000B);
static constexpr int SCE_PLAYGO_ERROR_BAD_LOCUS = static_cast<int>(0x80B20010);

struct ChunkTable {
    std::set<uint16_t> ids{0};
    bool defined = false;
    bool scenario = false;
};

static bool ReadPackageChunks(ChunkTable& table) {
    constexpr std::size_t CountOffset = 0x0A;
    std::ifstream file(ResolvePath_nid_no_patch("/app0/sce_sys/playgo-chunk.dat"), std::ios::binary);
    char header[CountOffset + sizeof(uint16_t)]{};
    if (!file) return false;
    const auto unreadable = [] { return std::runtime_error("scePlayGo: sce_sys/playgo-chunk.dat is not a chunk table this library reads (expected a plgx or plgo header with a nonzero chunk count)"); };
    if (!file.read(header, sizeof(header)) || (std::memcmp(header, "plgx", 4) != 0 && std::memcmp(header, "plgo", 4) != 0)) throw unreadable();
    uint16_t count = 0;
    std::memcpy(&count, header + CountOffset, sizeof(count));
    if (count == 0) throw unreadable();
    for (uint32_t id = 0; id < count; ++id) table.ids.insert(static_cast<uint16_t>(id));
    table.defined = true;
    return true;
}

static void RejectUnreadablePlayGoFiles() {
    std::error_code error;
    const std::filesystem::path directory = ResolvePath_nid_no_patch("/app0/sce_sys");
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        const auto name = entry.path().filename().string();
        if (name.rfind("playgo-", 0) == 0)
            throw std::runtime_error("scePlayGo: sce_sys/" + name + " is present but no chunk table (playgo-chunk.dat) or scenario file (playgo-scenario.json) is, so the chunk ids are unknown");
    }
}

static const ChunkTable& Chunks() {
    static const ChunkTable chunks = [] {
        ChunkTable table;
        std::ifstream file(ResolvePath_nid_no_patch("/app0/playgo-chunkdefs.xml"));
        if (!file) {
            if (ReadPackageChunks(table)) return table;
            table.scenario = std::ifstream(ResolvePath_nid_no_patch("/app0/sce_sys/playgo-scenario.json")).good();
            if (!table.scenario) RejectUnreadablePlayGoFiles();
            else for (uint32_t id = 1; id < PLAYGO_SCENARIO_CHUNK_COUNT; ++id) table.ids.insert(static_cast<uint16_t>(id));
            return table;
        }
        table.defined = true;
        auto& result = table.ids;
        const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        static const std::regex chunk(R"re(<chunk\s+id="(\d+)")re");
        for (auto it = std::sregex_iterator(text.begin(), text.end(), chunk); it != std::sregex_iterator(); ++it)
            result.insert(static_cast<uint16_t>(std::stoul((*it)[1].str())));
        static const std::regex defaultChunk(R"re(default_chunk="(\d+)")re");
        std::smatch match;
        if (std::regex_search(text, match, defaultChunk)) {
            const auto last = std::stoul(match[1].str());
            for (unsigned long id = 0; id <= last && id <= 0xFFFF; ++id) result.insert(static_cast<uint16_t>(id));
        }
        return table;
    }();
    return chunks;
}

enum class UnlistedChunkAnswer { Stop, Installed, Absent };

static UnlistedChunkAnswer UnlistedChunks() {
    static const UnlistedChunkAnswer answer = [] {
        const char* text = std::getenv("APS5_PLAYGO_UNLISTED_CHUNKS");
        if (text == nullptr || text[0] == '\0') return UnlistedChunkAnswer::Stop;
        const std::string value(text);
        if (value == "installed") return UnlistedChunkAnswer::Installed;
        if (value == "absent") return UnlistedChunkAnswer::Absent;
        throw std::runtime_error("scePlayGo: APS5_PLAYGO_UNLISTED_CHUNKS must be installed or absent, not \"" + value + "\"");
    }();
    return answer;
}

static bool IsKnownChunk(uint16_t id) {
    const auto& chunks = Chunks();
    if (chunks.defined) return chunks.ids.contains(id);
    if (chunks.scenario || id == 0) return true;
    switch (UnlistedChunks()) {
    case UnlistedChunkAnswer::Installed: return true;
    case UnlistedChunkAnswer::Absent: return false;
    case UnlistedChunkAnswer::Stop: break;
    }
    throw std::runtime_error("scePlayGo: sce_sys/playgo-chunk.dat and sce_sys/playgo-scenario.json are missing, so chunk ID " + std::to_string(id) + " cannot be checked; set APS5_PLAYGO_UNLISTED_CHUNKS=installed or =absent to answer for this dump");
}

static int ValidateChunks(const uint16_t* chunk_ids, uint32_t number_of_entries) {
    if (!chunk_ids) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (number_of_entries == 0) return SCE_PLAYGO_ERROR_BAD_SIZE;
    for (uint32_t index = 0; index < number_of_entries; ++index)
        if (!IsKnownChunk(chunk_ids[index])) return SCE_PLAYGO_ERROR_BAD_CHUNK_ID;
    return 0;
}

extern "C" {

int APS5_VABI scePlayGoClose(int handle) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    return 0;
}

int APS5_VABI scePlayGoGetChunkId(int handle, uint16_t* outChunkIdList, uint32_t numberOfEntries, uint32_t* outEntries) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!outEntries) return SCE_PLAYGO_ERROR_BAD_POINTER;
    const auto& valid = Chunks().ids;
    if (!outChunkIdList) {
        *outEntries = static_cast<uint32_t>(valid.size());
        return 0;
    }
    if (numberOfEntries == 0 || numberOfEntries > 1000) return SCE_PLAYGO_ERROR_BAD_SIZE;
    uint32_t written = 0;
    for (const auto id : valid) {
        if (written == numberOfEntries) break;
        outChunkIdList[written++] = id;
    }
    *outEntries = written;
    return 0;
}

int APS5_VABI scePlayGoGetEta(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int64_t* out_eta) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!out_eta) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (const int error = ValidateChunks(chunk_ids, number_of_entries)) return error;
    *out_eta = 0;
    return 0;
}

int APS5_VABI scePlayGoGetInstallChunkId(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
    return scePlayGoGetChunkId(handle, out_chunk_id_list, number_of_entries, out_entries);
}

int APS5_VABI scePlayGoGetInstallSpeed(int handle, int32_t* out_speed) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!out_speed) return SCE_PLAYGO_ERROR_BAD_POINTER;
    *out_speed = g_installSpeed;
    return 0;
}

int APS5_VABI scePlayGoGetLanguageMask(int handle, uint64_t* out_language_mask) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!out_language_mask) return SCE_PLAYGO_ERROR_BAD_POINTER;
    *out_language_mask = g_languageMask;
    return 0;
}

int APS5_VABI scePlayGoGetLocus(int handle, const uint16_t* chunkIds, uint32_t numberOfEntries, int8_t* outLoci) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!outLoci) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (chunkIds && numberOfEntries > 1000) return SCE_PLAYGO_ERROR_BAD_SIZE;
    if (const int error = ValidateChunks(chunkIds, numberOfEntries)) return error;
    for (uint32_t index = 0; index < numberOfEntries; ++index) outLoci[index] = PLAYGO_LOCUS_LOCAL_FAST;
    return 0;
}

int APS5_VABI scePlayGoGetOptionalChunk(int handle, std::uint32_t type, PlayGoOptionalChunk* option) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!option) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (type > 2) return static_cast<int>(0x80B20024);
    option->bitmask = PLAYGO_OPTIONAL_CHUNK_MASK_ALL;
    return 0;
}

int APS5_VABI scePlayGoGetProgress(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, PlayGoProgress* out_progress) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!out_progress) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (const int error = ValidateChunks(chunk_ids, number_of_entries)) return error;
    out_progress->progress_size = 1;
    out_progress->total_size = 1;
    return 0;
}

int APS5_VABI scePlayGoGetSupportedOptionalChunk(int handle, std::uint32_t type, PlayGoOptionalChunk* option) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!option) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (type > 2) return static_cast<int>(0x80B20024);
    option->bitmask = PLAYGO_OPTIONAL_CHUNK_MASK_ALL;
    return 0;
}

int APS5_VABI scePlayGoGetToDoList(int handle, PlayGoToDo* out_todo_list, uint32_t number_of_entries, uint32_t* out_entries) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    (void)out_todo_list;
    (void)number_of_entries;
    if (!out_entries) return SCE_PLAYGO_ERROR_BAD_POINTER;
    *out_entries = 0;
    return 0;
}

int APS5_VABI scePlayGoInitialize(const PlayGoInitParams* init) {
    if (!init) return SCE_PLAYGO_ERROR_BAD_POINTER;
    return 0;
}

int APS5_VABI scePlayGoOpen(int* out_handle, const void* param) {
    (void)param;
    if (!out_handle) return SCE_PLAYGO_ERROR_BAD_POINTER;
    *out_handle = PLAYGO_HANDLE;
    return 0;
}

int APS5_VABI scePlayGoPrefetch(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int8_t minimum_locus) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    (void)minimum_locus;
    return ValidateChunks(chunk_ids, number_of_entries);
}

int APS5_VABI scePlayGoPrefetchOptionalChunk(int handle, int32_t type, const PlayGoOptionalChunk* option) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    (void)type;
    (void)option;
    return 0;
}

int APS5_VABI scePlayGoSetInstallSpeed(int handle, int32_t speed) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    g_installSpeed = speed;
    return 0;
}

int APS5_VABI scePlayGoSetToDoList(int handle, const PlayGoToDo* todo_list, uint32_t number_of_entries) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!todo_list) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (number_of_entries == 0) return SCE_PLAYGO_ERROR_BAD_SIZE;
    for (uint32_t index = 0; index < number_of_entries; ++index) {
        if (!IsKnownChunk(todo_list[index].chunk_id)) return SCE_PLAYGO_ERROR_BAD_CHUNK_ID;
        const int8_t locus = todo_list[index].locus;
        if (locus != PLAYGO_LOCUS_NOT_DOWNLOADED && locus != PLAYGO_LOCUS_LOCAL_SLOW && locus != PLAYGO_LOCUS_LOCAL_FAST) return SCE_PLAYGO_ERROR_BAD_LOCUS;
    }
    return 0;
}

int APS5_VABI scePlayGoTerminate(void) {
    return 0;
}

int APS5_VABI scePlayGoRequestNextChunk() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayGoSetLanguageMask(int handle, uint64_t language_mask) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    g_languageMask = language_mask;
    return 0;
}

int APS5_VABI scePlayGoSnapshot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
