#include "prx/libSceHttp/include/Http.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"

namespace {

constexpr uint32_t URI_BUILD_WITH_SCHEME = 0x01;
constexpr uint32_t URI_BUILD_WITH_HOSTNAME = 0x02;
constexpr uint32_t URI_BUILD_WITH_PORT = 0x04;
constexpr uint32_t URI_BUILD_WITH_PATH = 0x08;
constexpr uint32_t URI_BUILD_WITH_USERNAME = 0x10;
constexpr uint32_t URI_BUILD_WITH_PASSWORD = 0x20;
constexpr uint32_t URI_BUILD_WITH_QUERY = 0x40;
constexpr uint32_t URI_BUILD_WITH_FRAGMENT = 0x80;
constexpr size_t URI_MAX_LENGTH = 0x3FFF;

struct UriParts {
    bool opaque = false;
    std::string_view scheme;
    std::string_view username;
    std::string_view password;
    std::string_view hostname;
    std::string path;
    size_t pathSpace = 0;
    std::string_view query;
    std::string_view fragment;
    uint16_t port = 0;
};

bool isAsciiAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isAsciiDigit(char c) {
    return c >= '0' && c <= '9';
}

char toAsciiLower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool equalsIgnoreCase(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (size_t i = 0; i < left.size(); ++i) {
        if (toAsciiLower(left[i]) != toAsciiLower(right[i])) return false;
    }
    return true;
}

uint16_t defaultPort(std::string_view scheme) {
    if (scheme.size() >= 5 && equalsIgnoreCase(scheme.substr(0, 5), "https")) return 443;
    if (scheme.size() >= 4 && equalsIgnoreCase(scheme.substr(0, 4), "http")) return 80;
    return 0;
}

bool isHostCharacter(char c) {
    return isAsciiAlpha(c) || isAsciiDigit(c) || c == '-' || c == '.' || c == '_';
}

bool isUserinfoCharacter(char c) {
    return isAsciiAlpha(c) || isAsciiDigit(c) || std::string_view("-._~!$&'()*+,;=%:").find(c) != std::string_view::npos;
}

std::string sweptPath(std::string_view path) {
    const std::string source(path);
    std::string swept(source.size() + 1, '\0');
    sceHttpUriSweepPath(swept.data(), source.c_str(), source.size() + 1);
    swept.resize(std::strlen(swept.c_str()));
    return swept;
}

size_t schemeLength(std::string_view uri) {
    size_t schemeEnd = 0;
    if (!uri.empty() && (isAsciiAlpha(uri[0]) || uri[0] == '+' || uri[0] == '-' || uri[0] == '.')) {
        schemeEnd = 1;
        while (schemeEnd < uri.size() && (isAsciiAlpha(uri[schemeEnd]) || isAsciiDigit(uri[schemeEnd]) || uri[schemeEnd] == '+'
            || uri[schemeEnd] == '-' || uri[schemeEnd] == '.')) ++schemeEnd;
    }
    return schemeEnd > 0 && schemeEnd < uri.size() && uri[schemeEnd] == ':' ? schemeEnd : 0;
}

bool isOpaque(std::string_view uri) {
    const size_t schemeEnd = schemeLength(uri);
    return !uri.substr(schemeEnd == 0 ? 0 : schemeEnd + 1).starts_with("//");
}

int parseUri(std::string_view uri, UriParts& parts) {
    if (const size_t schemeEnd = schemeLength(uri); schemeEnd != 0) {
        parts.scheme = uri.substr(0, schemeEnd);
        uri.remove_prefix(schemeEnd + 1);
    }

    parts.opaque = !uri.starts_with("//");
    if (!parts.opaque) uri.remove_prefix(2);

    size_t userinfoEnd = 0;
    while (userinfoEnd < uri.size() && isUserinfoCharacter(uri[userinfoEnd])) ++userinfoEnd;
    if (userinfoEnd < uri.size() && uri[userinfoEnd] == '@') {
        const std::string_view userinfo = uri.substr(0, userinfoEnd);
        const size_t colon = userinfo.find(':');
        parts.username = userinfo.substr(0, colon);
        if (colon != std::string_view::npos) parts.password = userinfo.substr(colon + 1);
        uri.remove_prefix(userinfoEnd + 1);
    }

    if (uri.starts_with('[')) {
        const size_t close = uri.find(']');
        if (close == std::string_view::npos) return ERROR_INVALID_URL;
        parts.hostname = uri.substr(1, close - 1);
        for (const char c : parts.hostname) {
            if (!isHostCharacter(c) && c != ':') return ERROR_INVALID_URL;
        }
        uri.remove_prefix(close + 1);
    } else {
        size_t hostEnd = 0;
        if (!uri.starts_with('.')) {
            while (hostEnd < uri.size() && isHostCharacter(uri[hostEnd])) ++hostEnd;
        }
        parts.hostname = uri.substr(0, hostEnd);
        uri.remove_prefix(hostEnd);
    }

    parts.port = defaultPort(parts.scheme);
    if (uri.starts_with(':')) {
        uri.remove_prefix(1);
        const std::string_view port = uri.substr(0, uri.find('/'));
        uri.remove_prefix(port.size());
        if (port.size() > 5) return ERROR_INVALID_URL;
        uint32_t portValue = 0;
        for (const char c : port) {
            if (!isAsciiDigit(c)) return ERROR_INVALID_URL;
            portValue = portValue * 10 + static_cast<uint32_t>(c - '0');
        }
        if (portValue > 0x10000) return ERROR_INVALID_URL;
        if (!port.empty()) parts.port = static_cast<uint16_t>(portValue);
    }

    const std::string_view path = uri.substr(0, uri.find_first_of("?#"));
    parts.path = sweptPath(path);
    parts.pathSpace = path.size();
    uri.remove_prefix(path.size());

    if (uri.starts_with('?')) {
        parts.query = uri.substr(0, uri.find('#'));
        uri.remove_prefix(parts.query.size());
    }
    parts.fragment = uri;
    return 0;
}

size_t poolSize(const UriParts& parts) {
    const std::string_view fields[] = {parts.scheme, parts.username, parts.password, parts.hostname, parts.query, parts.fragment};
    size_t size = parts.pathSpace + 1;
    for (const std::string_view field : fields) size += field.size() + 1;
    return size;
}

int copyOut(const std::string& value, char* out, size_t* require, size_t prepare) {
    if (require) *require = value.size() + 1;
    if (!out) return 0;
    if (prepare < value.size() + 1) return ERROR_OUT_OF_MEMORY;
    std::memcpy(out, value.c_str(), value.size() + 1);
    return 0;
}

}

