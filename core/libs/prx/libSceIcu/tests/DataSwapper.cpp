#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

using namespace Icu::Abi;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::printf("ICU DataSwapper check failed at line %u\n", location.line());
    std::exit(1);
}

void TestSwapperOpenClose() {
    std::int32_t status = 0;
    auto* ds = udata_openSwapper_nid_postfix(0, 0, 1, 0, &status);
    Check(status == 0 && ds != nullptr);
    Check(ds->inIsBigEndian == 0);
    Check(ds->outIsBigEndian == 1);
    Check(udata_readInt16_nid_postfix(ds, 0x1234) == 0x1234);
    Check(udata_readInt32_nid_postfix(ds, 0x12345678) == 0x12345678);

    udata_closeSwapper_nid_postfix(ds);
}

void TestSwapDataHeader() {
    std::int32_t status = 0;
    auto* ds = udata_openSwapper_nid_postfix(0, 0, 1, 0, &status);
    Check(status == 0 && ds != nullptr);

    alignas(4) unsigned char header[32] = {
        0x20, 0x00, 0xda, 0x27,
        0x14, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x02, 0x00,
        0x54, 0x72, 0x69, 0x33,
        0x01, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        'I', 'C', 'U', '4', 'C', 0x00, 0x00, 0x00
    };
    alignas(4) unsigned char outHeader[32] = {};
    auto written = udata_swapDataHeader_nid_postfix(ds, header, 32, outHeader, &status);
    Check(status == 0);
    Check(written == 32);
    Check(outHeader[0] == 0x00 && outHeader[1] == 0x20);
    Check(outHeader[8] == 1);

    auto* inDs = udata_openSwapperForInputData_nid_postfix(header, 32, 0, 0, &status);
    Check(status == 0 && inDs != nullptr);
    Check(inDs->inIsBigEndian == 0);
    udata_closeSwapper_nid_postfix(inDs);

    udata_closeSwapper_nid_postfix(ds);
}

void TestSwapInvStringBlock() {
    std::int32_t status = 0;
    auto* ds = udata_openSwapper_nid_postfix(0, 0, 0, 1, &status);
    Check(status == 0 && ds != nullptr);

    const char inStr[] = {'h', 'e', 'l', 'l', 'o', '\0', (char)0xaa, (char)0xbb};
    char outStr[8] = {};
    auto len = udata_swapInvStringBlock_nid_postfix(ds, inStr, sizeof(inStr), outStr, &status);
    Check(status == 0);
    Check(len == sizeof(inStr));
    Check(outStr[6] == (char)0xaa && outStr[7] == (char)0xbb);

    udata_closeSwapper_nid_postfix(ds);
}

void TestTrieSwappers() {
    std::int32_t status = 0;
    auto* ds = udata_openSwapper_nid_postfix(1, 0, 0, 0, &status);
    Check(status == 0 && ds != nullptr);

    alignas(4) unsigned char trie3[24] = {
        0x54, 0x72, 0x69, 0x33,
        0x00, 0x40,
        0x10, 0x00,
        0x00, 0x80,
        0x00, 0x00,
        0x00, 0x00,
        0x00, 0x00
    };
    auto size = ucptrie_swap_nid_postfix(ds, trie3, -1, nullptr, &status);
    Check(status == 0);
    Check(size > 0);

    alignas(4) unsigned char inBuf[8464] = {};
    alignas(4) unsigned char outBuf[8464] = {};
    std::memcpy(inBuf, trie3, 16);
    auto verSize = utrie_swapAnyVersion_nid_postfix(ds, inBuf, sizeof(inBuf), outBuf, &status);
    Check(status == 0);
    Check(verSize == size);

    udata_closeSwapper_nid_postfix(ds);
}

}

char printed[256];

void APS5_VABI CapturePrintError(void* context, const char* format, void* args) {
    Check(context == printed);
    vsnprintf_nid_postfix(printed, sizeof(printed), format, static_cast<VaList*>(args));
}

void TestPrintError() {
    std::int32_t status = 0;
    auto* ds = udata_openSwapper_nid_postfix(0, 0, 1, 0, &status);
    Check(status == 0 && ds != nullptr);
    ds->printError = CapturePrintError;
    ds->printErrorContext = printed;
    udata_printError_nid_postfix(ds, "%s %d %x %lld|", "swap", -7, 0xBEEFu, 1234567890123LL);
    Check(std::strcmp(printed, "swap -7 beef 1234567890123|") == 0);
    alignas(4) unsigned char junk[32] = {};
    unsigned char out[32] = {};
    printed[0] = 0;
    Check(udata_swapDataHeader_nid_postfix(ds, junk, 32, out, &status) == 0);
    Check(status == 16);
    Check(std::strcmp(printed, "udata_swapDataHeader(): initial bytes do not look like ICU data\n") == 0);
    ds->printError = nullptr;
    udata_printError_nid_postfix(ds, "%d", 1);
    udata_closeSwapper_nid_postfix(ds);
}

int main() {
    TestSwapperOpenClose();
    TestSwapDataHeader();
    TestSwapInvStringBlock();
    TestTrieSwappers();
    TestPrintError();
    std::printf("ICU DataSwapper tests passed\n");
    return 0;
}
