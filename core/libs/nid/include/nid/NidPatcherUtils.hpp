#ifndef NID_NIDPATCHERUTILS_HPP
#define NID_NIDPATCHERUTILS_HPP

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Nid {
namespace Internal {

constexpr char kNidPostfix[] = "_nid_postfix";
constexpr std::size_t kNidPostfixLen = sizeof(kNidPostfix) - 1u;
constexpr char kNidDisambigMarker[] = "_nid_disambig";
constexpr std::size_t kNidDisambigMarkerLen = sizeof(kNidDisambigMarker) - 1u;

inline std::string StripNidPostfix(const std::string& name) {
    std::string result = name;
    if (result.size() >= kNidPostfixLen &&
        result.compare(result.size() - kNidPostfixLen, kNidPostfixLen, kNidPostfix) == 0) {
        result = result.substr(0u, result.size() - kNidPostfixLen);
    }
    const auto disambigPos = result.rfind(kNidDisambigMarker);
    if (disambigPos != std::string::npos) {
        const auto suffixStart = disambigPos + kNidDisambigMarkerLen;
        const bool allDigits = std::all_of(
            result.begin() + static_cast<std::ptrdiff_t>(suffixStart), result.end(),
            [](unsigned char c) { return std::isdigit(c) != 0; }
        );
        if (allDigits && suffixStart < result.size())
            result = result.substr(0u, disambigPos);
    }
    return result;
}

template<typename T>
T Read(const std::vector<std::uint8_t>& buf, std::size_t offset) {
    if (offset + sizeof(T) > buf.size()) throw std::runtime_error("read out of bounds");
    T v;
    std::memcpy(&v, buf.data() + offset, sizeof(T));
    return v;
}

template<typename T>
void Write(std::vector<std::uint8_t>& buf, std::size_t offset, const T& v) {
    if (offset + sizeof(T) > buf.size()) throw std::runtime_error("write out of bounds");
    std::memcpy(buf.data() + offset, &v, sizeof(T));
}

inline std::string ReadCStr(const std::vector<std::uint8_t>& buf, std::size_t offset) {
    if (offset >= buf.size()) throw std::runtime_error("cstr offset out of bounds");
    std::string s;
    while (offset < buf.size() && buf[offset] != 0u)
        s.push_back(static_cast<char>(buf[offset++]));
    return s;
}

}
}

#endif
