#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/urename.h"
#include "unicode/uvernum.h"
#include <cstddef>
#include <cstdint>

namespace {

bool Writable(void* destination, std::size_t bytes) {
    return !destination || bytes == 0 || GuestWritable_nid_no_patch(destination, bytes);
}

}

extern "C" {

std::int32_t uloc_countAvailable();
const char* uloc_getAvailable(std::int32_t index);
std::int32_t uloc_addLikelySubtags(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t uloc_getName(const char* localeID, char* name, std::int32_t capacity, std::int32_t* status);
std::int32_t uloc_minimizeSubtags(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
void udata_setCommonData(const void* data, std::int32_t* status);
void udata_setFileAccess(std::int32_t access, std::int32_t* status);

}

extern "C" {

std::int32_t APS5_VABI uloc_countAvailable_nid_postfix() {
    return uloc_countAvailable();
}

const char* APS5_VABI uloc_getAvailable_nid_postfix(std::int32_t index) {
    return uloc_getAvailable(index);
}

std::int32_t APS5_VABI uloc_addLikelySubtags_nid_postfix(const char* locale, char* result, std::int32_t capacity,
    std::int32_t* status) {
    if (!Writable(status, sizeof(std::int32_t)) || !Writable(result, capacity > 0 ? static_cast<std::size_t>(capacity) : 0u)) {
        return 0;
    }
    if (!locale) locale = Icu::DefaultLocaleName();
    return uloc_addLikelySubtags(locale, result, capacity, status);
}

std::int32_t APS5_VABI uloc_minimizeSubtags_nid_postfix(const char* locale, char* result, std::int32_t capacity,
    std::int32_t* status) {
    if (!Writable(status, sizeof(std::int32_t)) || !Writable(result, capacity > 0 ? static_cast<std::size_t>(capacity) : 0u)) {
        return 0;
    }
    if (!locale) locale = Icu::DefaultLocaleName();
    return uloc_minimizeSubtags(locale, result, capacity, status);
}

std::int32_t APS5_VABI uloc_getName_nid_postfix(const char* localeID, char* name, std::int32_t capacity,
    std::int32_t* status) {
    if (!Writable(status, sizeof(std::int32_t)) || !Writable(name, capacity > 0 ? static_cast<std::size_t>(capacity) : 0u)) {
        return 0;
    }
    if (!localeID) localeID = Icu::DefaultLocaleName();
    return uloc_getName(localeID, name, capacity, status);
}

void APS5_VABI udata_setCommonData_nid_postfix(const void* data, std::int32_t* status) {
    if (!Writable(status, sizeof(std::int32_t))) return;
    udata_setCommonData(data, status);
}

void APS5_VABI udata_setFileAccess_nid_postfix(std::int32_t access, std::int32_t* status) {
    if (!Writable(status, sizeof(std::int32_t))) return;
    udata_setFileAccess(access, status);
}

}
