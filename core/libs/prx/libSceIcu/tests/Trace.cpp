#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU trace check failed at line %u\n", location.line());
    std::exit(EXIT_FAILURE);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

std::string message;
std::string exitFormat;
const void* seenContext = nullptr;
int seenNumber = -1;
int seenLevel = -1;

void APS5_VABI Entry_nid_no_patch(const void* context, std::int32_t number) {
    seenContext = context;
    seenNumber = number;
}

void APS5_VABI Exit_nid_no_patch(const void* context, std::int32_t number, const char* format, Icu::TraceArguments arguments) {
    seenContext = context;
    seenNumber = number;
    exitFormat = format;
    if (exitFormat == "Returns %d.  Status = %p.") return;
    char buffer[200];
    utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, format, arguments);
    message = buffer;
}

void APS5_VABI Data_nid_no_patch(const void* context, std::int32_t number, std::int32_t level, const char* format, Icu::TraceArguments arguments) {
    seenContext = context;
    seenNumber = number;
    seenLevel = level;
    char buffer[200];
    utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, format, arguments);
    message = buffer;
}

template<typename... TArgs>
void Format(const std::string& expected, const char* format, TArgs... arguments) {
    std::array<char, 512> buffer;
    buffer.fill('@');
    const auto length = utrace_format_nid_postfix(buffer.data(), buffer.size(), 0, format, arguments...);
    Require(length == static_cast<std::int32_t>(expected.size()) + 1);
    Require(!std::memcmp(buffer.data(), expected.c_str(), expected.size() + 1));
    Require(buffer[length] == '@');
    Require(utrace_format_nid_postfix(nullptr, 0, 0, format, arguments...) == length);
    Require(utrace_format_nid_postfix(nullptr, -1, 0, format, arguments...) == length);
    for (int capacity = 0; capacity <= length; ++capacity) {
        buffer.fill('@');
        Require(utrace_format_nid_postfix(buffer.data(), capacity, 0, format, arguments...) == length);
        Require(!std::memcmp(buffer.data(), expected.c_str(), capacity));
        Require(buffer[capacity] == '@');
    }
}

}

