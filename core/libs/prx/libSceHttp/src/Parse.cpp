#include "prx/libSceHttp/include/Http.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "prx/libc/include/General.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"

namespace {

bool isAsciiDigit(char c) {
    return c >= '0' && c <= '9';
}

struct GuestLine {
    const char* line;
    bool failed = false;

    char At(size_t index) {
        char value = 0;
        if (failed) return value;
        const int savedErrno = errno;
        failed = !GuestCopyIn_nid_no_patch(&value, line + index, sizeof(value));
        errno = savedErrno;
        return value;
    }
};

template<class T>
bool writeGuest(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &value, sizeof(value));
    errno = savedErrno;
    return copied;
}

int parseVersionNumber(GuestLine& line, size_t& index, int32_t* value) {
    if (!isAsciiDigit(line.At(index))) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    uint32_t number = 0;
    for (char digit = line.At(index); !line.failed && isAsciiDigit(digit); digit = line.At(index)) {
        number = number * 10 + static_cast<uint32_t>(digit - '0');
        ++index;
    }
    if (line.failed) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    if (!writeGuest(value, static_cast<int32_t>(number))) return ERROR_PARSE_HTTP_INVALID_VALUE;
    return 0;
}

}

extern "C" {

int APS5_VABI sceHttpParseStatusLine(const char* statusLine, size_t lineLen, int32_t* httpMajorVer, int32_t* httpMinorVer,
    int32_t* responseCode, const char** reasonPhrase, size_t* phraseLen) {
    if (!statusLine) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    if (!httpMajorVer || !httpMinorVer || !responseCode || !reasonPhrase || !phraseLen) return ERROR_PARSE_HTTP_INVALID_VALUE;

    if (!writeGuest(httpMajorVer, int32_t{0}) || !writeGuest(httpMinorVer, int32_t{0})) return ERROR_PARSE_HTTP_INVALID_VALUE;
    if (lineLen < 8) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    char prefix[5] = {};
    {
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(prefix, statusLine, sizeof(prefix));
        errno = savedErrno;
        if (!copied || std::memcmp(prefix, "HTTP/", 5) != 0) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    }

    GuestLine line{statusLine};
    size_t index = 5;
    if (const int error = parseVersionNumber(line, index, httpMajorVer)) return error;
    if (line.At(index) != '.' || line.failed) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    ++index;
    if (const int error = parseVersionNumber(line, index, httpMinorVer)) return error;
    if (line.At(index) != ' ' || line.failed) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    ++index;

    if (lineLen - index < 3) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    const char hundreds = line.At(index);
    const char tens = line.At(index + 1);
    const char units = line.At(index + 2);
    if (line.failed || !isAsciiDigit(hundreds) || !isAsciiDigit(tens) || !isAsciiDigit(units)) {
        return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    }
    if (!writeGuest(responseCode, (hundreds - '0') * 100 + (tens - '0') * 10 + (units - '0'))) return ERROR_PARSE_HTTP_INVALID_VALUE;
    index += 3;

    const char* phrase = statusLine + index;
    const size_t searched = lineLen - index + 1;
    size_t length = 0;
    while (length != searched && line.At(index + length) != '\n' && !line.failed) ++length;
    if (line.failed || length == searched) return ERROR_PARSE_HTTP_INVALID_RESPONSE;

    const size_t consumed = index + length + 1;
    if (consumed > static_cast<size_t>(INT32_MAX)) {
        throw std::out_of_range("sceHttpParseStatusLine: consumed byte count exceeds int");
    }
    if (length > 0 && line.At(index + length - 1) == '\r' && !line.failed) --length;

    if (!writeGuest(reasonPhrase, phrase) || !writeGuest(phraseLen, length)) return ERROR_PARSE_HTTP_INVALID_VALUE;
    return static_cast<int>(consumed);
}

}
