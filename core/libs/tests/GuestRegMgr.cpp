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

    Require(ReportsNotImplemented("sceRegMgrSetInt", [&] { sceRegMgrSetInt(0x3000, 42); }));
    Require(ReportsNotImplemented("sceRegMgrSetInt", [&] { sceRegMgrSetInt(-1, -1); }));
    Require(ReportsNotImplemented("sceRegMgrSetStr", [&] { sceRegMgrSetStr(0x3001, "abc", 4); }));
    Require(ReportsNotImplemented("sceRegMgrSetStr", [&] { sceRegMgrSetStr(0x3001, nullptr, 0); }));
    Require(ReportsNotImplemented("sceRegMgrSetBin", [&] { sceRegMgrSetBin(0x3002, binBuf, sizeof(binBuf)); }));
    Require(ReportsNotImplemented("sceRegMgrSetBin", [&] { sceRegMgrSetBin(0x3002, nullptr, 0); }));
    intVal = 0x55AA;
    Require(sceRegMgrGetInt(0x3000, &intVal) == static_cast<int>(0x80820002));
    Require(intVal == 0x55AA);
    Require(sceRegMgrGetStr(0x3001, strBuf, sizeof(strBuf)) == static_cast<int>(0x80820002));
    Require(sceRegMgrGetBin(0x3002, binBuf, sizeof(binBuf)) == static_cast<int>(0x80820002));

    Require(sceRegMgrResetVal(0x3003, 0) == 0);
    Require(sceRegMgrResetVal(0x3004, -1) == 0);
    Require(sceRegMgrResetVal(static_cast<std::int32_t>(0xC0808000u), 0) == 0);
    intVal = 0x55AA;
    Require(sceRegMgrGetInt(static_cast<std::int64_t>(0xC0808000u), &intVal) == static_cast<int>(0x80820002));
    Require(intVal == 0x55AA);

    Require(ReportsNotImplemented("sceRegMgrGetIntInitVal", [&] { sceRegMgrGetIntInitVal(0x3005, &intVal); }));
    Require(ReportsNotImplemented("sceRegMgrGetIntInitVal", [&] { sceRegMgrGetIntInitVal(0x3005, nullptr); }));
    Require(ReportsNotImplemented("sceRegMgrGetStrInitVal", [&] { sceRegMgrGetStrInitVal(); }));
    Require(ReportsNotImplemented("sceRegMgrGetBinInitVal", [&] { sceRegMgrGetBinInitVal(); }));

    Require(sceRegMgrIsChange(0x3006, 0x0d010001u) == 0);
    Require(sceRegMgrIsChange(0x3006, 0x0d010002u) == 0);
    Require(sceRegMgrIsChange(0x3006, 0x0d890080u) == 0);
    Require(sceRegMgrIsChange(0x3006, 1u) == 0);
    Require(sceRegMgrIsChange(0xC0808000u, 0x0d010001u) == 0);

    Require(sceRegMgrSrvGetQAFforReg() == 0);

    return 0;
}
