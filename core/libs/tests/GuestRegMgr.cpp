#include "prx/libSceRegMgr/include/Api.hpp"
#include <cerrno>
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
    int32_t intVal = 0;
    char strBuf[64] = {};
    uint8_t binBuf[32] = {};

    Require(sceRegMgrGetInt(0x1000, nullptr) == static_cast<int>(0x80820001));
    Require(sceRegMgrGetInt(0x1000, reinterpret_cast<int32_t*>(0x10)) == static_cast<int>(0x80820002));
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

    Require(sceRegMgrSetInt(0x2000, 0x12345678) == 0);
    Require(sceRegMgrGetInt(0x2000, &intVal) == 0 && intVal == 0x12345678);
    Require(sceRegMgrSetInt(0x2000, -7) == 0);
    Require(sceRegMgrGetInt(0x2000, &intVal) == 0 && intVal == -7);
    errno = 71;
    Require(sceRegMgrGetInt(0x2000, reinterpret_cast<int32_t*>(0x10)) == static_cast<int>(0x80820001));
    Require(errno == 71);
    Require(sceRegMgrSetStr(0x2001, "hello", 6) == 0);
    std::memset(strBuf, 0, sizeof(strBuf));
    Require(sceRegMgrGetStr(0x2001, strBuf, sizeof(strBuf)) == 0 && std::strcmp(strBuf, "hello") == 0);
    errno = 71;
    Require(sceRegMgrGetStr(0x2001, reinterpret_cast<char*>(0x10), sizeof(strBuf)) == static_cast<int>(0x80820001));
    Require(errno == 71);
    Require(sceRegMgrSetStr(0x2001, "world", 6) == 0);
    Require(sceRegMgrGetStr(0x2001, strBuf, sizeof(strBuf)) == 0 && std::strcmp(strBuf, "world") == 0);
    const std::uint8_t data[] = {1, 2, 3, 4};
    Require(sceRegMgrSetBin(0x2002, data, sizeof(data)) == 0);
    std::memset(binBuf, 0, sizeof(binBuf));
    Require(sceRegMgrGetBin(0x2002, binBuf, sizeof(binBuf)) == 0);
    Require(std::memcmp(binBuf, data, sizeof(data)) == 0);
    errno = 71;
    Require(sceRegMgrGetBin(0x2002, reinterpret_cast<uint8_t*>(0x10), sizeof(binBuf)) == static_cast<int>(0x80820001));
    Require(errno == 71);
    const std::uint8_t shorter[] = {9, 8};
    Require(sceRegMgrSetBin(0x2002, shorter, sizeof(shorter)) == 0);
    std::memset(binBuf, 0, sizeof(binBuf));
    Require(sceRegMgrGetBin(0x2002, binBuf, sizeof(binBuf)) == 0);
    Require(binBuf[0] == 9 && binBuf[1] == 8 && binBuf[2] == 0);
    const auto* unreadableBin = reinterpret_cast<const void*>(0x10);
    errno = 71;
    Require(sceRegMgrSetBin(0x2004, unreadableBin, sizeof(data)) == static_cast<int>(0x80820001));
    Require(errno == 71);
    Require(sceRegMgrGetBin(0x2004, binBuf, sizeof(binBuf)) == static_cast<int>(0x80820002));
    Require(sceRegMgrIsChange(0x2004, 0x0d010001u) == 0);
    Require(sceRegMgrIsChange(0x2002, 0x0d010001u) == 1);
    errno = 71;
    Require(sceRegMgrSetBin(0x2002, unreadableBin, sizeof(shorter)) == static_cast<int>(0x80820001));
    Require(errno == 71);
    std::memset(binBuf, 0, sizeof(binBuf));
    Require(sceRegMgrGetBin(0x2002, binBuf, sizeof(binBuf)) == 0);
    Require(binBuf[0] == 9 && binBuf[1] == 8);
    Require(sceRegMgrIsChange(0x2002, 0x0d010001u) == 0);
    Require(sceRegMgrSetInt(0x2001, 1) == static_cast<int>(0x80820006));
    Require(sceRegMgrSetStr(0x2000, "x", 2) == static_cast<int>(0x80820006));
    Require(sceRegMgrSetBin(0x2000, data, sizeof(data)) == static_cast<int>(0x80820006));
    Require(sceRegMgrSetStr(0x2003, nullptr, 5) == static_cast<int>(0x80820001));
    Require(sceRegMgrSetBin(0x2003, nullptr, 5) == static_cast<int>(0x80820001));
    Require(sceRegMgrSetInt(static_cast<int32_t>(0xC0808000u), 5) == 0);
    intVal = 0;
    Require(sceRegMgrGetInt(3229646848, &intVal) == 0 && intVal == 5);
    Require(sceRegMgrSetStr(static_cast<int32_t>(0xC0808001u), "hi", 3) == 0);
    std::memset(strBuf, 0, sizeof(strBuf));
    Require(sceRegMgrGetStr(3229646849, strBuf, sizeof(strBuf)) == 0 && std::strcmp(strBuf, "hi") == 0);

    Require(sceRegMgrNonSysGetInt(999999, 0, nullptr) == static_cast<int>(0x800D0202));
    intVal = 0x55AA;
    Require(sceRegMgrNonSysGetInt(999999, 0, &intVal) == static_cast<int>(0x80820002));
    Require(intVal == 0x55AA);
    Require(sceRegMgrNonSysSetInt(0x3000, 7, 0x1234) == 0);
    Require(sceRegMgrNonSysGetInt(0x3000, 9, &intVal) == 0 && intVal == 0x1234);
    Require(sceRegMgrNonSysSetStr(0x3001, 3, "abc", 3) == 0);
    Require(sceRegMgrNonSysGetStr(0x3001, 3, nullptr, 8) == static_cast<int>(0x800D0202));
    Require(sceRegMgrNonSysGetStr(0x3001, 3, strBuf, 0x801) == static_cast<int>(0x800D0208));
    Require(sceRegMgrNonSysGetStr(0x3001, 3, strBuf, 0) == static_cast<int>(0x80820004));
    std::memset(strBuf, 0, sizeof(strBuf));
    Require(sceRegMgrNonSysGetStr(0x3001, 3, strBuf, sizeof(strBuf)) == 0 && std::strcmp(strBuf, "abc") == 0);
    Require(sceRegMgrNonSysSetBin(0x3002, 0, data, sizeof(data)) == 0);
    Require(sceRegMgrNonSysGetBin(0x3002, 0, nullptr, 8) == static_cast<int>(0x800D0202));
    Require(sceRegMgrNonSysGetBin(0x3002, 0, binBuf, 0x801) == static_cast<int>(0x800D0208));
    Require(sceRegMgrNonSysGetBin(0x3002, 0, binBuf, 0) == static_cast<int>(0x80820004));
    std::memset(binBuf, 0, sizeof(binBuf));
    Require(sceRegMgrNonSysGetBin(0x3002, 0, binBuf, sizeof(binBuf)) == 0);
    Require(std::memcmp(binBuf, data, sizeof(data)) == 0);
    Require(sceRegMgrIsChange(0x3002, 0x0d010001u) == 1);
    errno = 71;
    Require(sceRegMgrNonSysSetBin(0x3002, 0, unreadableBin, sizeof(data)) == static_cast<int>(0x80820001));
    Require(errno == 71);
    std::memset(binBuf, 0, sizeof(binBuf));
    Require(sceRegMgrNonSysGetBin(0x3002, 0, binBuf, sizeof(binBuf)) == 0);
    Require(std::memcmp(binBuf, data, sizeof(data)) == 0);
    Require(sceRegMgrIsChange(0x3002, 0x0d010001u) == 0);
    Require(sceRegMgrNonSysSetStr(0x3003, 0, nullptr, 4) == static_cast<int>(0x800D0202));
    Require(sceRegMgrNonSysSetBin(0x3003, 0, nullptr, 4) == static_cast<int>(0x800D0202));
    Require(sceRegMgrNonSysSetStr(0x3003, 0, "x", 0x801) == static_cast<int>(0x800D0208));
    Require(sceRegMgrNonSysSetBin(0x3003, 0, data, 0x801) == static_cast<int>(0x800D0208));
    Require(sceRegMgrCheckError() == 0);
    Require(sceRegMgrNonSysCheckError() == 0);
    Require(sceRegMgrIsInitOK() == 1);

    Require(sceRegMgrSetInt(0x3006, 1) == 0);
    Require(sceRegMgrIsChange(0x3006, 0x0d010001u) == 1);
    Require(sceRegMgrIsChange(0x3006, 0x0d010001u) == 0);
    Require(sceRegMgrSetInt(0x3006, 2) == 0);
    Require(sceRegMgrIsChange(0x3006, 0x0d010002u) == 1);
    Require(sceRegMgrIsChange(0x3006, 1u) == 0);

    Require(sceRegMgrSetInt(0x3007, 5) == 0);
    Require(sceRegMgrIsChange(0x3007, 0x0d010001u) == 1);
    Require(sceRegMgrSetInt(0x3007, 5) == 0);
    Require(sceRegMgrIsChange(0x3007, 0x0d010001u) == 0);
    Require(sceRegMgrIsChange(0x3007, 0x0d010002u) == 1);

    Require(sceRegMgrSetInt(0x3003, 9) == 0);
    Require(sceRegMgrResetVal(0x3003, 0) == 0);
    intVal = 0x55AA;
    Require(sceRegMgrGetInt(0x3003, &intVal) == static_cast<int>(0x80820002));
    Require(intVal == 0x55AA);
    Require(sceRegMgrIsChange(0x3003, 0x0d010001u) == 1);
    Require(sceRegMgrResetVal(0x3004, -1) == 0);
    Require(sceRegMgrResetVal(static_cast<std::int32_t>(0xC0808003u), 0) == 0);
    intVal = 0x55AA;
    Require(sceRegMgrGetInt(static_cast<std::int64_t>(0xC0808003u), &intVal) == static_cast<int>(0x80820002));
    Require(intVal == 0x55AA);

    Require(sceRegMgrSetInt(static_cast<std::int32_t>(0xC0808002u), 3) == 0);
    Require(sceRegMgrIsChange(0xC0808002u, 0x0d010001u) == 1);

    Require(ReportsNotImplemented("sceRegMgrGetIntInitVal", [&] { sceRegMgrGetIntInitVal(0x3005, &intVal); }));
    Require(ReportsNotImplemented("sceRegMgrGetIntInitVal", [&] { sceRegMgrGetIntInitVal(0x3005, nullptr); }));
    Require(ReportsNotImplemented("sceRegMgrGetStrInitVal", [&] { sceRegMgrGetStrInitVal(); }));
    Require(ReportsNotImplemented("sceRegMgrGetBinInitVal", [&] { sceRegMgrGetBinInitVal(); }));
    Require(ReportsNotImplemented("sceRegMgrSrvGetQAFforReg", [&] { sceRegMgrSrvGetQAFforReg(); }));

    return 0;
}
