#pragma once

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

using SceFiosHandle = std::int32_t;
using SceFiosOp = SceFiosHandle;
using SceFiosFH = SceFiosHandle;
using SceFiosDH = SceFiosHandle;
using SceFiosTime = std::int64_t;
using SceFiosTimeInterval = std::int64_t;
using SceFiosDate = std::uint64_t;
using SceFiosSize = std::int64_t;
using SceFiosOffset = std::int64_t;
using SceFiosOpEvent = std::uint8_t;

using SceFiosOpCallback = int (APS5_VABI*)(void* context, SceFiosOp op, SceFiosOpEvent event, int error);

constexpr std::int32_t SCE_FIOS_OK = 0;
constexpr std::int32_t SCE_FIOS_ERROR_UNIMPLEMENTED = static_cast<std::int32_t>(0x80820000u);
constexpr std::int32_t SCE_FIOS_ERROR_CANT_ALLOCATE_OP = static_cast<std::int32_t>(0x80820001u);
constexpr std::int32_t SCE_FIOS_ERROR_CANT_ALLOCATE_FH = static_cast<std::int32_t>(0x80820002u);
constexpr std::int32_t SCE_FIOS_ERROR_CANT_ALLOCATE_DH = static_cast<std::int32_t>(0x80820003u);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_PATH = static_cast<std::int32_t>(0x80820005u);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_PTR = static_cast<std::int32_t>(0x80820006u);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_OFFSET = static_cast<std::int32_t>(0x80820007u);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_SIZE = static_cast<std::int32_t>(0x80820008u);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_IOVCNT = static_cast<std::int32_t>(0x80820009u);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_OP = static_cast<std::int32_t>(0x8082000Au);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_FH = static_cast<std::int32_t>(0x8082000Bu);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_DH = static_cast<std::int32_t>(0x8082000Cu);
constexpr std::int32_t SCE_FIOS_ERROR_NOT_A_FILE = static_cast<std::int32_t>(0x8082000Eu);
constexpr std::int32_t SCE_FIOS_ERROR_NOT_A_DIRECTORY = static_cast<std::int32_t>(0x8082000Fu);
constexpr std::int32_t SCE_FIOS_ERROR_EOF = static_cast<std::int32_t>(0x80820010u);
constexpr std::int32_t SCE_FIOS_ERROR_CANCELLED = static_cast<std::int32_t>(0x80820012u);
constexpr std::int32_t SCE_FIOS_ERROR_ACCESS = static_cast<std::int32_t>(0x80820013u);
constexpr std::int32_t SCE_FIOS_ERROR_READ_ONLY = static_cast<std::int32_t>(0x80820015u);
constexpr std::int32_t SCE_FIOS_ERROR_WRITE_ONLY = static_cast<std::int32_t>(0x80820016u);
constexpr std::int32_t SCE_FIOS_ERROR_PATH_TOO_LONG = static_cast<std::int32_t>(0x80820018u);
constexpr std::int32_t SCE_FIOS_ERROR_BAD_FLAGS = static_cast<std::int32_t>(0x80820021u);
constexpr std::int32_t SCE_FIOS_ERROR_UNKNOWN = static_cast<std::int32_t>(0x80820022u);
constexpr std::int32_t SCE_FIOS_ERROR_ALREADY_EXISTS = static_cast<std::int32_t>(0x80820023u);

constexpr SceFiosOpEvent SCE_FIOS_OPEVENT_COMPLETE = 1;
constexpr SceFiosOpEvent SCE_FIOS_OPEVENT_DELETE = 2;

constexpr std::int32_t SCE_FIOS_SEEK_SET = 0;
constexpr std::int32_t SCE_FIOS_SEEK_CUR = 1;
constexpr std::int32_t SCE_FIOS_SEEK_END = 2;

constexpr std::uint32_t SCE_FIOS_O_READ = 1u << 0;
constexpr std::uint32_t SCE_FIOS_O_WRITE = 1u << 1;
constexpr std::uint32_t SCE_FIOS_O_APPEND = 1u << 2;
constexpr std::uint32_t SCE_FIOS_O_CREAT = 1u << 3;
constexpr std::uint32_t SCE_FIOS_O_TRUNC = 1u << 4;
constexpr std::uint32_t SCE_FIOS_O_DIRECT = 1u << 12;

constexpr std::uint32_t SCE_FIOS_STAT_DIRECTORY = 1u << 0;

constexpr std::size_t SCE_FIOS_PATH_MAX = 1024;

struct SceFiosBuffer {
    void* pPtr;
    std::uint64_t length;
};

struct SceFiosOpAttr {
    SceFiosTime deadline;
    SceFiosOpCallback pCallback;
    void* pCallbackContext;
    std::int32_t priority : 8;
    std::uint32_t opflags : 24;
    std::uint32_t userTag;
    void* userPtr;
    void* pReserved;
};

struct SceFiosOpenParams {
    std::uint32_t openFlags : 16;
    std::uint32_t opFlags : 16;
    std::uint32_t reserved;
    SceFiosBuffer buffer;
};

struct SceFiosStat {
    SceFiosOffset fileSize;
    SceFiosDate accessDate;
    SceFiosDate modificationDate;
    SceFiosDate creationDate;
    std::uint32_t statFlags;
    std::uint32_t reserved;
    std::int64_t uid;
    std::int64_t gid;
    std::int64_t dev;
    std::int64_t ino;
    std::int64_t mode;
};

struct SceFiosDirEntry {
    SceFiosOffset fileSize;
    std::uint32_t statFlags;
    std::uint16_t nameLength;
    std::uint16_t fullPathLength;
    std::uint16_t offsetToName;
    std::uint16_t reserved[3];
    char fullPath[SCE_FIOS_PATH_MAX];
};

struct SceFiosParams;

static_assert(sizeof(SceFiosBuffer) == 16);
static_assert(sizeof(SceFiosOpAttr) == 48);
static_assert(sizeof(SceFiosOpenParams) == 24);
static_assert(sizeof(SceFiosStat) == 80);
static_assert(sizeof(SceFiosDirEntry) == 24 + SCE_FIOS_PATH_MAX);

struct SceFiosTm {
    std::int32_t tm_sec;
    std::int32_t tm_min;
    std::int32_t tm_hour;
    std::int32_t tm_mday;
    std::int32_t tm_mon;
    std::int32_t tm_year;
    std::int32_t tm_wday;
    std::int32_t tm_yday;
    std::int32_t tm_isdst;
};
