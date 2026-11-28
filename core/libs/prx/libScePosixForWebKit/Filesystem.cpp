#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
#else
#include <unistd.h>
#endif

namespace {

int Failure(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

#ifdef _WIN32
void IgnoreInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
#endif
}

extern "C" int APS5_VABI isatty_nid_postfix(int descriptor) {
    if (GuestSocketIsOpen_nid_no_patch(descriptor)) {
        Failure(25);
        return 0;
    }
#ifdef _WIN32
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    _set_thread_local_invalid_parameter_handler(previous);
    if (handle == INVALID_HANDLE_VALUE) {
        Failure(9);
        return 0;
    }
    DWORD mode = 0;
    if (::GetConsoleMode(handle, &mode)) return 1;
    Failure(25);
#else
    if (::isatty(descriptor)) return 1;
    Failure(errno == EBADF ? 9 : 25);
#endif
    return 0;
}

extern "C" int APS5_VABI ttyname_r_nid_postfix(int descriptor, char* buffer, std::size_t length) {
#ifdef _WIN32
    (void)descriptor;
    (void)buffer;
    (void)length;
    NotImplemented_nid_no_patch(__func__);
#else
    if (buffer == nullptr || length == 0) return 22;
    if (::isatty(descriptor) == 0) return errno == EBADF ? 9 : 25;
    std::array<char, 1024> name{};
    if (::ttyname_r(descriptor, name.data(), name.size()) != 0) return 25;
    const std::size_t size = std::strlen(name.data()) + 1;
    if (size > length) return 34;
    if (!GuestCopyOut_nid_no_patch(buffer, name.data(), size)) return 14;
    return 0;
#endif
}

extern "C" char* APS5_VABI ttyname_nid_postfix(int descriptor) {
#ifdef _WIN32
    (void)descriptor;
    NotImplemented_nid_no_patch(__func__);
#else
    thread_local std::array<char, 1024> name{};
    const int error = ttyname_r_nid_postfix(descriptor, name.data(), name.size());
    if (error != 0) {
        *__error_nid_postfix() = error;
        return nullptr;
    }
    return name.data();
#endif
}
