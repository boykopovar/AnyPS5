#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <io/BufferUtils.hpp>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void Require(const bool condition) {
    if (!condition) {
        throw std::runtime_error("Negative boundary condition failed");
    }
}

Bytes ValidUnwindFixture() {
    Bytes bytes(256, 0);
    bytes[0] = 1;
    bytes[1] = 0;
    bytes[2] = 3;
    bytes[3] = 0;

    Io::WriteU64(bytes, 4, 0x1040);
    Io::WriteU32(bytes, 12, 1);
    Io::WriteU64(bytes, 16, 0x2000);
    Io::WriteU64(bytes, 24, 0x1060);

    Io::WriteU32(bytes, 64, 16);
    Io::WriteU32(bytes, 68, 0);
    bytes[72] = 1;
    bytes[73] = 0;
    bytes[74] = 1;
    bytes[75] = 0x78;
    bytes[76] = 16;

    Io::WriteU32(bytes, 96, 24);
    Io::WriteU32(bytes, 100, 36);
    Io::WriteU64(bytes, 104, 0x2000);
    Io::WriteU64(bytes, 112, 0x40);

    return bytes;
}

std::vector<Domain::ProgramHeader> ValidHeaders(const Bytes& bytes) {
    Domain::ProgramHeader load{};
    load.Type = 1;
    load.MappedAddress = 0x1000;
    load.FileSize = bytes.size();
    load.MemorySize = bytes.size();

    Domain::ProgramHeader eh{};
    eh.Type = 0x6474E550;
    eh.MappedAddress = 0x1000;
    eh.FileSize = 32;
    eh.MemorySize = 32;

    return {load, eh};
}

void AssertThrowsMatching(const std::function<void()>& action, const std::string& expectedSubstring) {
    try {
        action();
    } catch (const Domain::RelinkerException& error) {
        if (!expectedSubstring.empty() && std::string(error.what()).find(expectedSubstring) == std::string::npos) {
            throw std::runtime_error("Exception message mismatch: expected '" + expectedSubstring + "' but got '" + error.what() + "'");
        }
        return;
    } catch (const std::exception& error) {
        throw std::runtime_error("Unexpected exception type: " + std::string(error.what()));
    }
    throw std::runtime_error("Expected exception was not thrown");
}

} // namespace

int main() {
    const auto seedBytes = ValidUnwindFixture();
    const auto seedHeaders = ValidHeaders(seedBytes);
    const auto validRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(seedBytes, seedHeaders, {}, {});
    Require(validRegions.size() == 1);
    Require(validRegions[0].Begin == 0x2000 && validRegions[0].End == 0x2040);

    const std::vector<std::pair<std::string, std::function<void()>>> testCases = {
        {"eh_frame_hdr segment smaller than 4 bytes rejected", [] {
            const auto bytes = ValidUnwindFixture();
            auto headers = ValidHeaders(bytes);
            headers[1].FileSize = 3;
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "EH frame header exceeds segment");
        }},
        {"eh_frame_hdr unsupported version rejected", [] {
            for (const std::uint8_t version : {0, 2, 3, 255}) {
                auto bytes = ValidUnwindFixture();
                bytes[0] = version;
                const auto headers = ValidHeaders(bytes);
                AssertThrowsMatching([&] {
                    Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
                }, "unsupported EH frame header");
            }
        }},
        {"eh_frame_hdr omitted encodings rejected", [] {
            for (int offset = 1; offset <= 3; ++offset) {
                auto bytes = ValidUnwindFixture();
                bytes[offset] = 0xFF;
                const auto headers = ValidHeaders(bytes);
                AssertThrowsMatching([&] {
                    Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
                }, "unsupported EH frame header");
            }
        }},
        {"eh_frame_hdr invalid frame count rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU32(bytes, 12, 1000);
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "invalid EH frame count");
        }},
        {"eh_frame_hdr search table exceeding segment bounds rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU32(bytes, 12, 3);
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "EH frame table exceeds segment");
        }},
        {"eh_frame_hdr unsupported pointer format in table encoding rejected", [] {
            for (const std::uint8_t format : {0x05, 0x06, 0x07, 0x08, 0x0D, 0x0E, 0x0F}) {
                auto bytes = ValidUnwindFixture();
                bytes[3] = format;
                const auto headers = ValidHeaders(bytes);
                AssertThrowsMatching([&] {
                    Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
                }, "unsupported unwind pointer format");
            }
        }},
        {"eh_frame_hdr unsupported pointer base in table encoding rejected", [] {
            for (const std::uint8_t base : {0x20, 0x50, 0x60, 0x70}) {
                auto bytes = ValidUnwindFixture();
                bytes[3] = base;
                const auto headers = ValidHeaders(bytes);
                AssertThrowsMatching([&] {
                    Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
                }, "unsupported unwind pointer base");
            }
        }},
        {"duplicate search table entries handled idempotently", [] {
            auto bytes = ValidUnwindFixture();
            bytes.resize(300, 0);
            Io::WriteU32(bytes, 12, 2);
            Io::WriteU64(bytes, 32, 0x2000);
            Io::WriteU64(bytes, 40, 0x1060);
            auto headers = ValidHeaders(bytes);
            headers[0].FileSize = bytes.size();
            headers[0].MemorySize = bytes.size();
            headers[1].FileSize = 48;
            headers[1].MemorySize = 48;
            const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            Require(regions.size() == 1);
            Require(regions[0].Begin == 0x2000 && regions[0].End == 0x2040);
        }}
    };

    for (const auto& [name, test] : testCases) {
        test();
    }

    return 0;
}
