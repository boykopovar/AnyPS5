#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "prx/libkernel/File/include/TrustedRoots.hpp"
#else
#include <unistd.h>
#endif

namespace {
constexpr char CertificateBegin[] = "-----BEGIN CERTIFICATE-----";
constexpr char CertificateEnd[] = "-----END CERTIFICATE-----";

std::string CertificateBlocksOnly(const std::string& bundle) {
    const std::string_view begin(CertificateBegin);
    const std::string_view end(CertificateEnd);
    std::string certificates;
    std::size_t certificateStart = 0;
    bool inCertificate = false;
    for (std::size_t lineStart = 0; lineStart < bundle.size();) {
        const auto newline = bundle.find('\n', lineStart);
        const auto lineEnd = newline == std::string::npos ? bundle.size() : newline;
        auto line = std::string_view(bundle).substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        if (line == begin) {
            certificateStart = lineStart;
            inCertificate = true;
        } else if (line == end && inCertificate) {
            const auto blockEnd = newline == std::string::npos ? lineEnd : newline + 1;
            certificates.append(bundle, certificateStart, blockEnd - certificateStart);
            inCertificate = false;
        }

        if (newline == std::string::npos) break;
        lineStart = newline + 1;
    }
    return certificates;
}

std::optional<std::string> ReadFile(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) return std::nullopt;
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::string text;
    char chunk[4096];
    while (file.read(chunk, sizeof(chunk)) || file.gcount() > 0) text.append(chunk, static_cast<std::size_t>(file.gcount()));
    if (file.bad()) return std::nullopt;
    return text;
}

#ifdef _WIN32
std::string HostCertificates() {
    return HostTrustedTlsRoots_nid_no_patch();
}

unsigned long ProcessId() { return GetCurrentProcessId(); }
#else
std::string HostCertificates() {
    std::array<const char*, 5> candidates{std::getenv("SSL_CERT_FILE"), "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem", "/etc/ssl/cert.pem"};
    for (const char* candidate : candidates) {
        std::error_code error;
        if (candidate == nullptr || *candidate == '\0' || !std::filesystem::is_regular_file(candidate, error)) continue;
        if (auto text = ReadFile(candidate)) return *text;
    }
    return {};
}

unsigned long ProcessId() { return static_cast<unsigned long>(getpid()); }
#endif

std::filesystem::path WriteCertificateList(const std::filesystem::path& root) {
    const auto bundle = CertificateBlocksOnly(HostCertificates());
    if (bundle.empty()) return {};
    const auto directory = root / "cert_cache";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return {};
    const auto target = directory / "CA_LIST.cer";
    if (ReadFile(target) == bundle) return target;
    auto temporary = target;
    temporary += "." + std::to_string(ProcessId()) + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file.write(bundle.data(), static_cast<std::streamsize>(bundle.size()));
        if (!file.flush()) {
            file.close();
            std::filesystem::remove(temporary, error);
            return {};
        }
    }
    using std::filesystem::perms;
    std::filesystem::permissions(temporary, perms::owner_read | perms::group_read | perms::others_read, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return {};
    }
    if (std::filesystem::exists(target, error)) std::filesystem::permissions(target, perms::owner_write, std::filesystem::perm_options::add, error);
    std::filesystem::rename(temporary, target, error);
    if (error) {
        std::filesystem::permissions(temporary, perms::owner_write, std::filesystem::perm_options::add, error);
        std::filesystem::remove(temporary, error);
        return ReadFile(target) == bundle ? target : std::filesystem::path{};
    }
    return target;
}

std::filesystem::path CertificateList(const std::filesystem::path& root) {
    static std::mutex lock;
    static std::filesystem::path list;
    std::lock_guard guard(lock);
    if (list.empty()) list = WriteCertificateList(root);
    return list;
}

const bool g_certificateListProvided = [] {
    AddPathProvider_nid_no_patch((std::string("/") + sceKernelGetFsSandboxRandomWord() + "/common/cert/CA_LIST.cer").c_str(), &CertificateList);
    AddPathProvider_nid_no_patch("/system/common/cert/CA_LIST.cer", &CertificateList);
    AddPathProvider_nid_no_patch("/common/cert/CA_LIST.cer", &CertificateList);
    return true;
}();
}
