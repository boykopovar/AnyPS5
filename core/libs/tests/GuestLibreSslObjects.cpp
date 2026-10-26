#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

template<typename TAction>
static void RequireThrows(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    std::abort();
}

static void CheckName(int nid, const char* name) {
    const char* shortName = OBJ_nid2sn_nid_postfix(nid);
    Require(shortName != nullptr && std::strcmp(shortName, name) == 0);
    Require(OBJ_sn2nid_nid_postfix(name) == nid);
}

static bool IsUnassigned(int nid) {
    constexpr std::array unassigned = {102, 118, 350, 511, 772, 774, 775};
    for (const int value : unassigned) if (value == nid) return true;
    return false;
}

int main() {
    CheckName(0, "UNDEF");
    CheckName(4, "MD5");
    CheckName(6, "rsaEncryption");
    CheckName(13, "CN");
    CheckName(64, "SHA1");
    CheckName(672, "SHA256");
    CheckName(950, "X25519");
    CheckName(955, "Ed448ph");
    Require(std::strcmp(OBJ_nid2sn_nid_postfix(393), "NULL") == 0);
    Require(std::strcmp(OBJ_nid2sn_nid_postfix(404), "NULL") == 0);
    Require(OBJ_sn2nid_nid_postfix("NULL") == 404);
    Require(OBJ_nid2sn_nid_postfix(-1) == nullptr);
    Require(OBJ_nid2sn_nid_postfix(956) == nullptr);
    Require(OBJ_sn2nid_nid_postfix("") == 0);
    Require(OBJ_sn2nid_nid_postfix("sha256") == 0);
    Require(OBJ_sn2nid_nid_postfix("ZZZ") == 0);
    for (int nid = 1; nid < 956; ++nid) {
        if (IsUnassigned(nid) || nid == 393) continue;
        Require(OBJ_sn2nid_nid_postfix(OBJ_nid2sn_nid_postfix(nid)) == nid);
    }
    RequireThrows([] { OBJ_sn2nid_nid_postfix(nullptr); });
    return 0;
}
