#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct ErrorMapping {
    std::int32_t code;
    const char* expected;
};

const ErrorMapping kKnownErrors[] = {
    { SCE_FIOS_OK, "SCE_FIOS_OK" },
    { 1, "SCE_FIOS_IN_PROGRESS" },
    { static_cast<std::int32_t>(0x80820000u), "SCE_FIOS_ERROR_UNIMPLEMENTED" },
    { static_cast<std::int32_t>(0x80820001u), "SCE_FIOS_ERROR_CANT_ALLOCATE_OP" },
    { static_cast<std::int32_t>(0x80820002u), "SCE_FIOS_ERROR_CANT_ALLOCATE_FH" },
    { static_cast<std::int32_t>(0x80820003u), "SCE_FIOS_ERROR_CANT_ALLOCATE_DH" },
    { static_cast<std::int32_t>(0x80820004u), "SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK" },
    { static_cast<std::int32_t>(0x80820005u), "SCE_FIOS_ERROR_BAD_PATH" },
    { static_cast<std::int32_t>(0x80820006u), "SCE_FIOS_ERROR_BAD_PTR" },
    { static_cast<std::int32_t>(0x80820007u), "SCE_FIOS_ERROR_BAD_OFFSET" },
    { static_cast<std::int32_t>(0x80820008u), "SCE_FIOS_ERROR_BAD_SIZE" },
    { static_cast<std::int32_t>(0x80820009u), "SCE_FIOS_ERROR_BAD_IOVCNT" },
    { static_cast<std::int32_t>(0x8082000Au), "SCE_FIOS_ERROR_BAD_OP" },
    { static_cast<std::int32_t>(0x8082000Bu), "SCE_FIOS_ERROR_BAD_FH" },
    { static_cast<std::int32_t>(0x8082000Cu), "SCE_FIOS_ERROR_BAD_DH" },
    { static_cast<std::int32_t>(0x8082000Du), "SCE_FIOS_ERROR_BAD_ALIGNMENT" },
    { static_cast<std::int32_t>(0x8082000Eu), "SCE_FIOS_ERROR_NOT_A_FILE" },
    { static_cast<std::int32_t>(0x8082000Fu), "SCE_FIOS_ERROR_NOT_A_DIRECTORY" },
    { static_cast<std::int32_t>(0x80820010u), "SCE_FIOS_ERROR_EOF" },
    { static_cast<std::int32_t>(0x80820011u), "SCE_FIOS_ERROR_TIMEOUT" },
    { static_cast<std::int32_t>(0x80820012u), "SCE_FIOS_ERROR_CANCELLED" },
    { static_cast<std::int32_t>(0x80820013u), "SCE_FIOS_ERROR_ACCESS" },
    { static_cast<std::int32_t>(0x80820014u), "SCE_FIOS_ERROR_DECOMPRESSION" },
    { static_cast<std::int32_t>(0x80820015u), "SCE_FIOS_ERROR_READ_ONLY" },
    { static_cast<std::int32_t>(0x80820016u), "SCE_FIOS_ERROR_WRITE_ONLY" },
    { static_cast<std::int32_t>(0x80820017u), "SCE_FIOS_ERROR_MEDIA_GONE" },
    { static_cast<std::int32_t>(0x80820018u), "SCE_FIOS_ERROR_PATH_TOO_LONG" },
    { static_cast<std::int32_t>(0x80820019u), "SCE_FIOS_ERROR_TOO_MANY_OVERLAYS" },
    { static_cast<std::int32_t>(0x8082001Au), "SCE_FIOS_ERROR_BAD_OVERLAY" },
    { static_cast<std::int32_t>(0x8082001Bu), "SCE_FIOS_ERROR_BAD_ORDER" },
    { static_cast<std::int32_t>(0x8082001Cu), "SCE_FIOS_ERROR_BAD_INDEX" },
    { static_cast<std::int32_t>(0x8082001Du), "SCE_FIOS_ERROR_EVENT_NOT_HANDLED" },
    { static_cast<std::int32_t>(0x8082001Eu), "SCE_FIOS_ERROR_BUSY" },
    { static_cast<std::int32_t>(0x8082001Fu), "SCE_FIOS_ERROR_BAD_ARCHIVE" },
    { static_cast<std::int32_t>(0x80820021u), "SCE_FIOS_ERROR_BAD_FLAGS" },
    { static_cast<std::int32_t>(0x80820022u), "SCE_FIOS_ERROR_UNKNOWN" },
    { static_cast<std::int32_t>(0x80820023u), "SCE_FIOS_ERROR_ALREADY_EXISTS" },
    { static_cast<std::int32_t>(0x80820024u), "SCE_FIOS_ERROR_IN_CALLBACK" },
};

