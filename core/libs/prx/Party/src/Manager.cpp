#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "prx/Party/Party.hpp"
#include "prx/libc/include/General.hpp"

struct PartyLocalUser {
    std::string entityId;
    std::string titlePlayerEntityToken;
};

struct PartyManagerState {
    std::string titleId;
    std::vector<std::unique_ptr<PartyLocalUser>> localUsers;
    std::vector<const PartyLocalUser*> localUserHandles;
};

namespace {

constexpr PartyError kSuccess = 0;
constexpr std::size_t kMaxLocalUsers = 8;

std::mutex mutex;
bool partyCalled = false;
bool memoryCallbacksSet = false;
std::unique_ptr<PartyManagerState> party;

PartyManagerState& Live(const PartyManagerState* handle, const char* function) {
    if (!handle || handle != party.get()) throw std::invalid_argument(std::string(function) + " requires the initialized Party handle");
    return *party;
}

PartyLocalUser& LiveUser(const PartyLocalUser* localUser, const char* function) {
    if (party) {
        for (auto& user : party->localUsers) {
            if (user.get() == localUser) return *user;
        }
    }
    throw std::invalid_argument(std::string(function) + " requires a local user of the initialized Party handle");
}

void RequireOutput(const void* output, const char* function) {
    if (!output) throw std::invalid_argument(std::string(function) + " requires its output pointers");
}

template<typename TValue>
void WriteOutput(TValue* output, const std::type_identity_t<TValue>& value, const char* function) {
    if (!GuestCopyOut_nid_no_patch(output, &value, sizeof(value))) {
        throw std::invalid_argument(std::string(function) + " requires writable output pointers");
    }
}

}

extern "C" {

PartyError APS5_VABI PartySetMemoryCallbacks(PartyAllocateMemory allocateMemory, PartyFreeMemory freeMemory) {
    std::scoped_lock lock(mutex);
    if (!allocateMemory || !freeMemory) throw std::invalid_argument("PartySetMemoryCallbacks requires both callbacks");
    if (partyCalled || memoryCallbacksSet) {
        throw std::invalid_argument("PartySetMemoryCallbacks must come before every other Party call and only once");
    }
    memoryCallbacksSet = true;
    return kSuccess;
}

PartyError APS5_VABI PartyInitialize(PartyString titleId, const PartyManagerState** handle) {
    std::scoped_lock lock(mutex);
    partyCalled = true;
    if (!titleId) throw std::invalid_argument("PartyInitialize requires a title ID");
    RequireOutput(handle, __func__);
    if (party) throw std::invalid_argument("PartyInitialize called again without PartyCleanup");
    auto state = std::make_unique<PartyManagerState>();
    state->titleId = titleId;
    WriteOutput(handle, state.get(), __func__);
    party = std::move(state);
    return kSuccess;
}

PartyError APS5_VABI PartyCleanup(const PartyManagerState* handle) {
    std::scoped_lock lock(mutex);
    Live(handle, __func__);
    party.reset();
    return kSuccess;
}

PartyError APS5_VABI PartyStartProcessingStateChanges(const PartyManagerState* handle, std::uint32_t* stateChangeCount,
                                                      const PartyStateChange* const** stateChanges) {
    static const PartyStateChange* const noStateChanges[1] = {};
    std::scoped_lock lock(mutex);
    Live(handle, __func__);
    RequireOutput(stateChangeCount, __func__);
    RequireOutput(stateChanges, __func__);
    WriteOutput(stateChangeCount, 0, __func__);
    WriteOutput(stateChanges, noStateChanges, __func__);
    return kSuccess;
}

PartyError APS5_VABI PartyFinishProcessingStateChanges(const PartyManagerState* handle, std::uint32_t stateChangeCount,
                                                       const PartyStateChange* const* stateChanges) {
    std::scoped_lock lock(mutex);
    Live(handle, __func__);
    (void)stateChanges;
    if (stateChangeCount != 0) {
        throw std::invalid_argument("PartyFinishProcessingStateChanges received state changes the library never provided");
    }
    return kSuccess;
}

PartyError APS5_VABI PartyCreateLocalUser(const PartyManagerState* handle, PartyString entityId,
                                          PartyString titlePlayerEntityToken, const PartyLocalUser** localUser) {
    std::scoped_lock lock(mutex);
    auto& state = Live(handle, __func__);
    if (!entityId || !titlePlayerEntityToken) throw std::invalid_argument("PartyCreateLocalUser requires an entity ID and token");
    RequireOutput(localUser, __func__);
    if (state.localUsers.size() >= kMaxLocalUsers) {
        NotImplemented_nid_no_patch("PartyCreateLocalUser: the error code for more than 8 local users");
    }
    auto user = std::make_unique<PartyLocalUser>(PartyLocalUser{entityId, titlePlayerEntityToken});
    state.localUsers.reserve(state.localUsers.size() + 1);
    state.localUserHandles.reserve(state.localUserHandles.size() + 1);
    WriteOutput(localUser, user.get(), __func__);
    state.localUserHandles.push_back(user.get());
    state.localUsers.push_back(std::move(user));
    return kSuccess;
}

PartyError APS5_VABI PartyGetLocalUsers(const PartyManagerState* handle, std::uint32_t* userCount,
                                        const PartyLocalUser* const** users) {
    std::scoped_lock lock(mutex);
    auto& state = Live(handle, __func__);
    RequireOutput(userCount, __func__);
    RequireOutput(users, __func__);
    WriteOutput(userCount, static_cast<std::uint32_t>(state.localUserHandles.size()), __func__);
    WriteOutput(users, state.localUserHandles.data(), __func__);
    return kSuccess;
}

PartyError APS5_VABI PartyGetNetworks(const PartyManagerState* handle, std::uint32_t* networkCount,
                                      const PartyNetwork* const** networks) {
    static const PartyNetwork* const noNetworks[1] = {};
    std::scoped_lock lock(mutex);
    Live(handle, __func__);
    RequireOutput(networkCount, __func__);
    RequireOutput(networks, __func__);
    WriteOutput(networkCount, 0, __func__);
    WriteOutput(networks, noNetworks, __func__);
    return kSuccess;
}

PartyError APS5_VABI PartyLocalUserGetEntityId(const PartyLocalUser* localUser, PartyString* entityId) {
    std::scoped_lock lock(mutex);
    auto& user = LiveUser(localUser, __func__);
    RequireOutput(entityId, __func__);
    WriteOutput(entityId, user.entityId.c_str(), __func__);
    return kSuccess;
}

PartyError APS5_VABI PartyLocalUserUpdateEntityToken(const PartyLocalUser* localUser, PartyString titlePlayerEntityToken) {
    std::scoped_lock lock(mutex);
    auto& user = LiveUser(localUser, __func__);
    if (!titlePlayerEntityToken) throw std::invalid_argument("PartyLocalUserUpdateEntityToken requires a token");
    user.titlePlayerEntityToken = titlePlayerEntityToken;
    return kSuccess;
}

}
