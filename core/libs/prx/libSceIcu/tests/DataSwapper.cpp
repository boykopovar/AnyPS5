#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t InvalidCharFound = 10;
constexpr std::int32_t UnsupportedError = 16;

static_assert(sizeof(Icu::DataSwapper) == 96);
static_assert(offsetof(Icu::DataSwapper, readUInt16) == 8);
static_assert(offsetof(Icu::DataSwapper, printErrorContext) == 88);
static_assert(std::is_same_v<decltype(&udata_openSwapper_nid_postfix),
                             Icu::DataSwapper* (APS5_VABI*)(std::int8_t, std::uint8_t, std::int8_t, std::uint8_t,
                                                            std::int32_t*)>);

struct Heap {
    int allocations = 0;
    int releases = 0;
    bool exhausted = false;
};

Heap heap;

struct Log {
    std::string text;
    std::string format;
    int calls = 0;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU data swapper: %s\n", message);
    std::abort();
}

void* APS5_VABI Allocate(const void*, std::size_t size) {
    if (heap.exhausted) return nullptr;
    ++heap.allocations;
    return std::malloc(size);
}

void* APS5_VABI Reallocate(const void*, void*, std::size_t) {
    std::abort();
}

void APS5_VABI Release(const void*, void* memory) {
    if (memory) ++heap.releases;
    std::free(memory);
}

void APS5_VABI Capture(void* context, const char* format, void* arguments) {
    auto* log = static_cast<Log*>(context);
    char buffer[512];
    vsnprintf_nid_postfix(buffer, sizeof(buffer), format, reinterpret_cast<VaList*>(arguments));
    log->text += buffer;
    log->format = format;
    ++log->calls;
}

using Bytes = std::vector<std::uint8_t>;

Bytes Header(bool bigEndian, bool ebcdic) {
    Bytes bytes(32, 0);
    auto put16 = [&](std::size_t offset, std::uint16_t value) {
        bytes[offset + (bigEndian ? 0 : 1)] = static_cast<std::uint8_t>(value >> 8);
        bytes[offset + (bigEndian ? 1 : 0)] = static_cast<std::uint8_t>(value);
    };
    put16(0, 32);
    bytes[2] = 0xda;
    bytes[3] = 0x27;
    put16(4, 20);
    bytes[8] = bigEndian ? 1 : 0;
    bytes[9] = ebcdic ? 1 : 0;
    bytes[10] = 2;
    const char format[] = "Test";
    const char text[] = "Copyrig";
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[12 + index] = static_cast<std::uint8_t>(ebcdic ? 0 : format[index]);
        bytes[16 + index] = static_cast<std::uint8_t>(index + 1);
        bytes[20 + index] = static_cast<std::uint8_t>(index + 5);
    }
    static const std::uint8_t ebcdicText[] = {0xc3, 0x96, 0x97, 0xa8, 0x99, 0x89, 0x87};
    for (std::size_t index = 0; index < 7; ++index) {
        bytes[24 + index] = ebcdic ? ebcdicText[index] : static_cast<std::uint8_t>(text[index]);
    }
    return bytes;
}

Icu::DataSwapper* Open(int inBig, int inCharset, int outBig, int outCharset) {
    std::int32_t status = 0;
    auto* swapper = udata_openSwapper_nid_postfix(static_cast<std::int8_t>(inBig), static_cast<std::uint8_t>(inCharset),
                                                  static_cast<std::int8_t>(outBig), static_cast<std::uint8_t>(outCharset),
                                                  &status);
    Require(status == 0 && swapper, "open failed");
    return swapper;
}

