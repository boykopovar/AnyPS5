#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint64_t UnknownNidError = (8ull << 24) | (0xfffull << 12) | 101;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

struct Encoded {
    std::vector<unsigned char> bytes;
    LibreSslAsn1Object object;

    Encoded(std::initializer_list<unsigned char> data, int nid = 0) : bytes(data), object{nullptr, nullptr, nid, static_cast<int>(bytes.size()), bytes.data(), 0} {}
};

void RequireText(const LibreSslAsn1Object& object, int space, int expected, const char* text, const char* message) {
    char buffer[128];
    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(i2t_ASN1_OBJECT_nid_postfix(buffer, space, &object) == expected, message);
    Require(std::strcmp(buffer, text) == 0, message);
    Require(static_cast<unsigned char>(buffer[std::strlen(text) + 1]) == 0x5a || std::strlen(text) + 1 >= static_cast<std::size_t>(space),
        "nothing is written after the terminator");
}

void Table() {
    const auto& undefined = LibreSslObjects_nid_no_patch[0];
    Require(std::strcmp(undefined.shortName, "UNDEF") == 0 && std::strcmp(undefined.longName, "undefined") == 0 && !undefined.data, "NID 0");
    const auto& commonName = LibreSslObjects_nid_no_patch[13];
    Require(commonName.nid == 13 && std::strcmp(commonName.shortName, "CN") == 0 && std::strcmp(commonName.longName, "commonName") == 0
        && commonName.length == 3 && std::memcmp(commonName.data, "\x55\x04\x03", 3) == 0 && commonName.flags == 0, "commonName");
    const auto& last = LibreSslObjects_nid_no_patch[LibreSslObjectCount - 1];
    Require(last.nid == 955 && std::strcmp(last.shortName, "Ed448ph") == 0, "the last 2.5.4 NID");
    for (int nid : {102, 118, 350, 511, 772, 774, 775}) {
        const auto& gap = LibreSslObjects_nid_no_patch[nid];
        Require(!gap.shortName && !gap.longName && gap.nid == 0 && gap.length == 0 && !gap.data, "unassigned NIDs are empty");
    }
    for (int index = 0; index < LibreSslObjectOrderCount; ++index)
        Require(LibreSslObjectOrder_nid_no_patch[index] < LibreSslObjectCount, "the search order indexes the table");
}

void Lookup() {
    Require(LibreSslObjectNid_nid_no_patch(nullptr) == 0, "null object");
    Encoded commonName{0x55, 0x04, 0x03};
    Require(LibreSslObjectNid_nid_no_patch(&commonName.object) == 13, "an encoding found by search");
    Encoded preset{0x55, 0x04, 0x03, 0x01}, unknown{0x2a, 0x03, 0x04};
    preset.object.nid = 6;
    Require(LibreSslObjectNid_nid_no_patch(&preset.object) == 6, "a set NID wins without a search");
    Require(LibreSslObjectNid_nid_no_patch(&unknown.object) == 0, "an unknown encoding");
    Encoded organization{0x2b};
    Require(LibreSslObjectNid_nid_no_patch(&organization.object) == 379, "1.3 is listed twice; the search lands on ORG as in LibreSSL");
    unsigned char none = 0;
    LibreSslAsn1Object empty{nullptr, nullptr, 0, 0, &none, 0};
    Require(LibreSslObjectNid_nid_no_patch(&empty) == 404, "an empty encoding lands on NID_ccitt");
    Require(OBJ_obj2nid_nid_postfix(nullptr) == 0, "a null object has no NID");
    Require(OBJ_obj2nid_nid_postfix(&commonName.object) == 13, "the export finds a built-in encoding");
    Require(OBJ_obj2nid_nid_postfix(&preset.object) == 6, "the export returns a set NID without a search");
    Require(OBJ_obj2nid_nid_postfix(&unknown.object) == 0, "the export leaves an unknown encoding undefined");
}

