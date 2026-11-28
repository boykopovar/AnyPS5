#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslUi) == 40);
static_assert(offsetof(LibreSslUi, userData) == 16);
static_assert(sizeof(LibreSslUiString) == 64);
static_assert(offsetof(LibreSslUiString, inputFlags) == 16);
static_assert(offsetof(LibreSslUiString, flags) == 56);

namespace {

constexpr int UiTypeNone = 0;

template <typename T>
bool ReadField(const T* field, T& value) {
    return GuestCopyIn_nid_no_patch(&value, field, sizeof(T));
}

}

extern "C" {

void* APS5_VABI UI_get0_user_data_nid_postfix(const LibreSslUi* ui) {
    if (!ui) throw std::invalid_argument(std::string(__func__) + ": null UI");
    void* data = nullptr;
    if (!ReadField(&ui->userData, data)) throw std::invalid_argument(std::string(__func__) + ": unreadable UI");
    return data;
}

int APS5_VABI UI_get_string_type_nid_postfix(const LibreSslUiString* string) {
    int type = UiTypeNone;
    return string && ReadField(&string->type, type) ? type : UiTypeNone;
}

int APS5_VABI UI_get_input_flags_nid_postfix(const LibreSslUiString* string) {
    int flags = 0;
    return string && ReadField(&string->inputFlags, flags) ? flags : 0;
}

}
