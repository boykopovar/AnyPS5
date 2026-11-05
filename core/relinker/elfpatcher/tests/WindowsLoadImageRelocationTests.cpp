#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Elfpatcher::Windows;

namespace {

void RequireThrows(const std::function<void()>& operation, const std::string& message) {
    try {
        operation();
    } catch (const Domain::RelinkerException&) {
        return;
    }
    throw std::runtime_error(message);
}

std::vector<std::uint8_t> ElfFixture() {
    std::vector<std::uint8_t> bytes(64, 0);
    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    Io::WriteU16(bytes, 16, 3);
    Io::WriteU16(bytes, 18, 62);
    return bytes;
}

}

int main() {
    try {
        const auto source = ElfFixture();
        const std::vector<Domain::ProgramHeader> headers = {{1, 5, 0, 0x1000, 0, 0, 0x1000, 0x1000}};
        const WindowsLoadImage image(source, headers, false);

        image.GetRelocatedAddress(0x1000);
        image.GetRelocatedAddress(0x1fff);
        image.GetRelocatedAddress(0x2000);

        RequireThrows([&] { image.GetRelocatedAddress(0x2001); },
            "GetRelocatedAddress accepted an address beyond the mapped image");
        RequireThrows([&] { image.GetRelocatedAddress(0xfff); },
            "GetRelocatedAddress accepted an address before the mapped image");

        std::cout << "Windows load image relocation bounds passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