void TestOpen() {
    for (int inBig = 0; inBig <= 1; ++inBig) {
        for (int outBig = 0; outBig <= 1; ++outBig) {
            auto* swapper = Open(inBig, 0, outBig, 0);
            Require(swapper->inIsBigEndian == inBig && swapper->outIsBigEndian == outBig, "endianness fields");
            Require(swapper->inCharset == 0 && swapper->outCharset == 0, "charset fields");
            Require(swapper->printError == nullptr && swapper->printErrorContext == nullptr, "print error not clear");
            Require(swapper->readUInt16(0x1234) == (inBig ? 0x3412 : 0x1234), "readUInt16");
            Require(swapper->readUInt32(0x12345678) == (inBig ? 0x78563412u : 0x12345678u), "readUInt32");
            std::uint16_t word = 0;
            std::uint32_t quad = 0;
            swapper->writeUInt16(&word, 0x1234);
            swapper->writeUInt32(&quad, 0x12345678);
            Require(word == (outBig ? 0x3412 : 0x1234), "writeUInt16");
            Require(quad == (outBig ? 0x78563412u : 0x12345678u), "writeUInt32");

            const std::array<std::uint8_t, 16> input{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
            std::array<std::uint8_t, 16> output{};
            std::int32_t status = 0;
            Require(swapper->swapArray16(swapper, input.data(), 16, output.data(), &status) == 16 && status == 0,
                    "swapArray16 length");
            Require(output[0] == (inBig != outBig ? 2 : 1) && output[1] == (inBig != outBig ? 1 : 2), "swapArray16");
            Require(swapper->swapArray32(swapper, input.data(), 16, output.data(), &status) == 16, "swapArray32 length");
            Require(output[0] == (inBig != outBig ? 4 : 1) && output[3] == (inBig != outBig ? 1 : 4), "swapArray32");
            Require(swapper->swapArray64(swapper, input.data(), 16, output.data(), &status) == 16, "swapArray64 length");
            Require(output[0] == (inBig != outBig ? 8 : 1) && output[15] == (inBig != outBig ? 9 : 16), "swapArray64");

            auto inPlace = input;
            Require(swapper->swapArray32(swapper, inPlace.data(), 16, inPlace.data(), &status) == 16, "in place length");
            Require(inPlace[0] == (inBig != outBig ? 4 : 1) && inPlace[7] == (inBig != outBig ? 5 : 8), "in place");
            Require(swapper->swapArray16(swapper, input.data(), 0, nullptr, &status) == 0 && status == IllegalArgument,
                    "null output accepted");
            udata_closeSwapper_nid_postfix(swapper);
        }
    }
}

void TestArrayErrors() {
    auto* swapper = Open(0, 0, 1, 0);
    std::array<std::uint8_t, 16> input{};
    std::array<std::uint8_t, 16> output{};
    std::int32_t status = 0;
    Require(swapper->swapArray16(swapper, input.data(), -1, output.data(), &status) == 0 && status == IllegalArgument,
            "preflight length accepted by swapArray16");
    status = 0;
    Require(swapper->swapArray16(swapper, input.data(), 3, output.data(), &status) == 0 && status == IllegalArgument,
            "odd swapArray16 length");
    status = 0;
    Require(swapper->swapArray32(swapper, input.data(), 6, output.data(), &status) == 0 && status == IllegalArgument,
            "swapArray32 length 6");
    status = 0;
    Require(swapper->swapArray64(swapper, input.data(), 12, output.data(), &status) == 0 && status == IllegalArgument,
            "swapArray64 length 12");
    status = 0;
    Require(swapper->swapArray16(swapper, nullptr, 2, output.data(), &status) == 0 && status == IllegalArgument,
            "null input accepted");
    status = 0;
    Require(swapper->swapArray16(nullptr, input.data(), 2, output.data(), &status) == 0 && status == IllegalArgument,
            "null swapper accepted");
    status = 5;
    output[0] = 0x55;
    Require(swapper->swapArray16(swapper, input.data(), 2, output.data(), &status) == 0 && status == 5 && output[0] == 0x55,
            "failed status not honoured");
    Require(swapper->swapArray16(swapper, input.data(), 2, output.data(), nullptr) == 0, "null status");
    udata_closeSwapper_nid_postfix(swapper);

    auto* copy = Open(1, 0, 1, 0);
    status = 0;
    Require(copy->swapArray16(copy, input.data(), 3, output.data(), &status) == 0 && status == IllegalArgument,
            "odd copyArray16 length");
    status = 0;
    Require(copy->swapArray64(copy, input.data(), 8, output.data(), &status) == 8 && status == 0, "copyArray64");
    udata_closeSwapper_nid_postfix(copy);
}

void TestOpenFailures() {
    std::int32_t status = 0;
    Require(udata_openSwapper_nid_postfix(0, 2, 0, 0, &status) == nullptr && status == IllegalArgument, "bad in charset");
    status = 0;
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 2, &status) == nullptr && status == IllegalArgument, "bad out charset");
    status = 3;
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 0, &status) == nullptr && status == 3, "failed status");
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 0, nullptr) == nullptr, "null status");
    udata_closeSwapper_nid_postfix(nullptr);
    Require(heap.releases == heap.allocations, "swapper leaked or freed twice");
    heap.exhausted = true;
    status = 0;
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 0, &status) == nullptr && status == 7, "allocation failure");
    heap.exhausted = false;
}

