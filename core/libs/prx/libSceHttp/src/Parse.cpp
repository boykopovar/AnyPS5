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

bool parseVersionNumber(const char* line, size_t lineLen, size_t& index, int32_t* value) {
    if (index >= lineLen || !isAsciiDigit(line[index])) return false;
    uint32_t number = 0;
    while (index < lineLen && isAsciiDigit(line[index])) {
        number = number * 10 + static_cast<uint32_t>(line[index] - '0');
        ++index;
    }
    *value = static_cast<int32_t>(number);
    return true;
}

}

extern "C" {

int APS5_VABI sceHttpParseStatusLine(const char* statusLine, size_t lineLen, int32_t* httpMajorVer, int32_t* httpMinorVer,
    int32_t* responseCode, const char** reasonPhrase, size_t* phraseLen) {
    if (!statusLine) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    if (!httpMajorVer || !httpMinorVer || !responseCode || !reasonPhrase || !phraseLen) return ERROR_PARSE_HTTP_INVALID_VALUE;

    *httpMajorVer = 0;
    *httpMinorVer = 0;
    if (lineLen < 8 || std::memcmp(statusLine, "HTTP/", 5) != 0) return ERROR_PARSE_HTTP_INVALID_RESPONSE;

    size_t index = 5;
    if (!parseVersionNumber(statusLine, lineLen, index, httpMajorVer)) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    if (index >= lineLen || statusLine[index] != '.') return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    ++index;
    if (!parseVersionNumber(statusLine, lineLen, index, httpMinorVer)) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    if (index >= lineLen || statusLine[index] != ' ') return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    ++index;

    if (lineLen - index < 3) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    if (!isAsciiDigit(statusLine[index]) || !isAsciiDigit(statusLine[index + 1]) || !isAsciiDigit(statusLine[index + 2])) {
        return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    }
    *responseCode = (statusLine[index] - '0') * 100 + (statusLine[index + 1] - '0') * 10 + (statusLine[index + 2] - '0');
    index += 3;

    const char* phrase = statusLine + index;
    const void* lineFeed = std::memchr(phrase, '\n', lineLen - index);
    if (!lineFeed) return ERROR_PARSE_HTTP_INVALID_RESPONSE;

    size_t length = static_cast<size_t>(static_cast<const char*>(lineFeed) - phrase);
    const size_t consumed = index + length + 1;
    if (consumed > static_cast<size_t>(INT32_MAX)) {
        throw std::out_of_range("sceHttpParseStatusLine: consumed byte count exceeds int");
    }
    if (length > 0 && phrase[length - 1] == '\r') --length;

    *reasonPhrase = phrase;
    *phraseLen = length;
    return static_cast<int>(consumed);
}

}