int main() {
    Require(utrace_getLevel_nid_postfix() == 0);
    for (const auto level : {-2147483647, -2, -1, 0, 3, 9, 10, 2147483647}) {
        utrace_setLevel_nid_postfix(level);
        Require(utrace_getLevel_nid_postfix() == (level < -1 ? -1 : level > 9 ? 9 : level));
    }
    Format("", "");
    Format("a% q%", "a%% %q%");
    Format("ff ffff ffffffff ffffffffffffffff", "%b %h %d %l", -1, -1, -1, std::int64_t{-1});
    Format("*NULL*", "%s", static_cast<const char*>(nullptr));
    Format("0000000000001234", "%p", reinterpret_cast<void*>(0x1234));
    Format("0041 d800 0000 ", "%S", std::array<char16_t, 3>{0x41, 0xd800, 0}.data(), -1);
    Format("0041 ", "%S", u"AB", 1);
    Format("", "%S", u"AB", -2);
    Format("*NULL*", "%S", static_cast<const char16_t*>(nullptr), 5);
    Format("ff 00 7f [00000003]", "%vb", std::array<std::int8_t, 3>{-1, 0, 127}.data(), 3);
    Format("ffff 0000 [ffffffff]", "%vh", std::array<std::int16_t, 3>{-1, 0, 12}.data(), -1);
    Format("12345678 [00000001]", "%vd", std::array<std::int32_t, 1>{0x12345678}.data(), 1);
    Format("123456789abcdef0 [00000001]", "%vl", std::array<std::int64_t, 1>{0x123456789abcdef0}.data(), 1);
    Format("ab[ffffffff]", "%vc", "ab", -1);
    Format("*NULL* [00000003]", "%vd", static_cast<const void*>(nullptr), 3);
    Format("00000000000000010000000000000000[ffffffff]", "%vp", std::array<const void*, 2>{reinterpret_cast<void*>(1), nullptr}.data(), -1);
    Format("a\n*NULL*\n[ffffffff]", "%vs", std::array<const char*, 2>{"a", nullptr}.data(), -1);
    Format("0041 0000 \n[00000001]", "%vS", std::array<const char16_t*, 1>{u"A"}.data(), 1);
    Format("[00000002]", "%vx", "a", 2);
    Format("[ffffffff]", "%vx", "a", -1);
    char buffer[100];
    Require(utrace_format_nid_postfix(buffer, 100, 2, "a\nb") == 8 && !std::strcmp(buffer, "  a\n  b"));
    Require(utrace_format_nid_postfix(nullptr, 0, 2, "a\nb") == 8);
    Require(utrace_format_nid_postfix(buffer, 100, 2, "%d", 1) == 9 && !std::strcmp(buffer, "00000001"));
    Invalid([&] { utrace_format_nid_postfix(nullptr, 1, 0, "a"); });
    Invalid([&] { utrace_format_nid_postfix(buffer, 100, 0, nullptr); });
    Invalid([&] { utrace_format_nid_postfix(buffer, 100, 0, "%l%vx", std::int64_t{1}, "a", -1); });
    int context = 0;
    utrace_setFunctions_nid_postfix(&context, Entry_nid_no_patch, Exit_nid_no_patch, Data_nid_no_patch);
    Icu::TraceEntry entry = nullptr;
    Icu::TraceExit exit = nullptr;
    Icu::TraceData data = nullptr;
    const void* foundContext = nullptr;
    utrace_getFunctions_nid_postfix(&foundContext, &entry, &exit, &data);
    Require(foundContext == &context && entry == Entry_nid_no_patch && exit == Exit_nid_no_patch && data == Data_nid_no_patch);
    utrace_setLevel_nid_postfix(-1);
    utrace_entry_nid_postfix(42);
    Require(seenContext == &context && seenNumber == 42);
    utrace_data_nid_postfix(7, 9, "%d %s", 10, "abc");
    Require(message == "0000000a abc" && seenNumber == 7 && seenLevel == 9);
    utrace_exit_nid_postfix(8, 0);
    Require(message == "Returns.");
    utrace_exit_nid_postfix(8, 1, 10);
    Require(message == "Returns 0000000a.");
    utrace_exit_nid_postfix(8, 16, 1);
    Require(message == "Returns.  Status = 00000001.");
    utrace_exit_nid_postfix(8, 17, 2, 3);
    Require(message == "Returns 00000002.  Status = 00000003.");
    utrace_exit_nid_postfix(8, 18, static_cast<void*>(nullptr), 0);
    Require(exitFormat == "Returns %d.  Status = %p.");
    Invalid([] { utrace_exit_nid_postfix(8, 3); });
    Invalid([&] { utrace_getFunctions_nid_postfix(nullptr, &entry, &exit, &data); });
    utrace_setFunctions_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    utrace_exit_nid_postfix(8, 3);
    utrace_data_nid_postfix(7, 9, nullptr);
    utrace_entry_nid_postfix(8);
    Require(!std::strcmp(utrace_functionName_nid_postfix(0), "u_init"));
    Require(!std::strcmp(utrace_functionName_nid_postfix(0x1007), "ucnv_unload"));
    Require(!std::strcmp(utrace_functionName_nid_postfix(0x2008), "ucol_strcollUTF8"));
    Require(!std::strcmp(utrace_functionName_nid_postfix(0x3003), "res-open"));
    for (const auto number : {-1, 2, 0x1008, 0x2009, 0x3004, 0x4000, 2147483647})
        Require(!std::strcmp(utrace_functionName_nid_postfix(number), "[BOGUS Trace Function Number]"));
    std::puts("ICU trace tests passed");
}
