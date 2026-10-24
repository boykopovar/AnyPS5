#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslUi {
    const void* meth;
    void* strings;
    void* userData;
    void* exData;
    int flags;
};

struct LibreSslUiString {
    int type;
    const char* outString;
    int inputFlags;
    char* resultBuf;
    unsigned char data[24];
    int flags;
};

static_assert(sizeof(LibreSslUi) == 40);
static_assert(offsetof(LibreSslUi, userData) == 16);
static_assert(sizeof(LibreSslUiString) == 64);
static_assert(offsetof(LibreSslUiString, inputFlags) == 16);
static_assert(offsetof(LibreSslUiString, flags) == 56);

namespace {

constexpr int UiTypeNone = 0;

}

extern "C" {

void* APS5_VABI UI_get0_user_data_nid_postfix(const LibreSslUi* ui) {
    if (!ui) throw std::invalid_argument(std::string(__func__) + ": null UI");
    return ui->userData;
}

int APS5_VABI UI_get_string_type_nid_postfix(const LibreSslUiString* string) {
    if (!string) return UiTypeNone;
    return string->type;
}

int APS5_VABI UI_get_input_flags_nid_postfix(const LibreSslUiString* string) {
    if (!string) return 0;
    return string->inputFlags;
}

}
