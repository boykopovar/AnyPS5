#include "common/DnsOverride.hpp"
#include <cctype>
#include <cstddef>

namespace DnsOverride {
namespace {
bool IsDigit(char value) { return std::isdigit(static_cast<unsigned char>(value)) != 0; }

bool ParseAddress(const std::string& text, Address& address) {
    std::size_t index = 0;
    for (std::size_t part = 0; part < address.size(); ++part) {
        if (part != 0) {
            if (index >= text.size() || text[index] != '.') return false;
            ++index;
        }
        const std::size_t start = index;
        unsigned value = 0;
        while (index < text.size() && IsDigit(text[index])) {
            value = value * 10 + static_cast<unsigned>(text[index] - '0');
            if (value > 255 || index - start == 3) return false;
            ++index;
        }
        const std::size_t digits = index - start;
        if (digits == 0 || (digits > 1 && text[start] == '0')) return false;
        address[part] = static_cast<std::uint8_t>(value);
    }
    return index == text.size();
}

bool ParseEntry(const std::string& text, Entry& entry) {
    const std::size_t equals = text.find('=');
    if (equals == std::string::npos || equals == 0) return false;
    const std::string hostname = text.substr(0, equals);
    for (const char value : hostname)
        if (std::isspace(static_cast<unsigned char>(value))) return false;
    if (!ParseAddress(text.substr(equals + 1), entry.address)) return false;
    entry.hostname.clear();
    for (const char value : hostname)
        entry.hostname.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(value))));
    return true;
}
}

Parsed Parse(const char* text) {
    Parsed parsed;
    if (!text || *text == '\0') return parsed;
    const std::string all(text);
    std::size_t start = 0;
    for (;;) {
        const std::size_t comma = all.find(',', start);
        const std::string item = all.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        Entry entry;
        if (ParseEntry(item, entry)) parsed.entries.push_back(entry);
        else parsed.malformed.push_back(item);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return parsed;
}
}
