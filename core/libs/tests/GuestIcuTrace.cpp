#include "prx/libSceIcu/libSceIcu.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU trace: %s\n", message);
    std::abort();
}

template<class F>
bool Throws(F call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct Record {
    const void* context = nullptr;
    std::int32_t function = 0;
    std::int32_t level = 0;
    std::int32_t calls = 0;
    std::string text;
};

Record entered;
Record exited;
Record data;

std::string Render(const char* format, VaList* args) {
    char buffer[256];
    const std::int32_t length = utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, format, args);
    Require(length > 0 && length <= static_cast<std::int32_t>(sizeof(buffer)), "a callback renders its arguments");
    return buffer;
}

void APS5_VABI Entered(const void* context, std::int32_t function) {
    entered.context = context;
    entered.function = function;
    ++entered.calls;
}

void APS5_VABI Exited(const void* context, std::int32_t function, const char* format, VaList* args) {
    exited.context = context;
    exited.function = function;
    exited.text = Render(format, args);
    ++exited.calls;
}

void APS5_VABI Traced(const void* context, std::int32_t function, std::int32_t level, const char* format, VaList* args) {
    data.context = context;
    data.function = function;
    data.level = level;
    data.text = Render(format, args);
    ++data.calls;
}

template<class... A>
std::string Format(std::int32_t indent, const char* format, A... args) {
    char buffer[256];
    std::memset(buffer, 'x', sizeof(buffer));
    const std::int32_t length = utrace_format_nid_postfix(buffer, sizeof(buffer), indent, format, args...);
    Require(length == static_cast<std::int32_t>(std::strlen(buffer)) + 1, "the length counts the output and its terminator");
    return buffer;
}

void Levels() {
    Require(utrace_getLevel_nid_postfix() == 0, "the level starts at UTRACE_ERROR");
    utrace_setLevel_nid_postfix(5);
    Require(utrace_getLevel_nid_postfix() == 5, "a level in range is kept");
    utrace_setLevel_nid_postfix(-7);
    Require(utrace_getLevel_nid_postfix() == -1, "a level below UTRACE_OFF becomes UTRACE_OFF");
    utrace_setLevel_nid_postfix(10);
    Require(utrace_getLevel_nid_postfix() == 9, "a level above UTRACE_VERBOSE becomes UTRACE_VERBOSE");
    utrace_setLevel_nid_postfix(-1);
    Require(utrace_getLevel_nid_postfix() == -1, "UTRACE_OFF is kept");
    utrace_setLevel_nid_postfix(9);
    Require(utrace_getLevel_nid_postfix() == 9, "UTRACE_VERBOSE is kept");
}

void Functions() {
    const void* context = &entered;
    UTraceEntry entry = Entered;
    UTraceExit exit = Exited;
    UTraceData trace = Traced;
    utrace_getFunctions_nid_postfix(&context, &entry, &exit, &trace);
    Require(!context && !entry && !exit && !trace, "no functions are installed at first");
    utrace_entry_nid_postfix(1);
    utrace_exit_nid_postfix(1, 3);
    utrace_data_nid_postfix(1, 5, "%d", 1);
    Require(entered.calls == 0 && exited.calls == 0 && data.calls == 0, "nothing is called without functions");

    int marker = 0;
    utrace_setFunctions_nid_postfix(&marker, Entered, Exited, Traced);
    utrace_getFunctions_nid_postfix(&context, &entry, &exit, &trace);
    Require(context == &marker && entry == Entered && exit == Exited && trace == Traced, "the installed functions come back");

    utrace_entry_nid_postfix(0x1003);
    Require(entered.calls == 1 && entered.context == &marker && entered.function == 0x1003, "entry passes the context and function");

    utrace_exit_nid_postfix(0x2001, 0);
    Require(exited.calls == 1 && exited.context == &marker && exited.function == 0x2001 && exited.text == "Returns.", "an exit without a value");
    utrace_exit_nid_postfix(2, 1, 42);
    Require(exited.function == 2 && exited.text == "Returns 0000002a.", "an exit with a value");
    utrace_exit_nid_postfix(2, 0x10, 7);
    Require(exited.text == "Returns.  Status = 00000007.", "an exit with a status");
    utrace_exit_nid_postfix(2, 0x11, 42, 7);
    Require(exited.text == "Returns 0000002a.  Status = 00000007.", "an exit with a value and a status");
    utrace_exit_nid_postfix(2, 0x12, 42, reinterpret_cast<void*>(0x1234));
    Require(exited.text == "Returns 0000002a.  Status = 0000000000001234.", "an exit with a pointer and a status keeps ICU's format");
    Require(Throws([] { utrace_exit_nid_postfix(2, 3); }), "an unknown return type is refused");
    Require(Throws([] { utrace_exit_nid_postfix(2, 0x13); }), "an unknown return type with a status is refused");
    Require(exited.calls == 5, "a refused exit calls nothing");

    utrace_data_nid_postfix(0x3001, 7, "%s=%d", "size", 12);
    Require(data.calls == 1 && data.context == &marker && data.function == 0x3001 && data.level == 7 && data.text == "size=0000000c",
            "data passes the level, the format and its arguments");

    utrace_setFunctions_nid_postfix(nullptr, Entered, nullptr, nullptr);
    utrace_exit_nid_postfix(2, 3);
    utrace_data_nid_postfix(2, 5, "%d", 1);
    utrace_entry_nid_postfix(9);
    Require(exited.calls == 5 && data.calls == 1 && entered.calls == 2 && !entered.context && entered.function == 9, "each function is optional");

    Require(Throws([&] { utrace_getFunctions_nid_postfix(nullptr, &entry, &exit, &trace); }), "a null context pointer is refused");
    Require(Throws([&] { utrace_getFunctions_nid_postfix(&context, nullptr, &exit, &trace); }), "a null entry pointer is refused");
    Require(Throws([&] { utrace_getFunctions_nid_postfix(&context, &entry, nullptr, &trace); }), "a null exit pointer is refused");
    Require(Throws([&] { utrace_getFunctions_nid_postfix(&context, &entry, &exit, nullptr); }), "a null data pointer is refused");
}

