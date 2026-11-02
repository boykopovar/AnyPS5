#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Sandbox certificates check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
static std::string ReadKernel(const std::string& path) {
    const int descriptor = sceKernelOpen(path.c_str(), 0, 0);
    Require(descriptor >= 0);
    std::string text;
    char buffer[4096];
    for (std::int64_t count; (count = sceKernelRead(descriptor, buffer, sizeof(buffer))) != 0;) {
        Require(count > 0);
        text.append(buffer, static_cast<std::size_t>(count));
    }
    Require(sceKernelClose(descriptor) == 0);
    return text;
}
static std::string ReadStream(const std::string& path) {
    FileStream* stream = fopen_nid_postfix(path.c_str(), "rb");
    Require(stream != nullptr);
    std::string text;
    char buffer[4096];
    for (std::size_t count; (count = fread_nid_postfix(buffer, 1, sizeof(buffer), stream)) != 0;) text.append(buffer, count);
    Require(fclose_nid_postfix(stream) == 0);
    return text;
}
static void CheckOverrides(const std::string& directory, const std::string& path) {
    const auto root = std::filesystem::current_path();
    const auto supplied = root / std::filesystem::path(path).relative_path();
    std::filesystem::create_directories(supplied.parent_path());
    const std::string expected = "explicit guest certificate bundle\n";
    { std::ofstream file(supplied, std::ios::binary | std::ios::trunc); file << expected; Require(static_cast<bool>(file)); }
    Require(ReadKernel(path) == expected);
    Require(ReadStream(path) == expected);
    Require(ResolvePath_nid_no_patch(path.c_str()) == supplied);
    Require(std::filesystem::remove(supplied));
    for (auto parent = supplied.parent_path(); parent != root; parent = parent.parent_path())
        Require(std::filesystem::remove(parent));
    const auto alias = root / "sandbox_certificate_override";
    std::filesystem::create_directory(alias);
    { std::ofstream file(alias / "CA_LIST.cer", std::ios::binary | std::ios::trunc); file << expected; Require(static_cast<bool>(file)); }
    AddPathAlias_nid_no_patch(directory.c_str(), alias.string().c_str());
    Require(ReadKernel(path) == expected && ReadStream(path) == expected);
    BlockPathAlias_nid_no_patch(directory.c_str());
    Require(sceKernelOpen(path.c_str(), 0, 0) == SCE_KERNEL_ERROR_ENOENT);
    Require(fopen_nid_postfix(path.c_str(), "rb") == nullptr);
    AddPathAlias_nid_no_patch(directory.c_str(), alias.string().c_str());
    RemovePathAlias_nid_no_patch(directory.c_str());
    Require(std::filesystem::remove(alias / "CA_LIST.cer"));
    Require(std::filesystem::remove(alias));
}
int main() {
#ifndef _WIN32
    const auto bundle = std::filesystem::absolute("host_bundle.pem");
    const std::string expected = "-----BEGIN CERTIFICATE-----\nMIIBAA==\n-----END CERTIFICATE-----\n"
                                 "-----BEGIN CERTIFICATE-----\r\nMIIBAg==\r\n-----END CERTIFICATE-----\r\n";
    const std::string input = "Bag Attributes\r\n    friendlyName: should-not-be-copied\r\n"
                              "-----BEGIN PRIVATE KEY-----\r\nprivate-key-material\r\n-----END PRIVATE KEY-----\r\n" +
                              expected + "subject=should-not-be-copied\r\n"
                              "-----BEGIN RSA PRIVATE KEY-----\r\nmore-private-key-material\r\n-----END RSA PRIVATE KEY-----\r\n";
    { std::ofstream file(bundle, std::ios::binary | std::ios::trunc); file << input; }
    Require(setenv("SSL_CERT_FILE", bundle.c_str(), 1) == 0);
#endif
    const std::string directory = std::string("/") + sceKernelGetFsSandboxRandomWord() + "/common/cert";
    const std::string path = directory + "/CA_LIST.cer";
    CheckOverrides(directory, path);
    const auto text = ReadKernel(path);
    const auto begin = text.find("-----BEGIN CERTIFICATE-----");
    Require(begin != std::string::npos);
    Require(text.find("-----END CERTIFICATE-----", begin) != std::string::npos);
#ifndef _WIN32
    Require(text == expected);
#endif
    Require(ReadStream(path) == text);
    Require(ReadKernel("/system/common/cert/CA_LIST.cer") == text);
    Require(ReadStream("/system/common/cert/CA_LIST.cer") == text);
    Require(ReadKernel("/common/cert/CA_LIST.cer") == text);
    Require(ReadStream("/common/cert/CA_LIST.cer") == text);
    Require(ReadKernel(directory + "/./ca_list.cer") == text);
    FileStat status{};
    Require(sceKernelStat(path.c_str(), &status) == 0);
    Require(status.st_size == static_cast<std::int64_t>(text.size()));
    const auto cached = std::filesystem::current_path() / "cert_cache" / "CA_LIST.cer";
    Require(std::filesystem::is_regular_file(cached));
    Require((std::filesystem::status(cached).permissions() & std::filesystem::perms::owner_write) == std::filesystem::perms::none);
    Require(sceKernelOpen((directory + "/CA_LIST2.cer").c_str(), 0, 0) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelOpen((directory + "/CA_LIST.cer.bak").c_str(), 0, 0) == SCE_KERNEL_ERROR_ENOENT);
    Require(fopen_nid_postfix((directory + "/CA_LIST2.cer").c_str(), "rb") == nullptr);
    return 0;
}
