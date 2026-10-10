#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

extern "C" {
FileStream* APS5_VABI _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(const char*, int, int);
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
int APS5_VABI fclose_nid_postfix(FileStream*);
std::int64_t APS5_VABI ftello_nid_postfix(FileStream*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int In = 0x01, Out = 0x02, Ate = 0x04, App = 0x08, Trunc = 0x10, Nocreate = 0x20, Noreplace = 0x40,
    Binary = 0x80;

class DirectoryFixture {
public:
    DirectoryFixture()
        : name("guest_fiopen_dir-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          directory(std::filesystem::current_path() / name),
          file(name + "/a"),
          missing(name + "/missing") {
        Require(std::filesystem::create_directory(directory), "create the test directory");
    }

    ~DirectoryFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    DirectoryFixture(const DirectoryFixture&) = delete;
    DirectoryFixture& operator=(const DirectoryFixture&) = delete;

    void Seed(const std::string& contents) const {
        std::ofstream(directory / "a", std::ios::binary) << contents;
    }

    const std::string name;
    const std::filesystem::path directory;
    const std::string file;
    const std::string missing;
};

class OpenStream {
public:
    explicit OpenStream(FileStream* opened) : stream(opened) {}

    ~OpenStream() {
        if (stream == nullptr) return;
        try {
            fclose_nid_postfix(stream);
        } catch (...) {
        }
    }

    OpenStream(const OpenStream&) = delete;
    OpenStream& operator=(const OpenStream&) = delete;

    bool IsOpen() const {
        return stream != nullptr;
    }

    std::FILE* Handle() const {
        Require(stream != nullptr, "stream is open");
        return stream->GetHandle();
    }

    FileStream* Stream() const {
        Require(stream != nullptr, "stream is open");
        return stream;
    }

private:
    FileStream* stream;
};

FileStream* Open(const std::string& name, int mode) {
    return _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(name.c_str(), mode, 0x1b6);
}

std::string Contents(const std::string& name) {
    const OpenStream stream(fopen_nid_postfix(name.c_str(), "rb"));
    Require(stream.IsOpen(), "open " + name + " for reading");
    std::string text;
    for (int c; (c = std::fgetc(stream.Handle())) != EOF;) text += static_cast<char>(c);
    return text;
}

void Write(FileStream* opened, const char* text) {
    const OpenStream stream(opened);
    Require(stream.IsOpen(), "stream is open for writing");
    Require(std::fputs(text, stream.Handle()) >= 0, std::string("write ") + text);
}

bool Exists(const std::string& name) {
    const OpenStream stream(fopen_nid_postfix(name.c_str(), "r"));
    return stream.IsOpen();
}

const Case invalidModes{"Fiopen_InvalidModesOnMissingFile_ReturnNullWithoutCreating", [] {
    const DirectoryFixture fixture;
    for (const int mode : {0, Ate, Trunc, Binary, Nocreate, Noreplace, In | Trunc, In | Trunc | Binary, Out | App | Trunc,
            In | Out | App | Trunc, Trunc | Binary, App | Trunc}) {
        const std::string label = "mode " + std::to_string(mode);
        const OpenStream stream(Open(fixture.missing, mode));
        Require(!stream.IsOpen(), label + " is rejected");
        Require(!Exists(fixture.missing), label + " does not create the file");
    }
}};

const Case outCreates{"Fiopen_OutOnMissingFile_CreatesAndWrites", [] {
    const DirectoryFixture fixture;
    Write(Open(fixture.file, Out), "abc");
    RequireEqual(Contents(fixture.file), std::string("abc"), "contents");
}};

const Case inReads{"Fiopen_In_ReadsFromStart", [] {
    const DirectoryFixture fixture;
    fixture.Seed("abc");
    const OpenStream stream(Open(fixture.file, In));
    Require(stream.IsOpen(), "open");
    RequireEqual(std::fgetc(stream.Handle()), static_cast<int>('a'), "first byte");
}};

const Case inRejectsWrites{"Fiopen_In_DoesNotModifyFileOnWrite", [] {
    const DirectoryFixture fixture;
    fixture.Seed("abc");
    {
        const OpenStream stream(Open(fixture.file, In));
        Require(stream.IsOpen(), "open");
        std::fputc('x', stream.Handle());
    }
    RequireEqual(Contents(fixture.file), std::string("abc"), "contents");
}};

const Case appendModes{"Fiopen_AppendModes_AppendToExistingFile", [] {
    const DirectoryFixture fixture;
    fixture.Seed("abc");
    Write(Open(fixture.file, App), "d");
    RequireEqual(Contents(fixture.file), std::string("abcd"), "contents after App");
    Write(Open(fixture.file, Out | App | Binary), "e");
    RequireEqual(Contents(fixture.file), std::string("abcde"), "contents after Out|App|Binary");
}};

const Case inAte{"Fiopen_InAte_StartsAtEnd", [] {
    const DirectoryFixture fixture;
    fixture.Seed("abcde");
    const OpenStream stream(Open(fixture.file, In | Ate));
    Require(stream.IsOpen(), "open");
    RequireEqual(ftello_nid_postfix(stream.Stream()), std::int64_t{5}, "position");
}};

const Case inBinary{"Fiopen_InBinary_StartsAtBeginning", [] {
    const DirectoryFixture fixture;
    fixture.Seed("abcde");
    const OpenStream stream(Open(fixture.file, In | Binary));
    Require(stream.IsOpen(), "open");
    RequireEqual(ftello_nid_postfix(stream.Stream()), std::int64_t{0}, "position");
}};

const Case noreplaceWrite{"Fiopen_NoreplaceWriteModesOnExistingFile_FailWithoutModifying", [] {
    const DirectoryFixture fixture;
    fixture.Seed("abcde");
    for (const int mode : {Out | Noreplace, In | Out | Binary | Noreplace, App | Noreplace}) {
        const OpenStream stream(Open(fixture.file, mode));
        Require(!stream.IsOpen(), "mode " + std::to_string(mode) + " is rejected");
    }
    RequireEqual(Contents(fixture.file), std::string("abcde"), "contents");
}};

const Case noreplaceRead{"Fiopen_InNoreplaceOnExistingFile_Opens", [] {
    const DirectoryFixture fixture;
    fixture.Seed("abcde");
    const OpenStream stream(Open(fixture.file, In | Noreplace));
    Require(stream.IsOpen(), "open");
}};

const Case nocreateMissing{"Fiopen_OutNocreateOnMissingFile_FailsWithoutCreating", [] {
    const DirectoryFixture fixture;
    const OpenStream stream(Open(fixture.missing, Out | Nocreate));
    Require(!stream.IsOpen(), "rejected");
    Require(!Exists(fixture.missing), "file not created");
}};

const Case nocreateOverwrite{"Fiopen_OutNocreateOnExistingFile_OverwritesWithoutTruncating", [] {
    const DirectoryFixture fixture;
    fixture.Seed("abcde");
    Write(Open(fixture.file, Out | Nocreate), "X");
    RequireEqual(Contents(fixture.file), std::string("Xbcde"), "contents");
}};

const Case nocreateAte{"Fiopen_OutNocreateBinaryAte_StartsAtEndAndAppendsWrite", [] {
    const DirectoryFixture fixture;
    fixture.Seed("Xbcde");
    FileStream* stream = Open(fixture.file, Out | Nocreate | Binary | Ate);
    {
        const OpenStream guard(stream);
        Require(guard.IsOpen(), "open");
        RequireEqual(ftello_nid_postfix(stream), std::int64_t{5}, "position");
        Require(std::fputs("f", guard.Handle()) >= 0, "write f");
    }
    RequireEqual(Contents(fixture.file), std::string("Xbcdef"), "contents");
}};

const Case inOut{"Fiopen_InOut_ReadsWithoutModifying", [] {
    const DirectoryFixture fixture;
    fixture.Seed("Xbcdef");
    {
        const OpenStream stream(Open(fixture.file, In | Out));
        Require(stream.IsOpen(), "open");
        RequireEqual(std::fgetc(stream.Handle()), static_cast<int>('X'), "first byte");
    }
    RequireEqual(Contents(fixture.file), std::string("Xbcdef"), "contents");
}};

const Case inOutApp{"Fiopen_InOutApp_AppendsToExistingFile", [] {
    const DirectoryFixture fixture;
    fixture.Seed("Xbcdef");
    Write(Open(fixture.file, In | Out | App), "g");
    RequireEqual(Contents(fixture.file), std::string("Xbcdefg"), "contents");
}};

const Case truncating{"Fiopen_TruncatingWriteModes_ReplaceContents", [] {
    const DirectoryFixture fixture;
    fixture.Seed("Xbcdefg");
    Write(Open(fixture.file, In | Out | Trunc | Binary), "h");
    RequireEqual(Contents(fixture.file), std::string("h"), "contents after In|Out|Trunc|Binary");
    Write(Open(fixture.file, Out | Trunc), "ij");
    RequireEqual(Contents(fixture.file), std::string("ij"), "contents after Out|Trunc");
    Write(Open(fixture.file, Out | Binary), "k");
    RequireEqual(Contents(fixture.file), std::string("k"), "contents after Out|Binary");
}};

const Case noreplaceCreates{"Fiopen_OutNoreplaceOnMissingFile_CreatesFile", [] {
    const DirectoryFixture fixture;
    Write(Open(fixture.missing, Out | Noreplace), "new");
    RequireEqual(Contents(fixture.missing), std::string("new"), "contents");
}};

const Case truncNocreateExisting{"Fiopen_OutTruncNocreateOnExistingFile_Truncates", [] {
    const DirectoryFixture fixture;
    Write(Open(fixture.missing, Out | Noreplace), "new");
    Write(Open(fixture.missing, Out | Trunc | Nocreate), "w+");
    RequireEqual(Contents(fixture.missing), std::string("w+"), "contents");
}};

const Case truncNocreateMissing{"Fiopen_OutTruncNocreateOnMissingFile_CreatesFile", [] {
    const DirectoryFixture fixture;
    Write(Open(fixture.missing, Out | Trunc | Nocreate), "created");
    RequireEqual(Contents(fixture.missing), std::string("created"), "contents");
}};

const Case unknownBits{"Fiopen_UnknownModeBits_AreIgnored", [] {
    const DirectoryFixture fixture;
    fixture.Seed("k");
    const OpenStream stream(Open(fixture.file, In | 0x100 | 0x8000));
    Require(stream.IsOpen(), "open");
    RequireEqual(std::fgetc(stream.Handle()), static_cast<int>('k'), "first byte");
}};

} // namespace
