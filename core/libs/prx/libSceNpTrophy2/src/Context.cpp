#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"
#include "prx/libc/include/General.hpp"

namespace NpTrophy2 {

namespace {

struct Contexts {
    std::mutex mutex;
    std::map<int, ContextOwner> owners;
    int next = NP_TROPHY2_CONTEXT_DEFAULT;
};

Contexts& contexts() {
    static Contexts table;
    return table;
}

}

int CreateContext(std::int32_t userId, std::uint32_t serviceLabel) {
    auto& table = contexts();
    std::lock_guard lock(table.mutex);
    const int context = table.next++;
    table.owners[context] = {userId, serviceLabel};
    return context;
}

void DestroyContext(int context) {
    auto& table = contexts();
    std::lock_guard lock(table.mutex);
    table.owners.erase(context);
}

std::optional<ContextOwner> FindContext(int context) {
    auto& table = contexts();
    std::lock_guard lock(table.mutex);
    const auto found = table.owners.find(context);
    if (found == table.owners.end()) return std::nullopt;
    return found->second;
}

}

extern "C" {

int APS5_VABI sceNpTrophy2CreateContext(int* context, int user_id, uint32_t service_label, uint64_t options) {
    if (context == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)options;
    *context = NpTrophy2::CreateContext(user_id, service_label);
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2DestroyContext(int context) {
    NpTrophy2::DestroyContext(context);
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2RegisterContext(int context, int handle, uint64_t options) {
    (void)context;
    (void)handle;
    (void)options;
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2UnregisterUnlockCallback() {
    return SCE_NP_TROPHY2_OK;
}


int APS5_VABI sceNpTrophy2GetRewardIcon(int context, int handle, int reward_id, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)reward_id;
    (void)buffer;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2ShowTrophyList(int context) {
    (void)context;
    return SCE_NP_TROPHY2_OK;
}

}