void TestDebugDumpError() {
    for (const auto& entry : kKnownErrors) {
        const char* defaultBuf = sceFiosDebugDumpError(entry.code, nullptr, 0);
        Require(defaultBuf != nullptr);
        Require(std::strcmp(defaultBuf, entry.expected) == 0);

        char customBuf[128];
        std::memset(customBuf, 0xaa, sizeof(customBuf));
        const char* customRes = sceFiosDebugDumpError(entry.code, customBuf, sizeof(customBuf));
        Require(customRes == customBuf);
        Require(std::strcmp(customBuf, entry.expected) == 0);
        Require(static_cast<unsigned char>(customBuf[std::strlen(entry.expected) + 1]) == 0xaa);
    }

    char unknownBuf[128];
    const char* unkRes = sceFiosDebugDumpError(static_cast<std::int32_t>(0x8082ffffu), unknownBuf, sizeof(unknownBuf));
    Require(unkRes == unknownBuf);
    Require(std::strcmp(unknownBuf, "Unknown FIOS error 0x8082ffff") == 0);

    const char* unkDefault = sceFiosDebugDumpError(static_cast<std::int32_t>(0x80821234u), nullptr, 0);
    Require(unkDefault != nullptr);
    Require(std::strcmp(unkDefault, "Unknown FIOS error 0x80821234") == 0);

    char truncated[4];
    std::memset(truncated, 0, sizeof(truncated));
    const char* truncRes = sceFiosDebugDumpError(SCE_FIOS_OK, truncated, sizeof(truncated));
    Require(truncRes == truncated);
    Require(std::strcmp(truncated, "SCE") == 0);

    char zeroCapacity[8];
    std::memset(zeroCapacity, 0x77, sizeof(zeroCapacity));
    const char* zeroRes = sceFiosDebugDumpError(SCE_FIOS_OK, zeroCapacity, 0);
    Require(zeroRes == zeroCapacity);
    Require(static_cast<unsigned char>(zeroCapacity[0]) == 0x77);

    Require(sceFiosDebugDumpError(SCE_FIOS_OK, reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x10)), 64) == nullptr);
}

void TestDebugDumpDate() {
    char dateBuf[128];
    const char* res = sceFiosDebugDumpDate(0, dateBuf, sizeof(dateBuf));
    Require(res == dateBuf);
    Require(std::strcmp(dateBuf, "1970-01-01 00:00:00.000000000") == 0);

    const char* defaultRes = sceFiosDebugDumpDate(0, nullptr, 0);
    Require(defaultRes != nullptr);
    Require(std::strcmp(defaultRes, "1970-01-01 00:00:00.000000000") == 0);

    const SceFiosDate customDate = 1700000000ull * 1000000000ull + 987654321ull;
    res = sceFiosDebugDumpDate(customDate, dateBuf, sizeof(dateBuf));
    Require(res == dateBuf);
    Require(std::strncmp(dateBuf, "2023-11-14", 10) == 0);
    Require(std::strstr(dateBuf, ".987654321") != nullptr);

    char truncated[11];
    res = sceFiosDebugDumpDate(customDate, truncated, sizeof(truncated));
    Require(res == truncated);
    Require(std::strcmp(truncated, "2023-11-14") == 0);

    Require(sceFiosDebugDumpDate(0, reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x10)), 64) == nullptr);
}

