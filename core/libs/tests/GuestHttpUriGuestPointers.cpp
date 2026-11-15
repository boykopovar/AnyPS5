#include "prx/libSceHttp/include/Http.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int invalidUrl = static_cast<int>(0x80433060u);

    errno = 71;
    Require(sceHttpUriParse(nullptr, reinterpret_cast<const char*>(0x10), nullptr, nullptr, 0) == invalidUrl);
    Require(errno == 71);

    const std::string longUrl = "http://example.com/" + std::string(0x5000, 'a');
    std::size_t required = 0;
    Require(sceHttpUriParse(nullptr, longUrl.c_str(), nullptr, &required, 0) == 0);
    Require(required > 0x5000);

    errno = 71;
    Require(sceHttpUriParse(nullptr, "http://example.com/", nullptr, reinterpret_cast<std::size_t*>(0x10), 0) == static_cast<int>(0x804311FEu));
    Require(errno == 71);
    return 0;
}
