#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libkernel/AppMetadata/include/ParamJsonParser.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool g_titleLoaded = false;
bool g_iconAttempted = false;
bool g_iconLoaded = false;
char g_title[128] = {};
char g_titleId[12] = {};
std::uint64_t g_downloadDataSizeMiB = 0;
std::int32_t g_userDefinedParams[4] = {};
std::vector<std::uint8_t> g_iconBytes;

constexpr const char* AppMetadataParamJsonGuestPath = "/app0/sce_sys/param.json";
constexpr const char* AppMetadataIconGuestPath = "/app0/sce_sys/icon0.png";

void copyToFixedBuffer(char* destination, std::size_t destinationSize, const std::string& source) {
    if (source.empty()) throw std::runtime_error("source string is empty");
    if (source.size() + 1 > destinationSize) throw std::length_error("source string does not fit into destination buffer");
    std::memcpy(destination, source.c_str(), source.size() + 1);
}

void ensureTitleLoaded() {
    if (g_titleLoaded) return;
    const auto resolvedPath = ResolvePath_nid_no_patch(AppMetadataParamJsonGuestPath);
    if (!std::filesystem::exists(resolvedPath)) throw std::runtime_error("param.json not found");
    const auto parsed = parseParamJson(resolvedPath);
    copyToFixedBuffer(g_title, sizeof(g_title), parsed.title);
    copyToFixedBuffer(g_titleId, sizeof(g_titleId), parsed.titleId);
    g_downloadDataSizeMiB = parsed.downloadDataSizeMiB;
    std::memcpy(g_userDefinedParams, parsed.userDefinedParams, sizeof(g_userDefinedParams));
    g_titleLoaded = true;
}

const bool g_downloadDataMounted = [] {
    try {
        const auto run = std::filesystem::current_path();
        const auto paramJson = run / "app0" / "sce_sys" / "param.json";
        if (!std::filesystem::exists(paramJson) || parseParamJson(paramJson).downloadDataSizeMiB == 0) return false;
        std::filesystem::create_directories(run / "download0");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}();

void ensureIconLoaded() {
    if (g_iconAttempted) return;
    g_iconAttempted = true;
    const auto resolvedPath = ResolvePath_nid_no_patch(AppMetadataIconGuestPath);
    if (!std::filesystem::exists(resolvedPath)) return;
    std::ifstream file(resolvedPath, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open icon0.png");
    file.seekg(0, std::ios::end);
    const auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (bytes.empty()) throw std::runtime_error("icon0.png is empty");
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
