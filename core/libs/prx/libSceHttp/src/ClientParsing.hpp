#pragma once

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace {

class HttpFailure : public std::runtime_error {
public:
    explicit HttpFailure(const char* message) : std::runtime_error(message) {}
};

std::uint64_t Number(std::string_view text, int base = 10) {
    std::uint64_t number = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number, base);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) throw HttpFailure("invalid HTTP size");
    return number;
}

bool IsToken(unsigned char byte) {
    constexpr std::string_view punctuation = "!#$%&'*+-.^_`|~";
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
        (byte >= '0' && byte <= '9') || punctuation.find(byte) != std::string_view::npos;
}

std::uint64_t ChunkSize(std::string_view text) {
    auto index = text.find_first_of("; \t");
    const auto count = Number(text.substr(0, index), 16);
    if (index == std::string_view::npos) return count;
    const auto whitespace = [&] {
        while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) ++index;
    };
    const auto token = [&] {
        const auto start = index;
        while (index < text.size() && IsToken(static_cast<unsigned char>(text[index]))) ++index;
        if (index == start) throw HttpFailure("invalid HTTP chunk extension token");
    };
    while (index < text.size()) {
        whitespace();
        if (index == text.size() || text[index++] != ';') throw HttpFailure("invalid HTTP chunk extension separator");
        whitespace();
        token();
        const auto nameEnd = index;
        whitespace();
        if (index == text.size() || text[index] != '=') {
            index = nameEnd;
            continue;
        }
        ++index;
        whitespace();
        if (index == text.size()) throw HttpFailure("missing HTTP chunk extension value");
        if (text[index] != '"') {
            token();
            continue;
        }
        ++index;
        bool closed = false;
        while (index < text.size()) {
            auto byte = static_cast<unsigned char>(text[index++]);
            if (byte == '"') {
                closed = true;
                break;
            }
            if (byte == '\\') {
                if (index == text.size()) throw HttpFailure("truncated HTTP chunk extension escape");
                byte = static_cast<unsigned char>(text[index++]);
            }
            if ((byte < 0x20 && byte != '\t') || byte == 0x7f) throw HttpFailure("invalid HTTP chunk extension quoted byte");
        }
        if (!closed) throw HttpFailure("unclosed HTTP chunk extension quote");
    }
    return count;
}

}
