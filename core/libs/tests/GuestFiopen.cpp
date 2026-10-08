#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" {
FileStream* APS5_VABI _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(const char*, int, int);
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
int APS5_VABI fclose_nid_postfix(FileStream*);
std::int64_t APS5_VABI ftello_nid_postfix(FileStream*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Fiopen check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int In = 0x01, Out = 0x02, Ate = 0x04, App = 0x08, Trunc = 0x10, Nocreate = 0x20, Noreplace = 0x40,
    Binary = 0x80;

static FileStream* Open(const char* name, int mode) {
    return _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(name, mode, 0x1b6);
}

static std::string Contents(const char* name) {
    auto* stream = fopen_nid_postfix(name, "rb");
    Require(stream != nullptr);
    std::string text;
    for (int c; (c = std::fgetc(stream->GetHandle())) != EOF;) text += static_cast<char>(c);
    fclose_nid_postfix(stream);
    return text;
}

static void Write(FileStream* stream, const char* text) {
    Require(stream != nullptr);
    Require(std::fputs(text, stream->GetHandle()) >= 0);
    fclose_nid_postfix(stream);
}

static bool Exists(const char* name) {
    auto* stream = fopen_nid_postfix(name, "r");
    if (stream) fclose_nid_postfix(stream);
    return stream != nullptr;
}

int main() {
    const std::filesystem::path dir = "guest_fiopen_dir";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directory(dir);
    const char* file = "guest_fiopen_dir/a";
    const char* missing = "guest_fiopen_dir/missing";

    for (int mode : {0, Ate, Trunc, Binary, Nocreate, Noreplace, In | Trunc, In | Trunc | Binary, Out | App | Trunc,
            In | Out | App | Trunc, Trunc | Binary, App | Trunc}) {
        Require(Open(missing, mode) == nullptr);
        Require(!Exists(missing));
    }

    Write(Open(file, Out), "abc");
    Require(Contents(file) == "abc");

    auto* stream = Open(file, In);
    Require(stream != nullptr && std::fgetc(stream->GetHandle()) == 'a');
    fclose_nid_postfix(stream);
    stream = Open(file, In);
    Require(stream != nullptr);
    std::fputc('x', stream->GetHandle());
    fclose_nid_postfix(stream);
    Require(Contents(file) == "abc");

    Write(Open(file, App), "d");
    Require(Contents(file) == "abcd");
    Write(Open(file, Out | App | Binary), "e");
    Require(Contents(file) == "abcde");

    stream = Open(file, In | Ate);
    Require(stream != nullptr && ftello_nid_postfix(stream) == 5);
    fclose_nid_postfix(stream);
    stream = Open(file, In | Binary);
    Require(stream != nullptr && ftello_nid_postfix(stream) == 0);
    fclose_nid_postfix(stream);

    Require(Open(file, Out | Noreplace) == nullptr);
    Require(Open(file, In | Out | Binary | Noreplace) == nullptr);
    Require(Open(file, App | Noreplace) == nullptr);
    Require(Contents(file) == "abcde");
    stream = Open(file, In | Noreplace);
    Require(stream != nullptr);
    fclose_nid_postfix(stream);

    Require(Open(missing, Out | Nocreate) == nullptr);
    Require(!Exists(missing));
    Write(Open(file, Out | Nocreate), "X");
    Require(Contents(file) == "Xbcde");
    stream = Open(file, Out | Nocreate | Binary | Ate);
    Require(stream != nullptr && ftello_nid_postfix(stream) == 5);
    Write(stream, "f");
    Require(Contents(file) == "Xbcdef");

    stream = Open(file, In | Out);
    Require(stream != nullptr && std::fgetc(stream->GetHandle()) == 'X');
    fclose_nid_postfix(stream);
    Require(Contents(file) == "Xbcdef");

    Write(Open(file, In | Out | App), "g");
    Require(Contents(file) == "Xbcdefg");

    Write(Open(file, In | Out | Trunc | Binary), "h");
    Require(Contents(file) == "h");
    Write(Open(file, Out | Trunc), "ij");
    Require(Contents(file) == "ij");
    Write(Open(file, Out | Binary), "k");
    Require(Contents(file) == "k");

    Write(Open(missing, Out | Noreplace), "new");
    Require(Contents(missing) == "new");
    Write(Open(missing, Out | Trunc | Nocreate), "w+");
    Require(Contents(missing) == "w+");
    std::filesystem::remove(dir / "missing");
    Write(Open(missing, Out | Trunc | Nocreate), "created");
    Require(Contents(missing) == "created");

    stream = Open(file, In | 0x100 | 0x8000);
    Require(stream != nullptr && std::fgetc(stream->GetHandle()) == 'k');
    fclose_nid_postfix(stream);

    std::filesystem::remove_all(dir);
    return 0;
}
