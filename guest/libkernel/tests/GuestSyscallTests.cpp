#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

constexpr int ErrorNoEntry = 2;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNameTooLong = 63;
constexpr int FlagCreate = 0x200;
constexpr int FlagExclusive = 0x800;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Func>
bool ExpectsException(Func&& func) {
    try {
        func();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void TestNamedSemNullNames() {
    Require(ExpectsException([] {
        sem_open_nid_postfix(nullptr, 0);
    }));

    Require(ExpectsException([] {
        sem_open_nid_postfix(nullptr, FlagCreate, 0600, 1);
    }));

    Require(ExpectsException([] {
        sem_unlink_nid_postfix(nullptr);
    }));

    Require(ExpectsException([] {
        sem_close_nid_postfix(nullptr);
    }));
}

void TestNamedSemNameOverflow() {
    const std::string longPath = "/" + std::string(1015, 'p');

    *__error_nid_postfix() = 0;
    Require(sem_open_nid_postfix(longPath.c_str(), FlagCreate, 0600, 1) == nullptr);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(sem_unlink_nid_postfix(longPath.c_str()) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    const std::string oversizedPath = "/" + std::string(2048, 'x');

    *__error_nid_postfix() = 0;
    Require(sem_open_nid_postfix(oversizedPath.c_str(), FlagCreate, 0600, 1) == nullptr);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(sem_unlink_nid_postfix(oversizedPath.c_str()) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    const std::string longFile = "/" + std::string(252, 'f');

    *__error_nid_postfix() = 0;
    Require(sem_open_nid_postfix(longFile.c_str(), FlagCreate, 0600, 1) == nullptr);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(sem_unlink_nid_postfix(longFile.c_str()) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    const std::string oversizedFile = "/" + std::string(512, 'f');

    *__error_nid_postfix() = 0;
    Require(sem_open_nid_postfix(oversizedFile.c_str(), FlagCreate, 0600, 1) == nullptr);
    Require(*__error_nid_postfix() == ErrorNameTooLong);

    *__error_nid_postfix() = 0;
    Require(sem_unlink_nid_postfix(oversizedFile.c_str()) == -1);
    Require(*__error_nid_postfix() == ErrorNameTooLong);
}

void TestNamedSemInvalidFlags() {
    const char* validName = "/anyps5-named-sem-flags-test";

    for (const int invalidFlag : {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x1000, ~0}) {
        *__error_nid_postfix() = 0;
        Require(sem_open_nid_postfix(validName, invalidFlag) == nullptr);
        Require(*__error_nid_postfix() == ErrorInvalid);

        *__error_nid_postfix() = 0;
        Require(sem_open_nid_postfix(validName, FlagCreate | invalidFlag, 0600, 1) == nullptr);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }

    *__error_nid_postfix() = 0;
    Require(sem_open_nid_postfix("relative-named-sem", FlagCreate, 0600, 1) == nullptr);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_open_nid_postfix("/anyps5-nonexistent-sem", 0) == nullptr);
    Require(*__error_nid_postfix() == ErrorNoEntry);

    *__error_nid_postfix() = 0;
    Require(sem_open_nid_postfix("/anyps5-nonexistent-sem", FlagExclusive) == nullptr);
    Require(*__error_nid_postfix() == ErrorNoEntry);

    *__error_nid_postfix() = 0;
    Require(sem_unlink_nid_postfix("relative-named-sem") == -1);
    Require(*__error_nid_postfix() == ErrorNoEntry);

    *__error_nid_postfix() = 0;
    Require(sem_unlink_nid_postfix("/anyps5-nonexistent-sem") == -1);
    Require(*__error_nid_postfix() == ErrorNoEntry);
}

}

int main() {
    TestNamedSemNullNames();
    TestNamedSemNameOverflow();
    TestNamedSemInvalidFlags();
    return 0;
}
