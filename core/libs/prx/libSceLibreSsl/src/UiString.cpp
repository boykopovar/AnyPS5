#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

static_assert(sizeof(LibreSslUi) == 40);
static_assert(offsetof(LibreSslUi, userData) == 16);
static_assert(sizeof(LibreSslUiString) == 64);
static_assert(offsetof(LibreSslUiString, inputFlags) == 16);
static_assert(offsetof(LibreSslUiString, flags) == 56);

namespace {

constexpr int UiTypeNone = 0;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

template <typename T>
bool ReadFieldAt(const void* base, std::size_t offset, T& value) {
    if (!base) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    if (offset > ~std::uintptr_t{0} - address) return false;
    const auto* field = reinterpret_cast<const unsigned char*>(address + offset);
    ErrnoGuard guard;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(field, sizeof(T));
    return GuestReadable_nid_no_patch(field, sizeof(T)) && GuestCopyIn_nid_no_patch(&value, field, sizeof(T));
}

}

extern "C" {

void* APS5_VABI UI_get0_user_data_nid_postfix(const LibreSslUi* ui) {
    if (!ui) throw std::invalid_argument(std::string(__func__) + ": null UI");
    void* data = nullptr;
    if (!ReadFieldAt(ui, offsetof(LibreSslUi, userData), data)) throw std::invalid_argument(std::string(__func__) + ": unreadable UI");
    return data;
}

int APS5_VABI UI_get_string_type_nid_postfix(const LibreSslUiString* string) {
    int type = UiTypeNone;
    return string && ReadFieldAt(string, offsetof(LibreSslUiString, type), type) ? type : UiTypeNone;
}

int APS5_VABI UI_get_input_flags_nid_postfix(const LibreSslUiString* string) {
    int flags = 0;
    return string && ReadFieldAt(string, offsetof(LibreSslUiString, inputFlags), flags) ? flags : 0;
}

}
