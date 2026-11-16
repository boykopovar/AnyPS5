#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <io.h>
#define DUPLICATE _dup
#define REPLACE _dup2
#define CLOSE _close
#define FILENO _fileno
#else
#include <unistd.h>
#define DUPLICATE dup
#define REPLACE dup2
#define CLOSE close
#define FILENO fileno
#endif

namespace {

using Handler = void (APS5_VABI *)(Ubsan::CfiCheckFailData*, std::uint64_t, std::uint64_t);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stdout, "UBSan CFI: %s\n", message);
    std::fflush(stdout);
    std::abort();
}

template<typename TException, typename TAction>
bool Throws(TAction action) {
    try { action(); } catch (const TException&) { return true; }
    return false;
}

template<typename TAction>
std::string Captured(TAction action) {
    const auto path = (std::filesystem::temp_directory_path() / "anyps5-ubsan-cfi.txt").string();
    std::fflush(stderr);
    const int saved = DUPLICATE(FILENO(stderr));
    std::FILE* file = std::fopen(path.c_str(), "wb");
    Require(file != nullptr, "temporary file");
    REPLACE(FILENO(file), FILENO(stderr));
    try {
        action();
    } catch (...) {
        std::fflush(stderr);
        REPLACE(saved, FILENO(stderr));
        CLOSE(saved);
        std::fclose(file);
        std::remove(path.c_str());
        throw;
    }
    std::fflush(stderr);
    REPLACE(saved, FILENO(stderr));
    CLOSE(saved);
    std::fclose(file);
    std::ifstream input(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    std::remove(path.c_str());
    return text;
}

struct Type {
    std::uint16_t kind;
    std::uint16_t info;
    char name[32];
};

Ubsan::CfiCheckFailData Data(std::uint8_t kind, const char* file, std::uint32_t line, std::uint32_t column, const Type& type) {
    Ubsan::CfiCheckFailData data{};
    data.kind = kind;
    data.location = {file, line, column};
    data.type = reinterpret_cast<const Ubsan::TypeDescriptor*>(&type);
    return data;
}

void Check(Handler report, Handler fatal) {
    const Type function{0xFFFF, 0, "void (int)"};
    const Type object{0, 0, "Base"};

    auto call = Data(4, "src/a.cpp", 12, 7, function);
    Require(Captured([&] { report(&call, 0x401000, 0); }) ==
            "src/a.cpp:12:7: runtime error: control flow integrity check for type 'void (int)' failed during indirect function call\n"
            "<unknown>: note: (unknown) defined here\n", "indirect call report");
    Require(call.location.column == 0xFFFFFFFFu, "the location is marked as reported");
    Require(Captured([&] { report(&call, 0x401000, 0); }).empty(), "a location reports once");

    auto member = Data(5, "src/m.cpp", 3, 0, function);
    Require(Captured([&] { report(&member, 0, 0); }) ==
            "src/m.cpp:3: runtime error: control flow integrity check for type 'void (int)' failed during non-virtual pointer to member function call\n"
            "<unknown>: note: (unknown) defined here\n", "member call report without a column");

    auto virtualCall = Data(0, "v.cpp", 0, 9, object);
    Require(Captured([&] { report(&virtualCall, 0x1234, 0); }) ==
            "v.cpp: runtime error: control flow integrity check for type 'Base' failed during virtual call (vtable address 0x000000001234)\n"
            "<unknown>: note: invalid vtable\n", "virtual call report without a line");

    const char* kinds[] = {"virtual call", "non-virtual call", "base-to-derived cast", "cast to unrelated type", nullptr, nullptr,
                           "virtual pointer to member function call"};
    for (std::uint8_t kind : {1, 2, 3, 6}) {
        auto cast = Data(kind, nullptr, 5, 5, object);
        const std::string expected = std::string("<unknown>: runtime error: control flow integrity check for type 'Base' failed during ") + kinds[kind] +
            " (vtable address 0x7fffabcd0010)\n<unknown>: note: vtable is of an unknown type\n";
        Require(Captured([&] { report(&cast, 0x7fffabcd0010, 1); }) == expected, "vtable check kinds");
    }

    auto fatalCall = Data(4, "f.cpp", 1, 1, function);
    std::string text;
    Require(Throws<std::runtime_error>([&] { text = Captured([&] { fatal(&fatalCall, 0, 0); }); }), "the abort handler ends the call");
    auto again = Data(4, "f.cpp", 1, 1, function);
    Require(Captured([&] {
        Require(Throws<std::runtime_error>([&] { fatal(&again, 0, 0); }), "first abort");
    }).find("f.cpp:1:1: runtime error") == 0, "the abort handler reports first");
    Require(Captured([&] {
        Require(Throws<std::runtime_error>([&] { fatal(&again, 0, 0); }), "a reported location still aborts");
    }).empty(), "a reported location is not printed again");

    auto unknown = Data(7, "u.cpp", 1, 1, object);
    Require(Throws<std::runtime_error>([&] { report(&unknown, 0, 0); }), "unknown check kind throws");
    Require(unknown.location.column == 1, "unknown check kind leaves the location");
    Require(Throws<std::invalid_argument>([&] { report(nullptr, 0, 0); }), "null data throws");
}

}

int main() {
    Check(__ubsan_handle_cfi_check_fail_nid_disambig1_nid_postfix, __ubsan_handle_cfi_check_fail_abort_nid_disambig1_nid_postfix);
    Check(__ubsan_handle_cfi_check_fail_nid_postfix, __ubsan_handle_cfi_check_fail_abort_nid_postfix);
    return 0;
}