void TestReadInt() {
    auto* swapper = Open(1, 0, 0, 0);
    Require(udata_readInt16_nid_postfix(swapper, 0x00ff) == -256, "readInt16 sign");
    Require(udata_readInt16_nid_postfix(swapper, static_cast<std::int16_t>(0xff00)) == 255, "readInt16 swap");
    Require(udata_readInt32_nid_postfix(swapper, 0x000000ff) == static_cast<std::int32_t>(0xff000000u), "readInt32 sign");
    Require(udata_readInt32_nid_postfix(swapper, 1) == 0x01000000, "readInt32 swap");
    udata_closeSwapper_nid_postfix(swapper);
    auto* direct = Open(0, 0, 0, 0);
    Require(udata_readInt16_nid_postfix(direct, -2) == -2, "direct readInt16");
    Require(udata_readInt32_nid_postfix(direct, -70000) == -70000, "direct readInt32");
    udata_closeSwapper_nid_postfix(direct);
}

void TestHeader() {
    const Bytes little = Header(false, false);
    const Bytes big = Header(true, false);
    auto* swapper = Open(0, 0, 1, 0);
    Log log;
    swapper->printError = Capture;
    swapper->printErrorContext = &log;
    std::int32_t status = 0;
    Require(udata_swapDataHeader_nid_postfix(swapper, little.data(), -1, nullptr, &status) == 32 && status == 0,
            "header preflight");
    Bytes out(32, 0xee);
    Require(udata_swapDataHeader_nid_postfix(swapper, little.data(), 32, out.data(), &status) == 32 && status == 0,
            "header swap length");
    Require(out == big, "header swap content");
    Bytes place = little;
    Require(udata_swapDataHeader_nid_postfix(swapper, place.data(), 32, place.data(), &status) == 32, "in place length");
    Require(place == big, "in place content");
    Require(log.calls == 0, "print error on success");

    Require(udata_swapDataHeader_nid_postfix(swapper, little.data(), 31, out.data(), &status) == 0 &&
                status == IndexOutOfBounds, "truncated header");
    Require(log.text == "udata_swapDataHeader(): header size mismatch - headerSize 32 infoSize 20 length 31\n",
            "truncated message");
    status = 0;
    Require(udata_swapDataHeader_nid_postfix(swapper, little.data(), 10, out.data(), &status) == 0 &&
                status == UnsupportedError, "short header");
    status = 0;
    Bytes broken = little;
    broken[2] = 0;
    Require(udata_swapDataHeader_nid_postfix(swapper, broken.data(), -1, nullptr, &status) == 0 &&
                status == UnsupportedError, "bad magic");
    Require(log.text.find("initial bytes do not look like ICU data\n") != std::string::npos, "magic message");
    status = 0;
    Bytes narrow = little;
    narrow[10] = 1;
    Require(udata_swapDataHeader_nid_postfix(swapper, narrow.data(), -1, nullptr, &status) == 0 &&
                status == UnsupportedError, "bad UChar size");
    status = 0;
    Require(udata_swapDataHeader_nid_postfix(swapper, little.data(), 32, nullptr, &status) == 0 &&
                status == IllegalArgument, "null output");
    status = 0;
    Require(udata_swapDataHeader_nid_postfix(swapper, little.data(), -2, out.data(), &status) == 0 &&
                status == IllegalArgument, "length below -1");
    status = 0;
    Require(udata_swapDataHeader_nid_postfix(nullptr, little.data(), -1, nullptr, &status) == 0 &&
                status == IllegalArgument, "null swapper");
    Require(udata_swapDataHeader_nid_postfix(swapper, little.data(), -1, nullptr, nullptr) == 0, "null status");
    udata_closeSwapper_nid_postfix(swapper);

    auto* reverse = Open(1, 0, 0, 0);
    status = 0;
    Bytes back(32, 0);
    Require(udata_swapDataHeader_nid_postfix(reverse, big.data(), 32, back.data(), &status) == 32, "reverse length");
    Require(back == little, "reverse content");
    udata_closeSwapper_nid_postfix(reverse);

    auto* toEbcdic = Open(0, 0, 0, 1);
    status = 0;
    Bytes converted(32, 0);
    Require(udata_swapDataHeader_nid_postfix(toEbcdic, little.data(), 32, converted.data(), &status) == 32 && status == 0,
            "ebcdic header length");
    Require(converted[9] == 1 && converted[24] == 0xc3 && converted[32 - 1] == 0 && converted[25] == 0x96,
            "ebcdic header content");
    udata_closeSwapper_nid_postfix(toEbcdic);
}

