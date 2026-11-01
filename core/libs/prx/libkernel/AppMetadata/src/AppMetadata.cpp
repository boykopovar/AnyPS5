#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libkernel/AppMetadata/include/ParamJsonParser.hpp"
#include "prx/libkernel/AppMetadata/include/ParamSfoParser.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::mutex g_metadataMutex;
bool g_titleLoaded = false;
bool g_iconAttempted = false;
bool g_iconLoaded = false;
char g_title[128] = {};
char g_titleId[12] = {};
char g_contentVersion[16] = {};
std::uint64_t g_downloadDataSizeMiB = 0;
std::uint32_t g_attribute3 = 0;
std::optional<std::uint32_t> g_paramSfoAttribute;
std::int32_t g_userDefinedParams[4] = {};
std::vector<std::uint8_t> g_iconBytes;

constexpr const char* AppMetadataParamJsonGuestPath = "/app0/sce_sys/param.json";
constexpr const char* AppMetadataParamSfoGuestPath = "/app0/sce_sys/param.sfo";
constexpr const char* AppMetadataIconGuestPath = "/app0/sce_sys/icon0.png";

void copyToFixedBuffer(char* destination, std::size_t destinationSize, const std::string& source) {
    if (source.empty()) throw std::runtime_error("source string is empty");
    if (source.size() + 1 > destinationSize) throw std::length_error("source string does not fit into destination buffer");
    std::memcpy(destination, source.c_str(), source.size() + 1);
}

ParsedParamJson parseParams(const std::filesystem::path& paramJson, const std::filesystem::path& paramSfo) {
    if (std::filesystem::exists(paramJson)) return parseParamJson(paramJson);
    if (std::filesystem::exists(paramSfo)) return parseParamSfo(paramSfo);
    throw std::runtime_error("param.json or param.sfo not found");
}

void ensureTitleLoaded() {
    std::lock_guard lock(g_metadataMutex);
    if (g_titleLoaded) return;
    const auto parsed = parseParams(ResolvePath_nid_no_patch(AppMetadataParamJsonGuestPath), ResolvePath_nid_no_patch(AppMetadataParamSfoGuestPath));
    copyToFixedBuffer(g_title, sizeof(g_title), parsed.title);
    copyToFixedBuffer(g_titleId, sizeof(g_titleId), parsed.titleId);
    if (!parsed.contentVersion.empty() && parsed.contentVersion.size() < sizeof(g_contentVersion)) copyToFixedBuffer(g_contentVersion, sizeof(g_contentVersion), parsed.contentVersion);
    g_downloadDataSizeMiB = parsed.downloadDataSizeMiB;
    g_attribute3 = parsed.attribute3;
    g_paramSfoAttribute = parsed.paramSfoAttribute;
    std::memcpy(g_userDefinedParams, parsed.userDefinedParams, sizeof(g_userDefinedParams));
    g_titleLoaded = true;
}

const bool g_downloadDataMounted = [] {
    try {
        const auto run = std::filesystem::current_path();
        const auto metadata = run / "app0" / "sce_sys";
        if (!std::filesystem::exists(metadata / "param.json") && !std::filesystem::exists(metadata / "param.sfo")) return false;
        if (parseParams(metadata / "param.json", metadata / "param.sfo").downloadDataSizeMiB == 0) return false;
        std::filesystem::create_directories(run / "download0");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}();

const bool g_dataMounted = [] {
    try {
        const auto run = std::filesystem::current_path();
        const auto metadata = run / "app0" / "sce_sys";
        if (!std::filesystem::exists(metadata / "param.json") && !std::filesystem::exists(metadata / "param.sfo")) return false;
        std::filesystem::create_directories(run / "data");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}();

void ensureIconLoaded() {
    std::lock_guard lock(g_metadataMutex);
    if (g_iconAttempted) return;
    g_iconAttempted = true;
    const auto resolvedPath = ResolvePath_nid_no_patch(AppMetadataIconGuestPath);
    if (!std::filesystem::exists(resolvedPath)) return;
    std::ifstream file(resolvedPath, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open icon0.png");
    file.seekg(0, std::ios::end);
    const auto fileSize = file.tellg();
    if (file.fail() || fileSize <= 0) throw std::runtime_error("icon0.png is empty or unreadable");
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file || file.gcount() != static_cast<std::streamsize>(bytes.size())) throw std::runtime_error("failed to read icon0.png");
    constexpr std::uint8_t PngSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (bytes.size() < 8 || std::memcmp(bytes.data(), PngSignature, sizeof(PngSignature)) != 0) throw std::runtime_error("icon0.png is not a valid PNG file");
    g_iconBytes = std::move(bytes);
    g_iconLoaded = true;
}

}

extern "C" {

AppTitle GetAppTitle_nid_postfix() {
    ensureTitleLoaded();
    AppTitle result{};
    std::memcpy(result.value, g_title, sizeof(result.value));
    return result;
}

std::uint64_t GetAppDownloadDataSizeMiB_nid_postfix() {
    ensureTitleLoaded();
    return g_downloadDataSizeMiB;
}

std::uint32_t GetAppAttribute3_nid_no_patch() {
    ensureTitleLoaded();
    return g_attribute3;
}

bool GetAppParamSfoAttribute_nid_no_patch(std::uint32_t* attribute) {
    ensureTitleLoaded();
    if (!g_paramSfoAttribute) return false;
    *attribute = *g_paramSfoAttribute;
    return true;
}

std::int32_t GetAppUserDefinedParam_nid_postfix(std::uint32_t index) {
    if (index >= 4) throw std::out_of_range("user defined param index out of range");
    ensureTitleLoaded();
    return g_userDefinedParams[index];
}

AppTitleId GetAppTitleId_nid_postfix() {
    ensureTitleLoaded();
    AppTitleId result{};
    std::memcpy(result.value, g_titleId, sizeof(result.value));
    return result;
}

bool HasAppParamJson_nid_no_patch() {
    return std::filesystem::exists(ResolvePath_nid_no_patch(AppMetadataParamJsonGuestPath));
}

AppContentVersion GetAppContentVersion_nid_no_patch() {
    ensureTitleLoaded();
    AppContentVersion result{};
    std::memcpy(result.value, g_contentVersion, sizeof(result.value));
    return result;
}

bool HasAppIcon_nid_postfix() {
    ensureIconLoaded();
    return g_iconLoaded;
}

AppIconData GetAppIconData_nid_postfix() {
    ensureIconLoaded();
    if (!g_iconLoaded) throw std::runtime_error("icon not available");
    return AppIconData{ g_iconBytes.data(), g_iconBytes.size() };
}

}
