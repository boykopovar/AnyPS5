#include <nid/NidCompute.hpp>
#include <nid/Sha1.hpp>
#include <stdexcept>
#include <cstdint>

namespace Nid {

constexpr uint8_t kNidSuffix[16] = {
    0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30
};

constexpr char kBase64S[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

std::string ComputeNid(const std::string& symbolName, const std::string& libraryName) {
    if (symbolName.empty())
        throw std::invalid_argument("symbolName is empty");

    std::vector<uint8_t> input;
    input.reserve(symbolName.size() + 16u);
    for (char c : symbolName)
        input.push_back(static_cast<uint8_t>(c));
    for (uint8_t b : kNidSuffix)
        input.push_back(b);

    const auto digest = Sha1(input);
    uint64_t v = 0;

    for (int i = 0; i < 8; i++)
        v = (v << 8) | digest[7 - i];
    char nid[12];
    for (int i = 10; i >= 0; i--) {
        nid[i] = kBase64S[v & 0x3F];
        v >>= 6;
    }
    nid[11] = '\0';

    return std::string(nid, 11);
}

}