void Cleanup() {
    int marker = 0;
    utrace_setFunctions_nid_postfix(&marker, Entered, Exited, Traced);
    utrace_setLevel_nid_postfix(7);
    u_cleanup_nid_postfix();
    const void* context = &marker;
    UTraceEntry entry = Entered;
    UTraceExit exit = Exited;
    UTraceData trace = Traced;
    utrace_getFunctions_nid_postfix(&context, &entry, &exit, &trace);
    Require(!context && !entry && !exit && !trace, "u_cleanup removes the functions");
    Require(utrace_getLevel_nid_postfix() == -1, "u_cleanup turns tracing off");
}

void Scalars() {
    Require(Format(0, "plain") == "plain", "literal text is copied");
    Require(Format(0, "") == "", "an empty format gives an empty string");
    Require(Format(0, "%c%c", 'A', 0x142) == "AB", "%c takes the low byte");
    Require(Format(0, "[%s]", "text") == "[text]", "%s copies a string");
    Require(Format(0, "[%s]", static_cast<const char*>(nullptr)) == "[*NULL*]", "%s of null");
    Require(Format(0, "%S", u"hi", 2) == "0068 0069 ", "%S prints code units with a length");
    Require(Format(0, "%S", u"ab", -1) == "0061 0062 0000 ", "%S with -1 stops after the terminator");
    Require(Format(0, "%S", u"ab", 0) == "", "%S with length 0 prints nothing");
    Require(Format(0, "%S", u"a\0b", 3) == "0061 0000 0062 ", "%S with a length prints past a zero");
    Require(Format(0, "%S", static_cast<const char16_t*>(nullptr), 3) == "*NULL*", "%S of null");
    Require(Format(0, "%b", 0x1ff) == "ff", "%b prints two digits");
    Require(Format(0, "%h", 0x12345) == "2345", "%h prints four digits");
    Require(Format(0, "%d", -1) == "ffffffff", "%d prints eight digits");
    Require(Format(0, "%l", static_cast<std::int64_t>(0x123456789abcdef0)) == "123456789abcdef0", "%l prints sixteen digits");
    Require(Format(0, "%p", reinterpret_cast<void*>(0xabc)) == "0000000000000abc", "%p prints the pointer");
    Require(Format(0, "%d %s %d", 1, "two", 3) == "00000001 two 00000003", "arguments are taken in order");
    Require(Format(0, "100%") == "100%", "a final % is kept");
    Require(Format(0, "%%") == "%", "%% prints %");
    Require(Format(0, "%q") == "q", "an unknown conversion prints its character");
}

