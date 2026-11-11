#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}

namespace {

void APS5_VABI IgnoreOutput(NKWebKit::GuestCommonPtr) {}

struct Formatters {
    jpeg_error_mgr hostError{};
    jpeg_common_struct host{};
    NKWebKit::GuestJpegErrorMgr guestError{};
    NKWebKit::GuestJpegCommon guest{};

    Formatters() {
        host.err = jpeg_std_error(&hostError);
        guest.err = jpeg_std_error_nid_postfix(&guestError);
        guestError.output_message = IgnoreOutput;
        hostError.trace_level = guestError.trace_level = 3;
        hostError.num_warnings = guestError.num_warnings = 17;
    }

    void Tables(const char* const* primary, int lastPrimary, const char* const* addon, int firstAddon, int lastAddon) {
        hostError.jpeg_message_table = guestError.jpeg_message_table = primary;
        hostError.last_jpeg_message = guestError.last_jpeg_message = lastPrimary;
        hostError.addon_message_table = guestError.addon_message_table = addon;
        hostError.first_addon_message = guestError.first_addon_message = firstAddon;
        hostError.last_addon_message = guestError.last_addon_message = lastAddon;
    }

    void Code(int code) {
        hostError.msg_code = guestError.msg_code = code;
        for (int i = 0; i < 8; ++i) hostError.msg_parm.i[i] = guestError.msg_parm.i[i] = 17 + i;
    }

    void Text(const char* text) {
        std::snprintf(hostError.msg_parm.s, sizeof(hostError.msg_parm.s), "%s", text);
        std::snprintf(guestError.msg_parm.s, sizeof(guestError.msg_parm.s), "%s", text);
    }

    bool Check(const char* name) {
        char expected[JMSG_LENGTH_MAX]{};
        char actual[JMSG_LENGTH_MAX]{};
        hostError.format_message(&host, expected);
        guestError.format_message(&guest, actual);
        bool passed = std::strcmp(actual, expected) == 0
            && std::memcmp(&guestError.msg_parm, &hostError.msg_parm, sizeof(hostError.msg_parm)) == 0;
        passed = passed && guestError.msg_code == hostError.msg_code && guestError.trace_level == hostError.trace_level
            && guestError.num_warnings == hostError.num_warnings;
        bool threw = false;
        try {
            guestError.error_exit(&guest);
        } catch (const std::runtime_error& error) {
            threw = error.what() == std::string("libjpeg: ") + expected;
        }
        passed = passed && threw;
        if (!passed) std::fprintf(stderr, "JPEG message %s (code %d): expected '%s', observed '%s'\n", name, hostError.msg_code, expected, actual);
        return passed;
    }
};

} // namespace

int main() {
    constexpr const char* primary[]{"Caller unknown %d", "Caller number %d", "Caller text %s"};
    constexpr const char* addon[]{"Addon number %d", "Addon pair %d %d", "Addon text %s"};
    const std::string longText(79, 'x');
    int checked = 0;
    int matched = 0;
    const auto check = [&](Formatters& formatters, const char* name) {
        ++checked;
        if (formatters.Check(name)) ++matched;
    };

    constexpr int standardCodes[]{JERR_NO_SOI, JERR_BAD_STATE, 0, -1, INT_MAX};
    for (const int code : standardCodes) {
        Formatters formatters;
        formatters.Code(code);
        check(formatters, "standard");
    }
    {
        Formatters formatters;
        formatters.Code(JERR_TFILE_CREATE);
        formatters.Text(longText.c_str());
        check(formatters, "standard long text");
    }
    for (const int code : {0, 1, 2, 3, 699, 700, 701, 702, 703, -1, INT_MIN, INT_MAX}) {
        Formatters formatters;
        formatters.Tables(primary, 2, addon, 700, 702);
        formatters.Code(code);
        if (code == 2 || code == 702) formatters.Text(longText.c_str());
        check(formatters, "caller tables");
    }
    for (const int code : {-4, -3, -2, -1, 0}) {
        Formatters formatters;
        formatters.Tables(primary, 2, addon, -3, -1);
        formatters.Code(code);
        if (code == -1) formatters.Text("negative addon range");
        check(formatters, "negative addon range");
    }
    for (const int code : {1, 2, 3}) {
        Formatters formatters;
        formatters.Tables(primary, 2, addon, 1, 3);
        formatters.Code(code);
        if (code == 2 || code == 3) formatters.Text("overlapping tables");
        check(formatters, "primary table precedence");
    }
    {
        Formatters formatters;
        formatters.Tables(primary, 2, nullptr, 700, 702);
        formatters.Code(701);
        check(formatters, "absent addon table");
    }
    {
        Formatters formatters;
        formatters.Tables(primary, 2, addon, 702, 700);
        formatters.Code(701);
        check(formatters, "empty addon range");
    }
    std::printf("JPEG message formatting and error propagation: %d/%d cases match bundled libjpeg\n", matched, checked);
    return matched == checked ? 0 : 1;
}
