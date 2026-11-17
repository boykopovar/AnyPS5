#include "prx/libSceHttp/include/Http.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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

    constexpr int invalidValue = static_cast<int>(0x804311FEu);
    SceHttpUriElement element{};
    std::uint8_t pool[1024]{};
    errno = 71;
    Require(sceHttpUriParse(reinterpret_cast<SceHttpUriElement*>(0x10), "http://example.com/", pool, nullptr, sizeof(pool)) == invalidValue);
    Require(errno == 71);
    errno = 71;
    Require(sceHttpUriParse(&element, "http://example.com/", reinterpret_cast<void*>(0x10), nullptr, sizeof(pool)) == invalidValue);
    Require(errno == 71);
    Require(sceHttpUriParse(&element, "http://example.com/", pool, nullptr, sizeof(pool)) == 0);
    Require(element.hostname != nullptr && std::string(element.hostname) == "example.com");

    std::memset(&element, 0xAB, sizeof(element));
    std::memset(pool, 0, sizeof(pool));
    errno = 71;
    Require(sceHttpUriCopy(&element, reinterpret_cast<const SceHttpUriElement*>(0x10), pool, sizeof(pool), nullptr) == invalidUrl);
    Require(errno == 71);

    SceHttpUriElement source{};
    source.hostname = const_cast<char*>("example.com");
    source.port = 8080;
    Require(sceHttpUriCopy(&element, &source, pool, sizeof(pool), nullptr) == 0);
    Require(std::string(element.hostname) == "example.com" && element.port == 8080);

    source.hostname = reinterpret_cast<char*>(0x10);
    Require(sceHttpUriCopy(&element, &source, pool, sizeof(pool), nullptr) == invalidUrl);

    source.hostname = const_cast<char*>("example.com");
    errno = 71;
    Require(sceHttpUriCopy(reinterpret_cast<SceHttpUriElement*>(0x10), &source, pool, sizeof(pool), nullptr) == invalidValue);
    Require(errno == 71);
    errno = 71;
    Require(sceHttpUriCopy(&element, &source, reinterpret_cast<void*>(0x10), sizeof(pool), nullptr) == invalidValue);
    Require(errno == 71);
    errno = 71;
    Require(sceHttpUriCopy(nullptr, &source, nullptr, 0, reinterpret_cast<std::size_t*>(0x10)) == invalidValue);
    Require(errno == 71);
    std::size_t copiedRequired = 0;
    Require(sceHttpUriCopy(nullptr, &source, nullptr, 0, &copiedRequired) == 0 && copiedRequired == 16);
    return 0;
}
