#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JXR file stream check failed at line %u\n", location.line());
    std::abort();
}

constexpr const char* Path = "guest_jxr_file_stream.bin";
constexpr std::array<std::uint8_t, 10> Data {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

Jxr::Stream Open(const char* mode) {
    Jxr::Stream stream {};
    auto* file = fopen_nid_postfix(Path, mode);
    Require(file != nullptr);
    stream.state[0] = reinterpret_cast<std::uintptr_t>(file);
    return stream;
}

FileStream* File(const Jxr::Stream& stream) { return reinterpret_cast<FileStream*>(stream.state[0]); }

void Close(Jxr::Stream& stream) { Require(fclose_nid_postfix(File(stream)) == 0); }

}

int main() {
    {
        auto stream = Open("w+b");
        Require(WriteWS_File_nid_postfix(&stream, Data.data(), Data.size()) == 0);
        Require(WriteWS_File_nid_postfix(&stream, nullptr, 0) == 0);
        Require(ftell_nid_postfix(File(stream)) == 10);
        Close(stream);
        stream = Open("rb");
        Require(WriteWS_File_nid_postfix(&stream, Data.data(), 1) == -102);
        Require(WriteWS_File_nid_postfix(&stream, Data.data(), 0) == 0);
        Close(stream);
    }
    {
        auto stream = Open("rb");
        std::array<std::uint8_t, 10> read {};
        Require(ReadWS_File_nid_postfix(&stream, read.data(), 4) == 0 && std::memcmp(read.data(), Data.data(), 4) == 0);
        Require(ReadWS_File_nid_postfix(&stream, read.data(), 0) == -102);
        Require(ReadWS_File_nid_postfix(&stream, read.data(), 7) == -102);
        Close(stream);
        stream = Open("rb");
        Require(ReadWS_File_nid_postfix(&stream, read.data(), 10) == 0 && read == Data);
        Close(stream);
    }
    {
        auto stream = Open("rb");
        std::uint8_t byte = 0;
        Require(SetPosWS_File_nid_postfix(&stream, 6) == 0 && ftell_nid_postfix(File(stream)) == 6);
        Require(ReadWS_File_nid_postfix(&stream, &byte, 1) == 0 && byte == 7);
        Require(SetPosWS_File_nid_postfix(&stream, 0) == 0 && ReadWS_File_nid_postfix(&stream, &byte, 1) == 0 && byte == 1);
        Require(SetPosWS_File_nid_postfix(&stream, SIZE_MAX) == -102);
        Close(stream);
    }
    {
        auto stream = Open("rb");
        std::size_t position = 99;
        Require(GetPosWS_File_nid_postfix(&stream, &position) == 0 && position == 0);
        Require(SetPosWS_File_nid_postfix(&stream, 9) == 0 && GetPosWS_File_nid_postfix(&stream, &position) == 0 && position == 9);
        Close(stream);
    }
    {
        auto stream = Open("rb");
        std::array<std::uint8_t, 11> read {};
        Require(EOSWS_File_nid_postfix(&stream) == 0);
        Require(ReadWS_File_nid_postfix(&stream, read.data(), read.size()) == -102 && EOSWS_File_nid_postfix(&stream) != 0);
        Require(SetPosWS_File_nid_postfix(&stream, 0) == 0 && EOSWS_File_nid_postfix(&stream) == 0);
        Close(stream);
        Require(std::remove(Path) == 0);
    }
}
