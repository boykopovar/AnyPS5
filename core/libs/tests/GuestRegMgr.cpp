#include "prx/libSceRegMgr/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool ReportsNotImplemented(const char* name, TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

int main() {
    std::int32_t value = 0;
    Require(ReportsNotImplemented("sceRegMgrNonSysGetInt", [&] { sceRegMgrNonSysGetInt(0, 0, &value); }));
    Require(ReportsNotImplemented("sceRegMgrNonSysGetInt", [&] { sceRegMgrNonSysGetInt(-1, 1, nullptr); }));

    int32_t intVal = 0;
    char strBuf[64] = {};
    uint8_t binBuf[32] = {};

    Require(sceRegMgrGetInt(0x1000, nullptr) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetStr(0x1001, nullptr, 10) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetStr(0x1001, strBuf, -1) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetBin(0x1002, nullptr, 10) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetBin(0x1002, binBuf, -1) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetStr(0x1001, nullptr, -1) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetBin(0x1002, nullptr, -1) == static_cast<int>(0x80820001));

    intVal = 0x55AA;
    Require(sceRegMgrGetInt(888888, &intVal) == static_cast<int>(0x80820002));
    Require(intVal == 0x55AA);
    std::strcpy(strBuf, "CANARY");
    Require(sceRegMgrGetStr(888888, strBuf, sizeof(strBuf)) == static_cast<int>(0x80820002));
    Require(std::strcmp(strBuf, "CANARY") == 0);
    binBuf[0] = 0x77;
    Require(sceRegMgrGetBin(888888, binBuf, sizeof(binBuf)) == static_cast<int>(0x80820002));
    Require(binBuf[0] == 0x77);
    Require(sceRegMgrGetInt(0, &intVal) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetInt(-1, &intVal) == static_cast<int>(0x80820002));
    Require(intVal == 0x55AA);
    Require(sceRegMgrGetStr(0, strBuf, 1) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetStr(-1, strBuf, 1) == static_cast<int>(0x80820002));
    Require(std::strcmp(strBuf, "CANARY") == 0);
    Require(sceRegMgrGetBin(0, binBuf, 1) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetBin(-1, binBuf, 1) == static_cast<int>(0x80820002));
    Require(binBuf[0] == 0x77);

    Require(sceRegMgrSetInt(0x3000, 42) == 0);
    intVal = 0;
    Require(sceRegMgrGetInt(0x3000, &intVal) == 0);
    Require(intVal == 42);
    Require(sceRegMgrSetInt(0x3000, 43) == 0);
    intVal = 0;
    Require(sceRegMgrGetInt(0x3000, &intVal) == 0);
    Require(intVal == 43);
    intVal = 0x55AA;
    Require(sceRegMgrGetInt(0x3000, nullptr) == static_cast<int>(0x80820001));
    Require(intVal == 0x55AA);

    Require(sceRegMgrSetStr(0x3001, "abc", 4) == 0);
    Require(sceRegMgrGetStr(0x3001, strBuf, sizeof(strBuf)) == 0);
    Require(std::strcmp(strBuf, "abc") == 0);
    Require(sceRegMgrGetStr(0x3001, strBuf, 5) == 0);
    Require(std::strcmp(strBuf, "abc") == 0);
    Require(sceRegMgrGetStr(0x3001, strBuf, 4) == static_cast<int>(0x80820004));
    intVal = 0x55AA;
    Require(sceRegMgrGetInt(0x3001, &intVal) == static_cast<int>(0x80820006));
    Require(intVal == 0x55AA);
    Require(sceRegMgrSetStr(0x3001, nullptr, 0) == 0);
    Require(sceRegMgrGetStr(0x3001, strBuf, sizeof(strBuf)) == 0);
    Require(strBuf[0] == '\0');

    const uint8_t blob[4] = {1, 2, 3, 4};
    Require(sceRegMgrSetBin(0x3002, blob, sizeof(blob)) == 0);
    std::memset(binBuf, 0, sizeof(binBuf));
    Require(sceRegMgrGetBin(0x3002, binBuf, sizeof(binBuf)) == 0);
    Require(binBuf[0] == 1 && binBuf[1] == 2 && binBuf[2] == 3 && binBuf[3] == 4);
    Require(sceRegMgrGetBin(0x3002, binBuf, 4) == 0);
    Require(sceRegMgrGetBin(0x3002, binBuf, 3) == static_cast<int>(0x80820004));
    intVal = 0x55AA;
    Require(sceRegMgrGetInt(0x3002, &intVal) == static_cast<int>(0x80820006));
    Require(intVal == 0x55AA);
    Require(sceRegMgrSetBin(0x3002, nullptr, 0) == 0);
    std::memset(binBuf, 0x5C, sizeof(binBuf));
    Require(sceRegMgrGetBin(0x3002, binBuf, sizeof(binBuf)) == 0);
    Require(binBuf[0] == 0x5C && binBuf[1] == 0x5C);

    Require(sceRegMgrSetInt(0x3003, 9) == 0);
    Require(sceRegMgrResetVal(0x3003, 0) == 0);
    intVal = 0x55AA;
    Require(sceRegMgrGetInt(0x3003, &intVal) == static_cast<int>(0x80820002));
    Require(intVal == 0x55AA);
    Require(sceRegMgrResetVal(0x3004, 0) == 0);

    intVal = 0x77;
    Require(sceRegMgrGetIntInitVal(0x3005, &intVal, 0) == 0);
    Require(intVal == 0);
    Require(sceRegMgrGetIntInitVal(0x3005, nullptr, 0) == 0);
    std::strcpy(strBuf, "CANARY");
    Require(sceRegMgrGetStrInitVal(0x3005, strBuf, sizeof(strBuf), 0) == 0);
    Require(strBuf[0] == '\0');
    Require(sceRegMgrGetStrInitVal(0x3005, nullptr, 4, 0) == 0);
    std::memset(binBuf, 0x77, sizeof(binBuf));
    Require(sceRegMgrGetBinInitVal(0x3005, binBuf, sizeof(binBuf), 0) == 0);
    bool zeroed = true;
    for (uint8_t byte : binBuf) zeroed = zeroed && byte == 0;
    Require(zeroed);
    Require(sceRegMgrGetBinInitVal(0x3005, nullptr, 4, 0) == 0);

    Require(sceRegMgrIsChange(0x3006, 0x0d010001u) == 0);
    Require(sceRegMgrSetInt(0x3006, 1) == 0);
    Require(sceRegMgrIsChange(0x3006, 0x0d010001u) == 1);
    Require(sceRegMgrIsChange(0x3006, 0x0d010001u) == 0);
    Require(sceRegMgrIsChange(0x3006, 0x0d010002u) == 1);
    Require(sceRegMgrIsChange(0x3006, 0x0d890080u) == 1);
    Require(sceRegMgrIsChange(0x3006, 1u) == 0);

    Require(sceRegMgrSrvGetQAFforReg() == 0);

    return 0;
}
