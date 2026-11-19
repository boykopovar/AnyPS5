#include "prx/Party/Party.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <string>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "Party manager check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, location);
}

void* APS5_VABI Allocate(std::size_t, PartyMemoryType) {
    return nullptr;
}

void APS5_VABI Free(void*, PartyMemoryType) {}

void CheckMemoryCallbacksComeFirstAndOnce() {
    RequireThrows([] { PartySetMemoryCallbacks(nullptr, Free); });
    RequireThrows([] { PartySetMemoryCallbacks(Allocate, nullptr); });
    Require(PartySetMemoryCallbacks(Allocate, Free) == 0);
    RequireThrows([] { PartySetMemoryCallbacks(Allocate, Free); });
}

void CheckLifecycle() {
    const PartyManagerState* handle = nullptr;
    RequireThrows([] { PartyInitialize(nullptr, nullptr); });
    Require(PartyInitialize("ABCDE", &handle) == 0 && handle);
    const PartyManagerState* second = nullptr;
    RequireThrows([&] { PartyInitialize("ABCDE", &second); });

    std::uint32_t count = 7;
    const PartyStateChange* const* changes = nullptr;
    Require(PartyStartProcessingStateChanges(handle, &count, &changes) == 0 && count == 0 && changes);
    Require(PartyFinishProcessingStateChanges(handle, count, changes) == 0);
    RequireThrows([&] { PartyFinishProcessingStateChanges(handle, 1, changes); });

    const PartyNetwork* const* networks = nullptr;
    count = 7;
    Require(PartyGetNetworks(handle, &count, &networks) == 0 && count == 0 && networks);

    Require(PartyCleanup(handle) == 0);
    RequireThrows([&] { PartyStartProcessingStateChanges(handle, &count, &changes); });
    RequireThrows([&] { PartyCleanup(handle); });
    Require(PartyInitialize("ABCDE", &handle) == 0);
    Require(PartyCleanup(handle) == 0);
}

void CheckLocalUsers() {
    const PartyManagerState* handle = nullptr;
    Require(PartyInitialize("ABCDE", &handle) == 0);

    std::uint32_t count = 7;
    const PartyLocalUser* const* users = nullptr;
    Require(PartyGetLocalUsers(handle, &count, &users) == 0 && count == 0);

    std::string entity = "1234567890ABCDEF";
    const PartyLocalUser* first = nullptr;
    Require(PartyCreateLocalUser(handle, entity.c_str(), "token", &first) == 0 && first);
    entity.assign("overwritten");
    PartyString entityId = nullptr;
    Require(PartyLocalUserGetEntityId(first, &entityId) == 0 && std::strcmp(entityId, "1234567890ABCDEF") == 0);
    Require(PartyLocalUserUpdateEntityToken(first, "fresh") == 0);
    RequireThrows([&] { PartyLocalUserUpdateEntityToken(first, nullptr); });

    for (int index = 1; index < 8; ++index) {
        const PartyLocalUser* user = nullptr;
        Require(PartyCreateLocalUser(handle, std::to_string(index).c_str(), "token", &user) == 0);
    }
    Require(PartyGetLocalUsers(handle, &count, &users) == 0 && count == 8 && users[0] == first);
    Require(PartyLocalUserGetEntityId(users[7], &entityId) == 0 && std::strcmp(entityId, "7") == 0);
    const PartyLocalUser* ninth = nullptr;
    RequireThrows([&] { PartyCreateLocalUser(handle, "8", "token", &ninth); });
    RequireThrows([&] { PartyCreateLocalUser(handle, nullptr, "token", &ninth); });

    Require(PartyCleanup(handle) == 0);
    RequireThrows([&] { PartyLocalUserGetEntityId(first, &entityId); });
}

}

int main() {
    CheckMemoryCallbacksComeFirstAndOnce();
    CheckLifecycle();
    CheckLocalUsers();
    return 0;
}
