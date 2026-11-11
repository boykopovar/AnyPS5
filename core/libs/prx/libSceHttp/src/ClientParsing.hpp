#pragma once

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace {

bool IsToken(unsigned char byte) {
    constexpr std::string_view punctuation = "!#$%&'*+-.^_`|~";
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
        (byte >= '0' && byte <= '9') || punctuation.find(byte) != std::string_view::npos;
}

std::optional<std::uint64_t> ChunkSize(std::string_view text) {
    auto index = text.find_first_of("; \t");
    const auto digits = text.substr(0, index);
    if (digits.empty()) return {};
    std::uint64_t count = 0;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), count, 16);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size()) return {};
    if (index == std::string_view::npos) return count;
    const auto whitespace = [&] {
        while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) ++index;
    };
    const auto token = [&] {
        const auto start = index;
        while (index < text.size() && IsToken(static_cast<unsigned char>(text[index]))) ++index;
        return index != start;
    };
    while (index < text.size()) {
        whitespace();
        if (index == text.size() || text[index++] != ';') return {};
        whitespace();
        if (!token()) return {};
        const auto nameEnd = index;
        whitespace();
        if (index == text.size() || text[index] != '=') {
            index = nameEnd;
            continue;
        }
        ++index;
        whitespace();
        if (index == text.size()) return {};
        if (text[index] != '"') {
            if (!token()) return {};
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
                if (index == text.size()) return {};
                byte = static_cast<unsigned char>(text[index++]);
            }
            if ((byte < 0x20 && byte != '\t') || byte == 0x7f) return {};
        }
        if (!closed) return {};
    }
    return count;
}

}
