#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Sandbox certificate failure check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Clear(const std::filesystem::path& root) {
    std::error_code error;
    if (!std::filesystem::exists(root, error)) return;
    for (std::filesystem::recursive_directory_iterator entry(root, error), end; !error && entry != end; entry.increment(error))
        std::filesystem::permissions(entry->path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add, error);
    std::filesystem::remove_all(root, error);
}

int main() {
#ifndef _WIN32
    const auto bundle = std::filesystem::absolute("host_bundle.pem");
    {
        std::ofstream file(bundle, std::ios::binary | std::ios::trunc);
        file << "-----BEGIN CERTIFICATE-----\nMIIBAA==\n-----END CERTIFICATE-----\n";
    }
    Require(setenv("SSL_CERT_FILE", bundle.c_str(), 1) == 0);
#endif
    const std::string path = "/common/cert/CA_LIST.cer";
    const auto cache = std::filesystem::current_path() / "cert_cache";
    Clear(cache);

    { std::ofstream file(cache, std::ios::binary | std::ios::trunc); file << "not a directory"; Require(static_cast<bool>(file)); }
    Require(sceKernelOpen(path.c_str(), 0, 0) == SCE_KERNEL_ERROR_ENOENT);
    Require(std::filesystem::remove(cache));

    std::filesystem::create_directories(cache / "CA_LIST.cer");
    Require(sceKernelOpen(path.c_str(), 0, 0) == SCE_KERNEL_ERROR_ENOENT);
    Require(std::filesystem::remove(cache / "CA_LIST.cer"));
    for (const auto& entry : std::filesystem::directory_iterator(cache)) Require(entry.path().extension() != ".tmp");

    const int descriptor = sceKernelOpen(path.c_str(), 0, 0);
    Require(descriptor >= 0);
    Require(sceKernelClose(descriptor) == 0);
    Clear(cache);
    return 0;
}
