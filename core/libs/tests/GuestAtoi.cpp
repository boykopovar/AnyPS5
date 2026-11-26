#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/HostErrno.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>

int main() {
    struct Case {
        const char* text;
        int value;
        int error;
    };
    constexpr int SavedError = 1234;
    constexpr Case cases[] = {
        {"0", 0, SavedError},
        {"-0", 0, SavedError},
        {"+0", 0, SavedError},
        {"+42", 42, SavedError},
        {" \t\n\r\f\v-0012tail", -12, SavedError},
        {"010", 10, SavedError},
        {"0x10", 0, SavedError},
        {"0b10", 0, SavedError},
        {"", 0, SavedError},
        {"words", 0, SavedError},
        {" +", 0, SavedError},
        {"--5", 0, SavedError},
        {"+-5", 0, SavedError},
        {"2147483647", INT32_MAX, SavedError},
        {"-2147483648", INT32_MIN, SavedError},
        {"2147483648", INT32_MIN, SavedError},
        {"-2147483649", INT32_MAX, SavedError},
        {"4294967295", -1, SavedError},
        {"-4294967295", 1, SavedError},
        {"4294967296", 0, SavedError},
        {"-4294967296", 0, SavedError},
        {" \t+4294967297tail", 1, SavedError},
        {"9223372036854775807", -1, SavedError},
        {"-9223372036854775808", 0, SavedError},
        {"9223372036854775808", 0, SavedError},
        {"-9223372036854775809", -1, SavedError},
        {"999999999999999999999999999999999999", -1, 34},
        {"-999999999999999999999999999999999999", -1, 34},
    };
    bool correct = true;
    for (const auto& test : cases) {
        *__error_nid_postfix() = SavedError;
        const int value = atoi_nid_postfix(test.text);
        const int error = *__error_nid_postfix();
        if (value != test.value || error != test.error) {
            std::fprintf(stderr, "atoi(%s): expected %d, errno %d; received %d, errno %d\n",
                test.text, test.value, test.error, value, error);
            correct = false;
        }
    }
    struct WideCase {
        const char* text;
        std::int64_t value;
        int error;
    };
    constexpr WideCase wideCases[] = {
        {"0", 0, SavedError},
        {"-0", 0, SavedError},
        {"+0", 0, SavedError},
        {"--5", 0, SavedError},
        {" \t+42tail", 42, SavedError},
        {"", 0, SavedError},
        {"words", 0, SavedError},
        {"9223372036854775807", INT64_MAX, SavedError},
        {"-9223372036854775808", INT64_MIN, SavedError},
        {"9223372036854775808", INT64_MIN, SavedError},
        {"-9223372036854775809", INT64_MAX, SavedError},
        {"18446744073709551615", -1LL, SavedError},
        {"-18446744073709551615", 1LL, SavedError},
        {"18446744073709551616", -1LL, 34},
        {"999999999999999999999999999999999999", -1LL, 34},
        {"-999999999999999999999999999999999999", -1LL, 34},
    };
    for (const auto& test : wideCases) {
        *__error_nid_postfix() = SavedError;
        const std::int64_t lvalue = atol_nid_postfix(test.text);
        const int lerror = *__error_nid_postfix();
        if (lvalue != test.value || lerror != test.error) {
            std::fprintf(stderr, "atol(%s): expected %lld, errno %d; received %lld, errno %d\n",
                test.text, static_cast<long long>(test.value), test.error,
                static_cast<long long>(lvalue), lerror);
            correct = false;
        }
        *__error_nid_postfix() = SavedError;
        const long long llvalue = atoll_nid_postfix(test.text);
        const int llerror = *__error_nid_postfix();
        if (llvalue != static_cast<long long>(test.value) || llerror != test.error) {
            std::fprintf(stderr, "atoll(%s): expected %lld, errno %d; received %lld, errno %d\n",
                test.text, static_cast<long long>(test.value), test.error, llvalue, llerror);
            correct = false;
        }
    }
    struct ErrnoCase {
        const char* name;
        int host;
        int guest;
    };
    constexpr ErrnoCase errnoCases[] = {
        {"EAGAIN", EAGAIN, 35},
#ifdef EDEADLK
        {"EDEADLK", EDEADLK, 11},
#endif
        {"ENAMETOOLONG", ENAMETOOLONG, 63},
        {"ENOTEMPTY", ENOTEMPTY, 66},
#ifdef ELOOP
        {"ELOOP", ELOOP, 62},
#endif
#ifdef EDQUOT
        {"EDQUOT", EDQUOT, 69},
#endif
#ifdef EOVERFLOW
        {"EOVERFLOW", EOVERFLOW, 84},
#endif
#ifdef ETXTBSY
        {"ETXTBSY", ETXTBSY, 26},
#endif
#ifdef EWOULDBLOCK
        {"EWOULDBLOCK", EWOULDBLOCK, 35},
#endif
#ifdef EINPROGRESS
        {"EINPROGRESS", EINPROGRESS, 36},
#endif
#ifdef EALREADY
        {"EALREADY", EALREADY, 37},
#endif
#ifdef ENOTSOCK
        {"ENOTSOCK", ENOTSOCK, 38},
#endif
#ifdef EDESTADDRREQ
        {"EDESTADDRREQ", EDESTADDRREQ, 39},
#endif
#ifdef EMSGSIZE
        {"EMSGSIZE", EMSGSIZE, 40},
#endif
#ifdef EPROTOTYPE
        {"EPROTOTYPE", EPROTOTYPE, 41},
#endif
#ifdef ENOPROTOOPT
        {"ENOPROTOOPT", ENOPROTOOPT, 42},
#endif
#ifdef EPROTONOSUPPORT
        {"EPROTONOSUPPORT", EPROTONOSUPPORT, 43},
#endif
#ifdef ESOCKTNOSUPPORT
        {"ESOCKTNOSUPPORT", ESOCKTNOSUPPORT, 44},
#endif
#ifdef EOPNOTSUPP
        {"EOPNOTSUPP", EOPNOTSUPP, 45},
#endif
#ifdef ENOTSUP
        {"ENOTSUP", ENOTSUP, 45},
#endif
#ifdef EPFNOSUPPORT
        {"EPFNOSUPPORT", EPFNOSUPPORT, 46},
#endif
#ifdef EAFNOSUPPORT
        {"EAFNOSUPPORT", EAFNOSUPPORT, 47},
#endif
#ifdef EADDRINUSE
        {"EADDRINUSE", EADDRINUSE, 48},
#endif
#ifdef EADDRNOTAVAIL
        {"EADDRNOTAVAIL", EADDRNOTAVAIL, 49},
#endif
#ifdef ENETDOWN
        {"ENETDOWN", ENETDOWN, 50},
#endif
#ifdef ENETUNREACH
        {"ENETUNREACH", ENETUNREACH, 51},
#endif
#ifdef ENETRESET
        {"ENETRESET", ENETRESET, 52},
#endif
#ifdef ECONNABORTED
        {"ECONNABORTED", ECONNABORTED, 53},
#endif
#ifdef ECONNRESET
        {"ECONNRESET", ECONNRESET, 54},
#endif
#ifdef ENOBUFS
        {"ENOBUFS", ENOBUFS, 55},
#endif
#ifdef EISCONN
        {"EISCONN", EISCONN, 56},
#endif
#ifdef ENOTCONN
        {"ENOTCONN", ENOTCONN, 57},
#endif
#ifdef ESHUTDOWN
        {"ESHUTDOWN", ESHUTDOWN, 58},
#endif
#ifdef ETOOMANYREFS
        {"ETOOMANYREFS", ETOOMANYREFS, 59},
#endif
#ifdef ETIMEDOUT
        {"ETIMEDOUT", ETIMEDOUT, 60},
#endif
#ifdef ECONNREFUSED
        {"ECONNREFUSED", ECONNREFUSED, 61},
#endif
#ifdef EHOSTDOWN
        {"EHOSTDOWN", EHOSTDOWN, 64},
#endif
#ifdef EHOSTUNREACH
        {"EHOSTUNREACH", EHOSTUNREACH, 65},
#endif
#ifdef ESTALE
        {"ESTALE", ESTALE, 70},
#endif
#ifdef ENOLCK
        {"ENOLCK", ENOLCK, 77},
#endif
#ifdef ENOSYS
        {"ENOSYS", ENOSYS, 78},
#endif
#ifdef EIDRM
        {"EIDRM", EIDRM, 82},
#endif
#ifdef ENOMSG
        {"ENOMSG", ENOMSG, 83},
#endif
#ifdef ECANCELED
        {"ECANCELED", ECANCELED, 85},
#endif
#ifdef EILSEQ
        {"EILSEQ", EILSEQ, 86},
#endif
#ifdef EBADMSG
        {"EBADMSG", EBADMSG, 89},
#endif
#ifdef EMULTIHOP
        {"EMULTIHOP", EMULTIHOP, 90},
#endif
#ifdef ENOLINK
        {"ENOLINK", ENOLINK, 91},
#endif
#ifdef EPROTO
        {"EPROTO", EPROTO, 92},
#endif
#ifdef ENOTRECOVERABLE
        {"ENOTRECOVERABLE", ENOTRECOVERABLE, 95},
#endif
#ifdef EOWNERDEAD
        {"EOWNERDEAD", EOWNERDEAD, 96},
#endif
        {"ENOENT", ENOENT, 2},
        {"ERANGE", ERANGE, 34},
        {"zero", 0, 5},
        {"negative", -1, 5},
        {"unmapped", 9999, 5},
    };
    for (const auto& test : errnoCases) {
        const int guest = GuestErrnoFromHost_nid_no_patch(test.host);
        if (guest != test.guest) {
            std::fprintf(stderr, "GuestErrnoFromHost(%s=%d): expected %d; received %d\n",
                test.name, test.host, test.guest, guest);
            correct = false;
        }
    }
    return correct ? 0 : 1;
}
