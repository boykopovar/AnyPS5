#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

void* APS5_VABI gethostbyname2_nid_postfix(const char*, int) {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI getservbyport_nid_postfix(int, const char*) {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI getprotobyname_nid_postfix(const char*) {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI getnetbyname_nid_postfix(const char*) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI endhostent_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI sethostent_nid_postfix(int) {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI gethostent_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI getservent_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI setservent_nid_postfix(int) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI endservent_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI getprotoent_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getgrouplist_nid_postfix(const char*, std::uint32_t, std::uint32_t*, int*) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI initgroups_nid_postfix(const char*, std::uint32_t) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI grantpt_nid_postfix(int) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI unlockpt_nid_postfix(int) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI ptsname_nid_postfix(int) {
    NotImplemented_nid_no_patch(__func__);
}

}
