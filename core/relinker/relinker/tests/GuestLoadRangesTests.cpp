#include "ElfFixture.hpp"
#include <relinker/guest/GuestImage.hpp>
#include <iostream>
#include <string>

using namespace RelinkerTests;

static Bytes Fixture(const std::uint64_t emptyAddress, const bool emptyFirst) {
    const std::uint8_t code[] = {0xc3};
    auto bytes = MakeModule(code);
    WriteProgramHeader(bytes, 3, 1, 4, emptyAddress % 0x1000, emptyAddress, 0, 1);
    if (emptyFirst)
        for (std::size_t index = 0; index < 56; ++index)
            std::swap(bytes[64 + index], bytes[64 + 3 * 56 + index]);
    return bytes;
}

static void Accept(const Bytes& bytes) {
    const auto image = Relinker::GuestImageReader().Read("load-ranges.prx", bytes);
    if (image.Bytes != bytes || image.Headers.size() != 3 + kSpareProgramHeaders)
        throw std::runtime_error("Guest bytes or program headers were changed");
}

static void Reject(const Bytes& bytes, const std::string& expected) {
    try {
        Relinker::GuestImageReader().Read("load-ranges.prx", bytes);
    } catch (const Domain::RelinkerException& error) {
        if (std::string(error.what()).find(expected) != std::string::npos) return;
        throw std::runtime_error("Expected " + expected + ", got " + error.what());
    }
    throw std::runtime_error("Accepted invalid input: " + expected);
}

int main() {
    unsigned failures = 0;
    const auto run = [&](const std::string& name, const auto& check) {
        try {
            check();
        } catch (const std::exception& error) {
            std::cerr << name << ": " << error.what() << '\n';
            ++failures;
        }
    };
    for (const bool first : {false, true}) {
        const auto order = first ? "empty-first" : "empty-last";
        for (const std::uint64_t address : {0ull, 1ull, 0xfffull, 0x1000ull, 0x2000ull, 0x2200ull, 0x2400ull, 0xffffffffffffffffull})
            run(std::string(order) + "-" + std::to_string(address), [&] { Accept(Fixture(address, first)); });

        const std::size_t empty = 64 + (first ? 0 : 3) * 56;
        run(std::string(order) + "-file-size", [&] {
            auto bytes = Fixture(0, first);
            Write<std::uint64_t>(bytes, empty + 32, 1);
            Reject(bytes, "PT_LOAD file size exceeds memory size");
        });
        run(std::string(order) + "-file-offset", [&] {
            auto bytes = Fixture(0, first);
            Write<std::uint64_t>(bytes, empty + 8, bytes.size() + 1);
            Reject(bytes, "Segment offset out of bounds");
        });
        run(std::string(order) + "-alignment", [&] {
            auto bytes = Fixture(0, first);
            Write<std::uint64_t>(bytes, empty + 48, 3);
            Reject(bytes, "Invalid load segment alignment");
        });
        run(std::string(order) + "-alignment-congruence", [&] {
            auto bytes = Fixture(1, first);
            Write<std::uint64_t>(bytes, empty + 8, 0);
            Write<std::uint64_t>(bytes, empty + 48, 8);
            Reject(bytes, "Invalid load segment alignment");
        });
        run(std::string(order) + "-flags", [&] {
            auto bytes = Fixture(0, first);
            Write<std::uint32_t>(bytes, empty + 4, 8);
            Reject(bytes, "Invalid load segment");
        });
        run(std::string(order) + "-address-overflow", [&] {
            auto bytes = Fixture(std::numeric_limits<std::uint64_t>::max(), first);
            Write<std::uint64_t>(bytes, empty + 40, 1);
            Reject(bytes, "Invalid load segment");
        });
        for (const std::uint64_t address : {0ull, 1ull, 0xfffull})
            run(std::string(order) + "-overlap-" + std::to_string(address), [&] {
                auto bytes = Fixture(0, first);
                WriteProgramHeader(bytes, 4, 1, 6, 0, address, 0, 1);
                Write<std::uint64_t>(bytes, 64 + 4 * 56 + 40, 1);
                Reject(bytes, "Overlapping load segments");
            });
    }
    run("only-empty-loads", [&] {
        auto bytes = Fixture(0, false);
        for (const auto index : {0, 1}) {
            Write<std::uint64_t>(bytes, 64 + index * 56 + 32, 0);
            Write<std::uint64_t>(bytes, 64 + index * 56 + 40, 0);
        }
        Reject(bytes, "Missing or invalid ELF load/dynamic segments");
    });
    if (failures != 0) return 1;
    std::cout << "Guest load range tests passed\n";
}