void TestOpenForInput() {
    const Bytes little = Header(false, false);
    const Bytes big = Header(true, false);
    std::int32_t status = 0;
    auto* swapper = udata_openSwapperForInputData_nid_postfix(little.data(), 32, 1, 0, &status);
    Require(swapper && status == 0, "open for little header");
    Require(swapper->inIsBigEndian == 0 && swapper->outIsBigEndian == 1 && swapper->readUInt16(0x1234) == 0x1234 &&
                swapper->writeUInt16 != nullptr, "little input swapper");
    udata_closeSwapper_nid_postfix(swapper);
    swapper = udata_openSwapperForInputData_nid_postfix(big.data(), -1, 0, 0, &status);
    Require(swapper && swapper->inIsBigEndian == 1 && swapper->readUInt16(0x1234) == 0x3412, "big input swapper");
    udata_closeSwapper_nid_postfix(swapper);

    const Bytes ebcdic = Header(false, true);
    swapper = udata_openSwapperForInputData_nid_postfix(ebcdic.data(), 32, 0, 0, &status);
    Require(swapper && swapper->inCharset == 1 && swapper->outCharset == 0, "ebcdic input swapper");
    udata_closeSwapper_nid_postfix(swapper);

    status = 0;
    Require(udata_openSwapperForInputData_nid_postfix(nullptr, 32, 0, 0, &status) == nullptr && status == IllegalArgument,
            "null data");
    status = 0;
    Require(udata_openSwapperForInputData_nid_postfix(little.data(), 23, 0, 0, &status) == nullptr &&
                status == IllegalArgument, "short length");
    status = 0;
    Require(udata_openSwapperForInputData_nid_postfix(little.data(), 32, 0, 2, &status) == nullptr &&
                status == IllegalArgument, "bad charset");
    status = 0;
    Bytes broken = little;
    broken[3] = 0;
    Require(udata_openSwapperForInputData_nid_postfix(broken.data(), 32, 0, 0, &status) == nullptr &&
                status == UnsupportedError, "bad magic");
    status = 0;
    Require(udata_openSwapperForInputData_nid_postfix(little.data(), 31, 0, 0, &status) == nullptr &&
                status == UnsupportedError, "length below header size");
    status = 0;
    Bytes small = little;
    small[0] = 16;
    Require(udata_openSwapperForInputData_nid_postfix(small.data(), -1, 0, 0, &status) == nullptr &&
                status == UnsupportedError, "small header size");
    status = 0;
    Bytes info = little;
    info[4] = 19;
    Require(udata_openSwapperForInputData_nid_postfix(info.data(), -1, 0, 0, &status) == nullptr &&
                status == UnsupportedError, "small info size");
    Require(udata_openSwapperForInputData_nid_postfix(little.data(), -1, 0, 0, nullptr) == nullptr, "null status");
}

