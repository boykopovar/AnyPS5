#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"
#include "prx/libSceNKWebKitRequirements/include/InventoryExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <wpe/input.h>

#include <cerrno>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

extern "C" {

std::uint32_t APS5_VABI wpe_key_code_to_unicode_nid_postfix(std::uint32_t keyCode) {
    return wpe_key_code_to_unicode(keyCode);
}

std::uint32_t APS5_VABI wpe_unicode_to_key_code_nid_postfix(std::uint32_t character) {
    return wpe_unicode_to_key_code(character);
}

void APS5_VABI wpe_pasteboard_string_initialize(NKWebKit::WpePasteboardString* string, const char* contents, std::uint64_t length) {
    NKWebKit::WpePasteboardString current{};
    {
        const int savedErrno = errno;
        const bool readable = GuestCopyIn_nid_no_patch(&current, string, sizeof(current));
        errno = savedErrno;
        if (!readable) throw std::invalid_argument("wpe_pasteboard_string_initialize: unreadable string");
    }
    if (current.data) return;
    if (length >= std::numeric_limits<std::size_t>::max()) throw std::bad_alloc();

    std::unique_ptr<char[]> staging(new char[length]);
    if (length) {
        const int savedErrno = errno;
        const bool readable = GuestCopyIn_nid_no_patch(staging.get(), contents, length);
        errno = savedErrno;
        if (!readable) throw std::invalid_argument("wpe_pasteboard_string_initialize: unreadable contents");
    }

    std::unique_ptr<void, decltype(&free_nid_postfix)> allocation(calloc_nid_postfix(length + 1, 1), free_nid_postfix);
    if (!allocation) throw std::bad_alloc();
    if (length) {
        const int savedErrno = errno;
        const bool writable = GuestCopyOut_nid_no_patch(allocation.get(), staging.get(), length);
        errno = savedErrno;
        if (!writable) throw std::invalid_argument("wpe_pasteboard_string_initialize: unwritable string data");
    }

    const NKWebKit::WpePasteboardString result{static_cast<char*>(allocation.get()), length};
    {
        const int savedErrno = errno;
        const bool writable = GuestCopyOut_nid_no_patch(string, &result, sizeof(result));
        errno = savedErrno;
        if (!writable) throw std::invalid_argument("wpe_pasteboard_string_initialize: unwritable string");
    }
    allocation.release();
}

void APS5_VABI wpe_pasteboard_string_free(NKWebKit::WpePasteboardString* string) {
    NKWebKit::WpePasteboardString current{};
    {
        const int savedErrno = errno;
        const bool readable = GuestCopyIn_nid_no_patch(&current, string, sizeof(current));
        errno = savedErrno;
        if (!readable) throw std::invalid_argument("wpe_pasteboard_string_free: unreadable string");
    }
    const NKWebKit::WpePasteboardString empty{};
    {
        const int savedErrno = errno;
        const bool writable = GuestCopyOut_nid_no_patch(string, &empty, sizeof(empty));
        errno = savedErrno;
        if (!writable) throw std::invalid_argument("wpe_pasteboard_string_free: unwritable string");
    }
    free_nid_postfix(current.data);
}

}
