#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

namespace ContentSearch_nid_no_patch {

enum class Column : std::int32_t {
    ContentId = 1, MimeType = 2, ContentType = 3, Title = 4, CreatedTime = 5,
    Size = 6, UserAccount = 7, Duration = 9, ContentPath = 10, GeneratorType = 11,
    Status = 12, UploadStatus = 14
};

enum class Comparison : std::int32_t { Equal, NotEqual, Greater, GreaterEqual, Less, LessEqual };
enum class Connector : std::int32_t { And, Or, None };
enum class Sort : std::int32_t { Ascending, Descending };
enum class MetadataType : std::int32_t { Int, Float, Text, Tick = 4 };

struct ColumnValue { std::uint32_t size; std::int64_t value; };
struct ContentColumn { Column column; Comparison comparison; const ColumnValue* value; Connector connector; };
struct ColumnSet { const ContentColumn* columns; std::int32_t length; Connector connector; };
struct OrderBy { Column column; Sort sort; };
struct MetadataValue { std::int32_t size; MetadataType type; std::int64_t value; };

struct ContentInfo {
    std::int64_t contentId;
    std::int32_t duration;
    std::int32_t mimeType;
    std::int32_t contentType;
    std::int32_t generatorType;
    char contentPath[1025];
    std::uint8_t reserved1[11];
    char title[257];
    std::uint8_t reserved2[6];
    char iconPath[1025];
    std::uint8_t reserved3[8];
    std::int32_t uploadStatus;
    std::uint64_t createdTime;
    std::int64_t size;
    std::int32_t status;
    std::int32_t accounts[4];
    std::int32_t reserved4;
};

static_assert(sizeof(ColumnValue) == 16);
static_assert(sizeof(ContentColumn) == 24);
static_assert(sizeof(ColumnSet) == 16);
static_assert(sizeof(OrderBy) == 8);
static_assert(sizeof(MetadataValue) == 16);
static_assert(sizeof(ContentInfo) == 2400);
static_assert(offsetof(ContentInfo, contentPath) == 0x18);
static_assert(offsetof(ContentInfo, title) == 0x424);
static_assert(offsetof(ContentInfo, iconPath) == 0x52b);
static_assert(offsetof(ContentInfo, uploadStatus) == 0x934);
static_assert(offsetof(ContentInfo, createdTime) == 0x938);
static_assert(offsetof(ContentInfo, accounts) == 0x94c);

}
