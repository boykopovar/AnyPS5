#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

extern "C" {
std::int32_t APS5_VABI sceZlibInitialize(const void*, std::uint64_t);
std::int32_t APS5_VABI sceZlibFinalize();
std::int32_t APS5_VABI sceZlibInflate(const void*, std::uint32_t, void*, std::uint32_t, std::uint64_t*);
std::int32_t APS5_VABI sceZlibWaitForDone(std::uint64_t*, std::uint32_t*);
std::int32_t APS5_VABI sceZlibGetResult(std::uint64_t, std::uint32_t*, std::int32_t*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::int32_t notInitialized = static_cast<std::int32_t>(0x81120032);
constexpr std::int32_t alreadyInitialized = static_cast<std::int32_t>(0x81120033);
constexpr std::int32_t noSpace = static_cast<std::int32_t>(0x8112001C);
constexpr std::int32_t fatal = static_cast<std::int32_t>(0x811200FF);
constexpr std::uint8_t guardByte = 0xA5;
constexpr std::size_t guardSize = 2048;
constexpr std::uint32_t maxOutput = 65536;

constexpr std::array<std::uint8_t, 77> compressed{0x78, 0x9c, 0xed, 0xca, 0xb1, 0xd, 0x80, 0x20, 0x10, 0x40, 0xd1, 0x9e, 0x29, 0x6e, 0x2, 0x3a, 0x6, 0x60, 0x3, 0x12, 0x26, 0x10, 0x30, 0x91, 0x88, 0x47, 0x71, 0xa1, 0xd0, 0xe9, 0x1d, 0x82, 0xf6, 0xbf, 0xfa, 0x45, 0x7d, 0x53, 0xe, 0xf2, 0x8d, 0x5e, 0xa4, 0x5e, 0x67, 0xbd, 0x6d, 0x3d, 0x72, 0x68, 0x93, 0x32, 0x97, 0x36, 0xf3, 0x2e, 0x12, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0xec, 0x85, 0x1f, 0xf5, 0xf3, 0xbd, 0x4c};
constexpr char text[] = "AnyPS5 zlib checksum and bounds.\n";
constexpr std::uint32_t textLength = sizeof(text) - 1;
constexpr std::uint32_t expectedLength = textLength * 128;

struct alignas(2048) GuardedOutput {
    std::array<std::uint8_t, guardSize + maxOutput + guardSize> storage{};

    std::uint8_t* Output() {
        return storage.data() + guardSize;
    }
};

class ZlibService {
public:
    explicit ZlibService(const void* buffer = nullptr, std::uint64_t length = 0) {
        RequireEqual(sceZlibInitialize(buffer, length), 0, "initialize");
    }

    ~ZlibService() {
        try {
            std::uint64_t pending = 0;
            if (sceZlibWaitForDone(&pending, nullptr) == 0) {
                std::uint32_t produced = 0;
                std::int32_t status = 0;
                sceZlibGetResult(pending, &produced, &status);
            }
        } catch (const std::exception&) {
        }
        try {
            sceZlibFinalize();
        } catch (const std::exception&) {
        }
    }

    ZlibService(const ZlibService&) = delete;
    ZlibService& operator=(const ZlibService&) = delete;
};

struct DecodeResult {
    std::uint64_t id = 0;
    std::uint32_t produced = 0;
};

DecodeResult Decode(GuardedOutput& output, const void* input, std::uint32_t inputLength, std::uint32_t capacity, std::int32_t expectedStatus) {
    DecodeResult decoded;
    std::int32_t status = 0;
    output.storage.fill(guardByte);
    RequireEqual(sceZlibInflate(input, inputLength, output.Output(), capacity, &decoded.id), 0, "inflate");
    RequireThrows<std::runtime_error>([&] { sceZlibInflate(input, inputLength, output.Output(), capacity, &decoded.id); }, "second inflate while a result is outstanding");
    RequireThrows<std::runtime_error>([] { sceZlibFinalize(); }, "finalize while a result is outstanding");
    std::uint32_t timeout = 0;
    RequireThrows<std::runtime_error>([&] { sceZlibWaitForDone(&decoded.id, &timeout); }, "wait with a timeout");
    std::uint64_t completed = 0;
    RequireEqual(sceZlibWaitForDone(&completed, nullptr), 0, "wait for done");
    RequireEqual(completed, decoded.id, "completed request id");
    RequireThrows<std::runtime_error>([&] { sceZlibGetResult(decoded.id + 1, &decoded.produced, &status); }, "result of an unknown request");
    RequireEqual(sceZlibGetResult(decoded.id, &decoded.produced, &status), 0, "get result");
    RequireEqual(status, expectedStatus, "inflate status");
    Require(decoded.produced <= capacity, "produced " + std::to_string(decoded.produced) + " bytes into a " + std::to_string(capacity) + " byte buffer");
    RequireThrows<std::runtime_error>([&] { sceZlibGetResult(decoded.id, &decoded.produced, &status); }, "repeated result retrieval");
    Require(std::all_of(output.storage.begin(), output.storage.begin() + guardSize, [](auto byte) { return byte == guardByte; }), "bytes before the output were modified");
    Require(std::all_of(output.storage.begin() + guardSize + capacity, output.storage.end(), [](auto byte) { return byte == guardByte; }), "bytes after the output capacity were modified");
    return decoded;
}

void RequireRepeatedText(GuardedOutput& output, std::uint32_t produced) {
    RequireEqual(produced, expectedLength, "produced length");
    for (std::uint32_t offset = 0; offset < produced; offset += textLength) {
        Require(std::memcmp(output.Output() + offset, text, textLength) == 0, "decoded text mismatch at offset " + std::to_string(offset));
    }
}

const Case uninitialized{"Calls_BeforeInitialize_ReturnNotInitialized", [] {
    RequireEqual(sceZlibFinalize(), notInitialized, "finalize");
    RequireEqual(sceZlibInflate(nullptr, 0, nullptr, 0, nullptr), notInitialized, "inflate");
}};

const Case invalidWorkBuffer{"Initialize_NullBufferWithLength_Throws", [] {
    RequireThrows<std::runtime_error>([] { sceZlibInitialize(nullptr, 1); }, "initialize with a null buffer and length 1");
}};

const Case initializeTwice{"Initialize_AlreadyInitialized_ReturnsAlreadyInitialized", [] {
    const ZlibService service;
    RequireEqual(sceZlibInitialize(nullptr, 0), alreadyInitialized, "second initialize");
}};

const Case invalidInflate{"Inflate_InvalidOutputOrInput_Throws", [] {
    const ZlibService service;
    auto output = std::make_unique<GuardedOutput>();
    std::uint64_t id = 0;
    RequireThrows<std::runtime_error>([&] { sceZlibInflate(compressed.data(), compressed.size(), output->Output() + 1, expectedLength, &id); }, "misaligned output");
    RequireThrows<std::runtime_error>([&] { sceZlibInflate(compressed.data(), compressed.size(), output->Output(), maxOutput + 1, &id); }, "output larger than 64 KiB");
    RequireThrows<std::runtime_error>([&] { sceZlibInflate(nullptr, compressed.size(), output->Output(), expectedLength, &id); }, "null input");
}};

const Case validStream{"Inflate_ValidStream_DecodesRepeatedText", [] {
    const ZlibService service;
    auto output = std::make_unique<GuardedOutput>();
    const auto decoded = Decode(*output, compressed.data(), compressed.size(), expectedLength, 0);
    RequireRepeatedText(*output, decoded.produced);
}};

const Case increasingIds{"Inflate_SuccessiveRequests_ReturnIncreasingIds", [] {
    const ZlibService service;
    auto output = std::make_unique<GuardedOutput>();
    const auto first = Decode(*output, compressed.data(), compressed.size(), expectedLength, 0);
    const auto second = Decode(*output, compressed.data(), compressed.size(), expectedLength, 0);
    Require(first.id > 0, "the first request id is zero");
    Require(second.id > first.id, "request ids did not increase");
}};

const Case shortOutput{"Inflate_OutputOneByteShort_ReportsNoSpace", [] {
    const ZlibService service;
    auto output = std::make_unique<GuardedOutput>();
    Decode(*output, compressed.data(), compressed.size(), expectedLength - 1, noSpace);
}};

const Case corruptChecksum{"Inflate_CorruptChecksum_ReportsFatal", [] {
    const ZlibService service;
    auto output = std::make_unique<GuardedOutput>();
    auto corrupt = compressed;
    corrupt.back() ^= 1;
    Decode(*output, corrupt.data(), corrupt.size(), expectedLength, fatal);
}};

const Case truncatedInput{"Inflate_TruncatedInput_ReportsFatal", [] {
    const ZlibService service;
    auto output = std::make_unique<GuardedOutput>();
    Decode(*output, compressed.data(), compressed.size() - 1, maxOutput, fatal);
}};

const Case resultAfterFinalize{"GetResult_AfterFinalize_ReturnsNotInitialized", [] {
    const ZlibService service;
    auto output = std::make_unique<GuardedOutput>();
    const auto decoded = Decode(*output, compressed.data(), compressed.size(), expectedLength, 0);
    RequireEqual(sceZlibFinalize(), 0, "finalize");
    std::uint32_t produced = 0;
    std::int32_t status = 0;
    RequireEqual(sceZlibGetResult(decoded.id, &produced, &status), notInitialized, "result after finalize");
}};

const Case scratchBuffer{"Initialize_WithScratchBuffer_DecodesFullCapacity", [] {
    auto scratch = std::make_unique<std::array<std::uint8_t, 65536>>();
    auto output = std::make_unique<GuardedOutput>();
    const ZlibService service(scratch->data(), scratch->size());
    Decode(*output, compressed.data(), compressed.size(), maxOutput, 0);
    RequireEqual(sceZlibFinalize(), 0, "finalize");
}};

} // namespace