void Names() {
    Encoded commonName{0x55, 0x04, 0x03};
    RequireText(commonName.object, 64, 10, "commonName", "the long name");
    RequireText(commonName.object, 5, 10, "comm", "a short buffer truncates and returns the full length");
    RequireText(commonName.object, 1, 10, "", "a one-byte buffer holds the terminator");
    Require(i2t_ASN1_OBJECT_nid_postfix(nullptr, 0, &commonName.object) == 10, "a zero-length buffer is not written");
    Encoded named{0x2a, 0x03, 0x04}, sha256{0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
    named.object.nid = 13;
    RequireText(named.object, 64, 10, "commonName", "a set NID names the object whatever its encoding");
    RequireText(sha256.object, 64, 6, "sha256", "a long name equal to the short name");
}

void Numbers() {
    Encoded unknown{0x2a, 0x03, 0x04};
    RequireText(unknown.object, 64, 7, "1.2.3.4", "an unknown OID is printed in dotted form");
    RequireText(unknown.object, 4, 7, "1.2", "a dotted OID is truncated part by part");
    RequireText(unknown.object, 2, 7, "1", "a two-byte buffer keeps the first arc");
    RequireText(unknown.object, 1, 7, "", "a one-byte buffer keeps nothing");
    Encoded second{0x88, 0x37, 0x05};
    RequireText(second.object, 64, 7, "2.999.5", "a first subidentifier of 80 or more starts with 2");
    Encoded large{0x2a, 0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    RequireText(large.object, 64, 24, "1.2.18446744073709551616", "an arc past 64 bits is printed exactly");
    Encoded largeFirst{0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    RequireText(largeFirst.object, 64, 22, "2.18446744073709551536", "a first subidentifier past 64 bits loses 80");
    Encoded truncated{0x2a, 0x03, 0x83};
    RequireText(truncated.object, 64, 0, "1.2.3", "a truncated subidentifier returns 0 and keeps what was written");
    Encoded high{0x2a, 0x05};
    high.object.nid = 5000;
    RequireText(high.object, 64, 5, "1.2.5", "a NID past the table prints the encoding");
    Require(ERR_get_error_nid_postfix() == 0, "a NID past the table reports no error");
}

void Errors() {
    ERR_clear_error_nid_postfix();
    Encoded gap{0x2a, 0x07};
    gap.object.nid = 102;
    RequireText(gap.object, 64, 5, "1.2.7", "an unassigned NID prints the encoding");
    Require(ERR_get_error_nid_postfix() == UnknownNidError && ERR_get_error_nid_postfix() == UnknownNidError
        && ERR_get_error_nid_postfix() == 0, "the long and short name lookups each report OBJ_R_UNKNOWN_NID");
    LibreSslAsn1Object none{nullptr, nullptr, 13, 3, nullptr, 0};
    RequireText(none, 64, 0, "", "an object without data");
    Require(i2t_ASN1_OBJECT_nid_postfix(nullptr, 0, nullptr) == 0, "a null object");
    char buffer[8];
    RequireThrows([&] { i2t_ASN1_OBJECT_nid_postfix(buffer, -1, &gap.object); }, "a negative length");
    RequireThrows([&] { i2t_ASN1_OBJECT_nid_postfix(nullptr, 8, &gap.object); }, "a null buffer with a length");
    Require(i2t_ASN1_OBJECT_nid_postfix(buffer, -1, nullptr) == 0, "a negative length with a null object");
}

constexpr std::uint64_t Asn1Error(int reason) {
    return (13ull << 24) | (0xfffull << 12) | static_cast<std::uint64_t>(reason);
}

void Rejects(const char* oid, std::uint64_t error, const char* message) {
    ERR_clear_error_nid_postfix();
    Require(OBJ_create_nid_postfix(oid, "x", "y") == 0, message);
    Require(ERR_get_error_nid_postfix() == error && ERR_get_error_nid_postfix() == 0, message);
}

void Created() {
    Encoded added{0x2a, 0x03, 0x04}, commonName{0x55, 0x04, 0x03};
    Encoded big{0x2a, 0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x05};
    Rejects("3.1", Asn1Error(122), "a first arc above 2");
    Rejects("1", Asn1Error(138), "a missing second arc");
    Rejects("12.3", Asn1Error(131), "a missing separator");
    Rejects("1.2x", Asn1Error(130), "a non-digit");
    Rejects("1.40", Asn1Error(147), "a second arc of 40 under 0 or 1");
    Rejects("", Asn1Error(122), "an empty text reads its terminator as the first arc");
    Rejects("1.", 0, "a text without a second arc value adds nothing and queues nothing");
    RequireThrows([] { OBJ_create_nid_postfix(nullptr, "x", "y"); }, "a null OID text");
    Require(OBJ_create_nid_postfix("1.2.3.4", "addedShort", "addedLong") == LibreSslObjectCount, "the first added object takes NID 956");
    RequireText(added.object, 64, 9, "addedLong", "an added object prints its long name");
    Require(LibreSslObjectNid_nid_no_patch(&added.object) == LibreSslObjectCount, "an added encoding resolves to its NID");
    Require(OBJ_create_nid_postfix("1 2 3 4", "secondShort", nullptr) == LibreSslObjectCount + 1, "spaces separate arcs and each object takes the next NID");
    RequireText(added.object, 64, 11, "secondShort", "the latest object with an encoding wins, and a missing long name falls back to the short name");
    Require(OBJ_create_nid_postfix("2.5.4.3", nullptr, nullptr) == LibreSslObjectCount + 2, "an added object may repeat a built-in OID");
    Require(OBJ_obj2nid_nid_postfix(&commonName.object) == LibreSslObjectCount + 2, "the export finds an added encoding before the built-in table");
    RequireText(commonName.object, 64, 7, "2.5.4.3", "added encodings are searched before the table, and a nameless one prints in dotted form");
    Require(OBJ_create_nid_postfix("1.2.18446744073709551616.5", "big", "bigLong") == LibreSslObjectCount + 3, "an arc past 64 bits");
    RequireText(big.object, 64, 7, "bigLong", "an arc past 64 bits is encoded exactly");
    Require(OBJ_create_nid_postfix("2.999..", "trailing", nullptr) == LibreSslObjectCount + 4, "empty arcs count as zero");
    Encoded trailing{0x88, 0x37, 0x00};
    RequireText(trailing.object, 64, 8, "trailing", "empty arcs count as zero");
    Encoded unknown{0x2a, 0x05};
    unknown.object.nid = 5000;
    ERR_clear_error_nid_postfix();
    RequireText(unknown.object, 64, 5, "1.2.5", "a NID past the table that was not added prints the encoding");
    Require(ERR_get_error_nid_postfix() == UnknownNidError && ERR_get_error_nid_postfix() == UnknownNidError && ERR_get_error_nid_postfix() == 0,
        "once objects were added, an unknown NID past the table reports OBJ_R_UNKNOWN_NID for each name");
    OPENSSL_add_all_algorithms_noconf_nid_postfix();
    RequireText(added.object, 64, 11, "secondShort", "OPENSSL_add_all_algorithms_noconf keeps added objects");
    OBJ_cleanup_nid_postfix();
    RequireText(added.object, 64, 7, "1.2.3.4", "OBJ_cleanup removes added objects");
    RequireText(commonName.object, 64, 10, "commonName", "OBJ_cleanup restores the built-in names");
    Require(OBJ_obj2nid_nid_postfix(&commonName.object) == 13, "after OBJ_cleanup the export finds the built-in encoding again");
    RequireText(unknown.object, 64, 5, "1.2.5", "after OBJ_cleanup an unknown NID past the table reports no error");
    Require(ERR_get_error_nid_postfix() == 0, "after OBJ_cleanup an unknown NID past the table reports no error");
    OBJ_cleanup_nid_postfix();
    Require(OBJ_create_nid_postfix("1.2.3.4", "again", nullptr) == LibreSslObjectCount + 5, "NIDs keep counting after OBJ_cleanup");
    RequireText(added.object, 64, 5, "again", "objects can be added again after OBJ_cleanup");
    OBJ_cleanup_nid_postfix();
}

std::string Written(const LibreSslAsn1Object* object, int expected) {
    LibreSsl::Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr, "a memory BIO");
    Require(i2a_ASN1_OBJECT_nid_postfix(bio, object) == expected, "i2a_ASN1_OBJECT returns the written length");
    char* data = nullptr;
    const auto length = BIO_ctrl_nid_postfix(bio, 3, 0, &data);
    std::string text(data, static_cast<std::size_t>(length));
    BIO_free_nid_postfix(bio);
    Require(ERR_get_error_nid_postfix() == 0, "i2a_ASN1_OBJECT queues no error");
    return text;
}

void Printed() {
    Require(Written(nullptr, 4) == "NULL", "a null object prints NULL");
    Require(Written(&LibreSslObjects_nid_no_patch[13], 10) == "commonName", "a known object prints its long name");
    LibreSslAsn1Object empty{};
    Require(Written(&empty, 4) == "NULL", "an object without an encoding prints NULL");
    Encoded unknown({0x2a, 0x03, 0x04});
    Require(Written(&unknown.object, 7) == "1.2.3.4", "an unknown object prints its OID");
    Encoded truncated({0x2a, 0x83});
    Require(Written(&truncated.object, 9) == "<INVALID>", "an undecodable object prints <INVALID>");
    std::vector<unsigned char> arcs(300, 0x7f);
    arcs[0] = 0x2a;
    LibreSslAsn1Object longer{nullptr, nullptr, 0, static_cast<int>(arcs.size()), arcs.data(), 0};
    std::string expected = "1.2";
    for (std::size_t index = 1; index < arcs.size(); ++index) expected += ".127";
    Require(Written(&longer, static_cast<int>(expected.size())) == expected, "text longer than 255 bytes is printed whole");
    Require(i2a_ASN1_OBJECT_nid_postfix(nullptr, &unknown.object) == 0, "a null BIO writes nothing");
}

struct ScratchAllocation {
    std::uintptr_t address;
    std::size_t bytes;
    bool released;
};

std::array<ScratchAllocation, 2> scratch{};
std::size_t scratchAllocations = 0;
std::size_t scratchReleases = 0;
bool trackingScratch = false;

void* APS5_VABI Allocate(std::size_t bytes) {
    void* pointer = std::malloc(bytes ? bytes : 1);
    if (pointer && trackingScratch) {
        Require(scratchAllocations < scratch.size(), "only the scratch buffers are allocated");
        if (scratchAllocations != 0) Require(scratch[0].released, "the old scratch buffer is freed before growing");
        scratch[scratchAllocations++] = {reinterpret_cast<std::uintptr_t>(pointer), bytes, false};
    }
    return pointer;
}

void APS5_VABI Release(void* pointer) {
    if (pointer && trackingScratch) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        ScratchAllocation* allocation = nullptr;
        for (auto& item : scratch)
            if (item.address == address && !item.released) allocation = &item;
        Require(allocation != nullptr, "only an owned scratch buffer is freed");
        const auto* bytes = static_cast<const unsigned char*>(pointer);
        for (std::size_t index = 0; index < allocation->bytes; ++index)
            Require(bytes[index] == 0, "the whole scratch capacity is cleansed before freeing");
        allocation->released = true;
        ++scratchReleases;
    }
    std::free(pointer);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    Require(!trackingScratch, "scratch allocation uses malloc");
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void* pointer, std::size_t bytes) {
    Require(!trackingScratch, "scratch growth does not use realloc");
    return std::realloc(pointer, bytes);
}

void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

enum class WriteBehavior { Complete, MethodThrows, BeforeThrows, AfterThrows };
struct WriteFailure {};

WriteBehavior writeBehavior;
std::string_view expectedWrite;
int methodCalls = 0;
int callbackCalls = 0;

int APS5_VABI Write(LibreSsl::Bio*, const char* text, int length) {
    ++methodCalls;
    Require(length >= 0 && static_cast<std::size_t>(length) == expectedWrite.size(), "the BIO receives the complete text");
    Require(std::memcmp(text, expectedWrite.data(), expectedWrite.size()) == 0, "the BIO receives the expected text");
    if (writeBehavior == WriteBehavior::MethodThrows) throw WriteFailure{};
    return length;
}

std::int64_t APS5_VABI Callback(LibreSsl::Bio*, int operation, const char*, int, std::int64_t, std::int64_t result) {
    ++callbackCalls;
    if ((operation == 3 && writeBehavior == WriteBehavior::BeforeThrows) ||
        (operation == 0x83 && writeBehavior == WriteBehavior::AfterThrows)) throw WriteFailure{};
    return result;
}

void CheckScratchCleanup(const LibreSslAsn1Object* object, std::string_view expected, WriteBehavior behavior) {
    LibreSsl::BioMethod method{};
    method.write = Write;
    LibreSsl::Bio bio{};
    bio.method = &method;
    bio.callback = Callback;
    bio.initialized = 1;
    scratch = {};
    scratchAllocations = scratchReleases = 0;
    methodCalls = callbackCalls = 0;
    writeBehavior = behavior;
    expectedWrite = expected;
    bool caught = false;
    int written = -1;
    trackingScratch = true;
    try {
        written = i2a_ASN1_OBJECT_nid_postfix(&bio, object);
    } catch (const WriteFailure&) {
        caught = true;
    }
    trackingScratch = false;
    Require(caught == (behavior != WriteBehavior::Complete), "the original BIO exception propagates");
    if (!caught) Require(written == static_cast<int>(expected.size()), "a completed BIO write returns its length");
    const std::size_t count = expected.size() >= 256 ? 2 : 1;
    if (scratchAllocations != count || scratchReleases != count) {
        std::fprintf(stderr, "i2a scratch cleanup failed: bytes=%zu behavior=%d allocations=%zu releases=%zu\n",
            expected.size(), static_cast<int>(behavior), scratchAllocations, scratchReleases);
        Require(false, "every scratch buffer is freed exactly once");
    }
    Require(scratch[0].bytes == 256 && scratch[0].released, "the initial scratch allocation is released");
    if (count == 2) Require(scratch[1].bytes == expected.size() + 1 && scratch[1].released, "the grown scratch allocation is released");
    Require(methodCalls == (behavior == WriteBehavior::BeforeThrows ? 0 : 1), "the BIO method is called at the expected point");
    const bool completedMethod = behavior == WriteBehavior::Complete || behavior == WriteBehavior::AfterThrows;
    Require(callbackCalls == (completedMethod ? 2 : 1), "the BIO callbacks keep their order");
    Require(bio.bytesWritten == (completedMethod ? expected.size() : 0), "completed BIO bytes remain accounted for");
    Require(ERR_get_error_nid_postfix() == 0, "scratch cleanup queues no error");
}

void ScratchCleanup() {
    std::vector<unsigned char> arcs(300, 0x7f);
    arcs[0] = 0x2a;
    LibreSslAsn1Object longer{nullptr, nullptr, 0, static_cast<int>(arcs.size()), arcs.data(), 0};
    std::string expected = "1.2";
    for (std::size_t index = 1; index < arcs.size(); ++index) expected += ".127";
    for (const auto behavior : {WriteBehavior::Complete, WriteBehavior::MethodThrows, WriteBehavior::BeforeThrows, WriteBehavior::AfterThrows}) {
        CheckScratchCleanup(&LibreSslObjects_nid_no_patch[13], "commonName", behavior);
        CheckScratchCleanup(&longer, expected, behavior);
    }
}

} // namespace

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release), reinterpret_cast<void*>(AllocateZeroed),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Table();
    Lookup();
    Names();
    Numbers();
    Errors();
    Created();
    Printed();
    ScratchCleanup();
}
