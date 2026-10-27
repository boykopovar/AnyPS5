#include "prx/libSceNpTrophy/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int NP_TROPHY_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80551604u);
constexpr int NP_TROPHY_ERROR_INSUFFICIENT_BUFFER = static_cast<int>(0x80551605u);
constexpr int NP_TROPHY_ERROR_ABORT = static_cast<int>(0x80551607u);
constexpr int NP_TROPHY_ERROR_INVALID_HANDLE = static_cast<int>(0x80551608u);
constexpr int NP_TROPHY_ERROR_INVALID_CONTEXT = static_cast<int>(0x80551609u);
constexpr int NP_TROPHY_ERROR_INVALID_TROPHY_ID = static_cast<int>(0x8055160au);
constexpr int NP_TROPHY_ERROR_INVALID_GROUP_ID = static_cast<int>(0x8055160bu);
constexpr int NP_TROPHY_ERROR_TROPHY_ALREADY_UNLOCKED = static_cast<int>(0x8055160cu);
constexpr int NP_TROPHY_ERROR_NOT_REGISTERED = static_cast<int>(0x8055160fu);
constexpr int NP_TROPHY_ERROR_ALREADY_REGISTERED = static_cast<int>(0x80551610u);
constexpr int NP_TROPHY_ERROR_CONTEXT_ALREADY_EXISTS = static_cast<int>(0x80551613u);
constexpr int NP_TROPHY_ERROR_ICON_FILE_NOT_FOUND = static_cast<int>(0x80551614u);
constexpr int NP_TROPHY_ERROR_INVALID_TRP_FILE_FORMAT = static_cast<int>(0x80551616u);
constexpr int NP_TROPHY_ERROR_TROPHY_NOT_UNLOCKED = static_cast<int>(0x8055161au);
constexpr int NP_TROPHY_ERROR_TITLE_CONF_NOT_INSTALLED = static_cast<int>(0x80551626u);
constexpr int NP_TROPHY_INVALID_ID = -1;

constexpr int NP_TROPHY_GRADE_UNKNOWN = 0;
constexpr int NP_TROPHY_BASE_GAME_GROUP_ID = -1;
constexpr std::uint32_t TrpMagic = 0xdca24d00u;
constexpr std::size_t TrpHeaderBytes = 0x60;
constexpr std::size_t TrpEntryBytes = 0x40;

static_assert(sizeof(GameDetails) == 0x4a0);

static_assert(sizeof(GameData) == 0x20);

static_assert(sizeof(GroupDetails) == 0x4a0);

static_assert(sizeof(GroupData) == 0x28);

static_assert(sizeof(TrophyDetails) == 0x498);

static_assert(sizeof(TrophyData) == 0x18);

struct TrpEntry {
    std::uint64_t offset;
    std::uint64_t size;
};

struct Context {
    int userId;
    std::uint32_t serviceLabel;
    bool registered = false;
    std::filesystem::path package;
    std::map<std::string, TrpEntry> entries;
    int trophies = 0;
    std::filesystem::path state;
    std::set<int> unlocked;
};

std::mutex g_mutex;
std::map<int, Context> g_contexts;
std::set<int> g_handles;
std::set<int> g_abortedHandles;
int g_nextContext = 1;
int g_nextHandle = 1;

std::uint32_t BigEndian32(const std::uint8_t* bytes) {
    return (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) | (std::uint32_t{bytes[2]} << 8) | bytes[3];
}

std::uint64_t BigEndian64(const std::uint8_t* bytes) {
    return (std::uint64_t{BigEndian32(bytes)} << 32) | BigEndian32(bytes + 4);
}

std::optional<std::map<std::string, TrpEntry>> ReadPackage(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::array<std::uint8_t, TrpHeaderBytes> header{};
    if (!file.read(reinterpret_cast<char*>(header.data()), header.size()) || BigEndian32(header.data()) != TrpMagic) return std::map<std::string, TrpEntry>{};
    const auto count = BigEndian32(header.data() + 0x10);
    if (BigEndian32(header.data() + 0x14) != TrpEntryBytes) return std::map<std::string, TrpEntry>{};
    std::map<std::string, TrpEntry> entries;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<std::uint8_t, TrpEntryBytes> entry{};
        if (!file.read(reinterpret_cast<char*>(entry.data()), entry.size())) return std::map<std::string, TrpEntry>{};
        const std::string name(reinterpret_cast<const char*>(entry.data()), strnlen(reinterpret_cast<const char*>(entry.data()), 32));
        entries[name] = {BigEndian64(entry.data() + 32), BigEndian64(entry.data() + 40)};
    }
    return entries;
}

