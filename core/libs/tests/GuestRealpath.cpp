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

extern "C" {
char* APS5_VABI realpath_nid_postfix(const char*, char*);
int APS5_VABI chdir_nid_postfix(const char*);
void APS5_VABI free_nid_postfix(void*);
int* APS5_VABI __error_nid_postfix();
}

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

    std::filesystem::remove_all(directory);
    return 0;
}
