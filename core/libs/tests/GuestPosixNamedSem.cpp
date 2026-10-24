#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
void* APS5_VABI sem_open_nid_postfix(const char* name, int flags, ...);
int APS5_VABI sem_close_nid_postfix(void* sem);
int APS5_VABI sem_unlink_nid_postfix(const char* name);
int APS5_VABI sem_init_nid_postfix(void* sem, int pshared, unsigned int value);
int APS5_VABI sem_destroy_nid_postfix(void* sem);
int APS5_VABI sem_wait_nid_postfix(void* sem);
int APS5_VABI sem_trywait_nid_postfix(void* sem);
int APS5_VABI sem_post_nid_postfix(void* sem);
int APS5_VABI sem_getvalue_nid_postfix(void* sem, int* value);
int* APS5_VABI __error_nid_postfix();
}

static constexpr int GUEST_ENOENT = 2;
static constexpr int GUEST_EEXIST = 17;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EAGAIN = 35;
static constexpr int GUEST_ENAMETOOLONG = 63;
static constexpr int O_CREAT_FLAG = 0x200;
static constexpr int O_EXCL_FLAG = 0x800;

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "line %d failed, errno=%d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define REQUIRE(condition) Require(condition, __LINE__)

template <typename Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static bool OpenFails(const char* name, int flags, int error) {
    *__error_nid_postfix() = 0;
    return sem_open_nid_postfix(name, flags, 0600, 1) == nullptr && *__error_nid_postfix() == error;
}

static bool UnlinkFails(const char* name, int error) {
    *__error_nid_postfix() = 0;
    return sem_unlink_nid_postfix(name) == -1 && *__error_nid_postfix() == error;
}

static int Value(void* sem) {
    int value = -1;
    REQUIRE(sem_getvalue_nid_postfix(sem, &value) == 0);
    return value;
}

static void CheckNames() {
    REQUIRE(OpenFails("relative", O_CREAT_FLAG, GUEST_EINVAL));
    REQUIRE(OpenFails("/anyps5-named-flags", 0x1, GUEST_EINVAL));
    REQUIRE(OpenFails("/anyps5-named-flags", O_CREAT_FLAG | 0x2, GUEST_EINVAL));
    REQUIRE(OpenFails("/anyps5-named-missing", 0, GUEST_ENOENT));
    REQUIRE(OpenFails("/anyps5-named-missing", O_EXCL_FLAG, GUEST_ENOENT));
    const std::string longPath = "/" + std::string(1015, 'p');
    REQUIRE(OpenFails(longPath.c_str(), O_CREAT_FLAG, GUEST_ENAMETOOLONG));
    REQUIRE(UnlinkFails(longPath.c_str(), GUEST_ENAMETOOLONG));
    const std::string longFile = "/" + std::string(252, 'f');
    REQUIRE(OpenFails(longFile.c_str(), O_CREAT_FLAG, GUEST_ENAMETOOLONG));
    REQUIRE(OpenFails(longFile.c_str(), 0x1, GUEST_EINVAL));
    REQUIRE(UnlinkFails(longFile.c_str(), GUEST_ENAMETOOLONG));
    const std::string longestFile = "/" + std::string(251, 'f');
    void* longest = sem_open_nid_postfix(longestFile.c_str(), O_CREAT_FLAG, 0600, 0);
    REQUIRE(longest != nullptr && sem_close_nid_postfix(longest) == 0 && sem_unlink_nid_postfix(longestFile.c_str()) == 0);
    REQUIRE(UnlinkFails("relative", GUEST_ENOENT));
    REQUIRE(UnlinkFails("/anyps5-named-missing", GUEST_ENOENT));
    REQUIRE(Throws([] { sem_open_nid_postfix(nullptr, 0); }));
    REQUIRE(Throws([] { sem_unlink_nid_postfix(nullptr); }));
}

static void CheckLifetime() {
    const char* name = "/anyps5-named-lifetime";
    void* sem = sem_open_nid_postfix(name, O_CREAT_FLAG | O_EXCL_FLAG, 0600, 2);
    REQUIRE(sem != nullptr && Value(sem) == 2);
    REQUIRE(sem_open_nid_postfix(name, 0) == sem);
    REQUIRE(sem_open_nid_postfix(name, O_CREAT_FLAG, 0600, 9) == sem && Value(sem) == 2);
    REQUIRE(OpenFails(name, O_CREAT_FLAG | O_EXCL_FLAG, GUEST_EEXIST));
    REQUIRE(sem_trywait_nid_postfix(sem) == 0 && sem_trywait_nid_postfix(sem) == 0);
    *__error_nid_postfix() = 0;
    REQUIRE(sem_trywait_nid_postfix(sem) == -1 && *__error_nid_postfix() == GUEST_EAGAIN);
    std::thread poster([name] {
        void* other = sem_open_nid_postfix(name, 0);
        REQUIRE(other != nullptr && sem_post_nid_postfix(other) == 0 && sem_close_nid_postfix(other) == 0);
    });
    REQUIRE(sem_wait_nid_postfix(sem) == 0);
    poster.join();
    REQUIRE(sem_post_nid_postfix(sem) == 0 && sem_post_nid_postfix(sem) == 0 && Value(sem) == 2);
    *__error_nid_postfix() = 0;
    REQUIRE(sem_destroy_nid_postfix(sem) == -1 && *__error_nid_postfix() == GUEST_EINVAL && Value(sem) == 2);
    for (int open = 0; open < 3; ++open) REQUIRE(sem_close_nid_postfix(sem) == 0);
    *__error_nid_postfix() = 0;
    REQUIRE(sem_close_nid_postfix(sem) == -1 && *__error_nid_postfix() == GUEST_EINVAL);

    void* reopened = sem_open_nid_postfix(name, 0);
    REQUIRE(reopened != nullptr && Value(reopened) == 2);
    REQUIRE(sem_unlink_nid_postfix(name) == 0);
    REQUIRE(UnlinkFails(name, GUEST_ENOENT));
    REQUIRE(OpenFails(name, 0, GUEST_ENOENT));
    REQUIRE(sem_post_nid_postfix(reopened) == 0 && Value(reopened) == 3);
    void* recreated = sem_open_nid_postfix(name, O_CREAT_FLAG | O_EXCL_FLAG, 0600, 5);
    REQUIRE(recreated != nullptr && recreated != reopened && Value(recreated) == 5 && Value(reopened) == 3);
    REQUIRE(sem_close_nid_postfix(reopened) == 0);
    REQUIRE(sem_close_nid_postfix(recreated) == 0 && sem_unlink_nid_postfix(name) == 0);
}

static void CheckUnnamed() {
    std::uintptr_t storage[2] = {};
    REQUIRE(sem_init_nid_postfix(storage, 0, 1) == 0);
    *__error_nid_postfix() = 0;
    REQUIRE(sem_close_nid_postfix(storage) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    REQUIRE(Value(storage) == 1 && sem_destroy_nid_postfix(storage) == 0);
    REQUIRE(Throws([] { sem_close_nid_postfix(nullptr); }));
}

int main() {
    CheckNames();
    CheckLifetime();
    CheckUnnamed();
    return 0;
}
