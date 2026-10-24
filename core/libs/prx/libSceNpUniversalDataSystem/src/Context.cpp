#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace Uds {

namespace {

struct Contexts {
    std::mutex mutex;
    std::map<int, ContextOwner> owners;
    int next = NP_UNIVERSAL_DATA_SYSTEM_CONTEXT_DEFAULT;
};

Contexts& contexts() {
    static Contexts table;
    return table;
}

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

int APS5_VABI sceNpUniversalDataSystemCreateContext(int* context, int user_id, uint32_t service_label, uint64_t options) {
    if (context == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": invalid argument context");
    }
    auto& table = Uds::contexts();
    std::lock_guard lock(table.mutex);
    *context = table.next++;
    table.owners[*context] = {user_id, service_label};
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyContext(int context) {
    auto& table = Uds::contexts();
    std::lock_guard lock(table.mutex);
    table.owners.erase(context);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemRegisterContext(int context, int handle, uint64_t options) {
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}
