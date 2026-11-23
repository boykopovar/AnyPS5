#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using IcuRecoveredTest::Require;

struct Field {
    std::int32_t field;
    std::int32_t begin;
    std::int32_t end;
};

static_assert(sizeof(Field) == 12 && alignof(Field) == alignof(std::int32_t));

template<class Format>
void CheckOutput(Format&& format, std::u16string_view expected, std::int32_t begin, std::int32_t end) {
    const auto required = static_cast<std::int32_t>(expected.size());
    std::int32_t status = 0;
    Require(format(nullptr, 0, nullptr, &status) == required && status == 15,
        "actual number NID returns precise null-output preflight");
    for (const auto capacity : {required, required + 1, required + 8}) {
        std::array<char16_t, 128> output;
        output.fill(0xa55a);
        struct {
            std::uint32_t before = 0x1234a55a;
            Field value{0, -91, -92};
            std::uint32_t after = 0x5678a55a;
        } field;
        status = 0;
        const auto count = format(output.data() + 2, capacity, reinterpret_cast<UFieldPosition*>(&field.value), &status);
        Require(count == required && status == (capacity == required ? -124 : 0),
            "actual number NID preserves exact/full capacity status");
        Require(std::u16string_view(output.data() + 2, required) == expected,
            "actual number NID produces the independent expected digits");
        Require(field.value.field == 0 && field.value.begin == begin && field.value.end == end &&
            field.before == 0x1234a55a && field.after == 0x5678a55a,
            "actual number NID writes only the integer field record");
        for (std::size_t index = 0; index < output.size(); ++index) {
            if (index >= 2 && index < static_cast<std::size_t>(required) + 2) continue;
            const auto wanted = capacity > required && index == static_cast<std::size_t>(required) + 2 ? char16_t{} : char16_t{0xa55a};
            Require(output[index] == wanted, "actual number NID respects terminator, full tail and outside guards");
        }
    }
}

void Check(const char* path) {
#ifdef _WIN32
    auto module = LoadLibraryA(path);
#else
    auto module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    Require(module != nullptr, "load actual canonical NID-patched ICU artifact");
    const auto import = [&](const char* name) {
#ifdef _WIN32
        auto* address = GetProcAddress(module, name);
#else
        auto* address = dlsym(module, name);
#endif
        Require(address != nullptr, "resolve classic number-format NID");
        return address;
    };
    const auto open = reinterpret_cast<decltype(&unum_open_nid_postfix)>(import("smR8wpRiaGo"));
    const auto clone = reinterpret_cast<decltype(&unum_clone_nid_postfix)>(import("9JDnDlccCuM"));
    const auto close = reinterpret_cast<decltype(&unum_close_nid_postfix)>(import("mlcTS2ELqQs"));
    const auto doubleValue = reinterpret_cast<decltype(&unum_formatDouble_nid_postfix)>(import("LoU4jxGpQjw"));
    const auto decimal = reinterpret_cast<decltype(&unum_formatDecimal_nid_postfix)>(import("tm89z9VpUQY"));
    const auto symbol = reinterpret_cast<decltype(&unum_getSymbol_nid_postfix)>(import("iDjDG1mddFk"));
    const auto text = reinterpret_cast<decltype(&unum_getTextAttribute_nid_postfix)>(import("Ingmz6RYxYQ"));
    const auto setAttribute = reinterpret_cast<decltype(&unum_setAttribute_nid_postfix)>(import("bnVV1jt1Qms"));
    const auto setText = reinterpret_cast<decltype(&unum_setTextAttribute_nid_postfix)>(import("s5JKSkl+iCU"));
    std::int32_t status = 0;
    auto* handle = open(0, u"#,##0.00", -1, "en_US", nullptr, &status);
    Require(handle && status <= 0, "actual number NID opens an explicit locale and pattern");
    CheckOutput([&](char16_t* output, std::int32_t capacity, UFieldPosition* field, std::int32_t* error) {
        return doubleValue(handle, -1234.5, output, capacity, field, error);
    }, u"-1,234.50", 1, 6);
    CheckOutput([&](char16_t* output, std::int32_t capacity, UFieldPosition* field, std::int32_t* error) {
        return decimal(handle, "123456789012345678901234567890.123456789012345", -1, output, capacity, field, error);
    }, u"123,456,789,012,345,678,901,234,567,890.12", 0, 39);
    std::array<char16_t, 32> output;
    output.fill(0xa55a);
    status = 0;
    Require(symbol(handle, 0, output.data() + 1, 16, &status) == 1 && status == 0 &&
        output[0] == 0xa55a && output[1] == u'.' && output[2] == 0 && output[3] == 0xa55a,
        "actual number symbol NID returns locale decimal separator with guards");
    output.fill(0xa55a);
    status = 0;
    Require(text(handle, 2, output.data() + 1, 16, &status) == 1 && status == 0 &&
        output[0] == 0xa55a && output[1] == u'-' && output[2] == 0 && output[3] == 0xa55a,
        "actual number text NID returns the negative prefix");
    status = 0;
    auto* copied = clone(handle, &status);
    Require(copied && copied != handle && status <= 0, "actual clone NID owns a distinct formatter");
    setAttribute(copied, 8, 3);
    status = 0;
    setText(copied, 0, u"clone:", 6, &status);
    Require(status == 0, "actual text setter NID changes clone prefix");
    output.fill(0xa55a);
    status = 0;
    Require(text(copied, 0, output.data() + 1, 16, &status) == 6 && status == 0 &&
        std::u16string_view(output.data() + 1, 6) == u"clone:" && output[0] == 0xa55a && output[7] == 0 && output[8] == 0xa55a,
        "actual getter NID observes clone text state with guarded termination");
    CheckOutput([&](char16_t* buffer, std::int32_t capacity, UFieldPosition* field, std::int32_t* error) {
        return doubleValue(handle, 12.5, buffer, capacity, field, error);
    }, u"12.50", 0, 2);
    close(handle);
    CheckOutput([&](char16_t* buffer, std::int32_t capacity, UFieldPosition* field, std::int32_t* error) {
        return doubleValue(copied, 12.5, buffer, capacity, field, error);
    }, u"clone:12.500", 6, 8);
    close(copied);
    close(nullptr);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0, "unload actual canonical number-format artifact");
#else
    Require(dlclose(module) == 0, "unload actual canonical number-format artifact");
#endif
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "canonical patched ICU path argument");
    try { Check(argv[1]); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "classic number NID fixture failure: %s\n", error.what());
        return 1;
    }
}
