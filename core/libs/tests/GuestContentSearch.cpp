#include "prx/libSceContentSearch/Types.hpp"
#include "prx/libSceContentExport/ContentCatalog.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace ContentSearch_nid_no_patch;
extern "C" {
int APS5_VABI sceContentSearchInit(const ContentSearchInitParam* initParam);
int APS5_VABI sceContentSearchTerm();
int APS5_VABI sceContentSearchSearchContent(const ColumnSet* sets, std::uint32_t setLength, const OrderBy* order, std::uint32_t orderLength, std::uint32_t offset, std::uint32_t limit, std::int64_t* count, ContentInfo* infos, std::int64_t* updateId);
int APS5_VABI sceContentSearchOpenMetadata(const char* path, std::int32_t* metadataId);
int APS5_VABI sceContentSearchCloseMetadata(std::int32_t metadataId);
int APS5_VABI sceContentSearchGetMetadataFieldInfo(std::int32_t metadataId, const char* field, MetadataType* type, std::int32_t* size);
int APS5_VABI sceContentSearchGetMetadataValue(std::int32_t metadataId, const char* field, MetadataValue* value);
int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* initParam);
int APS5_VABI sceContentExportTerm();
int APS5_VABI sceContentExportStart();
int APS5_VABI sceContentExportFinish(int session);
int APS5_VABI sceContentExportFromFileWithThumbnail(int session, const ContentExportParam* param, const char* path, const char* thumbnail, char* output, std::size_t capacity);
int APS5_VABI sceContentExportFromData(int session, const ContentExportParam* param, std::size_t length, ContentExportDataProvideFunction provide, void* userData, char* output, std::size_t capacity);
}

