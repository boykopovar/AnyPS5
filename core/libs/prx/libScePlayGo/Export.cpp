#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <string>

// The game is fully installed on the host, so every chunk is local and nothing is pending.
static constexpr int SCE_PLAYGO_ERROR_BAD_POINTER = static_cast<int>(0x80B20004);
static constexpr int SCE_PLAYGO_ERROR_BAD_HANDLE = static_cast<int>(0x80B20005);
static constexpr int PLAYGO_HANDLE = 1;
static constexpr int8_t PLAYGO_LOCUS_LOCAL_FAST = 3;
static constexpr int32_t PLAYGO_INSTALL_SPEED_FULL = 2;

static int32_t g_installSpeed = PLAYGO_INSTALL_SPEED_FULL;

static constexpr int SCE_PLAYGO_ERROR_BAD_CHUNK_ID = static_cast<int>(0x80B2000C);
static constexpr int SCE_PLAYGO_ERROR_BAD_SIZE = static_cast<int>(0x80B2000D);

// The package's chunk table is not part of the dump, so the chunk set comes from the title's
// playgo-chunkdefs.xml: every listed chunk plus chunks 0 through the default chunk. Games probe
// chunk IDs and size arrays from the result, so unknown IDs must be rejected.
static const std::set<uint16_t>& ValidChunks() {
    static const std::set<uint16_t> chunks = [] {
        std::set<uint16_t> result{0};
        std::ifstream file(ResolvePath_nid_no_patch("/app0/playgo-chunkdefs.xml"));
        if (!file) return result;
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
        return result;
    }();
    return chunks;
}

static int ValidateChunks(const uint16_t* chunk_ids, uint32_t number_of_entries) {
    if (!chunk_ids) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (number_of_entries == 0) return SCE_PLAYGO_ERROR_BAD_SIZE;
    const auto& valid = ValidChunks();
    for (uint32_t index = 0; index < number_of_entries; ++index)
        if (!valid.contains(chunk_ids[index])) return SCE_PLAYGO_ERROR_BAD_CHUNK_ID;
    return 0;
}

extern "C" {

int APS5_VABI scePlayGoClose(int handle) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    return 0;
}

int APS5_VABI scePlayGoGetChunkId(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!out_entries) return SCE_PLAYGO_ERROR_BAD_POINTER;
    const auto& valid = ValidChunks();
    if (!out_chunk_id_list) {
        *out_entries = static_cast<uint32_t>(valid.size());
        return 0;
    }
    uint32_t written = 0;
    for (const auto id : valid) {
        if (written == number_of_entries) break;
        out_chunk_id_list[written++] = id;
    }
    *out_entries = written;
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
 (void)handle;
 (void)out_chunk_id_list;
 (void)number_of_entries;
 (void)out_entries;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetInstallSpeed(int handle, int32_t* out_speed) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!out_speed) return SCE_PLAYGO_ERROR_BAD_POINTER;
    *out_speed = g_installSpeed;
    return 0;
}

int APS5_VABI scePlayGoGetLanguageMask(int handle, uint64_t* out_language_mask) {
 (void)handle;
 (void)out_language_mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetLocus(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int8_t* out_loci) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    if (!out_loci) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (const int error = ValidateChunks(chunk_ids, number_of_entries)) return error;
    for (uint32_t index = 0; index < number_of_entries; ++index) out_loci[index] = PLAYGO_LOCUS_LOCAL_FAST;
    return 0;
}

int APS5_VABI scePlayGoGetOptionalChunk(int handle, int32_t type, PlayGoOptionalChunk* option) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    (void)type;
    if (!option) return SCE_PLAYGO_ERROR_BAD_POINTER;
    option->bitmask = 0;
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

int APS5_VABI scePlayGoGetSupportedOptionalChunk(int handle, int32_t type, PlayGoOptionalChunk* option) {
    if (handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    (void)type;
    if (!option) return SCE_PLAYGO_ERROR_BAD_POINTER;
    option->bitmask = 0;
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
 (void)handle;
 (void)todo_list;
 (void)number_of_entries;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoTerminate(void) {
    return 0;
}

}
