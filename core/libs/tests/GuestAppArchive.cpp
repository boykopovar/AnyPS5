#include "AppArchiveFixture.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/exceptions/Unwind.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool Lookup(const char* guestPath, AppArchiveNode& node) {
    return AppArchiveLookup_nid_no_patch(ResolveArchivedPath_nid_no_patch(guestPath), &node);
}

bool GuardRefuses(const char* guestPath) {
    try {
        (void)ResolvePath_nid_no_patch(guestPath);
    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), "not implemented") != nullptr && std::strstr(error.what(), guestPath) != nullptr;
    }
    return false;
}

constexpr std::size_t BigSize = 200 * 1024 + 123;

std::map<std::string, std::vector<char>> Contents() {
    return {
        {"Data/Big.bin", AppArchiveFixture::Pattern(BigSize)},
        {"Data/Empty.txt", {}},
        {"Data/Sub/Deep.txt", {'d', 'e', 'e', 'p'}},
        {"Readme.TXT", {'r', 'e', 'a', 'd'}},
    };
}

void Present(const std::filesystem::path& root) {
    AppArchiveFixture::WriteArchive(root / "app0.zar", Contents(), {"Data/Unused"});
    { std::ofstream eboot(root / "app0" / "eboot.bin"); eboot << "elf"; }
    Require(AppArchivePath_nid_no_patch() == root / "app0.zar", "the archive beside app0 was not opened");

    AppArchiveNode node{};
    Require(Lookup("/app0/data/BIG.BIN", node) && !node.Directory && node.Size == BigSize, "a file was not found case-insensitively with its size");
    const auto big = node.Handle;
    Require(Lookup("/APP0/Data/Empty.txt", node) && !node.Directory && node.Size == 0, "an empty file was not found");
    Require(Lookup("/app0/Data", node) && node.Directory, "a directory was not found");
    const auto data = node.Handle;
    Require(Lookup("/app0/", node) && node.Directory, "the archive root was not app0");
    Require(Lookup("/app0/Data/Sub/../Sub/deep.txt", node) && node.Size == 4, "a normalised path was not found");
    Require(!Lookup("/app0/Data/Missing.bin", node), "a missing file was found");
    Require(!Lookup("/app0/Readme.TXT/x", node), "a path through a file was found");
    Require(!Lookup("/app0x/Readme.TXT", node), "a sibling of app0 was looked up in the archive");
    Require(!Lookup("/Readme.TXT", node), "a path outside app0 was looked up in the archive");

    Require(GuardRefuses("/app0/Data/Big.bin"), "an archive-only file resolved through an unaware call");
    Require(GuardRefuses("/app0/Data"), "an archive-only directory resolved through an unaware call");
    Require(ResolvePath_nid_no_patch("/app0/eboot.bin") == root / "app0" / "eboot.bin", "a file on disk was refused");
    Require(ResolvePath_nid_no_patch("/app0/Data/Missing.bin") == root / "app0" / "Data" / "Missing.bin", "a path missing everywhere was refused");
    Require(Lookup("/app0/sce_sys", node) == false && ResolvePath_nid_no_patch("/app0/sce_sys") == root / "app0" / "sce_sys", "a directory only on disk was not left to the disk");

    const auto expected = AppArchiveFixture::Pattern(BigSize);
    FileStream* stream = fopen_nid_postfix("/app0/data/big.bin", "rb");
    Require(stream != nullptr, "fopen did not open an archive-only file");
    std::vector<char> streamed(BigSize + 16, 0);
    Require(fread_nid_postfix(streamed.data(), 1, streamed.size(), stream) == BigSize && std::equal(expected.begin(), expected.end(), streamed.begin()), "fopen did not read the archived bytes");
    Require(fseek_nid_postfix(stream, 65536, 0) == 0 && ftell_nid_postfix(stream) == 65536, "fopen of an archived file did not seek");
    Require(fread_nid_postfix(streamed.data(), 1, 4, stream) == 4 && std::equal(streamed.begin(), streamed.begin() + 4, expected.begin() + 65536), "fopen read wrong bytes after a seek");
    const int streamed_descriptor = fileno_nid_postfix(stream);
    AppArchiveNode materialized{};
    Require(AppArchiveMaterializedDescriptor_nid_no_patch(streamed_descriptor, &materialized) && materialized.Size == BigSize && !materialized.Directory, "fopen did not record its descriptor as an archived file");
    Require(!AppArchiveDescriptor_nid_no_patch(streamed_descriptor, nullptr, nullptr), "fopen's copy was attached as an archive placeholder");
    constexpr int Duplicate = 1000005;
    AppArchiveShare_nid_no_patch(streamed_descriptor, Duplicate);
    Require(AppArchiveMaterializedDescriptor_nid_no_patch(Duplicate, nullptr), "a duplicate of fopen's descriptor lost the archived file");
    AppArchiveDetach_nid_no_patch(Duplicate);
    Require(!AppArchiveMaterializedDescriptor_nid_no_patch(Duplicate, nullptr) && AppArchiveMaterializedDescriptor_nid_no_patch(streamed_descriptor, nullptr), "detaching a duplicate detached fopen's descriptor");
    Require(fclose_nid_postfix(stream) == 0, "fclose of an archived file failed");
    Require(!AppArchiveMaterializedDescriptor_nid_no_patch(streamed_descriptor, nullptr), "fclose left the descriptor recorded as an archived file");
    stream = fopen_nid_postfix("/app0/Data/Empty.txt", "r");
    Require(stream != nullptr && fread_nid_postfix(streamed.data(), 1, 1, stream) == 0 && fclose_nid_postfix(stream) == 0, "fopen of an empty archived file failed");
    errno = 0;
    Require(fopen_nid_postfix("/app0/Readme.TXT", "r+") == nullptr && errno == 30, "fopen for update of an archived file did not report EROFS");
    errno = 0;
    Require(fopen_nid_postfix("/app0/Readme.TXT", "w") == nullptr && errno == 30, "fopen for writing an archived file did not report EROFS");
    errno = 0;
    Require(fopen_nid_postfix("/app0/Readme.TXT", "wx") == nullptr && errno == 17, "exclusive fopen of an archived file did not report EEXIST");
    bool refused = false;
    try { (void)fopen_nid_postfix("/app0/Data", "r"); } catch (const std::runtime_error& error) { refused = std::strstr(error.what(), "not implemented") != nullptr; }
    Require(refused, "fopen of an archived directory did not throw");
    Require(access_nid_postfix("/app0/Data/Big.bin", 0) == 0 && access_nid_postfix("/app0/Data/Big.bin", 4) == 0, "access did not find an archived file");
    Require(access_nid_postfix("/app0/Data", 5) == 0, "access did not search an archived directory");
    errno = 0;
    Require(access_nid_postfix("/app0/Data/Big.bin", 1) == -1 && errno == 13, "access granted execution of an archived file");
    refused = false;
    try { (void)access_nid_postfix("/app0/Data/Big.bin", 2); } catch (const std::runtime_error& error) { refused = std::strstr(error.what(), "not implemented") != nullptr; }
    Require(refused, "access for writing to an archived file did not throw");
    errno = 0;
    Require(access_nid_postfix("/app0/Data/Missing.bin", 0) == -1 && errno == 2, "access found a path missing everywhere");

    bool rejected = false;
    std::vector<char> buffer(BigSize + 64, 0);
    Require(AppArchiveRead_nid_no_patch(big, 0, buffer.data(), BigSize) == BigSize && std::equal(expected.begin(), expected.end(), buffer.begin()), "the whole file did not read back");
    for (const std::uint64_t offset : {std::uint64_t{1}, std::uint64_t{65535}, std::uint64_t{65536}, std::uint64_t{131071}, std::uint64_t{BigSize - 5}}) {
        std::fill(buffer.begin(), buffer.end(), 0);
        const auto read = AppArchiveRead_nid_no_patch(big, offset, buffer.data(), 70000);
        const auto wanted = std::min<std::uint64_t>(70000, BigSize - offset);
        Require(read == wanted && std::equal(expected.begin() + static_cast<std::ptrdiff_t>(offset), expected.begin() + static_cast<std::ptrdiff_t>(offset + wanted), buffer.begin()), "a read across block boundaries returned wrong bytes");
    }
    Require(AppArchiveRead_nid_no_patch(big, BigSize, buffer.data(), 10) == 0, "a read at the end returned bytes");
    Require(AppArchiveRead_nid_no_patch(big, 10, buffer.data(), 0) == 0, "a zero-length read returned bytes");

    AppArchiveNode bigNode{};
    Require(Lookup("/app0/Data/Big.bin", bigNode), "the big file vanished");
    constexpr int Descriptor = 1000003;
    constexpr int Shared = 1000004;
    Require(!AppArchiveDescriptor_nid_no_patch(Descriptor, nullptr, nullptr), "a descriptor was attached before attaching");
    AppArchiveAttach_nid_no_patch(Descriptor, bigNode);
    AppArchiveShare_nid_no_patch(Descriptor, Shared);
    std::uint64_t done = 0;
    std::vector<char> first(65530), second(20);
    const AppArchiveBuffer split[] = {{first.data(), first.size()}, {second.data(), second.size()}};
    Require(AppArchiveDescriptorRead_nid_no_patch(Descriptor, split, 2, nullptr, &done) && done == first.size() + second.size(), "a vector read returned the wrong length");
    Require(std::equal(first.begin(), first.end(), expected.begin()) && std::equal(second.begin(), second.end(), expected.begin() + 65530), "a vector read returned wrong bytes");
    std::uint64_t position = 0;
    Require(AppArchiveDescriptor_nid_no_patch(Shared, nullptr, &position) && position == 65550, "a shared descriptor did not share the position");
    const std::int64_t at = 131000;
    const AppArchiveBuffer single[] = {{second.data(), second.size()}};
    Require(AppArchiveDescriptorRead_nid_no_patch(Shared, single, 1, &at, &done) && done == 20 && std::equal(second.begin(), second.end(), expected.begin() + at), "a positioned read returned wrong bytes");
    Require(AppArchiveDescriptor_nid_no_patch(Descriptor, nullptr, &position) && position == 65550, "a positioned read moved the position");
    std::int64_t sought = 0;
    Require(AppArchiveDescriptorSeek_nid_no_patch(Shared, -3, 2, &sought) && sought == static_cast<std::int64_t>(BigSize - 3), "seeking from the end failed");
    Require(AppArchiveDescriptorRead_nid_no_patch(Descriptor, single, 1, nullptr, &done) && done == 3, "a read at the end was not short");
    Require(AppArchiveDescriptorRead_nid_no_patch(Descriptor, single, 1, nullptr, &done) && done == 0, "a read past the end returned bytes");
    Require(AppArchiveDescriptorSeek_nid_no_patch(Descriptor, -1, 0, &sought) && sought == -1, "a negative seek was accepted");
    Require(AppArchiveDescriptorSeek_nid_no_patch(Descriptor, 10, 1, &sought) && sought == static_cast<std::int64_t>(BigSize + 10), "seeking past the end failed");
    Require(!AppArchiveDescriptorRead_nid_no_patch(99999, single, 1, nullptr, &done), "unattached descriptor read succeeded");
    Require(!AppArchiveDescriptorSeek_nid_no_patch(99999, 0, 0, &sought), "unattached descriptor seek succeeded");
    rejected = false;
    try { (void)AppArchiveDescriptorRead_nid_no_patch(Descriptor, single, -1, nullptr, &done); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "negative buffer count was accepted");
    rejected = false;
    try { (void)AppArchiveDescriptorRead_nid_no_patch(Descriptor, single, 1, nullptr, nullptr); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "null read pointer was accepted");
    rejected = false;
    const std::int64_t negativeOffset = -10;
    try { (void)AppArchiveDescriptorRead_nid_no_patch(Descriptor, single, 1, &negativeOffset, &done); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "negative read offset was accepted");
    rejected = false;
    try { (void)AppArchiveDescriptorSeek_nid_no_patch(Descriptor, 0, 0, nullptr); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "null seek position was accepted");
    rejected = false;
    try { (void)AppArchiveDescriptorSeek_nid_no_patch(Descriptor, 0, 9, &sought); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "invalid whence was accepted");
    AppArchiveDetach_nid_no_patch(Descriptor);
    Require(!AppArchiveDescriptor_nid_no_patch(Descriptor, nullptr, nullptr) && AppArchiveDescriptor_nid_no_patch(Shared, nullptr, nullptr), "detaching one descriptor detached its duplicate");
    AppArchiveShare_nid_no_patch(Descriptor, Shared);
    Require(!AppArchiveDescriptor_nid_no_patch(Shared, nullptr, nullptr), "duplicating a plain descriptor over an archived one kept the archive");
    rejected = false;
    try { AppArchiveAttach_nid_no_patch(Descriptor, AppArchiveNode{0, true, 0}); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "a directory was attached to a descriptor");

    std::set<std::pair<std::string, bool>> names;
    for (const auto& entry : AppArchiveList_nid_no_patch(data)) names.insert({entry.Name, entry.Directory});
    Require(names == std::set<std::pair<std::string, bool>>{{"Big.bin", false}, {"Empty.txt", false}, {"Sub", true}, {"Unused", true}}, "the directory listing is wrong");

    rejected = false;
    try { (void)AppArchiveList_nid_no_patch(big); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "a file was listed as a directory");
    rejected = false;
    try { (void)AppArchiveRead_nid_no_patch(data, 0, buffer.data(), 1); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "a directory was read as a file");
}

