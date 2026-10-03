#include <elfpatcher/linux/LinuxDesktopEntryWriter.hpp>
#include <domain/Types.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::vector<std::uint8_t> Png{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0, 0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99, 0x3d, 0x1d, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void writeText(const std::filesystem::path& path, const std::string& text) {
    writeBytes(path, std::vector<std::uint8_t>(text.begin(), text.end()));
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::string param(const std::string& title) {
    return R"({"titleId": "PPSA00000", "defaultLanguage": "en-US", "localizedParameters": {"en-US": {"titleName": ")" + title + R"("}}})";
}

std::map<std::string, std::string> readEntry(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::string line;
    std::getline(stream, line);
    require(line == "[Desktop Entry]", "Missing [Desktop Entry] group in " + path.string());
    std::map<std::string, std::string> fields;
    while (std::getline(stream, line)) {
        const auto separator = line.find('=');
        require(separator != std::string::npos, "Malformed desktop entry line: " + line);
        fields[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return fields;
}

void requireField(const std::map<std::string, std::string>& fields, const std::string& key, const std::string& expected) {
    const auto found = fields.find(key);
    require(found != fields.end() && found->second == expected, key + " is '" + (found == fields.end() ? "<missing>" : found->second) + "', expected '" + expected + "'");
}

void requireFailure(const std::filesystem::path& sceSys, const std::filesystem::path& executable, const std::string& path) {
    try {
        Elfpatcher::Linux::LinuxDesktopEntryWriter().Write(sceSys, executable);
    } catch (const Domain::RelinkerException& e) {
        require(std::string(e.what()).find(path) != std::string::npos, "Failure does not name " + path + ": " + e.what());
        require(!std::filesystem::exists(executable.string() + ".desktop"), "Desktop entry written despite the failure");
        return;
    }
    throw std::runtime_error("Write succeeded for a malformed " + path);
}

void run(const std::filesystem::path& root) {
    const Elfpatcher::Linux::LinuxDesktopEntryWriter writer;

    require(writer.Write(root / "none" / "sce_sys", root / "none" / "out" / "game").empty(), "Desktop entry written without icon0.png");
    require(!std::filesystem::exists(root / "none" / "out"), "Files written without icon0.png");

    const auto titledSceSys = root / "titled" / "game" / "sce_sys";
    const auto titled = root / "titled" / "out dir" / "game";
    writeBytes(titledSceSys / "icon0.png", Png);
    writeText(titledSceSys / "param.json", param("Some™ Game®:\\n  Edition© "));
    std::filesystem::create_directories(titled.parent_path());
    require(writer.Write(titledSceSys, titled) == titled.string() + ".desktop", "Unexpected desktop entry path");
    const auto fields = readEntry(titled.string() + ".desktop");
    requireField(fields, "Type", "Application");
    requireField(fields, "Name", "Some Game: Edition");
    requireField(fields, "Exec", "\"" + titled.string() + "\"");
    requireField(fields, "Path", titled.parent_path().string());
    requireField(fields, "Icon", titled.string() + ".png");
    requireField(fields, "Terminal", "false");
    require(readBytes(titled.string() + ".png") == Png, "Icon bytes changed");
    const auto permissions = std::filesystem::status(titled.string() + ".desktop").permissions();
    require((permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none, "Desktop entry is not executable");

    const auto untitledSceSys = root / "untitled" / "game" / "sce_sys";
    const auto untitled = root / "untitled" / "out" / "100% $HOME";
    writeBytes(untitledSceSys / "icon0.png", Png);
    std::filesystem::create_directories(untitled.parent_path());
    writer.Write(untitledSceSys, untitled);
    const auto untitledFields = readEntry(untitled.string() + ".desktop");
    requireField(untitledFields, "Name", "100% $HOME");
    requireField(untitledFields, "Exec", "\"" + (root / "untitled" / "out").string() + "/100%% \\\\$HOME\"");

    const auto badPngSceSys = root / "badpng" / "game" / "sce_sys";
    writeText(badPngSceSys / "icon0.png", "GIF89a" + std::string(40, '\0'));
    std::filesystem::create_directories(root / "badpng" / "out");
    requireFailure(badPngSceSys, root / "badpng" / "out" / "game", "icon0.png");

    const auto badParamSceSys = root / "badparam" / "game" / "sce_sys";
    writeBytes(badParamSceSys / "icon0.png", Png);
    writeText(badParamSceSys / "param.json", "{");
    std::filesystem::create_directories(root / "badparam" / "out");
    requireFailure(badParamSceSys, root / "badparam" / "out" / "game", "param.json");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / "anyps5-desktop-entry-tests";
    std::filesystem::remove_all(root);
    try {
        run(root);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(root);
        return 1;
    }
    std::filesystem::remove_all(root);
    return 0;
}
