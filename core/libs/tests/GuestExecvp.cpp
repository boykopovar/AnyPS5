#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <string>
extern "C" {
int APS5_VABI execvp_nid_postfix(const char*, char* const*);
int APS5_VABI execv_nid_postfix(const char*, char* const*);
int APS5_VABI execve_nid_postfix(const char*, char* const*, char* const*);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    char name[] = "EXECVP_ABSENT_BINARY";
    char* arguments[] = {name, nullptr};
    char unpairedEntry[] = "EXECVP_SECOND_STAGE";
    char* unpaired[] = {unpairedEntry, nullptr};
    char variable[] = "EXECVP_SECOND_STAGE=1";
    char* environment[] = {variable, nullptr};
    std::string huge(300000, 'X');
    char* oversized[] = {name, &huge[0], nullptr};
    char* const empty[] = {nullptr};
    Require(execvp_nid_postfix(nullptr, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(execv_nid_postfix(nullptr, arguments) == -1 && *__error_nid_postfix() == 14);
    Require(execv_nid_postfix(name, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(execv_nid_postfix(name, empty) == -1 && *__error_nid_postfix() == 14);
    Require(execv_nid_postfix("", arguments) == -1 && *__error_nid_postfix() == 2);
    Require(execve_nid_postfix(name, arguments, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(execve_nid_postfix(name, arguments, unpaired) == -1 && *__error_nid_postfix() == 14);
    Require(execvp_nid_postfix("EXECVP_NO_SUCH_LIBRARY", arguments) == -1 && *__error_nid_postfix() == 2);
    Require(execv_nid_postfix(name, oversized) == -1 && *__error_nid_postfix() == 7);
}