void Absent() {
    AppArchiveNode node{};
    Require(AppArchivePath_nid_no_patch().empty(), "an archive was reported without app0.zar");
    Require(!Lookup("/app0/Readme.TXT", node), "a path was found without an archive");
    Require(ResolvePath_nid_no_patch("/app0/Readme.TXT").filename() == "Readme.TXT", "resolution changed without an archive");
}

void Damaged(const std::filesystem::path& root) {
    AppArchiveFixture::WriteArchive(root / "app0.zar", Contents());
    {
        std::fstream archive(root / "app0.zar", std::ios::binary | std::ios::in | std::ios::out);
        const char garbage[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x00};
        archive.seekp(0);
        archive.write(garbage, sizeof(garbage));
    }
    AppArchiveNode node{};
    Require(Lookup("/app0/Data/Big.bin", node), "the damaged archive's index was not read");
    std::vector<char> buffer(16);
    bool thrown = false;
    try { (void)AppArchiveRead_nid_no_patch(node.Handle, 0, buffer.data(), buffer.size()); } catch (const std::runtime_error& error) { thrown = std::strstr(error.what(), "damaged") != nullptr; }
    Require(thrown, "a damaged block did not fail the read");
}

void Invalid(const std::filesystem::path& root) {
    { std::ofstream archive(root / "app0.zar", std::ios::binary); archive << "not an archive"; }
    AppArchiveNode node{};
    bool thrown = false;
    try { (void)Lookup("/app0/Readme.TXT", node); } catch (const std::runtime_error& error) { thrown = std::strstr(error.what(), "app0.zar") != nullptr; }
    Require(thrown, "an invalid app0.zar was not reported");
}

