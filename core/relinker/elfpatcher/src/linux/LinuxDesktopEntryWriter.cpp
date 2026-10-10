#include <elfpatcher/linux/LinuxDesktopEntryWriter.hpp>
#include <domain/Types.hpp>
#include "prx/libkernel/AppMetadata/include/ParamJsonParser.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace Elfpatcher::Linux {

namespace {

bool fileExists(const std::filesystem::path& path) {
    std::error_code error;
    const bool result = std::filesystem::exists(path, error);
    if (error) throw Domain::RelinkerException("Cannot inspect '" + path.string() + "': " + error.message());
    return result;
}

std::vector<std::uint8_t> readPng(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw Domain::RelinkerException("Cannot open icon '" + path.string() + "'");
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    constexpr std::array<std::uint8_t, 16> header{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52};
    if (bytes.size() < 33 || !std::equal(header.begin(), header.end(), bytes.begin())) throw Domain::RelinkerException("Invalid icon PNG '" + path.string() + "': missing PNG signature or IHDR");
    return bytes;
}

std::string cleanTitle(const std::string& title) {
    std::string withoutMarks;
    for (std::size_t i = 0; i < title.size(); ++i) {
        const std::string_view rest(title.data() + i, title.size() - i);
        if (rest.starts_with("\xe2\x84\xa2")) { i += 2; continue; }
        if (rest.starts_with("\xc2\xae") || rest.starts_with("\xc2\xa9")) { i += 1; continue; }
        const auto c = static_cast<unsigned char>(title[i]);
        withoutMarks += c < 0x20 || c == 0x7f ? ' ' : title[i];
    }
    std::string result;
    for (const char c : withoutMarks) {
        if (c == ' ' && (result.empty() || result.back() == ' ')) continue;
        result += c;
    }
    if (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

std::string title(const std::filesystem::path& paramJsonPath, const std::filesystem::path& executablePath) {
    if (!fileExists(paramJsonPath)) return executablePath.stem().string();
    std::string parsed;
    try {
        parsed = cleanTitle(parseParamJson(paramJsonPath).title);
    } catch (const std::exception& e) {
        throw Domain::RelinkerException("Invalid '" + paramJsonPath.string() + "': " + e.what());
    }
    return parsed.empty() ? executablePath.stem().string() : parsed;
}

std::string escapeValue(const std::string& value) {
    std::string result;
    for (const char c : value) {
        if (c == '\\') result += "\\\\";
        else result += c;
    }
    return result;
}

void requirePrintable(const std::filesystem::path& path) {
    const auto text = path.string();
    const bool control = std::any_of(text.begin(), text.end(), [](const char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte < 0x20 || byte == 0x7f;
    });
    if (control) throw Domain::RelinkerException("Cannot write a desktop entry for '" + text + "': the path contains a control character");
}

std::string quoteExec(const std::string& path) {
    std::string result = "\"";
    for (const char c : path) {
        if (c == '"' || c == '`' || c == '$' || c == '\\') result += '\\';
        if (c == '%') result += '%';
        result += c;
    }
    return escapeValue(result + "\"");
}

}

std::optional<LinuxDesktopEntry> LinuxDesktopEntryWriter::Prepare(const std::filesystem::path& sceSysDirectory, const std::filesystem::path& executablePath) const {
    const auto iconSource = sceSysDirectory / "icon0.png";
    if (!fileExists(iconSource)) return std::nullopt;
    requirePrintable(executablePath);
    LinuxDesktopEntry entry;
    entry.Icon = readPng(iconSource);
    const auto name = title(sceSysDirectory / "param.json", executablePath);
    entry.IconPath = std::filesystem::path(executablePath.string() + ".png");
    entry.EntryPath = std::filesystem::path(executablePath.string() + ".desktop");
    entry.Text = "[Desktop Entry]\n"
                 "Type=Application\n"
                 "Name=" + escapeValue(name) + "\n"
                 "Exec=" + quoteExec(executablePath.string()) + "\n"
                 "Path=" + escapeValue(executablePath.parent_path().string()) + "\n"
                 "Icon=" + escapeValue(entry.IconPath.string()) + "\n"
                 "Terminal=false\n"
                 "Categories=Game;\n";
    return entry;
}

std::filesystem::path LinuxDesktopEntryWriter::Write(const LinuxDesktopEntry& entry) const {
    std::ofstream icon(entry.IconPath, std::ios::binary);
    icon.write(reinterpret_cast<const char*>(entry.Icon.data()), static_cast<std::streamsize>(entry.Icon.size()));
    icon.close();
    if (!icon) throw Domain::RelinkerException("Failed to write icon '" + entry.IconPath.string() + "'");

    std::ofstream text(entry.EntryPath, std::ios::binary);
    text << entry.Text;
    text.close();
    if (!text) throw Domain::RelinkerException("Failed to write desktop entry '" + entry.EntryPath.string() + "'");

    std::error_code error;
    std::filesystem::permissions(entry.EntryPath, std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec, std::filesystem::perm_options::add, error);
    if (error) throw Domain::RelinkerException("Cannot mark desktop entry '" + entry.EntryPath.string() + "' executable: " + error.message());
    return entry.EntryPath;
}

}