extern "C" {

int APS5_VABI sceHttpUriBuild(char* out, size_t* requiredSize, size_t capacity, const SceHttpUriElement* element, uint32_t option) {
    if (!element) return ERROR_INVALID_URL;
    if (!out && !requiredSize) return ERROR_INVALID_VALUE;

    const auto field = [](const char* value, size_t limit) {
        size_t length = 0;
        if (value) while (length < limit && value[length] != '\0') ++length;
        return value ? std::string_view(value, length) : std::string_view();
    };
    const auto scheme = option & (URI_BUILD_WITH_SCHEME | URI_BUILD_WITH_PORT) ? field(element->scheme, 32) : std::string_view();
    const auto username = option & URI_BUILD_WITH_USERNAME ? field(element->username, 256) : std::string_view();
    const auto password = option & URI_BUILD_WITH_PASSWORD ? field(element->password, 256) : std::string_view();
    const auto hostname = option & URI_BUILD_WITH_HOSTNAME ? field(element->hostname, 255) : std::string_view();

    std::string uri;
    if (option & URI_BUILD_WITH_SCHEME && !scheme.empty()) uri.append(scheme).push_back(':');
    if (element->opaque != 1) uri.append("//");
    uri.append(username);
    if (!password.empty()) uri.append(":").append(password);
    if (!username.empty() || !password.empty()) uri.push_back('@');
    uri.append(hostname);
    if (option & URI_BUILD_WITH_PORT && element->port != 0) {
        std::uint16_t port = 0;
        if (scheme.size() >= 5 && equalsIgnoreCase(scheme.substr(0, 5), "https")) port = 443;
        else if ((scheme.size() >= 4 && equalsIgnoreCase(scheme.substr(0, 4), "http")) || (scheme.size() >= 3 && equalsIgnoreCase(scheme.substr(0, 3), "ttp"))) port = 80;
        if (element->port != port) uri.append(":").append(std::to_string(element->port));
    }
    if (option & URI_BUILD_WITH_PATH) uri.append(field(element->path, URI_MAX_LENGTH));
    if (option & URI_BUILD_WITH_QUERY) uri.append(field(element->query, URI_MAX_LENGTH));
    if (option & URI_BUILD_WITH_FRAGMENT) uri.append(field(element->fragment, URI_MAX_LENGTH));
    const size_t size = uri.size() + 1 + ((option & URI_BUILD_WITH_SCHEME) && element->scheme && scheme.empty());
    if (requiredSize) *requiredSize = size;
    if (!out) return 0;
    if (capacity < size) return ERROR_OUT_OF_MEMORY;
    std::memcpy(out, uri.c_str(), uri.size() + 1);
    return 0;
}

int APS5_VABI sceHttpUriEscape(char* out, size_t* require, size_t prepare, const char* in) {
    if (!in) return ERROR_INVALID_VALUE;
    if (!out && !require) return ERROR_INVALID_VALUE;

    static constexpr char hex[] = "0123456789ABCDEF";
    std::string escaped;
    for (const char* c = in; *c; ++c) {
        const auto value = static_cast<unsigned char>(*c);
        if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9')
            || value == '-' || value == '_' || value == '.' || value == '~') {
            escaped.push_back(*c);
        } else {
            escaped.push_back('%');
            escaped.push_back(hex[value >> 4]);
            escaped.push_back(hex[value & 0xF]);
        }
    }
    return copyOut(escaped, out, require, prepare);
}

