#include "prx/libSceIcu/ResourceBundle.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

UResourceBundle* APS5_VABI ures_open(const char*, const char*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

UResourceBundle* APS5_VABI ures_openDirect_nid_postfix(const char*, const char*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ures_close(UResourceBundle* handle) {
    if (!handle) return;
    NotImplemented_nid_no_patch(__func__);
}

UResourceBundle* APS5_VABI ures_getByKey(const UResourceBundle*, const char*, UResourceBundle*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

UResourceBundle* APS5_VABI ures_getByIndex_nid_postfix(const UResourceBundle*, std::int32_t, UResourceBundle*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ures_getSize_nid_postfix(const UResourceBundle*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ures_getType_nid_postfix(const UResourceBundle*) {
    NotImplemented_nid_no_patch(__func__);
}

const char* APS5_VABI ures_getKey_nid_postfix(const UResourceBundle*) {
    NotImplemented_nid_no_patch(__func__);
}

const char16_t* APS5_VABI ures_getString_nid_postfix(const UResourceBundle*, std::int32_t*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

const char16_t* APS5_VABI ures_getStringByKey(const UResourceBundle*, const char*, std::int32_t*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

const char16_t* APS5_VABI ures_getStringByIndex_nid_postfix(const UResourceBundle*, std::int32_t, std::int32_t*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ures_getInt_nid_postfix(const UResourceBundle*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

const std::uint8_t* APS5_VABI ures_getBinary_nid_postfix(const UResourceBundle*, std::int32_t*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

const std::int32_t* APS5_VABI ures_getIntVector_nid_postfix(const UResourceBundle*, std::int32_t*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

}