void Vectors() {
    const char bytes[] = {1, 2, -1};
    const std::int16_t halves[] = {0x12, -2};
    const std::int32_t words[] = {5, 0};
    const std::int64_t longs[] = {0x1122334455667788};
    const void* pointers[] = {reinterpret_cast<void*>(0x10), nullptr};
    const char* strings[] = {"x", nullptr};
    const char16_t* units[] = {u"a", nullptr};
    Require(Format(0, "%vb", bytes, 3) == "01 02 ff [00000003]", "%vb");
    Require(Format(0, "%vh", halves, 2) == "0012 fffe [00000002]", "%vh");
    Require(Format(0, "%vd", words, -1) == "00000005 00000000 [ffffffff]", "%vd with -1 stops after a zero");
    Require(Format(0, "%vl", longs, 1) == "1122334455667788 [00000001]", "%vl");
    Require(Format(0, "%vp", pointers, -1) == "00000000000000100000000000000000[ffffffff]", "%vp with -1 stops after null");
    Require(Format(0, "%vc", "ab", -1) == "ab[ffffffff]", "%vc with -1 stops at the terminator");
    Require(Format(0, "%vc", "abc", 2) == "ab[00000002]", "%vc with a length");
    Require(Format(0, "%vs", strings, -1) == "x\n*NULL*\n[ffffffff]", "%vs puts each string on a line");
    Require(Format(0, "%vS", units, -1) == "0061 0000 \n*NULL*\n[ffffffff]", "%vS puts each string on a line");
    Require(Format(0, "%vd", static_cast<const void*>(nullptr), 2) == "*NULL* [00000002]", "a null vector");
    Require(Format(0, "%vx", words, 2) == "[00000002]", "an unknown vector type prints only the length");
    Require(Format(0, "%v", words, 2) == "[00000002]", "a vector at the end of the format");
    Require(Format(0, "%vd", words, 0) == "[00000000]", "an empty vector");
    Require(Format(0, "%l%vx", static_cast<std::int64_t>(0), words, -1) == "0000000000000000[ffffffff]",
            "an unknown type with -1 ends on the last zero value");
}

void Layout() {
    Require(Format(2, "a\nb") == "  a\n  b", "each line is indented");
    Require(Format(2, "a\n%d", 1) == "  a\n00000001", "hex digits are not indented");
    Require(Format(2, "a\n%c", 'z') == "  a\n  z", "characters are indented");
    Require(Format(1, "a\n") == " a\n", "a final newline is not followed by an indent");
    Require(Format(-3, "a\nb") == "a\nb", "a negative indent is none");

    char buffer[8];
    std::memset(buffer, '#', sizeof(buffer));
    Require(utrace_format_nid_postfix(buffer, 4, 0, "abcdef") == 7, "the length is the full length");
    Require(std::memcmp(buffer, "abcd####", 8) == 0, "output stops at the capacity, unterminated");
    Require(utrace_format_nid_postfix(nullptr, 0, 0, "abc") == 4, "preflighting");
    Require(utrace_format_nid_postfix(nullptr, -5, 0, "abc") == 4, "a negative capacity preflights");
    Require(utrace_format_nid_postfix(nullptr, 0, 2, "a\n") == 7, "preflighting counts an indent after a newline");
    std::memset(buffer, '#', sizeof(buffer));
    Require(utrace_format_nid_postfix(buffer, sizeof(buffer), 2, "a\n") == 5 && std::strcmp(buffer, "  a\n") == 0, "a newline inside the capacity");

    std::memset(buffer, '\n', sizeof(buffer));
    Require(utrace_format_nid_postfix(buffer, 2, 1, "a\nb") == 6, "text past the capacity is not read back");

    std::memset(buffer, '#', sizeof(buffer));
    Require(utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, "abc", nullptr) == 4 && std::strcmp(buffer, "abc") == 0,
            "a format without arguments needs no va_list");
    Require(Throws([&] { utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, "%d", nullptr); }), "a null va_list is refused when read");
    Require(Throws([&] { utrace_format_nid_postfix(buffer, sizeof(buffer), 0, nullptr); }), "a null format is refused");
    Require(Throws([] { utrace_format_nid_postfix(nullptr, 1, 0, "a"); }), "a null buffer with a capacity is refused");
}

void Names() {
    Require(std::strcmp(utrace_functionName_nid_postfix(0), "u_init") == 0, "u_init");
    Require(std::strcmp(utrace_functionName_nid_postfix(1), "u_cleanup") == 0, "u_cleanup");
    Require(std::strcmp(utrace_functionName_nid_postfix(0x1000), "ucnv_open") == 0, "ucnv_open");
    Require(std::strcmp(utrace_functionName_nid_postfix(0x1007), "ucnv_unload") == 0, "ucnv_unload");
    Require(std::strcmp(utrace_functionName_nid_postfix(0x2000), "ucol_open") == 0, "ucol_open");
    Require(std::strcmp(utrace_functionName_nid_postfix(0x2008), "ucol_strcollUTF8") == 0, "ucol_strcollUTF8");
    Require(std::strcmp(utrace_functionName_nid_postfix(0x3000), "resc") == 0, "resc");
    Require(std::strcmp(utrace_functionName_nid_postfix(0x3003), "res-open") == 0, "res-open");
    for (const std::int32_t bogus : {-1, 2, 0xfff, 0x1008, 0x1fff, 0x2009, 0x2fff, 0x3004, 0x4000}) {
        Require(std::strcmp(utrace_functionName_nid_postfix(bogus), "[BOGUS Trace Function Number]") == 0, "numbers outside the ranges are bogus");
    }
}

}

int main() {
    Levels();
    Functions();
    Cleanup();
    Scalars();
    Vectors();
    Layout();
    Names();
    std::puts("ICU trace: all checks passed");
    return 0;
}
