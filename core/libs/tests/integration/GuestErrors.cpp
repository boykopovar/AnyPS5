#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" {
char* APS5_VABI strerror_nid_postfix(int);
int APS5_VABI strerror_r_nid_postfix(int, char*, std::size_t);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    *__error_nid_postfix() = 13;
    Require(std::strcmp(strerror_nid_postfix(35), "Resource temporarily unavailable") == 0);
    Require(*__error_nid_postfix() == 13);
    Require(std::strcmp(strerror_nid_postfix(78), "Function not implemented") == 0);
    char* parent = strerror_nid_postfix(22);
    Require(*__error_nid_postfix() == 13);
    std::thread worker([] {
        Require(std::strcmp(strerror_nid_postfix(45), "Operation not supported") == 0);
    });
    worker.join();
    *__error_nid_postfix() = 13;
    Require(std::strcmp(parent, "Invalid argument") == 0);
    char buffer[128];
    for (int error = 0; error <= 96; ++error) {
        Require(strerror_r_nid_postfix(error, buffer, sizeof(buffer)) == 0);
        Require(buffer[0] && std::strstr(buffer, "Unknown") == nullptr);
    }
    Require(strerror_r_nid_postfix(-1, buffer, sizeof(buffer)) == 22);
    Require(std::strstr(buffer, "-1") != nullptr);
    Require(*__error_nid_postfix() == 13);
    char sentinel[] = "xyz";
    Require(strerror_r_nid_postfix(22, sentinel, 1) == 34);
    Require(sentinel[0] == 0 && sentinel[1] == 'y');
    sentinel[0] = 'x';
    Require(strerror_r_nid_postfix(22, sentinel, 0) == 34 && sentinel[0] == 'x');
    Require(strerror_r_nid_postfix(22, nullptr, 0) == 34);
}
