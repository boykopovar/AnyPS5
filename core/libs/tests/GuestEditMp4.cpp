#include "prx/libSceEditMp4/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void VerifyParamFields(const std::uint8_t* data) {
    for (std::size_t index = 0; index < 0x10; ++index) Require(data[index] == 0);
    std::uint64_t at10 = 0;
    std::uint32_t at18 = 0;
    std::memcpy(&at10, data + 0x10, sizeof(at10));
    std::memcpy(&at18, data + 0x18, sizeof(at18));
    Require(at10 == 0x3f);
    Require(at18 == 700);
}

std::array<std::uint8_t, 0x28> Initialized(std::uint8_t fill) {
    std::array<std::uint8_t, 0x28> bytes{};
    bytes.fill(fill);
    errno = EINVAL;
    sceEditMp4InitializeParam(bytes.data());
    Require(errno == EINVAL);
    return bytes;
}

}

int main() {
    for (const std::uint8_t fill : {std::uint8_t{0x00}, std::uint8_t{0xaa}}) {
        const auto bytes = Initialized(fill);
        VerifyParamFields(bytes.data());
        for (std::size_t index = 0x1c; index < bytes.size(); ++index) Require(bytes[index] == fill);
    }

    std::array<std::uint8_t, 0x30> unaligned{};
    unaligned.fill(0xcc);
    errno = EIO;
    sceEditMp4InitializeParam(unaligned.data() + 3);
    Require(errno == EIO);
    for (std::size_t index = 0; index < 3; ++index) Require(unaligned[index] == 0xcc);
    VerifyParamFields(unaligned.data() + 3);
    for (std::size_t index = 3 + 0x1c; index < unaligned.size(); ++index) Require(unaligned[index] == 0xcc);

    errno = EIO;
    sceEditMp4InitializeParam(nullptr);
    Require(errno == EIO);
    sceEditMp4InitializeParam(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10)));
    Require(errno == EIO);
    sceEditMp4InitializeParam(reinterpret_cast<void*>(~std::uintptr_t{0}));
    Require(errno == EIO);

    Require(sceEditMp4QueryMemSize() == 0x387d30);
    Require(sceEditMp4QueryMemSize() == 0x387d30);
    Require(errno == EIO);
    return 0;
}
