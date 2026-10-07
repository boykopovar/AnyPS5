#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstring>

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {
// Guest FreeBSD errno numbering, not the host CRT's numbering.
constexpr const char* messages[] = {"No error",
                                    "Operation not permitted",
                                    "No such file or directory",
                                    "No such process",
                                    "Interrupted system call",
                                    "Input/output error",
                                    "Device not configured",
                                    "Argument list too long",
                                    "Exec format error",
                                    "Bad file descriptor",
                                    "No child processes",
                                    "Resource deadlock avoided",
                                    "Cannot allocate memory",
                                    "Permission denied",
                                    "Bad address",
                                    "Block device required",
                                    "Device busy",
                                    "File exists",
                                    "Cross-device link",
                                    "Operation not supported by device",
                                    "Not a directory",
                                    "Is a directory",
                                    "Invalid argument",
                                    "Too many open files in system",
                                    "Too many open files",
                                    "Inappropriate ioctl for device",
                                    "Text file busy",
                                    "File too large",
                                    "No space left on device",
                                    "Illegal seek",
                                    "Read-only filesystem",
                                    "Too many links",
                                    "Broken pipe",
                                    "Numerical argument out of domain",
                                    "Result too large",
                                    "Resource temporarily unavailable",
                                    "Operation now in progress",
                                    "Operation already in progress",
                                    "Socket operation on non-socket",
                                    "Destination address required",
                                    "Message too long",
                                    "Protocol wrong type for socket",
                                    "Protocol not available",
                                    "Protocol not supported",
                                    "Socket type not supported",
                                    "Operation not supported",
                                    "Protocol family not supported",
                                    "Address family not supported",
                                    "Address already in use",
                                    "Cannot assign requested address",
                                    "Network is down",
                                    "Network is unreachable",
                                    "Network connection reset",
                                    "Connection aborted",
                                    "Connection reset by peer",
                                    "No buffer space available",
                                    "Socket is already connected",
                                    "Socket is not connected",
                                    "Socket is shut down",
                                    "Too many references",
                                    "Operation timed out",
                                    "Connection refused",
                                    "Too many symbolic links",
                                    "File name too long",
                                    "Host is down",
                                    "No route to host",
                                    "Directory not empty",
                                    "Too many processes",
                                    "Too many users",
                                    "Disk quota exceeded",
                                    "Stale file handle",
                                    "Remote path has too many levels",
                                    "Invalid RPC structure",
                                    "RPC version mismatch",
                                    "RPC program unavailable",
                                    "Program version mismatch",
                                    "Invalid RPC procedure",
                                    "No locks available",
                                    "Function not implemented",
                                    "Inappropriate file type",
                                    "Authentication error",
                                    "Authentication required",
                                    "Identifier removed",
                                    "No message of requested type",
                                    "Value too large",
                                    "Operation canceled",
                                    "Invalid byte sequence",
                                    "Attribute not found",
                                    "Programming error",
                                    "Bad message",
                                    "Multihop attempted",
                                    "Link severed",
                                    "Protocol error",
                                    "Insufficient capabilities",
                                    "Operation not permitted in capability mode",
                                    "State not recoverable",
                                    "Previous owner died"};
static_assert(sizeof(messages) / sizeof(messages[0]) == 97);
bool Known(int error) { return error >= 0 && error < 97; }
}

extern "C" {
char* APS5_VABI strerror_nid_postfix(int error) {
    thread_local char buffer[128];
    const int saved = *__error_nid_postfix();
    if (Known(error))
        std::snprintf(buffer, sizeof(buffer), "%s", messages[error]);
    else
        std::snprintf(buffer, sizeof(buffer), "Unknown error: %d", error);
    *__error_nid_postfix() = Known(error) ? saved : 22;
    return buffer;
}

int APS5_VABI strerror_r_nid_postfix(int error, char* buffer, std::size_t length) {
    const int saved = *__error_nid_postfix();
    char temporary[128];
    if (Known(error))
        std::snprintf(temporary, sizeof(temporary), "%s", messages[error]);
    else
        std::snprintf(temporary, sizeof(temporary), "Unknown error: %d", error);
    const auto required = std::strlen(temporary) + 1;
    int result = Known(error) ? 0 : 22;
    if (!buffer || length < required)
        result = 34;
    if (buffer && length) {
        const auto count = required <= length ? required - 1 : length - 1;
        std::memcpy(buffer, temporary, count);
        buffer[count] = '\0';
    }
    *__error_nid_postfix() = saved;
    return result;
}

void APS5_VABI perror_nid_postfix(const char* prefix) {
    const int saved = *__error_nid_postfix();
    char message[128];
    strerror_r_nid_postfix(saved, message, sizeof(message));
    if (prefix && *prefix)
        std::fprintf(stderr, "%s: %s\n", prefix, message);
    else
        std::fprintf(stderr, "%s\n", message);
    *__error_nid_postfix() = saved;
}
}
