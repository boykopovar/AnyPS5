#ifndef PKG_PACKAGEERROR_HPP
#define PKG_PACKAGEERROR_HPP

#include <cstdint>
#include <stdexcept>
#include <string>

namespace Pkg {

struct PackageError : std::runtime_error {
    explicit PackageError(const std::string& message) : std::runtime_error(message) {}

    PackageError(const std::string& message, const std::uint64_t offset) : std::runtime_error(withOffset(message, offset)) {}

private:
    static std::string withOffset(const std::string& message, std::uint64_t offset) {
        std::string digits;
        do {
            digits.insert(digits.begin(), "0123456789abcdef"[offset % 16]);
            offset /= 16;
        } while (offset != 0);
        return message + " (0x" + digits + ")";
    }
};

}

#endif