void TestDebugDumpHandles() {
    const SceFiosHandle invalidHandles[] = { 0, -1, 100, 999999 };
    for (const SceFiosHandle handle : invalidHandles) {
        char fhBuf[128];
        const char* fhRes = sceFiosDebugDumpFH(handle, fhBuf, sizeof(fhBuf));
        Require(fhRes == fhBuf);
        Require(std::strncmp(fhBuf, "Invalid FH ", 11) == 0);

        const char* fhDefault = sceFiosDebugDumpFH(handle, nullptr, 0);
        Require(fhDefault != nullptr);
        Require(std::strncmp(fhDefault, "Invalid FH ", 11) == 0);

        char dhBuf[128];
        const char* dhRes = sceFiosDebugDumpDH(handle, dhBuf, sizeof(dhBuf));
        Require(dhRes == dhBuf);
        Require(std::strncmp(dhBuf, "Invalid DH ", 11) == 0);

        const char* dhDefault = sceFiosDebugDumpDH(handle, nullptr, 0);
        Require(dhDefault != nullptr);
        Require(std::strncmp(dhDefault, "Invalid DH ", 11) == 0);

        char opBuf[128];
        const char* opRes = sceFiosDebugDumpOp(handle, opBuf, sizeof(opBuf));
        Require(opRes == opBuf);
        Require(std::strncmp(opBuf, "Invalid op ", 11) == 0);

        const char* opDefault = sceFiosDebugDumpOp(handle, nullptr, 0);
        Require(opDefault != nullptr);
        Require(std::strncmp(opDefault, "Invalid op ", 11) == 0);
    }

    Require(sceFiosDebugDumpFH(0, reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x10)), 64) == nullptr);
    Require(sceFiosDebugDumpDH(0, reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x10)), 64) == nullptr);
    Require(sceFiosDebugDumpOp(0, reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x10)), 64) == nullptr);
}

void TestTimestamps() {
    Require(sceFiosTraceTimestamp(nullptr, 64) == SCE_FIOS_ERROR_BAD_PTR);
    char traceBuf[64];
    Require(sceFiosTraceTimestamp(traceBuf, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosTraceTimestamp(nullptr, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosTraceTimestamp(reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x10)), 64) == SCE_FIOS_ERROR_BAD_PTR);

    std::memset(traceBuf, 0, sizeof(traceBuf));
    Require(sceFiosTraceTimestamp(traceBuf, sizeof(traceBuf)) == SCE_FIOS_OK);
    Require(traceBuf[0] != '\0');
    Require(std::strchr(traceBuf, '.') != nullptr);

    const std::int64_t t1 = sceFiosTimeGetCurrent();
    const std::int64_t t2 = sceFiosTimeGetCurrent();
    Require(t1 > 0);
    Require(t2 >= t1);

    Require(sceFiosSaveTimeStamp(nullptr, 1) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosSaveTimeStamp(reinterpret_cast<const char*>(static_cast<std::uintptr_t>(0x10)), 1) == SCE_FIOS_ERROR_BAD_PTR);

    char longTag[300];
    std::memset(longTag, 'x', sizeof(longTag) - 1);
    longTag[sizeof(longTag) - 1] = '\0';
    Require(sceFiosSaveTimeStamp(longTag, 2) == SCE_FIOS_ERROR_PATH_TOO_LONG);

    sceFiosClearTimeStamps();
    const std::int64_t tid1 = sceFiosSaveTimeStamp("alpha", 10);
    Require(tid1 > 0);
    const std::int64_t tid2 = sceFiosSaveTimeStamp("beta", 20);
    Require(tid2 == tid1);

    const std::int64_t printResult = sceFiosPrintTimeStamps();
    Require(printResult >= 0);

    sceFiosClearTimeStamps();
    const std::int64_t printEmpty = sceFiosPrintTimeStamps();
    Require(printEmpty >= 0);
}

void TestDLLLifecycle() {
    void* countPtr = sceFiosDLLInitialize();
    Require(countPtr != nullptr);
    Require(sceFiosDLLTerminate() == 0);
}

} // namespace

int main() {
    TestDebugDumpError();
    TestDebugDumpDate();
    TestDebugDumpHandles();
    TestTimestamps();
    TestDLLLifecycle();
    return 0;
}
