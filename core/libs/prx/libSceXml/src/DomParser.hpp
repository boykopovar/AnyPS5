#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace XmlDom {

constexpr int StatusInProgress = 0x00850001;
constexpr int StatusComplete = 0;
constexpr int ErrorLongName = static_cast<int>(0x80850001);
constexpr int ErrorTruncated = static_cast<int>(0x80850008);
constexpr int ErrorReference = static_cast<int>(0x80850009);
constexpr int ErrorEndTag = static_cast<int>(0x8085000a);
constexpr int ErrorOutsideRoot = static_cast<int>(0x8085000b);
constexpr int ErrorMarkup = static_cast<int>(0x8085000e);
constexpr int ErrorNoEquals = static_cast<int>(0x8085000f);
constexpr int ErrorNoSemicolon = static_cast<int>(0x80850010);
constexpr int ErrorUnquoted = static_cast<int>(0x80850011);
constexpr int ErrorOpenComment = static_cast<int>(0x80850012);
constexpr int ErrorOpenCdata = static_cast<int>(0x80850013);
constexpr int ErrorOpenDoctype = static_cast<int>(0x80850014);
constexpr int ErrorOpenPi = static_cast<int>(0x80850017);
constexpr int ErrorElementAfterRoot = static_cast<int>(0x80850018);
constexpr int ErrorNoRoot = static_cast<int>(0x80850019);
constexpr int ErrorTooManyAttributes = static_cast<int>(0x80850205);

struct ParseOptions {
    bool resolveEntity = false;
    bool skipWhiteSpace = false;
    bool skipText = false;
};

struct Buffers {
    std::vector<std::uint8_t> nodes;
    std::vector<std::uint8_t> text;
    std::vector<std::uint8_t> names;
    std::vector<std::uint8_t> attributeNames;
};

struct ParseResult {
    int status = 0;
    bool stalledInsideTag = false;
    Buffers buffers;
};

ParseResult Parse(const std::uint8_t* data, std::size_t size, const ParseOptions& options, bool last);

}
