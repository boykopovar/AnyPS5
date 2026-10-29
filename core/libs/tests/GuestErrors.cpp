#include "prx/libc/include/WindowsFormatting.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

static void Require(bool value) { if (!value) std::abort(); }
static const char* const freebsd[97] = {
    "No error: 0", "Operation not permitted", "No such file or directory",
    "No such process", "Interrupted system call", "Input/output error",
    "Device not configured", "Argument list too long", "Exec format error",
    "Bad file descriptor", "No child processes", "Resource deadlock avoided",
    "Cannot allocate memory", "Permission denied", "Bad address",
    "Block device required", "Device busy", "File exists",
    "Cross-device link", "Operation not supported by device", "Not a directory",
    "Is a directory", "Invalid argument", "Too many open files in system",
    "Too many open files", "Inappropriate ioctl for device", "Text file busy",
    "File too large", "No space left on device", "Illegal seek",
    "Read-only filesystem", "Too many links", "Broken pipe",
    "Numerical argument out of domain", "Result too large", "Resource temporarily unavailable",
    "Operation now in progress", "Operation already in progress", "Socket operation on non-socket",
    "Destination address required", "Message too long", "Protocol wrong type for socket",
    "Protocol not available", "Protocol not supported", "Socket type not supported",
    "Operation not supported", "Protocol family not supported", "Address family not supported by protocol family",
    "Address already in use", "Can't assign requested address", "Network is down",
    "Network is unreachable", "Network dropped connection on reset", "Software caused connection abort",
    "Connection reset by peer", "No buffer space available", "Socket is already connected",
    "Socket is not connected", "Can't send after socket shutdown", "Too many references: can't splice",
    "Operation timed out", "Connection refused", "Too many levels of symbolic links",
    "File name too long", "Host is down", "No route to host",
    "Directory not empty", "Too many processes", "Too many users",
    "Disc quota exceeded", "Stale NFS file handle", "Too many levels of remote in path",
    "RPC struct is bad", "RPC version wrong", "RPC prog. not avail",
    "Program version wrong", "Bad procedure for program", "No locks available",
    "Function not implemented", "Inappropriate file type or format", "Authentication error",
    "Need authenticator", "Identifier removed", "No message of desired type",
    "Value too large to be stored in data type", "Operation canceled", "Illegal byte sequence",
    "Attribute not found", "Programming error", "Bad message",
    "Multihop attempted", "Link has been severed", "Protocol error",
    "Capabilities insufficient", "Not permitted in capability mode", "State not recoverable",
    "Previous owner died"
};
int main() {
    *__error_nid_postfix() = 13;
    Require(std::strcmp(strerror_nid_postfix(35), "Resource temporarily unavailable") == 0);
    Require(*__error_nid_postfix() == 13);
    Require(std::strcmp(strerror_nid_postfix(78), "Function not implemented") == 0);
    char* parent = strerror_nid_postfix(22);
    Require(*__error_nid_postfix() == 13);
    std::thread worker([] {
        Require(std::strcmp(strerror_nid_postfix(45), "Operation not supported") == 0);
    });
    worker.join();
    *__error_nid_postfix() = 13;
    Require(std::strcmp(parent, "Invalid argument") == 0);
    char buffer[128];
    for (int error = 0; error <= 96; ++error) {
        char coded[32];
        std::snprintf(coded, sizeof(coded), "error_code=0x%08x", static_cast<unsigned>(error));
        const char* expected = error == 0 ? "No error" : error < 95 ? freebsd[error] : coded;
        Require(strerror_r_nid_postfix(error, buffer, sizeof(buffer)) == 0);
        Require(std::strcmp(buffer, expected) == 0);
        Require(std::strcmp(strerror_nid_postfix(error), expected) == 0);
        Require(*__error_nid_postfix() == 13);
    }
    for (const int error : {97, 100, 1000, 9999, INT_MAX, -1, INT_MIN}) {
        char coded[32];
        std::snprintf(coded, sizeof(coded), "error_code=0x%08x", static_cast<unsigned>(error));
        Require(strerror_r_nid_postfix(error, buffer, sizeof(buffer)) == 0);
        Require(std::strcmp(buffer, coded) == 0);
        Require(*__error_nid_postfix() == 13);
        Require(std::strcmp(strerror_nid_postfix(error), coded) == 0);
        Require(*__error_nid_postfix() == 13);
    }
    for (const int error : {0, 19, 22, 95, -1}) {
        for (const std::size_t length : {std::size_t{0}, std::size_t{1}, std::size_t{5}, std::size_t{16}, std::size_t{22}}) {
            char canary[64];
            std::memset(canary, 0xcc, sizeof(canary));
            Require(strerror_r_nid_postfix(error, canary, length) == 34);
            for (const char byte : canary) Require(static_cast<unsigned char>(byte) == 0xcc);
            Require(*__error_nid_postfix() == 13);
        }
        char smallest[64];
        std::memset(smallest, 0xcc, sizeof(smallest));
        Require(strerror_r_nid_postfix(error, smallest, 23) == 0);
        const char* text = strerror_nid_postfix(error);
        const std::size_t copied = std::min<std::size_t>(std::strlen(text), 22);
        Require(std::memcmp(smallest, text, copied) == 0);
        for (std::size_t index = copied; index < 23; ++index) Require(smallest[index] == 0);
        Require(static_cast<unsigned char>(smallest[23]) == 0xcc);
        Require(*__error_nid_postfix() == 13);
    }
    char truncated[32];
    Require(strerror_r_nid_postfix(19, truncated, 23) == 0);
    Require(std::strcmp(truncated, "Operation not supporte") == 0);
    Require(strerror_r_nid_postfix(22, nullptr, 0) == 34);
    Require(strerror_r_nid_postfix(22, nullptr, 22) == 34);
    bool threw = false;
    try {
        strerror_r_nid_postfix(22, nullptr, 23);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}