void LoadState(Context& context) {
    std::ifstream file(context.state);
    int id = 0;
    while (file >> id) {
        if (id >= 0 && id < context.trophies) context.unlocked.insert(id);
    }
}

void SaveState(const Context& context) {
    std::filesystem::create_directories(context.state.parent_path());
    const auto temporary = context.state.string() + ".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        for (const auto id : context.unlocked) file << id << '\n';
    }
    std::filesystem::rename(temporary, context.state);
}

int Lookup(int contextId, int handle, Context*& context) {
    if (contextId == NP_TROPHY_INVALID_ID) return NP_TROPHY_ERROR_INVALID_CONTEXT;
    if (handle == NP_TROPHY_INVALID_ID) return NP_TROPHY_ERROR_INVALID_HANDLE;
    const auto found = g_contexts.find(contextId);
    if (found == g_contexts.end()) return NP_TROPHY_ERROR_INVALID_CONTEXT;
    if (!g_handles.contains(handle)) return NP_TROPHY_ERROR_INVALID_HANDLE;
    context = &found->second;
    return 0;
}

int LookupLive(int contextId, int handle, Context*& context) {
    if (const int error = Lookup(contextId, handle, context)) return error;
    return g_abortedHandles.contains(handle) ? NP_TROPHY_ERROR_ABORT : 0;
}

int Groups(const Context& context) {
    int groups = 0;
    for (int id = 0; id < NP_TROPHY_NUM_MAX; ++id) {
        char icon[16];
        std::snprintf(icon, sizeof(icon), "GR%03d.PNG", id);
        if (!context.entries.contains(icon)) break;
        groups = id + 1;
    }
    return groups;
}

std::uint32_t Progress(const Context& context) {
    return context.trophies == 0 ? 0u : static_cast<std::uint32_t>(context.unlocked.size() * 100 / static_cast<std::size_t>(context.trophies));
}

int CopyIcon(const Context& context, const std::string& name, void* buffer, std::uint64_t* size) {
    const auto entry = context.entries.find(name);
    if (entry == context.entries.end()) return NP_TROPHY_ERROR_ICON_FILE_NOT_FOUND;
    if (buffer == nullptr) {
        *size = entry->second.size;
        return 0;
    }
    if (*size < entry->second.size) return NP_TROPHY_ERROR_INSUFFICIENT_BUFFER;
    std::ifstream file(context.package, std::ios::binary);
    file.seekg(static_cast<std::streamoff>(entry->second.offset));
    if (!file.read(static_cast<char*>(buffer), static_cast<std::streamsize>(entry->second.size))) return NP_TROPHY_ERROR_INVALID_TRP_FILE_FORMAT;
    *size = entry->second.size;
    return 0;
}

}

