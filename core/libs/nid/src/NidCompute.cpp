#include <nid/NidCompute.hpp>
#include <nid/Sha1.hpp>
#include <stdexcept>

namespace Nid {

namespace {

constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

std::string _base64Encode(const std::uint8_t* data, std::size_t len) {
    std::string out;
    out.reserve(((len + 2u) / 3u) * 4u);
    for (std::size_t i = 0; i < len; i += 3u) {
        std::uint32_t group = static_cast<std::uint32_t>(data[i]) << 16u;
        if (i + 1u < len) group |= static_cast<std::uint32_t>(data[i+1u]) << 8u;
        if (i + 2u < len) group |= static_cast<std::uint32_t>(data[i+2u]);

        out.push_back(kBase64Chars[(group >> 18u) & 0x3fu]);
        out.push_back(kBase64Chars[(group >> 12u) & 0x3fu]);
        out.push_back((i + 1u < len) ? kBase64Chars[(group >> 6u) & 0x3fu] : '=');
        out.push_back((i + 2u < len) ? kBase64Chars[(group) & 0x3fu] : '=');
    }
    return out;
}

}

std::string ComputeNid(const std::string& symbolName, const std::string& libraryName) {
    if (symbolName.empty()) throw std::invalid_argument("symbolName is empty");
    if (libraryName.empty()) throw std::invalid_argument("libraryName is empty");

    std::vector<std::uint8_t> input;
    input.reserve(symbolName.size() + 1u + libraryName.size() + 1u + libraryName.size() + 1u);

    for (char c : symbolName) input.push_back(static_cast<std::uint8_t>(c));
    input.push_back(0x00u);
    for (char c : libraryName) input.push_back(static_cast<std::uint8_t>(c));
    input.push_back(0x00u);
    for (char c : libraryName) input.push_back(static_cast<std::uint8_t>(c));
    input.push_back(0x00u);

    const auto digest = Sha1(input);
    const std::string b64 = _base64Encode(digest.data(), digest.size());

    if (b64.size() < 11u) throw std::runtime_error("base64 output too short");
    return b64.substr(0u, 11u) + '#';
}

}
