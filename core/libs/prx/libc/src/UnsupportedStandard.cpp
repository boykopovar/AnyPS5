#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

int APS5_VABI regcomp_nid_postfix(void*, const char*, int) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI regexec_nid_postfix(const void*, const char*, std::size_t, void*, int) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI regfree_nid_postfix(void*) {
    NotImplemented_nid_no_patch(__func__);
}

std::size_t APS5_VABI regerror_nid_postfix(int, const void*, char*, std::size_t) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI glob_nid_postfix(const char*, int, void*, void*) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI globfree_nid_postfix(void*) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI wordexp_nid_postfix(const char*, void*, int) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI wordfree_nid_postfix(void*) {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI catopen_nid_postfix(const char*, int) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI catgets_nid_postfix(void*, int, int, const char*) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI catclose_nid_postfix(void*) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI nl_langinfo_nid_postfix(int) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI fmtmsg_nid_postfix(std::int64_t, const char*, int, const char*, const char*, const char*) {
    NotImplemented_nid_no_patch(__func__);
}

std::size_t APS5_VABI confstr_nid_postfix(int, char*, std::size_t) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI uname_nid_postfix(void*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI gethostid_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}
