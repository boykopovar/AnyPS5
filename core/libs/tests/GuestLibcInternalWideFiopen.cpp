#include "prx/libSceLibcInternal/include/Fiopen.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

namespace {

constexpr int In = 0x01, Out = 0x02, Trunc = 0x10, Binary = 0x80;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal wide _Fiopen: %s\n", message);
    std::exit(1);
}

template<typename TCall>
bool ThrowsInvalidArgument(TCall&& call) {
    try {
        call();
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

std::vector<std::uint16_t> Wide(const std::u16string& text) {
    std::vector<std::uint16_t> units(text.begin(), text.end());
    units.push_back(0);
    return units;
}

std::u16string Ascii(const std::string& text) {
    return std::u16string(text.begin(), text.end());
}

} // namespace

int main() {
    const std::string stem = "wide-fiopen-" + std::to_string(getpid());
    const auto wideName = Wide(Ascii(stem) + u"-é.bin");
    const std::string narrowName = stem + "-\xc3\xa9.bin";
    const std::filesystem::path hostPath(reinterpret_cast<const char8_t*>(narrowName.c_str()));
    std::filesystem::remove(hostPath);

    auto* written = _ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi_nid_postfix(wideName.data(), Out | Binary, 0);
    Require(written != nullptr, "a wide name opened for output");
    constexpr char data[] = "wide";
    Require(fwrite_nid_postfix(data, 1, sizeof(data), written) == sizeof(data) && fclose_nid_postfix(written) == 0, "writing through the wide name");
    Require(std::filesystem::exists(hostPath), "the wide name opens the file named by its multibyte conversion");

    auto* read = _ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi_nid_postfix(wideName.data(), In | Binary, 0);
    Require(read != nullptr, "the wide name opened for input");
    char back[sizeof(data)] = {};
    Require(fread_nid_postfix(back, 1, sizeof(back), read) == sizeof(back) && std::memcmp(back, data, sizeof(data)) == 0, "the bytes read back through the wide name");
    Require(fclose_nid_postfix(read) == 0, "closing the wide-named stream");

    Require(_ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi_nid_postfix(wideName.data(), In | Trunc, 0) == nullptr, "a mode the narrow _Fiopen refuses is refused for a wide name too");
    const auto missing = Wide(Ascii(stem) + u"-missing.bin");
    Require(_ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi_nid_postfix(missing.data(), In, 0) == nullptr, "a missing file opened for input returns null");

    const auto tooLong = Wide(std::u16string(1024, u'a'));
    Require(ThrowsInvalidArgument([&] { _ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi_nid_postfix(tooLong.data(), In, 0); }), "a name of FILENAME_MAX bytes or more throws std::invalid_argument");
    Require(ThrowsInvalidArgument([&] { _ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi_nid_postfix(nullptr, In, 0); }), "a null name throws std::invalid_argument");

    std::filesystem::remove(hostPath);
    return 0;
}