namespace {
void Check(bool value, int line) {
    if (!value) { std::fprintf(stderr, "Content search failed at line %d\n", line); std::abort(); }
}
#define Require(value) Check((value), __LINE__)

template<typename TAction>
bool Throws(TAction action) {
    try { action(); } catch (const std::logic_error&) { return true; }
    return false;
}

constexpr std::array<std::uint8_t, 70> Png{
    0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
    0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x01,0x10,0x00,0x00,0x00,0x00,0x81,0xD9,0xFC,
    0x15,0x00,0x00,0x00,0x0D,0x49,0x44,0x41,0x54,0x78,0x9C,0x63,0x10,0x32,0x59,0x7D,
    0x16,0x00,0x03,0x0C,0x01,0xBF,0x6E,0xB9,0xC6,0x5D,0x00,0x00,0x00,0x00,0x49,0x45,
    0x4E,0x44,0xAE,0x42,0x60,0x82
};

void* APS5_VABI Allocate(std::size_t size, void*) { return std::malloc(size); }
void APS5_VABI Release(void* pointer, void*) { std::free(pointer); }
int APS5_VABI Provide(void** data, std::size_t* size, void*) {
    *data = const_cast<std::uint8_t*>(Png.data());
    *size = Png.size();
    return 1;
}

void Roundtrip() {
    std::filesystem::remove_all(ContentCatalog_nid_no_patch::Root_nid_no_patch());
    {
        std::ofstream stream("source.png", std::ios::binary);
        stream.write(reinterpret_cast<const char*>(Png.data()), Png.size());
        stream.close();
        Require(static_cast<bool>(stream));
    }
    ContentExportInitParam2 exportInit{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), nullptr, 4096, 0, 0};
    Require(sceContentExportInit2(&exportInit) == 0);
    const int session = sceContentExportStart();
    Require(session >= 0);
    ContentExportParam param{};
    std::strcpy(param.title, "File image");
    std::strcpy(param.content_type, "image/png");
    std::array<char, 1025> filePath{};
    std::array<char, 1025> dataPath{};
    Require(sceContentExportFromFileWithThumbnail(session, &param, "/source.png", "/source.png", filePath.data(), filePath.size()) == 0);
    std::strcpy(param.title, "Callback image");
    Require(sceContentExportFromData(session, &param, Png.size(), &Provide, nullptr, dataPath.data(), dataPath.size()) == 0);
    Require(sceContentExportFinish(session) == 0 && sceContentExportTerm() == 0);
    ContentSearchInitParam init{3 * 1024 * 1024};
    Require(sceContentSearchInit(&init) == 0);
    ColumnValue value{8, 1};
    ContentColumn column{Column::ContentType, Comparison::Equal, &value, Connector::None};
    ColumnSet set{&column, 1, Connector::None};
    OrderBy order{Column::Title, Sort::Ascending};
    std::array<ContentInfo, 2> rows{};
    std::int64_t count = -1;
    std::int64_t update = -1;
    Require(sceContentSearchSearchContent(&set, 1, &order, 1, 0, rows.size(), &count, rows.data(), &update) == 0);
    Require(count == 2 && update > 0);
    Require(std::string(rows[0].title) == "Callback image" && std::string(rows[0].contentPath) == dataPath.data());
    Require(std::string(rows[1].title) == "File image" && std::string(rows[1].contentPath) == filePath.data());
    Require(rows[1].iconPath[0] != 0 && rows[1].size == Png.size() && rows[1].mimeType == 3);
    Require(rows[0].accounts[0] == USER_SERVICE_INITIAL_USER_ID && rows[1].accounts[0] == USER_SERVICE_INITIAL_USER_ID);
    ColumnValue account{8, USER_SERVICE_INITIAL_USER_ID};
    ContentColumn accountColumn{Column::UserAccount, Comparison::Equal, &account, Connector::None};
    ColumnSet accountSet{&accountColumn, 1, Connector::None};
    std::array<ContentInfo, 2> accountRows{};
    Require(sceContentSearchSearchContent(&accountSet, 1, nullptr, 0, 0, accountRows.size(), &count, accountRows.data(), nullptr) == 0 && count == 2);
    const auto verifyBytes = [](const char* path) {
        std::ifstream stream(ResolvePath_nid_no_patch(path), std::ios::binary);
        std::array<std::uint8_t, Png.size()> actual{};
        stream.read(reinterpret_cast<char*>(actual.data()), actual.size());
        Require(static_cast<bool>(stream) && actual == Png && stream.peek() == std::char_traits<char>::eof());
    };
    verifyBytes(rows[0].contentPath);
    verifyBytes(rows[1].contentPath);
    verifyBytes(rows[1].iconPath);
    std::int32_t handle = -1;
    Require(sceContentSearchOpenMetadata(filePath.data(), &handle) == 0);
    MetadataType type{};
    std::int32_t size = -1;
    Require(sceContentSearchGetMetadataFieldInfo(handle, "title", &type, &size) == 0 && type == MetadataType::Text && size == 11);
    std::array<char, 11> title{};
    MetadataValue result{title.size(), MetadataType::Int, reinterpret_cast<std::int64_t>(title.data())};
    Require(sceContentSearchGetMetadataValue(handle, "title", &result) == 0 && std::string(title.data()) == "File image");
    Require(sceContentSearchGetMetadataValue(handle, "width", &result) == 0 && result.type == MetadataType::Int && result.value == 2);
    Require(sceContentSearchGetMetadataValue(handle, "height", &result) == 0 && result.value == 1);
    Require(sceContentSearchGetMetadataValue(handle, "size", &result) == 0 && result.value == Png.size());
    Require(sceContentSearchGetMetadataValue(handle, "created_time", &result) == 0 && result.type == MetadataType::Tick && std::bit_cast<std::uint64_t>(result.value) == rows[1].createdTime);
    Require(sceContentSearchCloseMetadata(handle) == 0 && sceContentSearchTerm() == 0);
    Require(sceContentSearchInit(&init) == 0);
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr) == 0 && count == 2);
    Require(sceContentSearchTerm() == 0);
}

