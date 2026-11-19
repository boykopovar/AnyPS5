#include "prx/libSceLibcInternal/include/Forwards.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal variadic forward: %s\n", message);
    std::abort();
}

}

int main() {
    char buffer[256];
    const int written = sprintf_nid_disambig1_nid_postfix(buffer, "%d|%s|%f|%x", 7, "hi", 2.5, 0xabc);
    Require(written == 17 && std::strcmp(buffer, "7|hi|2.500000|abc") == 0, "sprintf dropped or mangled variadic arguments");

    const int clipped = snprintf_nid_disambig1_nid_postfix(buffer, 5, "%s", "abcdef");
    Require(std::strcmp(buffer, "abcd") == 0, "snprintf ignored the buffer size");
    Require(clipped == 6, "snprintf did not report the full converted length");

    const int sized = snprintf_s_nid_disambig1_nid_postfix(buffer, sizeof(buffer), "%s:%d:%f", "ok", -3, 0.5);
    Require(sized > 0 && std::strcmp(buffer, "ok:-3:0.500000") == 0, "snprintf_s dropped variadic arguments");

    const int secured = sprintf_s_nid_disambig1_nid_postfix(buffer, sizeof(buffer), "%d %s", 9, "ok");
    Require(secured == 4 && std::strcmp(buffer, "9 ok") == 0, "sprintf_s dropped variadic arguments");

    int ivalue = 0;
    char svalue[32] {};
    double dvalue = 0;
    const int scanned = sscanf_nid_disambig1_nid_postfix("42 text 1.5", "%d %s %lf", &ivalue, svalue, &dvalue);
    Require(scanned == 3 && ivalue == 42 && std::strcmp(svalue, "text") == 0 && dvalue == 1.5,
        "sscanf dropped or mangled variadic arguments");

    ivalue = 0;
    svalue[0] = 0;
    const int securedScan = sscanf_s_nid_disambig1_nid_postfix("42 text", "%d %s", &ivalue, svalue,
        static_cast<unsigned int>(sizeof(svalue)));
    Require(securedScan == 2 && ivalue == 42 && std::strcmp(svalue, "text") == 0,
        "sscanf_s dropped the pointer or size variadic arguments");

    char16_t wide[64] {};
    const int wideWritten = swprintf_nid_disambig1_nid_postfix(wide, 64, u"%d|%f", 42, 1.5);
    Require(wideWritten == 11 && std::char_traits<char16_t>::compare(wide, u"42|1.500000", 11) == 0 && wide[11] == 0,
        "swprintf dropped or mangled variadic arguments");

    const int echoed = printf_nid_disambig1_nid_postfix("%d|%s|%f\n", 1, "x", 2.5);
    Require(echoed == 13, "printf dropped or mangled variadic arguments");

    const std::filesystem::path tempPath = std::filesystem::absolute("libc_internal_variadic_forwards.tmp");
    FileStream* stream = fopen_nid_postfix(tempPath.string().c_str(), "w+");
    Require(stream != nullptr, "fopen did not create the probe stream");
    const int streamWritten = fprintf_nid_disambig1_nid_postfix(stream, "%d %s %f", 11, "str", 3.25);
    Require(streamWritten == 15, "fprintf dropped or mangled variadic arguments");
    rewind_nid_postfix(stream);
    ivalue = 0;
    svalue[0] = 0;
    dvalue = 0;
    const int streamScanned = fscanf_nid_disambig1_nid_postfix(stream, "%d %31s %lf", &ivalue, svalue, &dvalue);
    Require(streamScanned == 3 && ivalue == 11 && std::strcmp(svalue, "str") == 0 && dvalue == 3.25,
        "fscanf dropped or mangled variadic arguments");
    fclose_nid_postfix(stream);
    std::filesystem::remove(tempPath);

    std::puts("libc internal variadic forwards pass varargs through the v* providers");
    return 0;
}
