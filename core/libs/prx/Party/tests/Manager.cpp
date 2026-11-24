#include "prx/Party/Party.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

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

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READONLY);
        Require(page != nullptr);
#else
        page = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(page != MAP_FAILED);
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(page, 0, MEM_RELEASE);
#else
        munmap(page, 4096);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template<typename TValue>
    TValue* As() const {
        return static_cast<TValue*>(page);
    }

private:
    void* page = nullptr;
};

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

void CheckUnwritableOutputsThrowAndChangeNothing() {
    const ReadOnlyPage page;
    RequireThrows([&] { PartyInitialize("ABCDE", page.As<const PartyManagerState*>()); });
    const PartyManagerState* handle = nullptr;
    Require(PartyInitialize("ABCDE", &handle) == 0 && handle);

    const PartyStateChange* const* changes = nullptr;
    std::uint32_t count = 7;
    RequireThrows([&] { PartyStartProcessingStateChanges(handle, page.As<std::uint32_t>(), &changes); });
    RequireThrows([&] { PartyGetNetworks(handle, &count, page.As<const PartyNetwork* const*>()); });

    RequireThrows([&] { PartyCreateLocalUser(handle, "1234567890ABCDEF", "token", page.As<const PartyLocalUser*>()); });
    const PartyLocalUser* const* users = nullptr;
    Require(PartyGetLocalUsers(handle, &count, &users) == 0 && count == 0);
    RequireThrows([&] { PartyGetLocalUsers(handle, page.As<std::uint32_t>(), &users); });

    const PartyLocalUser* user = nullptr;
    Require(PartyCreateLocalUser(handle, "1234567890ABCDEF", "token", &user) == 0 && user);
    RequireThrows([&] { PartyLocalUserGetEntityId(user, page.As<PartyString>()); });
    Require(PartyCleanup(handle) == 0);
}

int main() {
    CheckMemoryCallbacksComeFirstAndOnce();
    CheckLifecycle();
    CheckLocalUsers();
    CheckUnwritableOutputsThrowAndChangeNothing();
    return 0;
}
