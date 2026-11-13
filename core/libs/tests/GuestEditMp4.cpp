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

    constexpr int invalidParam = static_cast<int>(0x814B0001u);
    constexpr int nullArgument = static_cast<int>(0x814B0002u);
    constexpr int invalidHandle = static_cast<int>(0x814B0003u);
    std::array<std::uint8_t, 0x40> block{};
    const void* argument = block.data();
    const std::array<const void*, 4> handles{nullptr, block.data(), reinterpret_cast<const void*>(std::uintptr_t{0x10}),
                                             reinterpret_cast<const void*>(~std::uintptr_t{0})};
    for (const void* handle : handles) {
        Require(sceEditMp4Abort(handle) == invalidHandle);
        Require(sceEditMp4Terminate(handle) == invalidHandle);
        Require(sceEditMp4SetFinishCallback(handle) == invalidHandle);
        Require(sceEditMp4Edit(handle, argument, argument, argument) == invalidHandle);
        Require(sceEditMp4Unite(handle, argument, argument, argument) == invalidHandle);
        Require(sceEditMp4GetStatus(handle, block.data()) == invalidHandle);
        if (handle == nullptr) {
            Require(sceEditMp4Edit(handle, nullptr, nullptr, nullptr) == invalidHandle);
            Require(sceEditMp4Unite(handle, nullptr, nullptr, nullptr) == invalidHandle);
            Require(sceEditMp4GetStatus(handle, nullptr) == invalidHandle);
            continue;
        }
        Require(sceEditMp4GetStatus(handle, nullptr) == nullArgument);
        for (std::size_t missing = 0; missing < 3; ++missing) {
            std::array<const void*, 3> arguments{argument, argument, argument};
            arguments[missing] = nullptr;
            Require(sceEditMp4Edit(handle, arguments[0], arguments[1], arguments[2]) == nullArgument);
            Require(sceEditMp4Unite(handle, arguments[0], arguments[1], arguments[2]) == nullArgument);
        }
    }

    std::array<std::uint8_t, 0x28> param{};
    sceEditMp4InitializeParam(param.data());
    std::uint64_t handleOut = 0x1234;
    Require(sceEditMp4Initialize(nullptr, &handleOut) == nullArgument);
    Require(sceEditMp4Initialize(param.data(), nullptr) == nullArgument);
    Require(sceEditMp4Initialize(nullptr, nullptr) == nullArgument);
    Require(sceEditMp4Initialize(param.data(), &handleOut) == invalidParam);
    Require(handleOut == 0x1234);
    return 0;
}
