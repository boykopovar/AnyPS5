#pragma once

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace {

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

bool IsToken(unsigned char byte) {
    constexpr std::string_view punctuation = "!#$%&'*+-.^_`|~";
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
        (byte >= '0' && byte <= '9') || punctuation.find(byte) != std::string_view::npos;
}

std::optional<std::uint64_t> ChunkSize(std::string_view text) {
    const auto digits = Trim(text.substr(0, text.find(';')));
    if (digits.empty()) return {};
    std::uint64_t count = 0;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), count, 16);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size()) return {};
    return count;
}

}