void TestInvariantChars() {
    const char text[] = "Hello, World_0";
    const std::int32_t length = sizeof(text) - 1;
    auto* toEbcdic = Open(0, 0, 0, 1);
    Log log;
    toEbcdic->printError = Capture;
    toEbcdic->printErrorContext = &log;
    std::int32_t status = 0;
    char out[32] = {};
    Require(toEbcdic->swapInvChars(toEbcdic, text, length, out, &status) == length && status == 0, "to ebcdic");
    const std::uint8_t expected[] = {0xc8, 0x85, 0x93, 0x93, 0x96, 0x6b, 0x40, 0xe6, 0x96, 0x99, 0x93, 0x84, 0x6d, 0xf0};
    Require(std::memcmp(out, expected, sizeof(expected)) == 0, "ebcdic bytes");
    Require(toEbcdic->compareInvChars(toEbcdic, out, length, u"Hello, World_0", -1) == 0, "ebcdic compare equal");
    Require(toEbcdic->compareInvChars(toEbcdic, out, -1, u"Hello, World_1", length) < 0, "ebcdic compare less");
    Require(toEbcdic->compareInvChars(toEbcdic, out, 2, u"Hello", 5) == -3, "ebcdic compare length");
    const char variant[] = "ab@";
    Require(toEbcdic->swapInvChars(toEbcdic, variant, 3, out, &status) == 0 && status == InvalidCharFound, "variant char");
    Require(log.text == "uprv_ebcdicFromAscii() string[3] contains a variant character in position 2\n", "variant message");
    udata_closeSwapper_nid_postfix(toEbcdic);

    auto* toAscii = Open(0, 1, 0, 0);
    status = 0;
    char back[32] = {};
    Require(toAscii->swapInvChars(toAscii, out, 0, back, &status) == 0, "zero length");
    Require(toAscii->swapInvChars(toAscii, expected, length, back, &status) == length && status == 0, "to ascii");
    Require(std::memcmp(back, text, length) == 0, "ascii bytes");
    Require(toAscii->compareInvChars(toAscii, back, length, u"Hello, World_0", length) == 0, "ascii compare equal");
    Require(toAscii->compareInvChars(toAscii, "a@", 2, u"a@", 2) == 1, "variant compare");
    Require(toAscii->compareInvChars(toAscii, nullptr, 2, u"a", 1) == 0, "null compare");
    const std::uint8_t variantEbcdic[] = {0x7c};
    status = 0;
    Require(toAscii->swapInvChars(toAscii, variantEbcdic, 1, back, &status) == 0 && status == InvalidCharFound,
            "ebcdic variant");
    const std::uint8_t unmapped[] = {0x04};
    status = 0;
    Require(toAscii->swapInvChars(toAscii, unmapped, 1, back, &status) == 0 && status == InvalidCharFound,
            "ebcdic unmapped");
    udata_closeSwapper_nid_postfix(toAscii);

    auto* copyAscii = Open(0, 0, 0, 0);
    Log asciiLog;
    copyAscii->printError = Capture;
    copyAscii->printErrorContext = &asciiLog;
    status = 0;
    char same[8] = "ab\tc";
    Require(copyAscii->swapInvChars(copyAscii, same, 4, same, &status) == 4 && status == 0, "copy ascii in place");
    Require(copyAscii->swapInvChars(copyAscii, "a!", 2, back, &status) == 0 && status == InvalidCharFound, "copy ascii variant");
    Require(asciiLog.text == "uprv_copyFromAscii() string[2] contains a variant character in position 1\n", "copy message");
    std::memset(back, 0x5a, sizeof(back));
    Require(copyAscii->swapInvChars(copyAscii, same, 4, back, &status) == 0 && status == InvalidCharFound &&
                back[0] == 0x5a, "failed status left unchanged and nothing written");
    udata_closeSwapper_nid_postfix(copyAscii);

    auto* copyEbcdic = Open(0, 1, 0, 1);
    Log ebcdicLog;
    copyEbcdic->printError = Capture;
    copyEbcdic->printErrorContext = &ebcdicLog;
    status = 0;
    Require(copyEbcdic->swapInvChars(copyEbcdic, expected, length, back, &status) == length && status == 0, "copy ebcdic");
    Require(std::memcmp(back, expected, sizeof(expected)) == 0, "copy ebcdic bytes");
    Require(copyEbcdic->swapInvChars(copyEbcdic, variantEbcdic, 1, back, &status) == 0 && status == InvalidCharFound,
            "copy ebcdic variant");
    Require(ebcdicLog.calls == 1, "copy ebcdic message count");
    Require(ebcdicLog.format == "uprv_copyEbcdic() string[%] contains a variant character in position %d\n",
            "copy ebcdic message format");
    udata_closeSwapper_nid_postfix(copyEbcdic);
}

