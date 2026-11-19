#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace NKWebKit::Lifted {

constexpr std::uint64_t FirstMappedAddress = 0x10000;

inline void CheckAddress(std::uint64_t address) {
    if (address < FirstMappedAddress) throw std::invalid_argument("libSceNKWebKit: access to an unmapped low address");
}

template<typename T> T Read(std::uint64_t address) {
    CheckAddress(address);
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return value;
}

template<typename T> void Write(std::uint64_t address, T value) {
    CheckAddress(address);
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
}

template<std::size_t N> struct Bytes {
    unsigned char data[N] = {};
};

template<std::size_t N> Bytes<N> ReadBytes(std::uint64_t address) {
    CheckAddress(address);
    Bytes<N> value;
    std::memcpy(value.data, reinterpret_cast<const void*>(address), N);
    return value;
}

template<std::size_t N> void WriteBytes(std::uint64_t address, const Bytes<N>& value) {
    CheckAddress(address);
    std::memcpy(reinterpret_cast<void*>(address), value.data, N);
}

inline std::uint32_t ExtractBits(std::uint32_t source, std::uint32_t control) {
    const std::uint32_t start = control & 0xff;
    const std::uint32_t length = (control >> 8) & 0xff;
    if (start >= 32) return 0;
    const std::uint64_t shifted = static_cast<std::uint64_t>(source) >> start;
    return length >= 32 ? static_cast<std::uint32_t>(shifted) : static_cast<std::uint32_t>(shifted & ((std::uint64_t{1} << length) - 1));
}

template<typename R> R Call(void (*target)(), std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    using Function = R (APS5_VABI*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
    return reinterpret_cast<Function>(target)(a0, a1, a2, a3, a4, a5);
}

}  // namespace NKWebKit::Lifted
