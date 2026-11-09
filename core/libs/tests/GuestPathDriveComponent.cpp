#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }

static std::string Under(const std::filesystem::path& directory, const std::string& names) {
    auto text = directory.string();
    for (std::size_t start = 0; start <= names.size();) {
        const auto end = std::min(names.find('/', start), names.size());
        text += static_cast<char>(std::filesystem::path::preferred_separator);
        text += names.substr(start, end - start);
        start = end + 1;
    }
    return text;
}

int main() {
    const auto host = std::filesystem::canonical(std::filesystem::current_path());
    const auto name = "AnyPS5-Drive-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = host / name;
    const auto mounted = directory / "Mounted";
    Require(std::filesystem::create_directories(mounted / "Nested"));
    { std::ofstream file(directory / "Secret.txt"); file << 'x'; }
    auto drive = host.root_name().string();
    if (drive.empty()) drive = "C:";
    std::string outside;
    for (const auto& part : directory.relative_path()) outside += "/" + part.string();
    AddPathAlias_nid_no_patch("/drive-mount", mounted.string().c_str());

    Require(ResolvePath_nid_no_patch(("/drive-mount/" + drive + outside + "/Secret.txt").c_str()).string() ==
            Under(mounted, drive + outside + "/Secret.txt"));
    Require(ResolvePath_nid_no_patch(("/drive-mount/" + drive + "Nested").c_str()).string() == Under(mounted, drive + "Nested"));
    Require(ResolvePath_nid_no_patch(("/drive-mount/" + drive).c_str()).string() == Under(mounted, drive));
    Require(ResolvePath_nid_no_patch(("/drive-mount/Nested/" + drive + "/x").c_str()).string() == Under(mounted, "Nested/" + drive + "/x"));
    Require(ResolvePath_nid_no_patch(("/" + name + "/../" + drive + outside + "/Secret.txt").c_str()).string() ==
            Under(host, drive + outside + "/Secret.txt"));
    Require(ResolvePath_nid_no_patch(("/" + drive + "/" + name + "/Secret.txt").c_str()).string() ==
            Under(host, drive + "/" + name + "/Secret.txt"));
    Require(ResolvePath_nid_no_patch(("/" + drive).c_str()).string() == Under(host, drive));
    Require(ResolvePath_nid_no_patch(("/" + name + "/" + drive + "/x").c_str()).string() == Under(directory, drive + "/x"));
    Require(ResolvePath_nid_no_patch(("/" + name + "/Secret.txt").c_str()) == directory / "Secret.txt");
    Require(ResolvePath_nid_no_patch("/drive-mount/Nested").string() == Under(mounted, "Nested"));

    RemovePathAlias_nid_no_patch("/drive-mount");
    std::filesystem::remove_all(directory);
}
