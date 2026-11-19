#pragma once

#include <cstddef>
#include <cstdint>

#include "SceTypes.hpp"

using PartyError = std::uint32_t;
using PartyString = const char*;
using PartyMemoryType = std::uint32_t;
using PartyAllocateMemory = void* (APS5_VABI*)(std::size_t size, PartyMemoryType memoryType);
using PartyFreeMemory = void (APS5_VABI*)(void* pointer, PartyMemoryType memoryType);

struct PartyManagerState;
struct PartyLocalUser;
struct PartyNetwork;
struct PartyStateChange;

extern "C" {

PartyError APS5_VABI PartySetMemoryCallbacks(PartyAllocateMemory allocateMemory, PartyFreeMemory freeMemory);
PartyError APS5_VABI PartyInitialize(PartyString titleId, const PartyManagerState** handle);
PartyError APS5_VABI PartyCleanup(const PartyManagerState* handle);
PartyError APS5_VABI PartyStartProcessingStateChanges(const PartyManagerState* handle, std::uint32_t* stateChangeCount,
                                                      const PartyStateChange* const** stateChanges);
PartyError APS5_VABI PartyFinishProcessingStateChanges(const PartyManagerState* handle, std::uint32_t stateChangeCount,
                                                       const PartyStateChange* const* stateChanges);
PartyError APS5_VABI PartyCreateLocalUser(const PartyManagerState* handle, PartyString entityId,
                                          PartyString titlePlayerEntityToken, const PartyLocalUser** localUser);
PartyError APS5_VABI PartyGetLocalUsers(const PartyManagerState* handle, std::uint32_t* userCount,
                                        const PartyLocalUser* const** users);
PartyError APS5_VABI PartyGetNetworks(const PartyManagerState* handle, std::uint32_t* networkCount,
                                      const PartyNetwork* const** networks);
PartyError APS5_VABI PartyLocalUserGetEntityId(const PartyLocalUser* localUser, PartyString* entityId);
PartyError APS5_VABI PartyLocalUserUpdateEntityToken(const PartyLocalUser* localUser, PartyString titlePlayerEntityToken);

}
