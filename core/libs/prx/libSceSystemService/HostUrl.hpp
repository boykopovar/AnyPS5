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

constexpr bool IsValidIpv4Address(std::string_view address) {
    if (address.empty()) return false;
    std::size_t octets = 0;
    std::size_t pos = 0;
    while (true) {
        const std::size_t dot = address.find('.', pos);
        const bool isLast = dot == std::string_view::npos;
        const std::string_view segment = isLast ? address.substr(pos) : address.substr(pos, dot - pos);
        if (segment.empty() || segment.size() > 3) return false;
        if (segment.size() > 1 && segment.front() == '0') return false;
        int value = 0;
        for (const char digit : segment) {
            if (digit < '0' || digit > '9') return false;
            value = value * 10 + (digit - '0');
        }
        if (value > 255) return false;
        ++octets;
        if (isLast) break;
        pos = dot + 1;
    }
    return octets == 4;
}

constexpr bool CountIpv6Groups(std::string_view part, bool allowIpv4, std::size_t& groups) {
    if (part.empty()) return true;
    std::size_t pos = 0;
    while (true) {
        const std::size_t colon = part.find(':', pos);
        const bool isLast = colon == std::string_view::npos;
        const std::string_view segment = isLast ? part.substr(pos) : part.substr(pos, colon - pos);
        if (segment.empty()) return false;
        if (isLast && allowIpv4 && segment.find('.') != std::string_view::npos) {
            if (!IsValidIpv4Address(segment)) return false;
            groups += 2;
        } else {
            if (segment.size() > 4) return false;
            for (const char value : segment)
                if (!IsHexDigit(value)) return false;
            ++groups;
        }
        if (isLast) break;
        pos = colon + 1;
    }
    return true;
}

constexpr bool IsValidIpv6Literal(std::string_view host) {
    if (host.empty()) return false;
    const std::size_t doubleColon = host.find("::");
    if (doubleColon == std::string_view::npos) {
        std::size_t groups = 0;
        return CountIpv6Groups(host, true, groups) && groups == 8;
    }
    if (host.find("::", doubleColon + 1) != std::string_view::npos) return false;
    const std::string_view left = host.substr(0, doubleColon);
    const std::string_view right = host.substr(doubleColon + 2);
    std::size_t groups = 0;
    if (!CountIpv6Groups(left, false, groups)) return false;
    if (!CountIpv6Groups(right, true, groups)) return false;
    return groups < 8;
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
        if (!IsValidIpv6Literal(host)) return HostUrlVerdict::BadAuthority;
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
