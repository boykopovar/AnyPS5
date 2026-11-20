#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu data swap: " + message);
}

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IndexOutOfBoundsError = 8;
constexpr std::int32_t UnsupportedError = 16;

int printedErrors = 0;
const char* printedFormat = nullptr;

void APS5_VABI RecordError(const void* context, const char* format, va_list arguments) {
    (void)arguments;
    Require(context == reinterpret_cast<const void*>(0x4321), "the print error context was not passed");
    printedFormat = format;
    ++printedErrors;
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());

    std::int32_t status = ZeroError;
    UDataSwapper* swapper = udata_openSwapper_nid_postfix(1, 0, 0, 0, &status);
    Require(status == ZeroError && swapper != nullptr, "the big-endian swapper did not open");
    Require(swapper->inIsBigEndian == 1 && swapper->outIsBigEndian == 0, "the swapper endianness is wrong");
    Require(swapper->inCharset == 0 && swapper->outCharset == 0, "the swapper charset is wrong");
    Require(udata_readInt16_nid_postfix(swapper, 0x1234) == 0x3412, "readInt16 did not swap");
    Require(udata_readInt32_nid_postfix(swapper, 0x12345678) == 0x78563412, "readInt32 did not swap");
    Require(swapper->writeUInt16 != nullptr && swapper->writeUInt32 != nullptr, "the writer functions are missing");
    Require(swapper->swapArray16 != nullptr && swapper->swapArray32 != nullptr && swapper->swapArray64 != nullptr,
        "the array swappers are missing");
    Require(swapper->swapInvChars != nullptr && swapper->compareInvChars != nullptr,
        "the string swapper or comparator is missing");

    std::uint16_t words[2] = {0x1234, 0xabcd};
    std::uint16_t swapped[2] = {};
    status = ZeroError;
    Require(swapper->swapArray16(swapper, words, 4, swapped, &status) == 4, "swapArray16 reported the wrong length");
    Require(status == ZeroError && swapped[0] == 0x3412 && swapped[1] == 0xcdab, "swapArray16 swapped wrongly");

    swapper->printError = RecordError;
    swapper->printErrorContext = reinterpret_cast<void*>(0x4321);
    udata_printError_nid_postfix(swapper, "value %d", 7);
    Require(printedErrors == 1 && printedFormat != nullptr && std::string(printedFormat) == "value %d",
        "printError did not forward the format");
    udata_closeSwapper_nid_postfix(swapper);


    {
        std::uint8_t littleHeader[48] = {};
        littleHeader[0] = 0x1c;
        littleHeader[1] = 0x00;
        littleHeader[2] = 0xda;
        littleHeader[3] = 0x27;
        littleHeader[4] = 0x14;
        littleHeader[5] = 0x00;
        littleHeader[10] = 2;
        littleHeader[24] = 'c';
        littleHeader[25] = 0;
        status = ZeroError;
        UDataSwapper* inputSwapper = udata_openSwapperForInputData_nid_postfix(littleHeader,
            static_cast<std::int32_t>(sizeof(littleHeader)), 0, 0, &status);
        Require(status == ZeroError, "openSwapperForInputData status " + std::to_string(status));
        Require(inputSwapper != nullptr, "openSwapperForInputData returned null");
        Require(inputSwapper->inIsBigEndian == 0 && inputSwapper->outIsBigEndian == 0,
            "the input swapper direction is wrong");
        udata_closeSwapper_nid_postfix(inputSwapper);

        status = ZeroError;
        Require(udata_openSwapperForInputData_nid_postfix(littleHeader, 4, 0, 0, &status) == nullptr,
            "openSwapperForInputData accepted a truncated header");
        status = ZeroError;
        littleHeader[2] = 0x00;
        Require(udata_openSwapperForInputData_nid_postfix(littleHeader, 48, 0, 0, &status) == nullptr,
            "openSwapperForInputData accepted a wrong magic");
        littleHeader[2] = 0xda;

        std::uint8_t bigHeader[48] = {};
        bigHeader[0] = 0x00;
        bigHeader[1] = 0x1c;
        bigHeader[2] = 0xda;
        bigHeader[3] = 0x27;
        bigHeader[4] = 0x00;
        bigHeader[5] = 0x14;
        bigHeader[8] = 1;
        bigHeader[10] = 2;
        bigHeader[24] = 'c';
        bigHeader[25] = 0;
        status = ZeroError;
        UDataSwapper* outSwapper = udata_openSwapper_nid_postfix(1, 0, 0, 0, &status);
        std::uint8_t swappedHeader[48] = {};
        const std::int32_t headerLength = udata_swapDataHeader_nid_postfix(outSwapper, bigHeader,
            static_cast<std::int32_t>(sizeof(bigHeader)), swappedHeader, &status);
        Require(status == ZeroError && headerLength == 28, "swapDataHeader reported the wrong header size");
        Require(swappedHeader[0] == 0x1c && swappedHeader[1] == 0x00,
            "swapDataHeader did not swap the header size");
        Require(swappedHeader[2] == 0xda && swappedHeader[3] == 0x27, "swapDataHeader changed the magic bytes");
        Require(swappedHeader[4] == 0x14 && swappedHeader[5] == 0x00, "swapDataHeader did not swap the info size");
        Require(swappedHeader[8] == 0, "swapDataHeader did not write the output endianness");
        Require(swappedHeader[24] == 'c' && swappedHeader[25] == 0, "swapDataHeader dropped the copyright string");

        std::uint8_t longHeader[28] = {};
        longHeader[0] = 0x00;
        longHeader[1] = 0x1c;
        longHeader[2] = 0xda;
        longHeader[3] = 0x27;
        longHeader[4] = 0x00;
        longHeader[5] = 0x14;
        longHeader[8] = 1;
        longHeader[10] = 2;
        longHeader[24] = 'c';
        longHeader[25] = 0;
        std::uint8_t guarded[40];
        std::memset(guarded, 0x5a, sizeof(guarded));
        status = ZeroError;
        Require(udata_swapDataHeader_nid_postfix(outSwapper, longHeader, 26, guarded, &status) == 0,
            "swapDataHeader accepted a length below the header size");
        Require(status == IndexOutOfBoundsError, "swapDataHeader did not report the size mismatch");
        for (const std::uint8_t byte : guarded) Require(byte == 0x5a, "swapDataHeader wrote past the supplied length");
        udata_closeSwapper_nid_postfix(outSwapper);

        const char strings[] = {'a', 'b', 0, 'c', 'd', 0, 0};
        char copied[7] = {};
        status = ZeroError;
        UDataSwapper* asciiSwapper = udata_openSwapper_nid_postfix(0, 0, 0, 0, &status);
        Require(udata_swapInvStringBlock_nid_postfix(asciiSwapper, strings, 7, copied, &status) == 7,
            "swapInvStringBlock reported the wrong length");
        Require(std::memcmp(copied, strings, 7) == 0, "swapInvStringBlock did not copy the block");
        udata_closeSwapper_nid_postfix(asciiSwapper);
    }

    status = ZeroError;
    UDataSwapper* direct = udata_openSwapper_nid_postfix(0, 0, 0, 0, &status);
    Require(status == ZeroError && direct != nullptr, "the little-endian swapper did not open");
    Require(udata_readInt16_nid_postfix(direct, 0x1234) == 0x1234, "readInt16 swapped a direct value");
    Require(udata_readInt32_nid_postfix(direct, 0x12345678) == 0x12345678, "readInt32 swapped a direct value");
    udata_closeSwapper_nid_postfix(direct);

    status = ZeroError;
    Require(udata_openSwapper_nid_postfix(0, 1, 0, 0, &status) == nullptr, "an EBCDIC swapper opened");
    Require(status == UnsupportedError, "an EBCDIC request did not report the unsupported status");
    status = ZeroError;
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 2, &status) == nullptr, "an invalid charset opened");
    Require(status != ZeroError, "an invalid charset did not report an error");

    udata_closeSwapper_nid_postfix(nullptr);
    udata_printError_nid_postfix(nullptr, "ignored");
    return 0;
}
