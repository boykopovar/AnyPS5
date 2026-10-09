#include "Types.hpp"
#include "prx/libSceContentExport/ContentCatalog.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace ContentSearch_nid_no_patch;
using Entry = ContentCatalog_nid_no_patch::Entry;
using Value = std::variant<std::int64_t, std::uint64_t, std::string>;
std::mutex stateMutex;
bool initialized = false;
std::map<std::int32_t, Entry> metadata;
std::int32_t nextMetadataId = 1;
constexpr int ErrorNotInitialized = static_cast<int>(0x809d1001u);
constexpr int ErrorLimitTooBig = static_cast<int>(0x809d100au);

std::int32_t MimeType(const Entry& entry) {
    constexpr std::pair<std::string_view, std::int32_t> types[] = {
        {"image/jpeg", 1}, {"video/mp4", 2}, {"image/png", 3}, {"image/gif", 4}, {"video/webm", 5},
        {"audio/mpeg", 0x1000}, {"audio/mp4", 0x1001}, {"audio/mp3", 0x1002}, {"audio/aac", 0x1003},
        {"audio/wav", 0x1004}, {"audio/vorbis", 0x1005}, {"audio/opus", 0x1006}
    };
    for (const auto& [name, id] : types) if (entry.mimeType == name) return id;
    throw std::logic_error("content search: unknown MIME type");
}

Value ColumnData(const Entry& entry, Column column) {
    switch (column) {
    case Column::ContentId: return entry.id;
    case Column::MimeType: return static_cast<std::int64_t>(MimeType(entry));
    case Column::ContentType: return static_cast<std::int64_t>(entry.contentType);
    case Column::Title: return entry.title;
    case Column::CreatedTime: return entry.createdTime;
    case Column::Size: return entry.size;
    case Column::ContentPath: return entry.path;
    case Column::GeneratorType: return static_cast<std::int64_t>(entry.generatorType);
    case Column::Status: return static_cast<std::int64_t>(entry.status);
    case Column::UploadStatus: return static_cast<std::int64_t>(entry.uploadStatus);
    default: throw std::logic_error("content search: unsupported column");
    }
}

int Compare(const Value& left, const Value& right) {
    if (left.index() != right.index()) throw std::logic_error("content search: mismatched value type");
    return std::visit([&](const auto& value) {
        const auto& other = std::get<std::decay_t<decltype(value)>>(right);
        return value < other ? -1 : value > other ? 1 : 0;
    }, left);
}

Value FilterValue(const ContentColumn& column) {
    if (column.value == nullptr) throw std::invalid_argument("content search: missing column value");
    if (column.column == Column::Title || column.column == Column::ContentPath) {
        if (column.value->value == 0 || column.value->size == 0 || column.value->size > 1025) throw std::invalid_argument("content search: invalid text value");
        const auto* text = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(column.value->value));
        const auto* end = static_cast<const char*>(std::memchr(text, 0, column.value->size));
        if (end == nullptr) throw std::invalid_argument("content search: unterminated text value");
        return std::string(text, end);
    }
    if (column.value->size != 8) throw std::invalid_argument("content search: invalid integer value size");
    if (column.column == Column::CreatedTime) return std::bit_cast<std::uint64_t>(column.value->value);
    return column.value->value;
}

bool MatchesColumn(const Entry& entry, const ContentColumn& column) {
    const auto value = FilterValue(column);
    if (column.column == Column::UserAccount) {
        if (column.comparison != Comparison::Equal && column.comparison != Comparison::NotEqual) throw std::logic_error("content search: unsupported account comparison");
        const auto user = std::get<std::int64_t>(value);
        const bool exists = std::find(entry.accounts.begin(), entry.accounts.end(), user) != entry.accounts.end();
        return column.comparison == Comparison::Equal ? exists : !exists;
    }
    const int difference = Compare(ColumnData(entry, column.column), value);
    switch (column.comparison) {
    case Comparison::Equal: return difference == 0;
    case Comparison::NotEqual: return difference != 0;
    case Comparison::Greater: return difference > 0;
    case Comparison::GreaterEqual: return difference >= 0;
    case Comparison::Less: return difference < 0;
    case Comparison::LessEqual: return difference <= 0;
    default: throw std::logic_error("content search: unsupported comparison");
    }
}

bool Join(bool left, bool right, Connector connector) {
    switch (connector) {
    case Connector::And: return left && right;
    case Connector::Or: return left || right;
    default: throw std::logic_error("content search: unsupported connector");
    }
}

