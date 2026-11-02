#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }

static bool Resolves(const std::string& path, const std::string& expected) {
    char out[1024];
    std::memset(out, 'x', sizeof(out));
    return realpath_nid_postfix(path.c_str(), out) == out && expected == out;
}

static bool Fails(const char* path, int error, const std::string& trouble = {}) {
    char out[1024];
    std::memset(out, 'x', sizeof(out));
    *__error_nid_postfix() = 0;
    if (realpath_nid_postfix(path, out) != nullptr || *__error_nid_postfix() != error) return false;
    return trouble.empty() || trouble == out;
}

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const auto host = std::filesystem::canonical(std::filesystem::current_path());
    const auto name = "anyps5-realpath-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = host / name;
    Require(std::filesystem::create_directories(directory / "sub"));
    { std::ofstream file(directory / "file.txt"); file << "file"; }
    { std::ofstream file(directory / "sub" / "inner.txt"); file << "inner"; }
    const std::string guest = "/" + name;

    Require(Resolves("/", "/"));
    Require(Resolves(guest, guest));
    Require(Resolves(guest + "/", guest));
    Require(Resolves(guest + "/sub/../file.txt", guest + "/file.txt"));
    Require(Resolves(guest + "/./sub//inner.txt", guest + "/sub/inner.txt"));
    Require(Resolves("/../.." + guest + "/sub/", guest + "/sub"));
    Require(Resolves(guest + "/sub/..", guest));
    Require(Resolves("/..", "/"));
    Require(Resolves(guest + "\\sub\\..\\file.txt", guest + "/file.txt"));
    Require(Resolves("\\" + name + "\\sub\\", guest + "/sub"));

    std::string upper = guest;
    for (auto& character : upper) character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    Require(Resolves(upper + "/file.txt", upper + "/file.txt"));

    Require(chdir_nid_postfix(guest.c_str()) == 0);
    Require(Resolves("sub/inner.txt", guest + "/sub/inner.txt"));
    Require(Resolves("../" + name + "/file.txt", guest + "/file.txt"));
    Require(Resolves(".", guest));
    Require(Resolves("sub/./../sub", guest + "/sub"));
    Require(Resolves("..", "/"));
    Require(chdir_nid_postfix("/") == 0);

    AddPathAlias_nid_no_patch("realpath-alias", directory.string().c_str());
    Require(Resolves("/realpath-alias/sub/../file.txt", "/realpath-alias/file.txt"));
    Require(Fails("/realpath-alias/missing", 2, "/realpath-alias/missing"));
    RemovePathAlias_nid_no_patch("realpath-alias");
    BlockPathAlias_nid_no_patch("realpath-blocked");
    Require(Fails("/realpath-blocked/x", 2, "/realpath-blocked"));

    char* allocated = realpath_nid_postfix((guest + "/file.txt").c_str(), nullptr);
    Require(allocated != nullptr && guest + "/file.txt" == allocated);
    free_nid_postfix(allocated);

    Require(Fails(nullptr, 22));
    Require(Fails("", 2));
    Require(Fails((guest + "/missing").c_str(), 2, guest + "/missing"));
    Require(Fails((guest + "/missing/../file.txt").c_str(), 2, guest + "/missing"));
    Require(Fails((guest + "/file.txt/").c_str(), 20, guest + "/file.txt"));
    Require(Fails((guest + "/file.txt/x").c_str(), 20, guest + "/file.txt"));
    Require(Fails((guest + "/file.txt/.").c_str(), 20));
    Require(Fails((guest + "/file.txt/..").c_str(), 20));

    const std::string overflowing = "/" + std::string(1023, 'a');
    Require(Fails(overflowing.c_str(), 63, overflowing.substr(0, 1023)));
    const std::string longInput = guest + "/sub/" + std::string(1024, '.') + "/inner.txt";
    Require(Fails(longInput.c_str(), 63));
    std::string boundary = guest;
    while (boundary.size() + 8 <= 1023) boundary += "/missing";
    boundary += "/" + std::string(1023 - boundary.size() - 1, 'm');
    Require(boundary.size() == 1023);
    Require(Fails(boundary.c_str(), 2, guest + "/missing"));

    *__error_nid_postfix() = 0;
    Require(realpath_nid_postfix((guest + "/missing").c_str(), nullptr) == nullptr && *__error_nid_postfix() == 2);

    const auto createLink = [&](const std::filesystem::path& link, const std::string& target, bool toDirectory) {
#ifdef _WIN32
        return CreateSymbolicLinkW(link.c_str(), std::filesystem::path(target).make_preferred().c_str(),
            (toDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
#else
        std::error_code linkError;
        if (toDirectory) std::filesystem::create_directory_symlink(target, link, linkError);
        else std::filesystem::create_symlink(target, link, linkError);
        return !linkError;
#endif
    };
    std::vector<std::filesystem::path> files, directories;
    const auto fileLink = [&](const std::filesystem::path& link, const std::string& target) {
        files.push_back(link);
        return createLink(link, target, false);
    };
    if (fileLink(directory / "file-link", "file.txt")) {
        directories.push_back(directory / "directory-link");
        Require(createLink(directory / "directory-link", "sub", true));
        Require(fileLink(directory / "dangling-link", "missing"));
        Require(fileLink(directory / "loop-a", "loop-b") && fileLink(directory / "loop-b", "loop-a"));
        Require(fileLink(directory / "chain-link", "file-link"));
        Require(fileLink(directory / "sub" / "up-link", "../file.txt"));
        Require(Resolves(guest + "/file-link", guest + "/file.txt"));
        Require(Resolves(guest + "/directory-link", guest + "/sub"));
        Require(Resolves(guest + "/directory-link/inner.txt", guest + "/sub/inner.txt"));
        Require(Resolves(guest + "/directory-link/../file.txt", guest + "/file.txt"));
        Require(Resolves(guest + "/chain-link", guest + "/file.txt"));
        Require(Resolves(guest + "/sub/up-link", guest + "/file.txt"));
        Require(Resolves(guest + "/directory-link/up-link", guest + "/file.txt"));
        Require(Resolves(guest + "/directory-link/", guest + "/sub"));
        directories.push_back(directory / "absolute-link");
        Require(createLink(directory / "absolute-link", (directory / "sub").string(), true));
        Require(Resolves(guest + "/absolute-link/inner.txt", guest + "/absolute-link/inner.txt"));
        Require(Resolves(guest + "/absolute-link/up-link", guest + "/absolute-link/up-link"));
        Require(Fails((guest + "/dangling-link").c_str(), 2, guest + "/missing"));
        Require(Fails((guest + "/dangling-link/x").c_str(), 2, guest + "/missing"));
        Require(Fails((guest + "/loop-a").c_str(), 62));
        Require(Fails((guest + "/file-link/x").c_str(), 20));
        Require(Fails((guest + "/file-link/").c_str(), 20));
        for (int index = 0; index < 34; ++index) {
            Require(fileLink(directory / ("long-" + std::to_string(index)), index == 33 ? "file.txt" : "long-" + std::to_string(index + 1)));
        }
        Require(Resolves(guest + "/long-1", guest + "/file.txt"));
        Require(Fails((guest + "/long-0").c_str(), 62));
    }
#ifdef _WIN32
    for (const auto& link : files) DeleteFileW(link.c_str());
    for (const auto& link : directories) RemoveDirectoryW(link.c_str());
#endif

    std::filesystem::remove_all(directory);
    return 0;
}
