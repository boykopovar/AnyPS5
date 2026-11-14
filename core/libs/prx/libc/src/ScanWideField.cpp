#include "prx/libc/include/ScanWideField.hpp"


namespace LibcDetail {

bool InWideScanset(const std::string& specifier, unsigned char byte) {
    std::size_t index = 1;
    const bool negated = specifier[index] == '^';
    if (negated) ++index;
    const std::size_t end = specifier.size() - 1;
    bool member = false;
    while (index < end && !member) {
        const auto first = static_cast<unsigned char>(specifier[index]);
        if (end - index >= 3 && specifier[index + 1] == '-') {
            member = byte >= first && byte <= static_cast<unsigned char>(specifier[index + 2]);
            index += 3;
        } else {
            member = byte == first;
            ++index;
        }
    }
    return member != negated;
}

}
