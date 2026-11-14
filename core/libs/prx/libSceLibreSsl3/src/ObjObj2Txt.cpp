#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

using Number = std::vector<std::uint64_t>;

void MultiplyAdd(Number& number, unsigned add) {
    std::uint64_t carry = add;
    for (auto& limb : number) {
        const auto product = static_cast<unsigned __int128>(limb) * 128 + carry;
        limb = static_cast<std::uint64_t>(product);
        carry = static_cast<std::uint64_t>(product >> 64);
    }
    if (carry) number.push_back(carry);
}

void SubtractSmall(Number& number, std::uint64_t value) {
    std::uint64_t borrow = value;
    for (auto& limb : number) {
        const std::uint64_t previous = limb;
        limb -= borrow;
        borrow = previous < borrow ? 1 : 0;
        if (!borrow) break;
    }
    while (!number.empty() && number.back() == 0) number.pop_back();
}

std::string Decimal(Number number) {
    if (number.empty()) return "0";
    std::string digits;
    while (!number.empty()) {
        std::uint64_t remainder = 0;
        for (std::size_t index = number.size(); index-- > 0;) {
            const auto value = (static_cast<unsigned __int128>(remainder) << 64) | number[index];
            number[index] = static_cast<std::uint64_t>(value / 1000000000);
            remainder = static_cast<std::uint64_t>(value % 1000000000);
        }
        while (!number.empty() && number.back() == 0) number.pop_back();
        for (int digit = 0; digit < 9; ++digit) {
            digits.push_back(static_cast<char>('0' + remainder % 10));
            remainder /= 10;
        }
    }
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    return std::string(digits.rbegin(), digits.rend());
}

}

extern "C" int APS5_VABI OBJ_obj2txt_nid_postfix(char* buffer, int bufferLength, const Asn1Object_nid_no_patch* object, int noName) {
    if (buffer && bufferLength > 0) buffer[0] = '\0';
    if (!object || !object->data) return 0;
    std::string text;
    if (!noName && (object->longName || object->shortName)) {
        text = object->longName ? object->longName : object->shortName;
    } else if (object->length > 0) {
        Number number;
        bool first = true;
        bool continued = false;
        for (int index = 0; index < object->length; ++index) {
            const unsigned byte = object->data[index];
            continued = (byte & 0x80) != 0;
            MultiplyAdd(number, byte & 0x7f);
            if (continued) continue;
            if (first) {
                unsigned leading = 0;
                if (number.empty())
                    leading = 0;
                else if (number.size() == 1)
                    leading = number[0] < 40 ? 0 : number[0] < 80 ? 1 : 2;
                else
                    leading = 2;
                text += std::to_string(leading);
                text += '.';
                SubtractSmall(number, leading * 40);
                first = false;
            }

            if (!text.empty() && text.back() != '.') text += '.';
            text += Decimal(number);
            number.clear();
        }
        if (continued) return 0;
    }
    if (buffer && bufferLength > 0) {
        const auto count = text.size() < static_cast<std::size_t>(bufferLength - 1) ? text.size() : static_cast<std::size_t>(bufferLength - 1);
        std::memcpy(buffer, text.data(), count);
        buffer[count] = '\0';
    }
    const auto length = text.size();
    return length > static_cast<std::size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : static_cast<int>(length);
}
