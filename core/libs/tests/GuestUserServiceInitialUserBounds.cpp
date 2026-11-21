#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

class GuardedPage {
public:
    std::size_t size;
    unsigned char* data;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (data == nullptr) {
            std::abort();
        }
        DWORD previous = 0;
        if (VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) == 0) {
            std::abort();
        }
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) {
            std::abort();
        }
        size = static_cast<std::size_t>(pageSize);
        data = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (data == MAP_FAILED) {
            std::abort();
        }
        if (mprotect(data + size, size, PROT_NONE) != 0) {
            std::abort();
        }
#endif
    }

    void SetFirstPageReadOnly(bool readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD protect = readOnly ? PAGE_READONLY : PAGE_READWRITE;
        if (VirtualProtect(data, size, protect, &previous) == 0) {
            std::abort();
        }
#else
        const int protect = readOnly ? PROT_READ : (PROT_READ | PROT_WRITE);
        if (mprotect(data, size, protect) != 0) {
            std::abort();
        }
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
};

void TestInitialAndForegroundUserInvariants() {
    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);

    int foregroundUser = -1;
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);
    Require(initialUser == foregroundUser);

    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);

    initialUser = 0;
    foregroundUser = 0;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 0);

    initialUser = 0;
    foregroundUser = 0;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);

    UserServiceSetUserConnected_nid_postfix(1, true);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 0);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 1);

    initialUser = 0;
    foregroundUser = 0;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);

    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(3, true);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 0);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 1);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 2) == 1);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 3) == 1);

    initialUser = 0;
    foregroundUser = 0;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);

    UserServiceSetUserConnected_nid_postfix(1, false);
    UserServiceSetUserConnected_nid_postfix(2, false);
    UserServiceSetUserConnected_nid_postfix(3, false);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + i) == 0);
    }

    initialUser = 0;
    foregroundUser = 0;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);

    initialUser = 0;
    foregroundUser = 0;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    initialUser = 0;
    foregroundUser = 0;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);
}

void TestPointerFaultBounds() {
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    GuardedPage guard;

    Require(sceUserServiceGetInitialUser(reinterpret_cast<int*>(guard.data + guard.size)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(reinterpret_cast<int*>(guard.data + guard.size)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceGetInitialUser(reinterpret_cast<int*>(guard.data + guard.size - 2)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(reinterpret_cast<int*>(guard.data + guard.size - 2)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    guard.SetFirstPageReadOnly(true);
    Require(sceUserServiceGetInitialUser(reinterpret_cast<int*>(guard.data)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(reinterpret_cast<int*>(guard.data)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    guard.SetFirstPageReadOnly(false);
    int* valid = reinterpret_cast<int*>(guard.data);
    Require(sceUserServiceGetInitialUser(valid) == USER_SERVICE_OK);
    Require(*valid == USER_SERVICE_INITIAL_USER_ID);

    *valid = 0;
    Require(sceUserServiceGetForegroundUser(valid) == USER_SERVICE_OK);
    Require(*valid == USER_SERVICE_INITIAL_USER_ID);
}

} // namespace

int main() {
    TestInitialAndForegroundUserInvariants();
    TestPointerFaultBounds();
    return 0;
}
