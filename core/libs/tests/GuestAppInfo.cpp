#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

extern "C" {
int APS5_VABI sceKernelGetAppInfo(int pid, void* info);
int APS5_VABI getpid_nid_postfix(void);
int APS5_VABI sceKernelUuidCreate(std::uint32_t* uuid);
int APS5_VABI sceKernelCreateUuidV4(std::uint8_t* uuid);
}

static constexpr int SCE_KERNEL_ERROR_EPERM = static_cast<int>(0x80020001);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);

static void Require(bool value) { if (!value) std::abort(); }

struct AppInfo {
    std::uint32_t appId;
    std::uint64_t unknown1;
    char titleId[14];
    char unknown2[0x3c];
};

static void WriteParamSfo(const std::filesystem::path& path) {
    const char title[] = "Example";
    const char titleId[] = "CUSA00000";
    const char keys[16] = "TITLE\0TITLE_ID";
    unsigned char bytes[86] = {0, 'P', 'S', 'F', 1, 1, 0, 0, 52, 0, 0, 0, 68, 0, 0, 0, 2, 0, 0, 0,
                               0, 0, 4, 2, 8, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0,
                               6, 0, 4, 2, 10, 0, 0, 0, 10, 0, 0, 0, 8, 0, 0, 0};
    std::memcpy(bytes + 52, keys, sizeof(keys));
    std::memcpy(bytes + 68, title, sizeof(title));
    std::memcpy(bytes + 76, titleId, sizeof(titleId));
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

static int RunSelf(const char* program, std::string_view mode, const std::filesystem::path& directory) {
    const std::string arguments = "\"" + std::string(program) + "\" " + std::string(mode) + " \"" + directory.string() + "\"";
#ifdef _WIN32
    const std::string command = "\"" + arguments + "\"";
#else
    const std::string command = arguments;
#endif
    return std::system(command.c_str());
}

static std::filesystem::path EnterTemporaryRoot(std::string_view name) {
    const auto root = std::filesystem::temp_directory_path() / (std::string(name) + std::to_string(getpid_nid_postfix()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    return root;
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--without-param-json") {
        std::filesystem::current_path(argv[2]);
        AppInfo info{};
        try { sceKernelGetAppInfo(getpid_nid_postfix(), &info); }
        catch (const std::runtime_error& error) {
            return std::string_view(error.what()) == "param.json or param.sfo not found" ? 0 : 1;
        }
        return 1;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--with-param-sfo") {
        std::filesystem::current_path(argv[2]);
        AppInfo info{};
        Require(sceKernelGetAppInfo(getpid_nid_postfix(), &info) == 0);
        Require(std::strcmp(info.titleId, "CUSA00000") == 0);
        return 0;
    }
    const auto previous = std::filesystem::current_path();
    const auto root = EnterTemporaryRoot("guest_app_info_");
    std::filesystem::create_directories(root / "app0" / "sce_sys");
    WriteParamSfo(root / "app0" / "sce_sys" / "param.sfo");
    std::ofstream(root / "app0" / "sce_sys" / "param.json") << "{\n  \"contentId\": \"IV0000-PPSA97358_00-XPSEMU0000000000\",\n  \"localizedParameters\": {\"defaultLanguage\": \"en-US\", \"en-US\": {\"titleName\": \"XPSemu\"}},\n  \"titleId\": \"PPSA97358\"\n}\n";
    AppInfo info;
    std::memset(&info, 0x5a, sizeof(info));
    const int self = getpid_nid_postfix();
    Require(sceKernelGetAppInfo(self, &info) == 0);
    Require(info.appId == 1);
    Require(std::strcmp(info.titleId, "PPSA97358") == 0);
    Require(info.unknown1 == 0x5a5a5a5a5a5a5a5aull);
    Require(info.unknown2[0] == 0x5a);
    Require(sceKernelGetAppInfo(self + 1, &info) == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelGetAppInfo(self, nullptr) == 0);
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    const auto empty = std::filesystem::temp_directory_path() / ("guest_app_info_empty_" + std::to_string(self));
    std::filesystem::remove_all(empty);
    std::filesystem::create_directories(empty);
    Require(RunSelf(argv[0], "--without-param-json", empty) == 0);
    std::filesystem::remove_all(empty);
    const auto sfo = std::filesystem::temp_directory_path() / ("guest_app_info_sfo_" + std::to_string(self));
    std::filesystem::remove_all(sfo);
    std::filesystem::create_directories(sfo / "app0" / "sce_sys");
    WriteParamSfo(sfo / "app0" / "sce_sys" / "param.sfo");
    Require(RunSelf(argv[0], "--with-param-sfo", sfo) == 0);
    std::filesystem::remove_all(sfo);

    Require(sceKernelUuidCreate(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    std::uint32_t first[4]{};
    std::uint32_t second[4]{};
    Require(sceKernelUuidCreate(first) == 0 && sceKernelUuidCreate(second) == 0);
    for (const std::uint32_t* uuid : {first, second}) Require((uuid[1] >> 28) == 4u && (uuid[2] & 0xc0u) == 0x80u);
    Require(std::memcmp(first, second, sizeof(first)) != 0);

    Require(sceKernelCreateUuidV4(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    std::uint8_t third[16]{};
    std::uint8_t fourth[16]{};
    Require(sceKernelCreateUuidV4(third) == 0 && sceKernelCreateUuidV4(fourth) == 0);
    for (const std::uint8_t* uuid : {third, fourth}) Require((uuid[6] >> 4) == 4u && (uuid[8] & 0xc0u) == 0x80u);
    Require(std::memcmp(third, fourth, sizeof(third)) != 0);
}