void TestFlagValues() {
    auto* odd = Open(2, 0, 1, 0);
    Require(odd->inIsBigEndian == 2 && odd->outIsBigEndian == 1, "raw flags stored");
    Require(odd->readUInt16(0x1234) == 0x3412 && odd->readUInt32(0x12345678) == 0x78563412u, "flag 2 reads swap");
    std::uint16_t word = 0;
    odd->writeUInt16(&word, 0x1234);
    Require(word == 0x3412, "flag 1 writes swap");
    const std::array<std::uint8_t, 4> input{1, 2, 3, 4};
    std::array<std::uint8_t, 4> output{};
    std::int32_t status = 0;
    Require(odd->swapArray16(odd, input.data(), 4, output.data(), &status) == 4 && output[0] == 2 && output[1] == 1,
            "unequal raw flags swap arrays");
    udata_closeSwapper_nid_postfix(odd);

    auto* same = Open(2, 0, 2, 0);
    Require(same->readUInt16(0x1234) == 0x3412, "equal flag 2 reads swap");
    same->writeUInt16(&word, 0x1234);
    Require(word == 0x3412, "equal flag 2 writes swap");
    Require(same->swapArray16(same, input.data(), 4, output.data(), &status) == 4 && output[0] == 1 && output[1] == 2,
            "equal raw flags copy arrays");
    udata_closeSwapper_nid_postfix(same);

    Bytes header = Header(true, false);
    header[8] = 2;
    status = 0;
    auto* swapper = udata_openSwapperForInputData_nid_postfix(header.data(), 32, 0, 0, &status);
    Require(swapper && status == 0 && swapper->inIsBigEndian == 2 && swapper->readUInt16(0x1234) == 0x3412,
            "header flag 2 swaps header size");
    udata_closeSwapper_nid_postfix(swapper);
}

void TestAlignment() {
    for (std::size_t offset : {std::size_t{0}, std::size_t{1}, std::size_t{3}}) {
        auto* swapper = Open(0, 0, 1, 0);
        alignas(8) std::uint8_t input[40];
        alignas(8) std::uint8_t output[40];
        std::int32_t status = 0;
        for (std::size_t index = 0; index < 40; ++index) input[index] = static_cast<std::uint8_t>(index);
        std::memset(output, 0, sizeof(output));
        Require(swapper->swapArray16(swapper, input + offset, 16, output + offset, &status) == 16 &&
                    output[offset] == offset + 1 && output[offset + 1] == offset, "swapArray16 alignment");
        Require(swapper->swapArray32(swapper, input + offset, 16, output + offset, &status) == 16 &&
                    output[offset] == offset + 3 && output[offset + 15] == offset + 12, "swapArray32 alignment");
        Require(swapper->swapArray64(swapper, input + offset, 16, output + offset, &status) == 16 &&
                    output[offset] == offset + 7 && output[offset + 8] == offset + 15, "swapArray64 alignment");
        Require(output[offset + 16] == 0 && (offset == 0 || output[offset - 1] == 0), "swap wrote outside the range");
        Require(swapper->swapArray64(swapper, input + offset, 16, input + offset, &status) == 16 &&
                    input[offset] == offset + 7 && input[offset + 7] == offset && input[offset + 8] == offset + 15,
                "swapArray64 in place alignment");
        Require(swapper->swapArray32(swapper, input + offset, 16, input + offset, &status) == 16 &&
                    input[offset] == offset + 4 && input[offset + 3] == offset + 7, "swapArray32 in place alignment");
        Require(swapper->swapArray16(swapper, input + offset, 16, input + offset, &status) == 16 &&
                    input[offset] == offset + 5, "swapArray16 in place alignment");
        std::uint8_t cell[16] = {};
        swapper->writeUInt32(reinterpret_cast<std::uint32_t*>(cell + 1), 0x01020304);
        swapper->writeUInt16(reinterpret_cast<std::uint16_t*>(cell + 8), 0x0506);
        Require(cell[1] == 1 && cell[4] == 4 && cell[8] == 5 && cell[9] == 6, "misaligned write");
        udata_closeSwapper_nid_postfix(swapper);
    }
}