void UnwindBounds() {
    const LibcUnwind::Byte cieAsFde[36] = {
        0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x78,
        0x10
    };
    {
        LibcUnwind::Frame frame{};
        LibcUnwind::Lookup query{0x1000, cieAsFde};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(!LibcUnwind::DecodeRange(frame, query, p, augmentation, lsdaEncoding), "DecodeRange accepted a CIE record with cieOffset == 0");
    }

    const LibcUnwind::Byte zeroCieLength[48] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x78,
        0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x00, 0x00
    };
    {
        LibcUnwind::Frame frame{};
        LibcUnwind::Lookup query{0x1000, &zeroCieLength[20]};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(!LibcUnwind::DecodeRange(frame, query, p, augmentation, lsdaEncoding), "DecodeRange accepted a CIE with cieLength == 0");
    }

    const LibcUnwind::Byte shortFdeLength[44] = {
        0x0c, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x78,
        0x10, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00
    };
    {
        LibcUnwind::Frame frame{};
        LibcUnwind::Lookup query{0x1000, &shortFdeLength[16]};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(!LibcUnwind::DecodeRange(frame, query, p, augmentation, lsdaEncoding), "DecodeRange accepted an FDE with length < 4");
    }

    const LibcUnwind::Byte badAugSize[40] = {
        0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 'z', 'R', 0x00,
        0x01, 0x78, 0x10, 0x0a,
        0x03, 0x00, 0x00, 0x00,
        0x0c, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00,
        0x10, 0x00, 0x00, 0x00
    };
    {
        LibcUnwind::Frame frame{};
        LibcUnwind::Lookup query{0x1000, &badAugSize[20]};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(!LibcUnwind::DecodeRange(frame, query, p, augmentation, lsdaEncoding), "DecodeRange accepted a 'z' augmentation size exceeding cieEnd");
    }

    const LibcUnwind::Byte fdePastSection[36] = {
        0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 'z', 'R', 0x00,
        0x01, 0x78, 0x10, 0x01,
        0x03, 0x0c, 0x07, 0x08,
        0x20, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00,
        0x10, 0x00, 0x00, 0x00
    };
    {
        LibcUnwind::Frame frame{};
        LibcUnwind::Lookup query{0x1000, &fdePastSection[20], 0, 0, 0, fdePastSection + sizeof(fdePastSection)};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(!LibcUnwind::DecodeRange(frame, query, p, augmentation, lsdaEncoding), "DecodeRange accepted an FDE exceeding fdeEnd");
    }
    {
        LibcUnwind::Frame frame{};
        LibcUnwind::Lookup shortQuery{0x1000, &fdePastSection[20], 0, 0, 0, &fdePastSection[22]};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(!LibcUnwind::DecodeRange(frame, shortQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted an FDE with fewer than 4 bytes before fdeEnd");
        LibcUnwind::Lookup emptyQuery{0x1000, &fdePastSection[20], 0, 0, 0, &fdePastSection[20]};
        Require(!LibcUnwind::DecodeRange(frame, emptyQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted an empty fdeEnd range");
    }

    {
        LibcUnwind::Frame frame{};
        frame.start = 0x1000;
        frame.codeAlign = 1;
        frame.dataAlign = -8;
        LibcUnwind::Rules initial{};

        const LibcUnwind::Byte truncDefCfaExpr[8] = {0x0f, 0x05};
        LibcUnwind::Rules s1 = initial;
        Require(!LibcUnwind::Instructions(truncDefCfaExpr, truncDefCfaExpr + 2, frame, 0x1000, s1, initial), "Instructions accepted truncated DW_CFA_def_cfa_expression");

        const LibcUnwind::Byte truncExpr[8] = {0x10, 0x03, 0x04};
        LibcUnwind::Rules s2 = initial;
        Require(!LibcUnwind::Instructions(truncExpr, truncExpr + 3, frame, 0x1000, s2, initial), "Instructions accepted truncated DW_CFA_expression");

        const LibcUnwind::Byte truncValExpr[8] = {0x16, 0x06, 0x04};
        LibcUnwind::Rules s3 = initial;
        Require(!LibcUnwind::Instructions(truncValExpr, truncValExpr + 3, frame, 0x1000, s3, initial), "Instructions accepted truncated DW_CFA_val_expression");

        const LibcUnwind::Byte truncAdvanceLoc4[8] = {0x04, 0x01};
        LibcUnwind::Rules s4 = initial;
        Require(!LibcUnwind::Instructions(truncAdvanceLoc4, truncAdvanceLoc4 + 2, frame, 0x1000, s4, initial), "Instructions accepted truncated DW_CFA_advance_loc4");
    }

    const LibcUnwind::Byte validEhFrame[44] = {
        0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 'z', 'R', 0x00,
        0x01, 0x78, 0x10, 0x01,
        0x03, 0x0c, 0x07, 0x08,
        0x14, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00,
        0x00, 0x44, 0x0e, 0x10,
        0x86, 0x02, 0x90, 0x01
    };
    {
        LibcUnwind::Frame frame{};
        LibcUnwind::Lookup query{0x1010, &validEhFrame[20], 0, 0, 0, validEhFrame + sizeof(validEhFrame)};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(LibcUnwind::DecodeRange(frame, query, p, augmentation, lsdaEncoding), "valid CIE/FDE failed DecodeRange");
        Require(frame.start == 0x1000 && frame.length == 0x40, "valid FDE decoded unexpected range");
        Require(frame.codeAlign == 1 && frame.dataAlign == -8 && frame.returnRegister == 16, "valid CIE decoded unexpected alignment or return register");
        Require(std::strcmp(augmentation, "zR") == 0 && lsdaEncoding == 255, "valid CIE decoded unexpected augmentation");
        Require(LibcUnwind::DecodeFdeAugmentation(frame, query, p, augmentation, lsdaEncoding), "valid FDE augmentation failed");
        LibcUnwind::Rules initial{};
        Require(LibcUnwind::Instructions(frame.cieBegin, frame.cieEnd, frame, frame.start, initial, initial), "valid CIE instructions failed");
        Require(initial.cfaRegister == 7 && initial.cfaOffset == 8, "valid CIE instructions computed unexpected initial CFA");
        LibcUnwind::Rules state = initial;
        Require(LibcUnwind::Instructions(p, frame.end, frame, 0x1010, state, initial), "valid FDE instructions failed");
        Require(state.cfaRegister == 7 && state.cfaOffset == 16, "valid FDE instructions computed unexpected CFA");
        Require(state.registers[6].kind == 1 && state.registers[6].value == -16, "valid FDE instructions computed unexpected r6 rule");
        Require(state.registers[16].kind == 1 && state.registers[16].value == -8, "valid FDE instructions computed unexpected r16 rule");
    }

    {
        std::array<LibcUnwind::Byte, 68> ehBuf = {
            0x14, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01, 'z', 'R', 'L',
            0x00, 0x01, 0x78, 0x10,
            0x02, 0x1b, 0x03, 0x0c,
            0x07, 0x08, 0x00, 0x00,
            0x18, 0x00, 0x00, 0x00,
            0x1c, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00,
            0x04, 0x44, 0x33, 0x22,
            0x11, 0x44, 0x0e, 0x10,
            0x00, 0x00, 0x00, 0x00,
            0x01, 0xff, 0x03, 0x3b,
            0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        const LibcUnwind::Byte* const ehHdr = &ehBuf[52];
        const LibcUnwind::Byte* const ehHdrEnd = ehBuf.data() + ehBuf.size();
        const LibcUnwind::Word targetPc = reinterpret_cast<LibcUnwind::Word>(ehBuf.data()) + 0x100;
        const std::int32_t fdeStartRel = static_cast<std::int32_t>(targetPc - reinterpret_cast<LibcUnwind::Word>(&ehBuf[32]));
        const std::int32_t hdrPcRel = static_cast<std::int32_t>(targetPc - reinterpret_cast<LibcUnwind::Word>(ehHdr));
        const std::int32_t hdrFdeRel = static_cast<std::int32_t>(&ehBuf[24] - ehHdr);
        std::memcpy(&ehBuf[32], &fdeStartRel, sizeof(fdeStartRel));
        std::memcpy(&ehBuf[60], &hdrPcRel, sizeof(hdrPcRel));
        std::memcpy(&ehBuf[64], &hdrFdeRel, sizeof(hdrFdeRel));

        LibcUnwind::Lookup truncHdrQuery{targetPc + 4};
        Require(!LibcUnwind::SearchEhFrameHeader(ehHdr, ehHdrEnd - 1, truncHdrQuery), "SearchEhFrameHeader accepted a truncated .eh_frame_hdr table");

        auto alignedHdrBuf = ehBuf;
        alignedHdrBuf[55] = 0x5b;
        LibcUnwind::Lookup alignedHdrQuery{targetPc + 4};
        Require(!LibcUnwind::SearchEhFrameHeader(&alignedHdrBuf[52], alignedHdrBuf.data() + alignedHdrBuf.size(), alignedHdrQuery), "SearchEhFrameHeader accepted aligned table encoding");

        LibcUnwind::Lookup query{targetPc + 4};
        Require(LibcUnwind::SearchEhFrameHeader(ehHdr, ehHdrEnd, query), "SearchEhFrameHeader failed on valid .eh_frame_hdr");
        Require(query.fde == &ehBuf[24], "SearchEhFrameHeader returned unexpected FDE");
        query.fdeBegin = ehBuf.data();
        query.fdeEnd = ehHdr;

        LibcUnwind::Frame frame{};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(LibcUnwind::DecodeRange(frame, query, p, augmentation, lsdaEncoding), "DecodeRange failed on looked-up FDE");
        Require(frame.start == targetPc && frame.length == 0x40, "looked-up FDE decoded unexpected range");
        Require(LibcUnwind::DecodeFdeAugmentation(frame, query, p, augmentation, lsdaEncoding), "DecodeFdeAugmentation failed on valid LSDA");
        Require(frame.lsda == 0x11223344, "DecodeFdeAugmentation decoded unexpected LSDA");

        {
            const LibcUnwind::Byte emptyZLAug[] = {0x00, 0x00};
            const LibcUnwind::Byte* ep = emptyZLAug;
            LibcUnwind::Frame emptyLsdaFrame{};
            emptyLsdaFrame.end = emptyZLAug + sizeof(emptyZLAug);
            Require(LibcUnwind::DecodeFdeAugmentation(emptyLsdaFrame, query, ep, "zRL", 0x1b), "DecodeFdeAugmentation rejected empty zL FDE augmentation");
            Require(emptyLsdaFrame.lsda == 0 && ep == emptyZLAug + 1, "empty zL FDE augmentation moved pointer unexpectedly");

            const LibcUnwind::Byte shortZLAug[] = {0x01, 0x44};
            const LibcUnwind::Byte* sp = shortZLAug;
            LibcUnwind::Frame shortLsdaFrame{};
            shortLsdaFrame.end = shortZLAug + sizeof(shortZLAug);
            Require(!LibcUnwind::DecodeFdeAugmentation(shortLsdaFrame, query, sp, "zRL", 0x1b), "DecodeFdeAugmentation accepted truncated nonempty zL FDE augmentation");
        }

        LibcUnwind::Lookup truncFdeQuery = query;
        truncFdeQuery.fdeEnd = &ehBuf[48];
        LibcUnwind::Frame badFrame{};
        Require(!LibcUnwind::DecodeRange(badFrame, truncFdeQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted truncated looked-up FDE");

        LibcUnwind::Lookup badCieBeginQuery = query;
        badCieBeginQuery.fdeBegin = &ehBuf[1];
        Require(!LibcUnwind::DecodeRange(badFrame, badCieBeginQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted CIE starting before fdeBegin");

        auto truncCieBuf = ehBuf;
        truncCieBuf[0] = 0x40;
        LibcUnwind::Lookup truncCieQuery{targetPc + 4};
        const LibcUnwind::Byte* const truncHdr = &truncCieBuf[52];
        const LibcUnwind::Byte* const truncHdrEnd = truncCieBuf.data() + truncCieBuf.size();
        const std::int32_t truncFdeStartRel = static_cast<std::int32_t>(targetPc - reinterpret_cast<LibcUnwind::Word>(&truncCieBuf[32]));
        const std::int32_t truncHdrPcRel = static_cast<std::int32_t>(targetPc - reinterpret_cast<LibcUnwind::Word>(truncHdr));
        std::memcpy(&truncCieBuf[32], &truncFdeStartRel, sizeof(truncFdeStartRel));
        std::memcpy(&truncCieBuf[60], &truncHdrPcRel, sizeof(truncHdrPcRel));
        Require(LibcUnwind::SearchEhFrameHeader(truncHdr, truncHdrEnd, truncCieQuery), "SearchEhFrameHeader failed on truncCieBuf");
        truncCieQuery.fdeBegin = truncCieBuf.data();
        truncCieQuery.fdeEnd = truncHdr;
        Require(!LibcUnwind::DecodeRange(badFrame, truncCieQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted CIE extending past fdeEnd");

        auto overlapCieBuf = ehBuf;
        overlapCieBuf[0] = 0x18;
        LibcUnwind::Lookup overlapCieQuery = query;
        overlapCieQuery.fde = &overlapCieBuf[24];
        overlapCieQuery.fdeBegin = overlapCieBuf.data();
        overlapCieQuery.fdeEnd = &overlapCieBuf[52];
        const std::int32_t overlapFdeStartRel = static_cast<std::int32_t>(targetPc - reinterpret_cast<LibcUnwind::Word>(&overlapCieBuf[32]));
        std::memcpy(&overlapCieBuf[32], &overlapFdeStartRel, sizeof(overlapFdeStartRel));
        Require(!LibcUnwind::DecodeRange(badFrame, overlapCieQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted CIE overlapping FDE within fdeEnd");

        std::array<LibcUnwind::Byte, 56> splitBuf = {
            0x14, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01, 'z', 'R', 'L',
            0x00, 0x01, 0x78, 0x10,
            0x02, 0x1b, 0x03, 0x0c,
            0x07, 0x08, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x18, 0x00, 0x00, 0x00,
            0x20, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00,
            0x04, 0x44, 0x33, 0x22,
            0x11, 0x44, 0x0e, 0x10,
            0x00, 0x00, 0x00, 0x00
        };
        const std::int32_t splitFdeStartRel = static_cast<std::int32_t>(targetPc - reinterpret_cast<LibcUnwind::Word>(&splitBuf[36]));
        std::memcpy(&splitBuf[36], &splitFdeStartRel, sizeof(splitFdeStartRel));
        LibcUnwind::Lookup validSplitQuery{targetPc + 4, &splitBuf[28], 0, 0, 0, splitBuf.data() + splitBuf.size(), &splitBuf[28], splitBuf.data(), &splitBuf[24]};
        LibcUnwind::Frame splitFrame{};
        Require(LibcUnwind::DecodeRange(splitFrame, validSplitQuery, p, augmentation, lsdaEncoding), "DecodeRange failed on valid split CIE/FDE sections");

        auto crossCieBuf = splitBuf;
        crossCieBuf[0] = 0x18;
        const std::int32_t crossFdeStartRel = static_cast<std::int32_t>(targetPc - reinterpret_cast<LibcUnwind::Word>(&crossCieBuf[36]));
        std::memcpy(&crossCieBuf[36], &crossFdeStartRel, sizeof(crossFdeStartRel));
        LibcUnwind::Lookup crossCieQuery{targetPc + 4, &crossCieBuf[28], 0, 0, 0, crossCieBuf.data() + crossCieBuf.size(), &crossCieBuf[28], crossCieBuf.data(), &crossCieBuf[24]};
        Require(!LibcUnwind::DecodeRange(badFrame, crossCieQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted CIE crossing cieEnd before fdeEnd");

        LibcUnwind::Lookup shortCieQuery{targetPc + 4, &splitBuf[28], 0, 0, 0, splitBuf.data() + splitBuf.size(), &splitBuf[28], splitBuf.data(), &splitBuf[2]};
        Require(!LibcUnwind::DecodeRange(badFrame, shortCieQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted CIE with fewer than 4 bytes before cieEnd");

        auto badFdeAugBuf = ehBuf;
        badFdeAugBuf[40] = 0x20;
        LibcUnwind::Lookup badAugQuery{targetPc + 4, &badFdeAugBuf[24], 0, 0, 0, &badFdeAugBuf[52], badFdeAugBuf.data()};
        Require(LibcUnwind::DecodeRange(badFrame, badAugQuery, p, augmentation, lsdaEncoding), "DecodeRange failed before FDE augmentation check");
        Require(!LibcUnwind::DecodeFdeAugmentation(badFrame, badAugQuery, p, augmentation, lsdaEncoding), "DecodeFdeAugmentation accepted augmentation size exceeding frame.end");

        auto shortLsdaBuf = ehBuf;
        shortLsdaBuf[40] = 0x02;
        LibcUnwind::Lookup shortLsdaQuery{targetPc + 4, &shortLsdaBuf[24], 0, 0, 0, &shortLsdaBuf[52], shortLsdaBuf.data()};
        Require(LibcUnwind::DecodeRange(badFrame, shortLsdaQuery, p, augmentation, lsdaEncoding), "DecodeRange failed before LSDA bounds check");
        Require(!LibcUnwind::DecodeFdeAugmentation(badFrame, shortLsdaQuery, p, augmentation, lsdaEncoding), "DecodeFdeAugmentation accepted truncated LSDA inside FDE augmentation");

        std::array<LibcUnwind::Byte, 32> unterminatedAugBuf = {
            0x07, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01, 'z', 'R',
            0x10, 0x00, 0x00, 0x00,
            0x0f, 0x00, 0x00, 0x00,
            0x00, 0x10, 0x00, 0x00,
            0x10, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00
        };
        LibcUnwind::Lookup unterminatedAugQuery{0x1000, &unterminatedAugBuf[11], 0, 0, 0, unterminatedAugBuf.data() + 31, unterminatedAugBuf.data(), unterminatedAugBuf.data(), &unterminatedAugBuf[11]};
        Require(!LibcUnwind::DecodeRange(badFrame, unterminatedAugQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted an unterminated CIE augmentation string reaching cieEnd");

        LibcUnwind::Lookup unterminatedAugNoSectionEndQuery{0x1000, &unterminatedAugBuf[11], 0, 0, 0, unterminatedAugBuf.data() + 31, unterminatedAugBuf.data()};
        Require(!LibcUnwind::DecodeRange(badFrame, unterminatedAugNoSectionEndQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted an unterminated CIE augmentation string reaching CIE record end");
    }

    {
        const std::array<LibcUnwind::Byte, 10> ulebMax = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
        const LibcUnwind::Byte* cursor = ulebMax.data();
        LibcUnwind::Word u = 0;
        Require(LibcUnwind::TryUleb(cursor, ulebMax.data() + ulebMax.size(), u) && cursor == ulebMax.data() + ulebMax.size() && u == static_cast<LibcUnwind::Word>(UINT64_MAX), "TryUleb rejected UINT64_MAX");

        for (const LibcUnwind::Byte tail : {LibcUnwind::Byte{0x02}, LibcUnwind::Byte{0x7f}, LibcUnwind::Byte{0x80}}) {
            const std::array<LibcUnwind::Byte, 10> ulebOverflow = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, tail};
            cursor = ulebOverflow.data();
            Require(!LibcUnwind::TryUleb(cursor, ulebOverflow.data() + ulebOverflow.size(), u), "TryUleb accepted overflowing 10th byte");
        }

        const std::array<LibcUnwind::Byte, 10> slebMax = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
        cursor = slebMax.data();
        std::intptr_t s = 0;
        Require(LibcUnwind::TrySleb(cursor, slebMax.data() + slebMax.size(), s) && cursor == slebMax.data() + slebMax.size() && s == static_cast<std::intptr_t>(INT64_MAX), "TrySleb rejected INT64_MAX");

        const std::array<LibcUnwind::Byte, 10> slebMin = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f};
        cursor = slebMin.data();
        Require(LibcUnwind::TrySleb(cursor, slebMin.data() + slebMin.size(), s) && cursor == slebMin.data() + slebMin.size() && s == static_cast<std::intptr_t>(INT64_MIN), "TrySleb rejected INT64_MIN");

        for (const LibcUnwind::Byte tail : {LibcUnwind::Byte{0x01}, LibcUnwind::Byte{0x40}, LibcUnwind::Byte{0x7e}, LibcUnwind::Byte{0x80}}) {
            const std::array<LibcUnwind::Byte, 10> slebOverflowPos = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, tail};
            cursor = slebOverflowPos.data();
            Require(!LibcUnwind::TrySleb(cursor, slebOverflowPos.data() + slebOverflowPos.size(), s), "TrySleb accepted overflowing 10th byte");
            const std::array<LibcUnwind::Byte, 10> slebOverflowNeg = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, tail};
            cursor = slebOverflowNeg.data();
            Require(!LibcUnwind::TrySleb(cursor, slebOverflowNeg.data() + slebOverflowNeg.size(), s), "TrySleb accepted overflowing negative 10th byte");
        }

        const std::array<LibcUnwind::Byte, 48> overflowCieCodeAlign = {
            0x1c, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01, 'z', 'R', 0x00,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02,
            0x78,
            0x10,
            0x01,
            0x03,
            0x0c, 0x07, 0x08, 0x00, 0x00, 0x00,
            0x0c, 0x00, 0x00, 0x00,
            0x24, 0x00, 0x00, 0x00,
            0x00, 0x10, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00
        };
        LibcUnwind::Frame overflowFrame{};
        LibcUnwind::Lookup overflowCieCodeQuery{0x1010, &overflowCieCodeAlign[32], 0, 0, 0, overflowCieCodeAlign.data() + overflowCieCodeAlign.size(), overflowCieCodeAlign.data(), overflowCieCodeAlign.data(), &overflowCieCodeAlign[32]};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        Require(!LibcUnwind::DecodeRange(overflowFrame, overflowCieCodeQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted overflowing ULEB128 codeAlign in CIE");

        const std::array<LibcUnwind::Byte, 48> overflowCieDataAlign = {
            0x1c, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01, 'z', 'R', 0x00,
            0x01,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7e,
            0x10,
            0x01,
            0x03,
            0x0c, 0x07, 0x08, 0x00, 0x00, 0x00,
            0x0c, 0x00, 0x00, 0x00,
            0x24, 0x00, 0x00, 0x00,
            0x00, 0x10, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00
        };
        overflowFrame = {};
        LibcUnwind::Lookup overflowCieDataQuery{0x1010, &overflowCieDataAlign[32], 0, 0, 0, overflowCieDataAlign.data() + overflowCieDataAlign.size(), overflowCieDataAlign.data(), overflowCieDataAlign.data(), &overflowCieDataAlign[32]};
        Require(!LibcUnwind::DecodeRange(overflowFrame, overflowCieDataQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted overflowing SLEB128 dataAlign in CIE");

        const std::array<LibcUnwind::Byte, 40> overflowFdeRange = {
            0x10, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01, 'z', 'R', 0x00,
            0x01, 0x78, 0x10, 0x01,
            0x01, 0x0c, 0x07, 0x08,
            0x10, 0x00, 0x00, 0x00,
            0x18, 0x00, 0x00, 0x00,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02,
            0x40,
            0x00
        };
        overflowFrame = {};
        LibcUnwind::Lookup overflowFdeQuery{0x1010, &overflowFdeRange[20], 0, 0, 0, overflowFdeRange.data() + overflowFdeRange.size(), overflowFdeRange.data(), overflowFdeRange.data(), &overflowFdeRange[20]};
        Require(!LibcUnwind::DecodeRange(overflowFrame, overflowFdeQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted overflowing ULEB128 start in FDE");
    }

    {
        Require(LibcUnwind::EffectiveSectionSize(0, sizeof(validEhFrame)) == sizeof(validEhFrame), "EffectiveSectionSize failed SizeOfRawData fallback");
        Require(LibcUnwind::EffectiveSectionSize(24, sizeof(validEhFrame)) == 24, "EffectiveSectionSize ignored nonzero VirtualSize");
        Require(LibcUnwind::EffectiveSectionSize(0, 0) == 0, "EffectiveSectionSize failed zero-size section");

        std::array<LibcUnwind::Byte, 64> sectionImage{};
        std::memcpy(&sectionImage[16], validEhFrame, sizeof(validEhFrame));

        const LibcUnwind::Byte* frames = nullptr;
        const LibcUnwind::Byte* framesEnd = nullptr;
        Require(LibcUnwind::SectionBounds(sectionImage.data(), 16, 0, sizeof(validEhFrame), frames, framesEnd), "SectionBounds rejected VirtualSize == 0 with nonzero SizeOfRawData");
        Require(frames == &sectionImage[16] && framesEnd == &sectionImage[16] + sizeof(validEhFrame), "SectionBounds computed unexpected fallback bounds");

        LibcUnwind::Frame sectionFrame{};
        const LibcUnwind::Byte* p = nullptr;
        const char* augmentation = nullptr;
        LibcUnwind::Byte lsdaEncoding = 0;
        LibcUnwind::Lookup fallbackQuery{0x1010, frames + 20, 0, 0, 0, framesEnd, frames, frames, framesEnd};
        Require(LibcUnwind::DecodeRange(sectionFrame, fallbackQuery, p, augmentation, lsdaEncoding), "DecodeRange rejected .ehfram bounds with VirtualSize == 0 fallback");
        Require(sectionFrame.start == 0x1000 && sectionFrame.length == 0x40, "DecodeRange decoded unexpected range from fallback .ehfram bounds");

        const LibcUnwind::Byte* truncFrames = nullptr;
        const LibcUnwind::Byte* truncFramesEnd = nullptr;
        Require(LibcUnwind::SectionBounds(sectionImage.data(), 16, 24, sizeof(validEhFrame), truncFrames, truncFramesEnd), "SectionBounds rejected nonzero VirtualSize");
        Require(truncFrames == &sectionImage[16] && truncFramesEnd == &sectionImage[16] + 24, "SectionBounds did not prefer nonzero VirtualSize");
        LibcUnwind::Lookup truncQuery{0x1010, truncFrames + 20, 0, 0, 0, truncFramesEnd, truncFrames, truncFrames, truncFramesEnd};
        LibcUnwind::Frame truncFrame{};
        Require(!LibcUnwind::DecodeRange(truncFrame, truncQuery, p, augmentation, lsdaEncoding), "DecodeRange accepted FDE past nonzero VirtualSize");

        const LibcUnwind::Byte* badBegin = &sectionImage[0];
        const LibcUnwind::Byte* badEnd = &sectionImage[0];
        Require(!LibcUnwind::SectionBounds(sectionImage.data(), 16, 0, 0, badBegin, badEnd) && !badBegin && !badEnd, "SectionBounds accepted empty section");
        Require(!LibcUnwind::SectionBounds(nullptr, 16, 0, sizeof(validEhFrame), badBegin, badEnd), "SectionBounds accepted null image base");
        Require(!LibcUnwind::SectionBounds(sectionImage.data(), ~LibcUnwind::Word(0) - 4, 16, 0, badBegin, badEnd), "SectionBounds accepted virtualAddress wraparound");
        Require(!LibcUnwind::SectionBounds(sectionImage.data(), 16, ~LibcUnwind::Word(0) - 4, 0, badBegin, badEnd), "SectionBounds accepted section size wraparound");
    }
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: guest_app_archive_tests present|absent|damaged|invalid\n";
        return 2;
    }
    const std::string mode = argv[1];
    const auto root = AppArchiveFixture::EnterFreshRoot("anyps5-app-archive");
    if (mode == "present") Present(root);
    else if (mode == "absent") Absent();
    else if (mode == "damaged") Damaged(root);
    else if (mode == "invalid") Invalid(root);
    else return 2;
    UnwindBounds();
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
