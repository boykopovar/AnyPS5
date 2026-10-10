#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
extern "C" {
char* APS5_VABI getenv_nid_postfix(const char*);
int APS5_VABI setenv_nid_postfix(const char*, const char*, int);
int APS5_VABI unsetenv_nid_postfix(const char*);
int APS5_VABI putenv_nid_postfix(char*);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const char* key = "ANYPS5_GUEST_ENV_TEST_4C27";
#ifdef _WIN32
    Require(_putenv_s(key, "inherited") == 0);
#else
    Require(::setenv(key, "inherited", 1) == 0);
#endif
    Require(std::strcmp(getenv_nid_postfix(key), "inherited") == 0);
    Require(setenv_nid_postfix(key, "kept out", 0) == 0);
    Require(std::strcmp(getenv_nid_postfix(key), "inherited") == 0);
    char value[] = "copied";
    Require(setenv_nid_postfix(key, value, 1) == 0);
    value[0] = 'X';
    Require(std::strcmp(getenv_nid_postfix(key), "copied") == 0);
    Require(std::strcmp(std::getenv(key), "inherited") == 0);
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
    Require(unsetenv_nid_postfix("") == -1);
    char invalid[] = "no-separator";
    Require(putenv_nid_postfix(invalid) == -1);
    Require(unsetenv_nid_postfix(key) == 0);
    Require(getenv_nid_postfix(key) == nullptr);
    Require(unsetenv_nid_postfix(key) == 0);
    Require(unsetenv_nid_postfix("anyps5_guest_env_test_4c27") == 0);
}
