#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {
constexpr int GuestEnoent = 2;
constexpr int GuestEio = 5;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;

std::int64_t Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

int GuestError(const std::error_code& error) {
    const int value = error.default_error_condition().value();
    return value > 0 && value <= 34 ? value : GuestEio;
}
}

extern "C" {

std::int64_t APS5_VABI readlink_nid_postfix(const char* path, char* buffer, std::size_t size) {
    if (path == nullptr) return Fail(GuestEfault);
    if (*path == '\0') return Fail(GuestEnoent);
    if (size > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) return Fail(GuestEinval);
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    const auto status = std::filesystem::symlink_status(native, error);
    if (error) return Fail(GuestError(error));
    if (status.type() == std::filesystem::file_type::not_found) return Fail(GuestEnoent);
    if (!std::filesystem::is_symlink(status)) return Fail(GuestEinval);
    const auto target = std::filesystem::read_symlink(native, error).generic_string();
    if (error) return Fail(GuestError(error));
    const auto count = std::min(size, target.size());
    if (count != 0 && buffer == nullptr) return Fail(GuestEfault);
    if (count != 0) std::memcpy(buffer, target.data(), count);
    return static_cast<std::int64_t>(count);
}

}