void ValidateConditions(const ColumnSet* sets, std::uint32_t length) {
    Connector setConnector = Connector::None;
    for (std::uint32_t i = 0; i < length; ++i) {
        const auto& set = sets[i];
        if (set.length <= 0 || set.columns == nullptr) throw std::invalid_argument("content search: empty column set");
        Connector columnConnector = Connector::None;
        for (std::int32_t j = 0; j < set.length; ++j) {
            const auto& column = set.columns[j];
            FilterValue(column);
            switch (column.column) {
            case Column::ContentId: case Column::MimeType: case Column::ContentType: case Column::Title:
            case Column::CreatedTime: case Column::Size: case Column::UserAccount: case Column::ContentPath:
            case Column::GeneratorType: case Column::Status: case Column::UploadStatus: break;
            default: throw std::logic_error("content search: unsupported column");
            }
            if (column.column == Column::UserAccount && column.comparison != Comparison::Equal && column.comparison != Comparison::NotEqual)
                throw std::logic_error("content search: unsupported account comparison");
            if (column.comparison < Comparison::Equal || column.comparison > Comparison::LessEqual) throw std::logic_error("content search: unsupported comparison");
            if (j + 1 < set.length) {
                if (column.connector != Connector::And && column.connector != Connector::Or) throw std::logic_error("content search: missing column connector");
                if (columnConnector != Connector::None && columnConnector != column.connector) throw std::logic_error("content search: mixed column connector precedence is unknown");
                columnConnector = column.connector;
            } else if (column.connector != Connector::None) throw std::logic_error("content search: unexpected final column connector");
        }
        if (i + 1 < length) {
            if (set.connector != Connector::And && set.connector != Connector::Or) throw std::logic_error("content search: missing set connector");
            if (setConnector != Connector::None && setConnector != set.connector) throw std::logic_error("content search: mixed set connector precedence is unknown");
            setConnector = set.connector;
        } else if (set.connector != Connector::None) throw std::logic_error("content search: unexpected final set connector");
    }
}

bool Matches(const Entry& entry, const ColumnSet* sets, std::uint32_t length) {
    bool result = true;
    for (std::uint32_t i = 0; i < length; ++i) {
        const auto& set = sets[i];
        bool group = MatchesColumn(entry, set.columns[0]);
        for (std::int32_t j = 1; j < set.length; ++j) group = Join(group, MatchesColumn(entry, set.columns[j]), set.columns[j - 1].connector);
        result = i == 0 ? group : Join(result, group, sets[i - 1].connector);
    }
    return result;
}

void ValidateOrder(const OrderBy* conditions, std::uint32_t length) {
    for (std::uint32_t i = 0; i < length; ++i) {
        const auto& condition = conditions[i];
        if (condition.sort != Sort::Ascending && condition.sort != Sort::Descending) throw std::logic_error("content search: unsupported sort direction");
        switch (condition.column) {
        case Column::ContentId: case Column::Title: case Column::CreatedTime: case Column::Size: case Column::ContentPath: break;
        default: throw std::logic_error("content search: unsupported sort column");
        }
    }
}

ContentInfo MakeInfo(const Entry& entry) {
    if (entry.path.size() > 1024 || entry.iconPath.size() > 1024 || entry.title.size() > 256) throw std::logic_error("content search: content text exceeds guest capacity");
    if (entry.hasDuration || entry.contentType == 2) throw std::logic_error("content search: content duration units are unknown");
    ContentInfo info{};
    info.contentId = entry.id;
    info.mimeType = MimeType(entry);
    info.contentType = entry.contentType;
    info.generatorType = entry.generatorType;
    std::memcpy(info.contentPath, entry.path.c_str(), entry.path.size() + 1);
    std::memcpy(info.title, entry.title.c_str(), entry.title.size() + 1);
    std::memcpy(info.iconPath, entry.iconPath.c_str(), entry.iconPath.size() + 1);
    info.uploadStatus = entry.uploadStatus;
    info.createdTime = entry.createdTime;
    info.size = entry.size;
    info.status = entry.status;
    std::copy(entry.accounts.begin(), entry.accounts.end(), info.accounts);
    return info;
}

std::pair<MetadataType, Value> Field(const Entry& entry, std::string_view field) {
    if (field == "title") return {MetadataType::Text, entry.title};
    if (field == "created_time") return {MetadataType::Tick, entry.createdTime};
    if (field == "size") return {MetadataType::Int, entry.size};
    if (field == "width" && entry.hasDimensions) return {MetadataType::Int, static_cast<std::int64_t>(entry.width)};
    if (field == "height" && entry.hasDimensions) return {MetadataType::Int, static_cast<std::int64_t>(entry.height)};
    throw std::logic_error("content search: unavailable metadata field");
}

std::int32_t FieldSize(const Value& value) {
    if (const auto* text = std::get_if<std::string>(&value)) {
        if (text->size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) throw std::overflow_error("content search: metadata size overflow");
        return static_cast<std::int32_t>(text->size() + 1);
    }
    return 8;
}

const Entry& GetMetadata(std::int32_t metadataId) {
    const auto found = metadata.find(metadataId);
    if (found == metadata.end()) throw std::invalid_argument("content search: unknown metadata id");
    return found->second;
}
}