ContentCatalog_nid_no_patch::Entry Publish(const std::string& title, const std::string& contents, std::int32_t user) {
    using namespace ContentCatalog_nid_no_patch;
    Transaction_nid_no_patch transaction;
    Entry entry;
    entry.id = transaction.Id_nid_no_patch();
    entry.title = title;
    entry.mimeType = "image/png";
    entry.contentType = 1;
    entry.createdTime = CurrentTick_nid_no_patch();
    entry.size = contents.size();
    entry.width = 4;
    entry.height = 3;
    entry.hasDimensions = true;
    entry.accounts = {user, -1, -1, -1};
    entry.path = transaction.GuestPath_nid_no_patch("content.png");
    std::ofstream stream(transaction.Directory_nid_no_patch() / "content.png", std::ios::binary);
    stream.write(contents.data(), contents.size());
    stream.close();
    Require(static_cast<bool>(stream));
    transaction.Commit_nid_no_patch(entry);
    const auto published = Read_nid_no_patch();
    const auto found = std::find_if(published.entries.begin(), published.entries.end(), [&](const Entry& item) { return item.id == entry.id; });
    Require(found != published.entries.end());
    return *found;
}
}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-content-search-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::current_path(root);
    ContentSearchInitParam init{3 * 1024 * 1024};
    std::array<ContentInfo, 4> rows{};
    std::int64_t count = -1;
    std::int64_t update = -1;
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, rows.size(), &count, rows.data(), &update) == static_cast<int>(0x809d1001u));
    Require(count == -1 && update == -1);
    Require(Throws([&] { sceContentSearchInit(nullptr); }));
    Require(sceContentSearchInit(&init) == 0);
    Require(Throws([&] { sceContentSearchInit(&init); }));
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, rows.size(), &count, rows.data(), &update) == 0);
    Require(count == 0);
    const auto zebra = Publish("Zebra", "abcde", 7);
    const auto alpha = Publish("Alpha", "abcdefghi", 8);
    OrderBy order{Column::Title, Sort::Ascending};
    Require(sceContentSearchSearchContent(nullptr, 0, &order, 1, 0, rows.size(), &count, rows.data(), &update) == 0);
    Require(count == 2 && update > 0 && rows[0].contentId == alpha.id && rows[1].contentId == zebra.id);
    Require(std::string(rows[0].title) == "Alpha" && std::string(rows[0].contentPath) == alpha.path);
    Require(rows[0].mimeType == 3 && rows[0].generatorType == 2 && rows[0].status == 1 && rows[0].accounts[0] == 8);
    Require(rows[0].size == 9 && rows[0].createdTime == alpha.createdTime);
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 92, &count, rows.data(), nullptr) == 0 && count == 2);
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 0, &count, nullptr, nullptr) == 0 && count == 0);
    Require(sceContentSearchSearchContent(nullptr, 0, &order, 1, 1, 1, &count, rows.data(), nullptr) == 0);
    Require(count == 1 && rows[0].contentId == zebra.id);
    Require(sceContentSearchSearchContent(nullptr, 0, &order, 1, 99, 1, &count, rows.data(), nullptr) == 0 && count == 0);
    order.sort = Sort::Descending;
    Require(sceContentSearchSearchContent(nullptr, 0, &order, 1, 0, 1, &count, rows.data(), nullptr) == 0 && rows[0].contentId == zebra.id);
    ColumnValue minimum{8, 5};
    ColumnValue user{8, 8};
    std::array<ContentColumn, 2> columns{{
        {Column::Size, Comparison::Greater, &minimum, Connector::And},
        {Column::UserAccount, Comparison::Equal, &user, Connector::None}
    }};
    ColumnSet set{columns.data(), static_cast<std::int32_t>(columns.size()), Connector::None};
    ContentColumn sizeColumn{Column::Size, Comparison::Equal, &minimum, Connector::None};
    ColumnSet sizeSet{&sizeColumn, 1, Connector::None};
    constexpr std::array<std::int64_t, 6> comparisonCounts{1, 1, 1, 2, 0, 1};
    for (std::size_t i = 0; i < comparisonCounts.size(); ++i) {
        sizeColumn.comparison = static_cast<Comparison>(i);
        Require(sceContentSearchSearchContent(&sizeSet, 1, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr) == 0 && count == comparisonCounts[i]);
    }
    Require(sceContentSearchSearchContent(&set, 1, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr) == 0);
    Require(count == 1 && rows[0].contentId == alpha.id);
    columns[0].connector = Connector::Or;
    user.value = 7;
    Require(sceContentSearchSearchContent(&set, 1, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr) == 0 && count == 2);
    const char title[] = "Alpha";
    ColumnValue titleValue{sizeof(title), reinterpret_cast<std::int64_t>(title)};
    ContentColumn titleColumn{Column::Title, Comparison::Equal, &titleValue, Connector::None};
    ColumnSet titleSet{&titleColumn, 1, Connector::None};
    Require(sceContentSearchSearchContent(&titleSet, 1, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr) == 0 && count == 1 && rows[0].contentId == alpha.id);
    std::array<ColumnSet, 2> sets{{{&titleColumn, 1, Connector::Or}, {columns.data() + 1, 1, Connector::None}}};
    Require(sceContentSearchSearchContent(sets.data(), sets.size(), nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr) == 0 && count == 2);
    count = -9;
    rows[0].contentId = -9;
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 93, &count, rows.data(), nullptr) == static_cast<int>(0x809d100au));
    Require(count == -9 && rows[0].contentId == -9);
    titleColumn.column = static_cast<Column>(999);
    Require(Throws([&] { sceContentSearchSearchContent(&titleSet, 1, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr); }));
    Require(count == -9 && rows[0].contentId == -9);
    titleColumn.column = Column::Title;
    titleValue.size = 2;
    Require(Throws([&] { sceContentSearchSearchContent(&titleSet, 1, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr); }));
    Require(Throws([&] { sceContentSearchSearchContent(nullptr, 1, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr); }));
    std::array<ContentColumn, 3> mixedColumns{{
        {Column::Size, Comparison::Equal, &minimum, Connector::And},
        {Column::Size, Comparison::Equal, &minimum, Connector::Or},
        {Column::Size, Comparison::Equal, &minimum, Connector::None}
    }};
    ColumnSet mixedSet{mixedColumns.data(), static_cast<std::int32_t>(mixedColumns.size()), Connector::None};
    Require(Throws([&] { sceContentSearchSearchContent(&mixedSet, 1, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr); }));
    Require(count == -9 && rows[0].contentId == -9);
    std::int32_t handle = -1;
    Require(Throws([&] { sceContentSearchOpenMetadata("/data/missing.png", &handle); }));
    Require(handle == -1);
    Require(sceContentSearchOpenMetadata(alpha.path.c_str(), &handle) == 0);
    MetadataType type{};
    std::int32_t size = -1;
    Require(sceContentSearchGetMetadataFieldInfo(handle, "title", &type, &size) == 0 && type == MetadataType::Text && size == 6);
    std::array<char, 16> text;
    text.fill('x');
    MetadataValue value{5, MetadataType::Int, reinterpret_cast<std::int64_t>(text.data())};
    Require(Throws([&] { sceContentSearchGetMetadataValue(handle, "title", &value); }));
    Require(text[0] == 'x' && value.size == 5 && value.type == MetadataType::Int);
    value.size = text.size();
    Require(sceContentSearchGetMetadataValue(handle, "title", &value) == 0);
    Require(value.value == reinterpret_cast<std::int64_t>(text.data()) && value.size == 6 && value.type == MetadataType::Text && std::string(text.data()) == "Alpha" && text[6] == 'x');
    value = {};
    Require(sceContentSearchGetMetadataValue(handle, "size", &value) == 0 && value.type == MetadataType::Int && value.size == 8 && value.value == 9);
    Require(sceContentSearchGetMetadataValue(handle, "created_time", &value) == 0 && value.type == MetadataType::Tick && std::bit_cast<std::uint64_t>(value.value) == alpha.createdTime);
    Require(sceContentSearchGetMetadataValue(handle, "width", &value) == 0 && value.value == 4);
    Require(sceContentSearchGetMetadataValue(handle, "height", &value) == 0 && value.value == 3);
    Require(Throws([&] { sceContentSearchGetMetadataValue(handle, "mime_type", &value); }));
    Require(Throws([&] { sceContentSearchGetMetadataValue(handle, "duration", &value); }));
    Require(Throws([&] { sceContentSearchGetMetadataValue(handle, "unknown", &value); }));
    Require(sceContentSearchCloseMetadata(handle) == 0);
    Require(Throws([&] { sceContentSearchCloseMetadata(handle); }));
    Require(sceContentSearchOpenMetadata(alpha.path.c_str(), &handle) == 0);
    Require(sceContentSearchTerm() == 0);
    Require(sceContentSearchGetMetadataFieldInfo(handle, "title", &type, &size) == static_cast<int>(0x809d1001u));
    Require(sceContentSearchInit(&init) == 0);
    Require(Throws([&] { sceContentSearchGetMetadataFieldInfo(handle, "title", &type, &size); }));
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr) == 0 && count == 2);
    auto video = Publish("Video", "sample", 7);
    video.contentType = 2;
    video.mimeType = "video/mp4";
    video.hasDuration = true;
    video.duration = 1.5;
    const auto videoDirectory = ResolvePath_nid_no_patch(video.path.c_str()).parent_path();
    ContentCatalog_nid_no_patch::WriteEntry_nid_no_patch(videoDirectory, video);
    count = -9;
    rows[0].contentId = -9;
    Require(Throws([&] { sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr); }));
    Require(count == -9 && rows[0].contentId == -9);
    std::filesystem::remove_all(videoDirectory);
    std::filesystem::remove(ResolvePath_nid_no_patch(zebra.path.c_str()));
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, rows.size(), &count, rows.data(), nullptr) == 0 && rows[0].status == 0);
    Require(sceContentSearchTerm() == 0);
    Roundtrip();
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