int APS5_VABI sceHttpUriUnescape(char* out, size_t* requiredSize, size_t capacity, const char* input) {
    if (!input || (!out && !requiredSize)) return ERROR_INVALID_VALUE;

    const std::string_view source(input);
    const auto decode = [](const char* digits, unsigned int& value) {
        const auto result = std::from_chars(digits, digits + 2, value, 16);
        return result.ec == std::errc{} && result.ptr == digits + 2;
    };
    for (size_t index = 0; index < source.size(); ++index) {
        if (source[index] != '%') continue;
        unsigned int value = 0;
        if (source.size() - index < 3 || !decode(source.data() + index + 1, value)) return ERROR_INVALID_URL;
        index += 2;
    }

    if (!source.empty()) {
        std::uint32_t sdkVersion = 0;
        const auto* parameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
        std::memcpy(&sdkVersion, parameters + 0x10, sizeof(sdkVersion));
        if (sdkVersion < 0x1000000) {
            for (size_t index = 0; index < source.size(); ++index) {
                const char value = source[index];
                if (value == '%') {
                    index += 2;
                    continue;
                }
                if (!isAsciiAlpha(value) && !isAsciiDigit(value) && value != '*' && value != '-' && value != '.' && value != '_' && value != '+') return ERROR_INVALID_URL;
            }
        }
    }

    size_t written = 0;
    for (size_t index = 0; index < source.size(); ++index) {
        if (out && written >= capacity) return ERROR_OUT_OF_MEMORY;
        char value = source[index];
        if (value == '%') {
            unsigned int decoded = 0;
            decode(source.data() + index + 1, decoded);
            value = static_cast<char>(decoded);
            index += 2;
        }
        if (out) out[written] = value;
        ++written;
    }
    if (out) {
        if (written >= capacity) return ERROR_OUT_OF_MEMORY;
        out[written] = '\0';
    }
    if (requiredSize) *requiredSize = written + 1;
    return 0;
}

int APS5_VABI sceHttpUriMerge(char* merged_url, const char* url, const char* relative_uri, size_t* require, size_t prepare, uint32_t option) {
    if (option != 0 || !url || !relative_uri) return ERROR_INVALID_VALUE;

    UriParts base;
    if (const int result = parseUri(url, base); result != 0) return result;
    const bool relativeOpaque = isOpaque(relative_uri);
    if (UriParts relative; !relativeOpaque) {
        if (const int result = parseUri(relative_uri, relative); result != 0) return result;
    }

    const size_t urlLength = strnlen(url, URI_MAX_LENGTH);
    const size_t relativeLength = strnlen(relative_uri, URI_MAX_LENGTH);
    const size_t size = poolSize(base) + 2 + (urlLength + relativeLength) * 2;
    if (require) *require = size;
    if (!merged_url) return 0;
    if (prepare < size) return ERROR_OUT_OF_MEMORY;

    if (!relativeOpaque) {
        std::strncpy(merged_url, relative_uri, size);
        merged_url[size - 1] = '\0';
        if (require) *require = relativeLength + 1;
        return 0;
    }

    std::string path = base.path;
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos) path.push_back('/');
    else path.erase(slash + 1);
    if (relative_uri[0] == '/') path.clear();
    path.append(relative_uri, std::min(relativeLength, URI_MAX_LENGTH - std::min(path.size(), URI_MAX_LENGTH)));

    std::string scheme(base.scheme), username(base.username), password(base.password), hostname(base.hostname);
    SceHttpUriElement element;
    element.opaque = base.opaque;
    element.scheme = scheme.data();
    element.username = username.data();
    element.password = password.data();
    element.hostname = hostname.data();
    element.path = path.data();
    element.port = base.port;
    return sceHttpUriBuild(merged_url, nullptr, prepare - (urlLength + relativeLength + 1), &element,
        URI_BUILD_WITH_SCHEME | URI_BUILD_WITH_HOSTNAME | URI_BUILD_WITH_PORT | URI_BUILD_WITH_PATH | URI_BUILD_WITH_USERNAME | URI_BUILD_WITH_PASSWORD);
}

