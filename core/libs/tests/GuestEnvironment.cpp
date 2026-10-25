#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const char* key = "ANYPS5_GUEST_ENV_TEST_4C27";
#ifdef _WIN32
    Require(_putenv_s(key, "host") == 0);
#else
    Require(::setenv(key, "host", 1) == 0);
#endif
    Require(getenv_nid_postfix(key) == nullptr);
#ifdef _WIN32
    Require(_putenv_s("HOME", "C:\\Users\\host") == 0);
#else
    Require(::setenv("HOME", "/home/host", 1) == 0);
#endif
    for (const char* host : {"HOME", "PATH"}) Require(std::getenv(host) != nullptr && getenv_nid_postfix(host) == nullptr);
    Require(setenv_nid_postfix(key, "inherited", 0) == 0);
    Require(setenv_nid_postfix(key, "kept out", 0) == 0);
    Require(std::strcmp(getenv_nid_postfix(key), "inherited") == 0);
    char value[] = "copied";
    Require(setenv_nid_postfix(key, value, 1) == 0);
    value[0] = 'X';
    Require(std::strcmp(getenv_nid_postfix(key), "copied") == 0);
    Require(std::strcmp(std::getenv(key), "host") == 0);
    Require(setenv_nid_postfix(key, "", 1) == 0);
    Require(getenv_nid_postfix(key) && *getenv_nid_postfix(key) == 0);
    char borrowed[] = "ANYPS5_GUEST_ENV_TEST_4C27=one";
    Require(putenv_nid_postfix(borrowed) == 0);
    char* position = std::strchr(borrowed, '=') + 1;
    Require(getenv_nid_postfix(key) == position);
    std::memcpy(position, "two", 3);
    Require(std::strcmp(getenv_nid_postfix(key), "two") == 0);
    Require(setenv_nid_postfix("anyps5_guest_env_test_4c27", "lower", 1) == 0);
    Require(std::strcmp(getenv_nid_postfix(key), "two") == 0);
    Require(setenv_nid_postfix("bad=name", "x", 1) == -1);
    Require(*__error_nid_postfix() == 22);
    const char* const invalidNames[] = {nullptr, "", "bad=name"};
    for (const char* name : invalidNames) {
        *__error_nid_postfix() = 0;
        Require(getenv_nid_postfix(name) == nullptr && *__error_nid_postfix() == 22);
    }
    *__error_nid_postfix() = 34;
    Require(getenv_nid_postfix("ANYPS5_GUEST_ENV_ABSENT_4C27") == nullptr && *__error_nid_postfix() == 34);
    Require(unsetenv_nid_postfix("") == -1);
    char invalid[] = "no-separator";
    Require(putenv_nid_postfix(invalid) == -1);
    Require(unsetenv_nid_postfix(key) == 0);
    Require(getenv_nid_postfix(key) == nullptr);
    Require(unsetenv_nid_postfix(key) == 0);
    Require(unsetenv_nid_postfix("anyps5_guest_env_test_4c27") == 0);
    Require(setenv_nid_postfix("Q7A", "a", 1) == 0);
    Require(setenv_nid_postfix("Q7B", "b", 1) == 0);
    const char* kept = getenv_nid_postfix("Q7B");
    Require(unsetenv_nid_postfix("Q7A") == 0);
    for (int index = 0; index < 64; ++index)
        Require(setenv_nid_postfix(("ANYPS5_GUEST_ENV_GROW_4C27_" + std::to_string(index)).c_str(), "x", 1) == 0);
    Require(getenv_nid_postfix("Q7B") == kept && std::strcmp(kept, "b") == 0);
}