extern "C" {
int APS5_VABI sceContentSearchInit(const ContentSearchInitParam* initParam) {
    if (initParam == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(stateMutex);
    if (initialized) throw std::logic_error("content search: already initialized");
    initialized = true;
    return 0;
}

int APS5_VABI sceContentSearchTerm() {
    std::lock_guard lock(stateMutex);
    if (!initialized) return ErrorNotInitialized;
    metadata.clear();
    initialized = false;
    return 0;
}

int APS5_VABI sceContentSearchSearchContent(const ColumnSet* columnSets, std::uint32_t columnSetLength, const OrderBy* orderBy, std::uint32_t orderByLength, std::uint32_t offset, std::uint32_t limit, std::int64_t* numOfContent, ContentInfo* infos, std::int64_t* lastUpdateId) {
    if (numOfContent == nullptr || (columnSets == nullptr && columnSetLength != 0) || (orderBy == nullptr && orderByLength != 0) || (infos == nullptr && limit != 0)) APS5_INVALID_ARG_EX;
    std::lock_guard lock(stateMutex);
    if (!initialized) return ErrorNotInitialized;
    if (limit > 92) return ErrorLimitTooBig;
    ValidateConditions(columnSets, columnSetLength);
    ValidateOrder(orderBy, orderByLength);
    auto snapshot = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    std::erase_if(snapshot.entries, [&](const Entry& entry) { return !Matches(entry, columnSets, columnSetLength); });
    std::stable_sort(snapshot.entries.begin(), snapshot.entries.end(), [&](const Entry& left, const Entry& right) {
        for (std::uint32_t i = 0; i < orderByLength; ++i) {
            const int difference = Compare(ColumnData(left, orderBy[i].column), ColumnData(right, orderBy[i].column));
            if (difference != 0) return orderBy[i].sort == Sort::Ascending ? difference < 0 : difference > 0;
        }
        return left.id < right.id;
    });
    const auto start = std::min<std::size_t>(offset, snapshot.entries.size());
    const auto count = std::min<std::size_t>(limit, snapshot.entries.size() - start);
    std::vector<ContentInfo> rows;
    rows.reserve(count);
    for (std::size_t i = 0; i < count; ++i) rows.push_back(MakeInfo(snapshot.entries[start + i]));
    if (count != 0) std::copy(rows.begin(), rows.end(), infos);
    *numOfContent = static_cast<std::int64_t>(count);
    if (lastUpdateId != nullptr) *lastUpdateId = snapshot.updateId;
    return 0;
}

int APS5_VABI sceContentSearchOpenMetadata(const char* filePath, std::int32_t* metadataId) {
    if (filePath == nullptr || metadataId == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(stateMutex);
    if (!initialized) return ErrorNotInitialized;
    const auto snapshot = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    const auto found = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const Entry& entry) { return entry.path == filePath; });
    if (found == snapshot.entries.end()) throw std::invalid_argument("content search: path not in content library");
    if (nextMetadataId == std::numeric_limits<std::int32_t>::max()) throw std::overflow_error("content search: metadata handle exhaustion");
    const auto id = nextMetadataId;
    metadata.emplace(id, *found);
    ++nextMetadataId;
    *metadataId = id;
    return 0;
}

int APS5_VABI sceContentSearchCloseMetadata(std::int32_t metadataId) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return ErrorNotInitialized;
    GetMetadata(metadataId);
    metadata.erase(metadataId);
    return 0;
}

int APS5_VABI sceContentSearchGetMetadataFieldInfo(std::int32_t metadataId, const char* field, MetadataType* type, std::int32_t* size) {
    if (field == nullptr || type == nullptr || size == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(stateMutex);
    if (!initialized) return ErrorNotInitialized;
    const auto [fieldType, value] = Field(GetMetadata(metadataId), field);
    const auto fieldSize = FieldSize(value);
    *type = fieldType;
    *size = fieldSize;
    return 0;
}

int APS5_VABI sceContentSearchGetMetadataValue(std::int32_t metadataId, const char* field, MetadataValue* output) {
    if (field == nullptr || output == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(stateMutex);
    if (!initialized) return ErrorNotInitialized;
    const auto [type, value] = Field(GetMetadata(metadataId), field);
    const auto size = FieldSize(value);
    if (const auto* text = std::get_if<std::string>(&value)) {
        if (output->value == 0 || output->size < size) APS5_INVALID_ARG_EX;
        auto* destination = reinterpret_cast<char*>(static_cast<std::uintptr_t>(output->value));
        std::memcpy(destination, text->c_str(), size);
    } else {
        output->value = std::visit([](const auto& data) -> std::int64_t {
            using Type = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<Type, std::int64_t>) return data;
            else if constexpr (std::is_same_v<Type, std::uint64_t>) return std::bit_cast<std::int64_t>(data);
            else throw std::logic_error("content search: unsupported metadata value");
        }, value);
    }
    output->size = size;
    output->type = type;
    return 0;
}
}
