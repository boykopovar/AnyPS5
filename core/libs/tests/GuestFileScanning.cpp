#include "SceTypes.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <stdexcept>

extern "C" int APS5_VABI fscanf_nid_postfix(FileStream*, const char*, ...);
extern "C" int APS5_VABI vfscanf_nid_postfix(FileStream*, const char*, VaList*);
#ifdef _WIN32
extern "C" int* APS5_VABI __error_nid_postfix();
#endif

static void Require(bool condition) { if (!condition) std::abort(); }

static std::FILE* Input(const char* text) {
    auto* file = std::tmpfile();
    Require(file != nullptr);
    Require(std::fwrite(text, 1, std::strlen(text), file) == std::strlen(text));
    std::rewind(file);
    return file;
}

static int APS5_VABI ScanBridge(std::FILE* file, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::ScanStreamWindows(file, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI ScanExportVa(FileStream* file, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = vfscanf_nid_postfix(file, format, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
    return result;
}

int main() {
    {
        auto* file = Input("  -42 0xff 3.25 Doom\nTAIL");
        int number = 0, hex = 0, consumed = -1;
        double floating = 0;
        char word[16]{};
        Require(ScanBridge(file, "%d %i %lf %15s%n", &number, &hex, &floating, word, &consumed) == 4);
        Require(number == -42 && hex == 255 && floating == 3.25 && std::strcmp(word, "Doom") == 0);
        Require(consumed == 20 && std::fgetc(file) == '\n');
        std::fclose(file);
    }
    {
        auto* file = Input("abc,tail next");
        char bracket[8]{};
        Require(ScanBridge(file, "%3[a-z],%*4s", bracket) == 1);
        Require(std::strcmp(bracket, "abc") == 0 && std::fgetc(file) == ' ');
        std::fclose(file);
    }
    {
        auto* file = Input("foo");
        int number = -1;
        Require(ScanBridge(file, "%d", &number) == 0);
        Require(std::fgetc(file) == 'f');
        std::fclose(file);
    }
    {
        auto* file = Input("");
        int number = -1;
        Require(ScanBridge(file, "%d", &number) == EOF);
        std::fclose(file);
    }
    {
        auto* file = Input("4294967297 7 8 9 10 11 12 13");
        std::int64_t large = 0;
        int value[7]{};
        Require(ScanBridge(file, "%ld %d %d %d %d %d %d %d", &large,
                           &value[0], &value[1], &value[2], &value[3], &value[4], &value[5], &value[6]) == 8);
        Require(large == 4294967297LL && value[6] == 13);
        std::fclose(file);
    }
    {
        auto* file = Input("too-long");
        struct Guarded { char before = 'B'; char data[4]{}; char after = 'A'; } guarded;
        bool rejected = false;
        try { ScanBridge(file, "%s", guarded.data); }
        catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected && guarded.before == 'B' && guarded.after == 'A' && guarded.data[0] == 0);
        rejected = false;
        try { ScanBridge(file, "%[a-z]", guarded.data); }
        catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected && std::fgetc(file) == 't');
        std::fclose(file);
    }
    {
        auto* native = Input("23 Z");
        FileStream file(native);
        int number = -1, consumed = -1;
        char letter = 0;
        Require(fscanf_nid_postfix(&file, "%d %c%n", &number, &letter, &consumed) == 2);
        Require(number == 23 && letter == 'Z' && consumed == 4);
        Require(ScanExportVa(&file, "%d", &number) == EOF);
        Require((file.GuestState().flags & 0x20) != 0);
        std::fclose(native);
    }
    {
        auto* native = Input("70");
        FileStream file(native);
        int number = 0;
        Require(ScanExportVa(&file, "%d", &number) == 1 && number == 70);
        std::fclose(native);
    }
#ifdef _WIN32
    {
        auto* native = Input("too-long");
        FileStream file(native);
        char word[4]{};
        *__error_nid_postfix() = 0;
        Require(fscanf_nid_postfix(&file, "%s", word) == -1);
        Require(*__error_nid_postfix() == 22 && word[0] == 0);
        std::fclose(native);
    }
#endif
}
