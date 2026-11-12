#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace NKWebKit::WtfAbi;
using WebCore::VPCodecConfigurationRecord;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "VP codec check failed at line %u\n", location.line());
        std::abort();
    }
}

std::optional<VPCodecConfigurationRecord> Parse(std::string_view text, bool bytes) {
    if (bytes)
        return _ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({text.data(), static_cast<unsigned>(text.size()), true});
    std::u16string wide(text.begin(), text.end());
    return _ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({wide.data(), static_cast<unsigned>(wide.size()), false});
}

void Check(std::string_view text, bool bytes, std::string_view name, std::vector<unsigned> fields) {
    auto result = Parse(text, bytes);
    Require(result.has_value());
    const auto* impl = result->codecName.impl;
    Require(impl && impl->length == name.size() && impl->refCount == 2);
    for (unsigned i = 0; i < name.size(); ++i)
        Require((impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(impl->data)[i] : static_cast<const char16_t*>(impl->data)[i]) == name[i]);
    Require(result->profile == fields[0]);
    Require(result->level == fields[1]);
    Require(result->bitDepth == fields[2]);
    Require(result->chromaSubsampling == fields[3]);
    Require(result->colorPrimaries == fields[4]);
    Require(result->transferCharacteristics == fields[5]);
    Require(result->matrixCoefficients == fields[6]);
    Require(result->videoFullRangeFlag == fields[7]);
    unsigned char storage[24];
    std::memcpy(storage, &result, sizeof(result));
    Require(storage[16] == 1);
    {
        auto copy = result;
        Require(copy->codecName.impl == impl && impl->refCount == 4);
    }
    Require(impl->refCount == 2);
}

bool In(unsigned value, std::initializer_list<unsigned> values) {
    for (auto candidate : values)
        if (candidate == value)
            return true;
    return false;
}
}  // namespace

int main() {
    try {
        for (bool bytes : {true, false}) {
            for (auto name : {"vp8", "vp9"}) {
                Check(name, bytes, name, {0, 10, 8, 1, 1, 1, 1, 0});
                Check(std::string(name) + ".0", bytes, name, {0, 10, 8, 1, 1, 1, 1, 0});
                Check(std::string(name) + "...", bytes, name, {0, 10, 8, 1, 1, 1, 1, 0});
                Require(!Parse(std::string(name) + ".00", bytes));
                Require(!Parse(std::string(name) + ".0.", bytes));
            }
            for (auto name : {"vp08", "vp09"}) {
                Check(std::string(name) + ".02.31.10", bytes, name, {2, 31, 10, 1, 1, 1, 1, 0});
                Check(std::string(name) + ".03.62.12.03.22.18.00.01", bytes, name, {3, 62, 12, 3, 22, 18, 0, 1});
                Check(std::string(name) + ".00.10.08.00.02.02.02.00", bytes, name, {0, 10, 8, 0, 2, 2, 2, 0});
            }
            Check("..vp09..+0002. 31\v.\t08\r...", bytes, "vp09", {2, 31, 8, 1, 1, 1, 1, 0});
            for (auto text : {"", ".", "vp09", "vp09.0", "vp09.0.10", "vp09..10.8", "vp09.0..8", "vp09.0.10..", "VP09.0.10.8", "vp09.0.10.8.1", "vp09.0.10.8.1.1", "vp09.0.10.8.1.1.1", "vp09.0.10.8.1.1.1.1", "vp09.0.10.8.1.1.1.1.0.0", "vp09.0.10.8.1.1.1.0.0", "vp09.-0.10.8", "vp09.0x.10.8", "vp09.0.1e1.8", "vp09.0.10.8x", "vp09.0.10.\xc2\xa0"})
                Require(!Parse(text, bytes));
            for (unsigned field = 0; field < 8; ++field) {
                for (unsigned value = 0; value <= 256; ++value) {
                    std::vector<unsigned> fields{0, 10, 8, 3, 1, 1, 1, 0};
                    fields[field] = value;
                    bool valid = field == 0 ? value <= 3
                        : field == 1 ? In(value, {10, 11, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62})
                        : field == 2 ? In(value, {8, 10, 12})
                        : field == 3 ? value <= 3
                        : field == 4 ? In(value, {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 22})
                        : field == 5 ? value >= 1 && value <= 18 && value != 3
                        : field == 6 ? value <= 14 && value != 3 : value <= 1;
                    std::string text = "vp09";
                    for (auto number : fields)
                        text += "." + std::to_string(number);
                    if (valid)
                        Check(text, bytes, "vp09", fields);
                    else
                        Require(!Parse(text, bytes));
                }
            }
            for (unsigned chroma = 0; chroma < 4; ++chroma)
                Require(Parse("vp09.0.10.8." + std::to_string(chroma) + ".1.1.0.0", bytes).has_value() == (chroma == 3));
            Check("vp09.00000000000000000000000000000002.31.08", bytes, "vp09", {2, 31, 8, 1, 1, 1, 1, 0});
            auto result = Parse("bad", bytes);
            unsigned char storage[24];
            std::memcpy(storage, &result, sizeof(result));
            Require(!result && storage[16] == 0);
        }
        Require(!_ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({nullptr, 0, true}));
        const char16_t nonASCII[] = u"vp09.\u0100.10.8";
        Require(!_ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({nonASCII, 12, false}));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "VP codec exception: %s\n", error.what());
        return 1;
    }
}
