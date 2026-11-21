#pragma once

#include <cstddef>
#include <string_view>

constexpr std::size_t HOST_URL_MAX_LENGTH = 2048;
constexpr std::size_t HOST_URL_MAX_PORT = 65535;

enum class HostUrlVerdict {
    Ok,
    Empty,
    TooLong,
    BadCharacter,
    BadEscape,
    BadScheme,
    BadAuthority,
    Credentials,
    BadPort,
};

namespace host_url {

constexpr bool IsAlphaNumeric(char value) {
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

constexpr bool IsHexDigit(char value) {
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F');
}

constexpr int HexValue(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return value - 'A' + 10;
}

constexpr bool IsUrlCharacter(char value) {
    if (IsAlphaNumeric(value)) return true;
    for (const char allowed : std::string_view("-._~:/?#[]@!$&'()*+,;=%"))
        if (value == allowed) return true;
    return false;
}

constexpr bool IsHostCharacter(char value) {
    return IsAlphaNumeric(value) || value == '-' || value == '.' || value == '_';
}

constexpr HostUrlVerdict ValidateEscapes(std::string_view url) {
    for (std::size_t index = 0; index < url.size(); ++index) {
        if (url[index] != '%') continue;
        if (index + 2 >= url.size()) return HostUrlVerdict::BadEscape;
        if (!IsHexDigit(url[index + 1]) || !IsHexDigit(url[index + 2])) return HostUrlVerdict::BadEscape;
        const int decoded = HexValue(url[index + 1]) * 16 + HexValue(url[index + 2]);
        if (decoded < 0x20 || decoded == 0x7f) return HostUrlVerdict::BadEscape;
        index += 2;
    }
    return HostUrlVerdict::Ok;
}

constexpr HostUrlVerdict ValidatePort(std::string_view port) {
    if (port.empty() || port.size() > 5) return HostUrlVerdict::BadPort;
    std::size_t value = 0;
    for (const char digit : port) {
        if (digit < '0' || digit > '9') return HostUrlVerdict::BadPort;
        value = value * 10 + static_cast<std::size_t>(digit - '0');
    }
    return value >= 1 && value <= HOST_URL_MAX_PORT ? HostUrlVerdict::Ok : HostUrlVerdict::BadPort;
}

constexpr HostUrlVerdict ValidateAuthority(std::string_view authority) {
    if (authority.empty()) return HostUrlVerdict::BadAuthority;
    if (authority.find('@') != std::string_view::npos) return HostUrlVerdict::Credentials;
    std::string_view host = authority;
    std::string_view port;
    bool hasPort = false;
    if (authority.front() == '[') {
        const std::size_t close = authority.find(']');
        if (close == std::string_view::npos || close == 1) return HostUrlVerdict::BadAuthority;
        host = authority.substr(1, close - 1);
        for (const char value : host)
            if (!IsHexDigit(value) && value != ':' && value != '.') return HostUrlVerdict::BadAuthority;
        const std::string_view rest = authority.substr(close + 1);
        if (!rest.empty()) {
            if (rest.front() != ':') return HostUrlVerdict::BadAuthority;
            hasPort = true;
            port = rest.substr(1);
        }
    } else {
        const std::size_t colon = authority.find(':');
        if (colon != std::string_view::npos) {
            host = authority.substr(0, colon);
            hasPort = true;
            port = authority.substr(colon + 1);
        }
        if (host.empty()) return HostUrlVerdict::BadAuthority;
        for (const char value : host)
            if (!IsHostCharacter(value)) return HostUrlVerdict::BadAuthority;
    }
    if (hasPort) return ValidatePort(port);
    return HostUrlVerdict::Ok;
}

}

constexpr HostUrlVerdict ValidateHostUrl(std::string_view url) {
    if (url.empty()) return HostUrlVerdict::Empty;
    if (url.size() > HOST_URL_MAX_LENGTH) return HostUrlVerdict::TooLong;
    for (const char value : url)
        if (!host_url::IsUrlCharacter(value)) return HostUrlVerdict::BadCharacter;
    if (const HostUrlVerdict escapes = host_url::ValidateEscapes(url); escapes != HostUrlVerdict::Ok) return escapes;
    std::string_view rest = url;
    if (rest.substr(0, 7) == "http://") rest.remove_prefix(7);
    else if (rest.substr(0, 8) == "https://") rest.remove_prefix(8);
    else return HostUrlVerdict::BadScheme;
    const std::size_t end = rest.find_first_of("/?#");
    return host_url::ValidateAuthority(end == std::string_view::npos ? rest : rest.substr(0, end));
}
