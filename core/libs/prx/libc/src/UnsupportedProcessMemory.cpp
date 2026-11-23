#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

char* APS5_VABI ctermid_nid_postfix(char* name) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI brk_nid_postfix(const void* address) {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI sbrk_nid_postfix(std::intptr_t increment) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI getpass_nid_postfix(const char* prompt) {
    NotImplemented_nid_no_patch(__func__);
}

}