void TestHeaderVariants() {
    const Bytes little = Header(false, false);
    auto* swapper = Open(0, 0, 1, 0);
    Log log;
    swapper->printError = Capture;
    swapper->printErrorContext = &log;
    std::int32_t status = 0;
    Bytes out(32, 0);

    Bytes tooWide = little;
    tooWide[4] = 30;
    Require(udata_swapDataHeader_nid_postfix(swapper, tooWide.data(), -1, nullptr, &status) == 0 &&
                status == IndexOutOfBounds, "header smaller than info plus four");
    Require(log.text == "udata_swapDataHeader(): header size mismatch - headerSize 32 infoSize 30 length -1\n",
            "info size message");

    status = 0;
    Bytes variant = little;
    variant[24] = '@';
    Require(udata_swapDataHeader_nid_postfix(swapper, variant.data(), 32, out.data(), &status) == 32 &&
                status == InvalidCharFound, "variant copyright returns the header size");

    status = 0;
    Bytes unterminated = little;
    for (std::size_t index = 24; index < 32; ++index) unterminated[index] = 'A';
    std::fill(out.begin(), out.end(), 0xee);
    Require(udata_swapDataHeader_nid_postfix(swapper, unterminated.data(), 32, out.data(), &status) == 32 && status == 0,
            "unterminated copyright");
    for (std::size_t index = 24; index < 32; ++index) Require(out[index] == 'A', "unterminated copyright content");
    udata_closeSwapper_nid_postfix(swapper);

    auto* toAscii = Open(0, 1, 0, 0);
    status = 0;
    const Bytes ebcdic = Header(false, true);
    std::fill(out.begin(), out.end(), 0xee);
    Require(udata_swapDataHeader_nid_postfix(toAscii, ebcdic.data(), 32, out.data(), &status) == 32 && status == 0,
            "ebcdic to ascii header");
    Require(out[9] == 0 && out[8] == 0 && std::memcmp(&out[24], "Copyrig", 8) == 0, "ebcdic to ascii content");
    udata_closeSwapper_nid_postfix(toAscii);

    Bytes charset = little;
    charset[9] = 2;
    status = 0;
    Require(udata_openSwapperForInputData_nid_postfix(charset.data(), 32, 0, 0, &status) == nullptr &&
                status == IllegalArgument, "input charset byte 2");
}

void TestInvStringBlock() {
    const std::uint8_t block[] = {'a', 'b', 0, 'c', 'd', 0, 0x01, 0xff};
    auto* swapper = Open(0, 0, 0, 1);
    std::int32_t status = 0;
    std::uint8_t out[8] = {};
    Require(udata_swapInvStringBlock_nid_postfix(swapper, block, 8, out, &status) == 8 && status == 0, "block length");
    const std::uint8_t expected[] = {0x81, 0x82, 0, 0x83, 0x84, 0, 0x01, 0xff};
    Require(std::memcmp(out, expected, 8) == 0, "block content");

    std::uint8_t place[8];
    std::memcpy(place, block, 8);
    Require(udata_swapInvStringBlock_nid_postfix(swapper, place, 8, place, &status) == 8 &&
                std::memcmp(place, expected, 8) == 0, "block in place");
    Require(udata_swapInvStringBlock_nid_postfix(swapper, block, 0, nullptr, &status) == 0 && status == 0, "empty block");
    Require(udata_swapInvStringBlock_nid_postfix(swapper, block, 8, nullptr, &status) == 0 && status == IllegalArgument,
            "null output");
    status = 0;
    Require(udata_swapInvStringBlock_nid_postfix(swapper, block, -1, out, &status) == 0 && status == IllegalArgument,
            "negative length");
    status = 0;
    Require(udata_swapInvStringBlock_nid_postfix(nullptr, block, 8, out, &status) == 0 && status == IllegalArgument,
            "null swapper");
    const std::uint8_t bad[] = {'a', '@', 0, 'x'};
    status = 0;
    Require(udata_swapInvStringBlock_nid_postfix(swapper, bad, 4, out, &status) == 0 && status == InvalidCharFound,
            "variant char in block");
    Require(udata_swapInvStringBlock_nid_postfix(swapper, block, 8, out, nullptr) == 0, "null status");
    udata_closeSwapper_nid_postfix(swapper);
}

void TestPrintError() {
    auto* swapper = Open(0, 0, 0, 0);
    udata_printError_nid_postfix(swapper, "no callback %d", 1);
    Log log;
    swapper->printError = Capture;
    swapper->printErrorContext = &log;
    udata_printError_nid_postfix(swapper, "value %d %s %lld\n", -5, "text", 1234567890123ll);
    Require(log.calls == 1 && log.text == "value -5 text 1234567890123\n", "print error formatting");
    udata_closeSwapper_nid_postfix(swapper);
}

}

int main() {
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate, Reallocate, Release, &status);
    Require(status == 0, "memory functions rejected");
    TestOpen();
    TestArrayErrors();
    TestOpenFailures();
    TestReadInt();
    TestHeader();
    TestOpenForInput();
    TestInvariantChars();
    TestInvStringBlock();
    TestFlagValues();
    TestAlignment();
    TestHeaderVariants();
    TestPrintError();
    Require(heap.releases == heap.allocations, "swappers not all closed");
    return 0;
}