int APS5_VABI sceHttpUriParse(SceHttpUriElement* out, const char* src_url, void* pool, size_t* require, size_t prepare) {
    if (!src_url) return ERROR_INVALID_URL;
    const bool write = out && pool;
    if (!write && !require) return ERROR_INVALID_VALUE;

    UriParts parts;
    if (const int result = parseUri(src_url, parts); result != 0) return result;

    const std::string_view fields[] = {parts.scheme, parts.username, parts.password, parts.hostname, parts.path, parts.query, parts.fragment};
    const size_t spaces[] = {parts.scheme.size(), parts.username.size(), parts.password.size(), parts.hostname.size(), parts.pathSpace, parts.query.size(),
        parts.fragment.size()};
    const size_t size = poolSize(parts);
    if (write && prepare < size) return ERROR_OUT_OF_MEMORY;
    if (require) *require = size;
    if (!write) return 0;

    char* next = static_cast<char*>(pool);
    char* strings[7];
    for (size_t i = 0; i < 7; ++i) {
        strings[i] = next;
        if (!fields[i].empty()) std::memcpy(next, fields[i].data(), fields[i].size());
        next[fields[i].size()] = '\0';
        next += spaces[i] + 1;
    }
    *out = {};
    out->opaque = parts.opaque;
    out->scheme = strings[0];
    out->username = strings[1];
    out->password = strings[2];
    out->hostname = strings[3];
    out->path = strings[4];
    out->query = strings[5];
    out->fragment = strings[6];
    out->port = parts.port;
    return 0;
}

int APS5_VABI sceHttpUriCopy(SceHttpUriElement* out, const SceHttpUriElement* src, void* pool, size_t prepare, size_t* require) {
    if (!src) return ERROR_INVALID_URL;
    const bool write = out && pool;
    if (!write && !require) return ERROR_INVALID_VALUE;

    if (write) {
        std::memset(out, 0, sizeof(*out));
        std::memset(pool, 0, prepare);
    }
    const char* const fields[] = {src->scheme, src->hostname, src->path, src->username, src->password};
    const uint16_t port = src->port;
    size_t lengths[5];
    size_t used = 0;
    for (size_t i = 0; i < 5; ++i) {
        lengths[i] = fields[i] ? std::strlen(fields[i]) : 0;
        used += lengths[i] + 1;
    }
    if (!write) {
        *require = used;
        return 0;
    }
    if (used > prepare) return ERROR_OUT_OF_MEMORY;

    char* next = static_cast<char*>(pool);
    char* strings[5];
    for (size_t i = 0; i < 5; ++i) {
        strings[i] = next;
        if (lengths[i] != 0) std::memcpy(next, fields[i], lengths[i]);
        next[lengths[i]] = '\0';
        next += lengths[i] + 1;
    }
    out->scheme = strings[0];
    out->hostname = strings[1];
    out->path = strings[2];
    out->username = strings[3];
    out->password = strings[4];
    out->port = port;
    if (require) *require = used;
    return 0;
}

int APS5_VABI sceHttpUriSweepPath(char* dst, const char* src, size_t srcSize) {
    if (srcSize == 0) return 0;
    if (!dst || !src) return ERROR_INVALID_VALUE;

    const size_t length = srcSize - 1;
    if (src[0] != '/') {
        std::memcpy(dst, src, length);
        dst[length] = '\0';
        return 0;
    }

    dst[0] = '/';
    dst[1] = '\0';
    size_t end = 0;
    size_t pos = 1;
    while (pos < length) {
        if (src[pos] == '.' && pos + 1 < length && src[pos + 1] == '/') {
            pos += 2;
            continue;
        }
        if (src[pos] == '.' && pos + 2 <= length && src[pos + 1] == '.' && src[pos + 2] == '/') {
            if (end != 0) {
                dst[end] = '\0';
                end = static_cast<size_t>(std::strrchr(dst, '/') - dst);
                dst[end + 1] = '\0';
            }
            pos += 3;
            continue;
        }
        size_t count = length - pos;
        if (const void* slash = std::memchr(src + pos, '/', length - pos)) {
            count = static_cast<size_t>(static_cast<const char*>(slash) + 1 - (src + pos));
        }
        std::memcpy(dst + end + 1, src + pos, count);
        dst[end + 1 + count] = '\0';
        end += count;
        pos += count;
    }
    return 0;
}

}
