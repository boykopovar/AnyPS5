#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "Testing.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace {

constexpr std::uint64_t UnknownNidError = (8ull << 24) | (0xfffull << 12) | 101;
constexpr std::array UnassignedNids = {102, 118, 350, 511, 772, 774, 775};

bool IsUnassigned(int nid) {
    for (const int value : UnassignedNids) if (value == nid) return true;
    return false;
}

bool Equal(const char* actual, const char* expected) {
    return actual && std::strcmp(actual, expected) == 0;
}

void CheckName(int nid, const char* name, const char* message) {
    Require(Equal(OBJ_nid2sn_nid_postfix(nid), name) && OBJ_sn2nid_nid_postfix(name) == nid, message);
}

void CheckUnknownNid(int nid, const char* message) {
    ERR_clear_error_nid_postfix();
    Require(OBJ_nid2sn_nid_postfix(nid) == nullptr, message);
    Require(ERR_get_error_nid_postfix() == UnknownNidError && ERR_get_error_nid_postfix() == 0, message);
}

void CheckBuiltIn() {
    CheckName(0, "UNDEF", "NID 0");
    CheckName(4, "MD5", "MD5");
    CheckName(6, "rsaEncryption", "rsaEncryption");
    CheckName(13, "CN", "commonName");
    CheckName(64, "SHA1", "SHA1");
    CheckName(672, "SHA256", "SHA256");
    CheckName(950, "X25519", "X25519");
    CheckName(955, "Ed448ph", "the last 2.5.4 NID");
    Require(Equal(OBJ_nid2sn_nid_postfix(393), "NULL") && Equal(OBJ_nid2sn_nid_postfix(404), "NULL"), "393 and 404 share the short name NULL");
    Require(OBJ_sn2nid_nid_postfix("NULL") == 404, "the short-name search resolves NULL to 404");
    ERR_clear_error_nid_postfix();
    Require(OBJ_nid2sn_nid_postfix(-1) == nullptr && OBJ_nid2sn_nid_postfix(LibreSslObjectCount) == nullptr, "NIDs outside the table");
    Require(ERR_get_error_nid_postfix() == 0, "NIDs outside the table report no error before OBJ_create");
    for (const int nid : UnassignedNids) CheckUnknownNid(nid, "an unassigned NID returns null and reports OBJ_R_UNKNOWN_NID");
    Require(OBJ_sn2nid_nid_postfix("") == 0 && OBJ_sn2nid_nid_postfix("sha256") == 0 && OBJ_sn2nid_nid_postfix("ZZZ") == 0, "unknown and case-mismatched names");
    for (int nid = 1; nid < LibreSslObjectCount; ++nid) {
        if (IsUnassigned(nid) || nid == 393) continue;
        Require(OBJ_sn2nid_nid_postfix(OBJ_nid2sn_nid_postfix(nid)) == nid, "every assigned short name maps back to its NID");
    }
    ExpectErr([] { OBJ_sn2nid_nid_postfix(nullptr); }, "a null name");
    errno = 71;
    ExpectErr([] { OBJ_sn2nid_nid_postfix(reinterpret_cast<const char*>(0x10)); }, "an unreadable name");
    Require(errno == 71, "an unreadable name keeps errno");
    alignas(4096) static char pages[8192];
    std::memcpy(pages + 4093, "SHA256", 7);
    Require(OBJ_sn2nid_nid_postfix(pages + 4093) == 672, "a name that crosses a page boundary");
    std::memset(pages, 'a', sizeof(pages) - 1);
    Require(OBJ_sn2nid_nid_postfix(pages) == 0, "a name longer than a page");
}

void CheckAdded() {
    const int added = OBJ_create_nid_postfix("1.2.3.4.5", "addedShort", "added long");
    Require(added == LibreSslObjectCount, "the first added object takes NID 956");
    CheckName(added, "addedShort", "an added object is found by NID and by short name");
    const int shadow = OBJ_create_nid_postfix("1.2.3.4.6", "CN", nullptr);
    Require(OBJ_sn2nid_nid_postfix("CN") == shadow && Equal(OBJ_nid2sn_nid_postfix(13), "CN"), "added objects are searched before the built-in table");
    const int renamed = OBJ_create_nid_postfix("1.2.3.4.7", "addedShort", nullptr);
    Require(OBJ_sn2nid_nid_postfix("addedShort") == renamed && Equal(OBJ_nid2sn_nid_postfix(added), "addedShort"), "the newest added object owns a reused short name");
    const int unnamed = OBJ_create_nid_postfix("1.2.3.4.8", nullptr, "long only");
    ERR_clear_error_nid_postfix();
    Require(OBJ_nid2sn_nid_postfix(unnamed) == nullptr && ERR_get_error_nid_postfix() == 0, "an added object without a short name");
    CheckUnknownNid(unnamed + 1, "an unused NID past the table reports OBJ_R_UNKNOWN_NID once objects are added");
    CheckUnknownNid(-1, "a negative NID reports OBJ_R_UNKNOWN_NID once objects are added");
    OBJ_cleanup_nid_postfix();
    Require(OBJ_sn2nid_nid_postfix("CN") == 13 && OBJ_sn2nid_nid_postfix("addedShort") == 0, "OBJ_cleanup removes the added names");
    ERR_clear_error_nid_postfix();
    Require(OBJ_nid2sn_nid_postfix(added) == nullptr && ERR_get_error_nid_postfix() == 0, "OBJ_cleanup removes the added NIDs");
}

} // namespace

int main() {
    CheckBuiltIn();
    CheckAdded();
    return 0;
}
