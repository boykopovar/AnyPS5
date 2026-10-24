#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceRegMgrGetBin(std::int64_t optionId, void* data, int size);
int APS5_VABI sceRegMgrGetInt(std::int64_t optionId, int32_t* outInt);
int APS5_VABI sceRegMgrGetStr(std::int64_t optionId, char* outString, int stringLength);
int APS5_VABI sceRegMgrSetBin(std::int64_t optionId, const void* data, int size);
int APS5_VABI sceRegMgrSetInt(std::int64_t optionId, int32_t inInt);
int APS5_VABI sceRegMgrSetStr(std::int64_t optionId, const char* inString, int stringLength);
int APS5_VABI sceRegMgrSetbin(std::int64_t optionId, const void* data, int size);
int APS5_VABI sceRegMgrResetVal(std::int64_t optionId);
void* APS5_VABI sceRegMgrMemset(void* dst, int val, std::size_t len);
}

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    int32_t intVal = 0;
    char strBuf[64] = {};
    uint8_t binBuf[32] = {};
    const uint8_t sampleBin[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    const uint8_t sampleBin2[4] = {0xCA, 0xFE, 0xBA, 0xBE};

    Require(sceRegMgrSetInt(0x5000, 12345) == 0);
    Require(sceRegMgrGetInt(0x5000, &intVal) == 0);
    Require(intVal == 12345);

    Require(sceRegMgrSetStr(0x5001, "CustomTest", 0) == 0);
    Require(sceRegMgrGetStr(0x5001, strBuf, sizeof(strBuf)) == 0);
    Require(std::strcmp(strBuf, "CustomTest") == 0);

    Require(sceRegMgrSetBin(0x5002, sampleBin, sizeof(sampleBin)) == 0);
    Require(sceRegMgrGetBin(0x5002, binBuf, sizeof(binBuf)) == 0);
    Require(std::memcmp(binBuf, sampleBin, sizeof(sampleBin)) == 0);

    Require(sceRegMgrSetbin(0x5003, sampleBin2, sizeof(sampleBin2)) == 0);
    Require(sceRegMgrGetBin(0x5003, binBuf, sizeof(binBuf)) == 0);
    Require(std::memcmp(binBuf, sampleBin2, sizeof(sampleBin2)) == 0);

    const std::int64_t largeKey = 0x100000005004LL;
    Require(sceRegMgrSetInt(largeKey, 98765) == 0);
    Require(sceRegMgrGetInt(largeKey, &intVal) == 0);
    Require(intVal == 98765);
    Require(sceRegMgrResetVal(largeKey) == 0);
    Require(sceRegMgrGetInt(largeKey, &intVal) == static_cast<int>(0x80820002));

    Require(sceRegMgrSetInt(0x6001, 100) == 0);
    Require(sceRegMgrGetInt(0x6001, &intVal) == 0);
    Require(intVal == 100);
    Require(sceRegMgrSetStr(0x6001, "str-transition", 0) == 0);
    Require(sceRegMgrGetStr(0x6001, strBuf, sizeof(strBuf)) == 0);
    Require(std::strcmp(strBuf, "str-transition") == 0);
    Require(sceRegMgrGetInt(0x6001, &intVal) == static_cast<int>(0x80820006));
    Require(sceRegMgrResetVal(0x6001) == 0);
    Require(sceRegMgrGetInt(0x6001, &intVal) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetStr(0x6001, strBuf, sizeof(strBuf)) == static_cast<int>(0x80820002));

    Require(sceRegMgrSetInt(0x6002, 200) == 0);
    Require(sceRegMgrGetInt(0x6002, &intVal) == 0);
    Require(intVal == 200);
    Require(sceRegMgrSetBin(0x6002, sampleBin, sizeof(sampleBin)) == 0);
    Require(sceRegMgrGetBin(0x6002, binBuf, sizeof(binBuf)) == 0);
    Require(std::memcmp(binBuf, sampleBin, sizeof(sampleBin)) == 0);
    Require(sceRegMgrGetInt(0x6002, &intVal) == static_cast<int>(0x80820006));
    Require(sceRegMgrResetVal(0x6002) == 0);
    Require(sceRegMgrGetInt(0x6002, &intVal) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetBin(0x6002, binBuf, sizeof(binBuf)) == static_cast<int>(0x80820002));

    Require(sceRegMgrSetStr(0x6003, "initial-str", 0) == 0);
    Require(sceRegMgrGetStr(0x6003, strBuf, sizeof(strBuf)) == 0);
    Require(std::strcmp(strBuf, "initial-str") == 0);
    Require(sceRegMgrSetInt(0x6003, 300) == 0);
    Require(sceRegMgrGetInt(0x6003, &intVal) == 0);
    Require(intVal == 300);
    Require(sceRegMgrGetStr(0x6003, strBuf, sizeof(strBuf)) == static_cast<int>(0x80820006));
    Require(sceRegMgrResetVal(0x6003) == 0);
    Require(sceRegMgrGetInt(0x6003, &intVal) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetStr(0x6003, strBuf, sizeof(strBuf)) == static_cast<int>(0x80820002));

    Require(sceRegMgrResetVal(0x5000) == 0);
    Require(sceRegMgrGetInt(0x5000, &intVal) == static_cast<int>(0x80820002));
    Require(sceRegMgrResetVal(0x9999) == static_cast<int>(0x80820002));

    Require(sceRegMgrGetInt(0x5001, nullptr) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetStr(0x5001, nullptr, 10) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetStr(0x5001, strBuf, 0) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetStr(0x5001, strBuf, -1) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetStr(0x5001, strBuf, 5) == static_cast<int>(0x80820004));
    Require(sceRegMgrGetBin(0x5002, nullptr, 10) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetBin(0x5002, binBuf, 0) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetBin(0x5002, binBuf, -1) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetBin(0x5002, binBuf, 2) == static_cast<int>(0x80820004));

    Require(sceRegMgrSetStr(0x5001, nullptr, 0) == static_cast<int>(0x80820001));
    Require(sceRegMgrSetStr(0x5001, "Test", -1) == static_cast<int>(0x80820001));
    Require(sceRegMgrSetBin(0x5002, nullptr, 4) == static_cast<int>(0x80820001));
    Require(sceRegMgrSetBin(0x5002, sampleBin, 0) == static_cast<int>(0x80820001));
    Require(sceRegMgrSetBin(0x5002, sampleBin, -1) == static_cast<int>(0x80820001));

    Require(sceRegMgrGetInt(888888, &intVal) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetStr(888888, strBuf, sizeof(strBuf)) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetBin(888888, binBuf, sizeof(binBuf)) == static_cast<int>(0x80820002));

    Require(sceRegMgrSetStr(0x7001, "", 1) == 0);
    char singleByteBuf[1] = {'X'};
    Require(sceRegMgrGetStr(0x7001, singleByteBuf, 1) == 0);
    Require(singleByteBuf[0] == '\0');
    Require(sceRegMgrGetStr(0x7001, strBuf, sizeof(strBuf)) == 0);
    Require(strBuf[0] == '\0');
    Require(sceRegMgrGetStr(0x7001, singleByteBuf, 0) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetStr(0x7001, singleByteBuf, -1) == static_cast<int>(0x80820001));
    Require(sceRegMgrSetStr(0x7002, "", 0) == 0);
    Require(sceRegMgrGetStr(0x7002, singleByteBuf, 1) == 0);
    Require(singleByteBuf[0] == '\0');
    Require(sceRegMgrResetVal(0x7001) == 0);
    Require(sceRegMgrGetStr(0x7001, singleByteBuf, 1) == static_cast<int>(0x80820002));

    uint8_t memBuf[8] = {};
    Require(sceRegMgrMemset(memBuf, 0xAB, sizeof(memBuf)) == memBuf);
    Require(memBuf[0] == 0xAB && memBuf[7] == 0xAB);
    Require(sceRegMgrMemset(nullptr, 0, 8) == nullptr);

    return 0;
}
