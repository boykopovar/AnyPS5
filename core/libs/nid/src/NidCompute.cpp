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

    uint8_t reversed[8];
    for (int i = 0; i < 8; i++)
        reversed[i] = digest[7 - i];

    char nid[12];
    int nidIndex = 0;
    for (int i = 0; i < 6; i += 3) {
        const uint32_t triple = (static_cast<uint32_t>(reversed[i]) << 16) |
                                 (static_cast<uint32_t>(reversed[i + 1]) << 8) |
                                 static_cast<uint32_t>(reversed[i + 2]);
        nid[nidIndex++] = kBase64S[(triple >> 18) & 0x3F];
        nid[nidIndex++] = kBase64S[(triple >> 12) & 0x3F];
        nid[nidIndex++] = kBase64S[(triple >> 6) & 0x3F];
        nid[nidIndex++] = kBase64S[triple & 0x3F];
    }
    const uint32_t tail = (static_cast<uint32_t>(reversed[6]) << 16) |
                           (static_cast<uint32_t>(reversed[7]) << 8);
    nid[nidIndex++] = kBase64S[(tail >> 18) & 0x3F];
    nid[nidIndex++] = kBase64S[(tail >> 12) & 0x3F];
    nid[nidIndex++] = kBase64S[(tail >> 6) & 0x3F];
    nid[11] = '\0';

    return std::string(nid, 11);
}

}
