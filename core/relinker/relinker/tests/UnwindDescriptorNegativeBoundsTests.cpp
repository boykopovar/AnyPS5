#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <io/BufferUtils.hpp>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
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

}

int main() {
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
        {"CIE record size under 8 rejected", [] {
            for (const std::uint32_t size : {0u, 1u, 4u, 7u}) {
                auto bytes = ValidUnwindFixture();
                Io::WriteU32(bytes, 64, size);
                const auto headers = ValidHeaders(bytes);
                AssertThrowsMatching([&] {
                    Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
                }, "unsupported CIE record size");
            }
        }},
        {"CIE 64-bit length 0xFFFFFFFF rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU32(bytes, 64, 0xFFFFFFFF);
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "unsupported CIE record size");
        }},
        {"CIE invalid non-zero identifier rejected", [] {
            for (const std::uint32_t cieId : {1u, 0x10u, 0xFFFFFFFFu}) {
                auto bytes = ValidUnwindFixture();
                Io::WriteU32(bytes, 68, cieId);
                const auto headers = ValidHeaders(bytes);
                AssertThrowsMatching([&] {
                    Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
                }, "invalid CIE identifier");
            }
        }},
        {"CIE unsupported version rejected", [] {
            for (const std::uint8_t version : {0, 2, 5, 10}) {
                auto bytes = ValidUnwindFixture();
                bytes[72] = version;
                const auto headers = ValidHeaders(bytes);
                AssertThrowsMatching([&] {
                    Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
                }, "unsupported CIE version");
            }
        }},
        {"CIE version 4 invalid address or segment size rejected", [] {
            auto bytes = ValidUnwindFixture();
            bytes[72] = 4;
            bytes[73] = 0;
            bytes[74] = 4;
            bytes[75] = 0;
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "unsupported CIE address size");
        }},
        {"CIE non-z augmentation prefix rejected", [] {
            auto bytes = ValidUnwindFixture();
            bytes[73] = 'x';
            bytes[74] = 0;
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "unsupported CIE augmentation");
        }},
        {"CIE unknown augmentation field character rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU32(bytes, 64, 20);
            bytes[73] = 'z';
            bytes[74] = 'Q';
            bytes[75] = 0;
            bytes[76] = 1;
            bytes[77] = 0x78;
            bytes[78] = 16;
            bytes[79] = 1;
            bytes[80] = 0;
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "unknown CIE augmentation field");
        }},
        {"FDE record size under 8 rejected", [] {
            for (const std::uint32_t size : {0u, 1u, 5u, 7u}) {
                auto bytes = ValidUnwindFixture();
                Io::WriteU32(bytes, 96, size);
                const auto headers = ValidHeaders(bytes);
                AssertThrowsMatching([&] {
                    Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
                }, "unsupported FDE record size");
            }
        }},
        {"FDE zero CIE offset rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU32(bytes, 100, 0);
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "invalid FDE CIE offset");
        }},
        {"FDE out of bounds CIE offset rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU32(bytes, 100, 0xFFFF);
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "invalid FDE CIE offset");
        }},
        {"FDE inconsistent function start address rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU64(bytes, 104, 0x3000);
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "inconsistent FDE function range");
        }},
        {"FDE function range integer overflow rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU64(bytes, 16, std::numeric_limits<std::uint64_t>::max() - 16);
            Io::WriteU64(bytes, 104, std::numeric_limits<std::uint64_t>::max() - 16);
            Io::WriteU64(bytes, 112, 32);
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "inconsistent FDE function range");
        }},
        {"unterminated LEB128 sequence rejected", [] {
            auto bytes = ValidUnwindFixture();
            for (std::size_t i = 74; i < 74 + 10; ++i) bytes[i] = 0x80;
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "unterminated LEB128 value");
        }},
        {"overflowing LEB128 sequence rejected", [] {
            auto bytes = ValidUnwindFixture();
            for (std::size_t i = 74; i < 74 + 9; ++i) bytes[i] = 0x80;
            bytes[74 + 9] = 0x40;
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "overflowing LEB128 value");
        }},
        {"unwind pointer relative displacement overflow rejected", [] {
            auto bytes = ValidUnwindFixture();
            bytes[3] = 0x1B;
            Io::WriteU32(bytes, 16, 0x7FFFFFFF);
            Io::WriteU32(bytes, 20, 0x1060 - 0x1014);
            auto headers = ValidHeaders(bytes);
            headers[0].MappedAddress = std::numeric_limits<std::uint64_t>::max() - 0x1000;
            headers[1].MappedAddress = headers[0].MappedAddress;
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "unwind pointer address overflowed");
        }},
        {"unwind pointer negative displacement underflow rejected", [] {
            auto bytes = ValidUnwindFixture();
            bytes[3] = 0x1B;
            Io::WriteU32(bytes, 16, static_cast<std::uint32_t>(-0x2000));
            Io::WriteU32(bytes, 20, 0x1060 - 0x1014);
            auto headers = ValidHeaders(bytes);
            headers[0].MappedAddress = 0x1000;
            headers[1].MappedAddress = 0x1000;
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "unwind pointer address overflowed");
        }},
        {"unwind range exceeding mapped file segments rejected", [] {
            auto bytes = ValidUnwindFixture();
            Io::WriteU32(bytes, 96, 200);
            const auto headers = ValidHeaders(bytes);
            AssertThrowsMatching([&] {
                Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
            }, "unwind range is not file-backed");
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

    std::size_t passed = 0;
    for (const auto& [name, test] : testCases) {
        try {
            test();
            ++passed;
        } catch (const std::exception& error) {
            std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
            return 1;
        }
    }

    std::cout << "All " << passed << " unwind descriptor negative boundary tests passed.\n";
    return 0;
}