extern "C" {

int APS5_VABI sceNpTrophyCreateContext(int* context, int userId, std::uint32_t serviceLabel, std::uint64_t options) {
    if (!context || options != 0) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    for (const auto& [id, existing] : g_contexts) {
        if (existing.userId == userId && existing.serviceLabel == serviceLabel) return NP_TROPHY_ERROR_CONTEXT_ALREADY_EXISTS;
    }
    const int id = g_nextContext++;
    g_contexts[id] = Context{userId, serviceLabel};
    *context = id;
    return 0;
}

int APS5_VABI sceNpTrophyDestroyContext(int context) {
    std::lock_guard lock(g_mutex);
    return g_contexts.erase(context) == 1 ? 0 : NP_TROPHY_ERROR_INVALID_CONTEXT;
}

int APS5_VABI sceNpTrophyCreateHandle(int* handle) {
    if (!handle) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    const int id = g_nextHandle++;
    g_handles.insert(id);
    *handle = id;
    return 0;
}

int APS5_VABI sceNpTrophyDestroyHandle(int handle) {
    std::lock_guard lock(g_mutex);
    g_abortedHandles.erase(handle);
    return g_handles.erase(handle) == 1 ? 0 : NP_TROPHY_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceNpTrophyAbortHandle(int handle) {
    std::lock_guard lock(g_mutex);
    if (!g_handles.contains(handle)) return NP_TROPHY_ERROR_INVALID_HANDLE;
    g_abortedHandles.insert(handle);
    return 0;
}

int APS5_VABI sceNpTrophyRegisterContext(int contextId, int handle, std::uint64_t options) {
    if (options != 0) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (context->registered) return NP_TROPHY_ERROR_ALREADY_REGISTERED;
    char name[32];
    std::snprintf(name, sizeof(name), "trophy%02u.trp", context->serviceLabel);
    context->package = ResolvePath_nid_no_patch((std::string("/app0/sce_sys/trophy/") + name).c_str());
    const auto entries = ReadPackage(context->package);
    if (!entries) return NP_TROPHY_ERROR_TITLE_CONF_NOT_INSTALLED;
    if (entries->empty()) return NP_TROPHY_ERROR_INVALID_TRP_FILE_FORMAT;
    context->entries = *entries;
    for (int id = 0; id < NP_TROPHY_NUM_MAX; ++id) {
        char icon[16];
        std::snprintf(icon, sizeof(icon), "TROP%03d.PNG", id);
        if (!context->entries.contains(icon)) break;
        context->trophies = id + 1;
    }
    std::snprintf(name, sizeof(name), "trophy%02u.%d", context->serviceLabel, context->userId);
    context->state = std::filesystem::path("_trophy") / name;
    LoadState(*context);
    context->registered = true;
    return 0;
}

int APS5_VABI sceNpTrophyGetTrophyUnlockState(int contextId, int handle, TrophyFlagArray* flags, std::uint32_t* count) {
    if (!flags || !count) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (!context->registered) return NP_TROPHY_ERROR_NOT_REGISTERED;
    std::memset(flags, 0, sizeof(*flags));
    for (const auto id : context->unlocked) flags->bits[id >> 5] |= 1u << (id & 31);
    *count = static_cast<std::uint32_t>(context->trophies);
    return 0;
}

int APS5_VABI sceNpTrophyUnlockTrophy(int contextId, int handle, int trophyId, int* platinumId) {
    if (!platinumId) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (!context->registered) return NP_TROPHY_ERROR_NOT_REGISTERED;
    if (trophyId < 0 || trophyId >= context->trophies) return NP_TROPHY_ERROR_INVALID_TROPHY_ID;
    if (context->unlocked.contains(trophyId)) return NP_TROPHY_ERROR_TROPHY_ALREADY_UNLOCKED;
    context->unlocked.insert(trophyId);
    SaveState(*context);
    *platinumId = NP_TROPHY_INVALID_ID;
    return 0;
}

int APS5_VABI sceNpTrophyGetGameIcon(int contextId, int handle, void* buffer, std::uint64_t* size) {
    if (!size) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (!context->registered) return NP_TROPHY_ERROR_NOT_REGISTERED;
    return CopyIcon(*context, "ICON0.PNG", buffer, size);
}

int APS5_VABI sceNpTrophyGetGroupIcon(int contextId, int handle, int groupId, void* buffer, std::uint64_t* size) {
    if (!size) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (!context->registered) return NP_TROPHY_ERROR_NOT_REGISTERED;
    if (groupId < 0) return NP_TROPHY_ERROR_INVALID_GROUP_ID;
    char icon[16];
    std::snprintf(icon, sizeof(icon), "GR%03d.PNG", groupId);
    return CopyIcon(*context, icon, buffer, size);
}

int APS5_VABI sceNpTrophyGetTrophyIcon(int contextId, int handle, int trophyId, void* buffer, std::uint64_t* size) {
    if (!size) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (!context->registered) return NP_TROPHY_ERROR_NOT_REGISTERED;
    if (trophyId < 0 || trophyId >= context->trophies) return NP_TROPHY_ERROR_INVALID_TROPHY_ID;
    if (!context->unlocked.contains(trophyId)) return NP_TROPHY_ERROR_TROPHY_NOT_UNLOCKED;
    char icon[16];
    std::snprintf(icon, sizeof(icon), "TROP%03d.PNG", trophyId);
    return CopyIcon(*context, icon, buffer, size);
}

int APS5_VABI sceNpTrophyGetGameInfo(int contextId, int handle, GameDetails* details, GameData* data) {
    if (!details && !data) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    if ((details && details->size != sizeof(GameDetails)) || (data && data->size != sizeof(GameData))) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (!context->registered) return NP_TROPHY_ERROR_NOT_REGISTERED;
    if (details) {
        std::memset(reinterpret_cast<std::uint8_t*>(details) + sizeof(details->size), 0, sizeof(GameDetails) - sizeof(details->size));
        details->numGroups = static_cast<std::uint32_t>(Groups(*context));
        details->numTrophies = static_cast<std::uint32_t>(context->trophies);
    }
    if (data) {
        std::memset(reinterpret_cast<std::uint8_t*>(data) + sizeof(data->size), 0, sizeof(GameData) - sizeof(data->size));
        data->unlockedTrophies = static_cast<std::uint32_t>(context->unlocked.size());
        data->progressPercentage = Progress(*context);
    }
    return 0;
}

int APS5_VABI sceNpTrophyGetGroupInfo(int contextId, int handle, int groupId, GroupDetails* details, GroupData* data) {
    if (!details && !data) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    if ((details && details->size != sizeof(GroupDetails)) || (data && data->size != sizeof(GroupData))) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (!context->registered) return NP_TROPHY_ERROR_NOT_REGISTERED;
    if (groupId != NP_TROPHY_BASE_GAME_GROUP_ID) {
        if (groupId < 0 || groupId >= Groups(*context)) return NP_TROPHY_ERROR_INVALID_GROUP_ID;
        NotImplemented_nid_no_patch(__func__);
    }
    if (details) {
        std::memset(reinterpret_cast<std::uint8_t*>(details) + sizeof(details->size), 0, sizeof(GroupDetails) - sizeof(details->size));
        details->groupId = groupId;
        details->numTrophies = static_cast<std::uint32_t>(context->trophies);
    }
    if (data) {
        std::memset(reinterpret_cast<std::uint8_t*>(data) + sizeof(data->size), 0, sizeof(GroupData) - sizeof(data->size));
        data->groupId = groupId;
        data->unlockedTrophies = static_cast<std::uint32_t>(context->unlocked.size());
        data->progressPercentage = Progress(*context);
    }
    return 0;
}

int APS5_VABI sceNpTrophyGetTrophyInfo(int contextId, int handle, int trophyId, TrophyDetails* details, TrophyData* data) {
    if (!details && !data) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    if ((details && details->size != sizeof(TrophyDetails)) || (data && data->size != sizeof(TrophyData))) return NP_TROPHY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_mutex);
    Context* context = nullptr;
    if (const int error = LookupLive(contextId, handle, context)) return error;
    if (!context->registered) return NP_TROPHY_ERROR_NOT_REGISTERED;
    if (trophyId < 0 || trophyId >= context->trophies) return NP_TROPHY_ERROR_INVALID_TROPHY_ID;
    if (Groups(*context) != 0) NotImplemented_nid_no_patch(__func__);
    if (details) {
        std::memset(reinterpret_cast<std::uint8_t*>(details) + sizeof(details->size), 0, sizeof(TrophyDetails) - sizeof(details->size));
        details->trophyId = trophyId;
        details->trophyGrade = NP_TROPHY_GRADE_UNKNOWN;
        details->groupId = NP_TROPHY_BASE_GAME_GROUP_ID;
    }
    if (data) {
        std::memset(reinterpret_cast<std::uint8_t*>(data) + sizeof(data->size), 0, sizeof(TrophyData) - sizeof(data->size));
        data->trophyId = trophyId;
        data->unlocked = context->unlocked.contains(trophyId);
    }
    return 0;
}

}
