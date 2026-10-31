#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cstdio>
#include <string>

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

static bool Unsupported(const std::runtime_error& error, const char* function) {
    const std::string text = error.what();
    return text.find(function) != std::string::npos && text.find("not implemented") != std::string::npos;
}

static bool TestAddedObjects() {
    bool passed = true;
    for (const char* name : {"dynamicShortName", "prime256v1"}) {
        OBJ_cleanup_nid_postfix();
        ERR_clear_error_nid_postfix();
        const int nid = OBJ_create_nid_postfix("1.2.3.4", name, "dynamic long name");
        Require(nid >= 956);
        bool correct = false;
        try { correct = OBJ_sn2nid_nid_postfix(name) == nid; }
        catch (const std::runtime_error& error) { correct = Unsupported(error, "OBJ_sn2nid"); }
        if (!correct) std::fprintf(stderr, "registered short name %s returns an incorrect NID\n", name);
        passed &= correct;
        correct = false;
        try {
            const char* result = OBJ_nid2sn_nid_postfix(nid);
            correct = result && std::strcmp(result, name) == 0;
        } catch (const std::runtime_error& error) { correct = Unsupported(error, "OBJ_nid2sn"); }
        if (!correct) std::fprintf(stderr, "registered NID %d returns an incorrect short name\n", nid);
        passed &= correct;
        OBJ_cleanup_nid_postfix();
        ERR_clear_error_nid_postfix();
    }
    return passed;
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
    Require(OBJ_sn2nid_nid_postfix("") == 0);
    Require(OBJ_sn2nid_nid_postfix("sha256") == 0);
    Require(OBJ_sn2nid_nid_postfix("ZZZ") == 0);
    for (int nid = 1; nid < 956; ++nid) {
        if (IsUnassigned(nid) || nid == 393) continue;
        Require(OBJ_sn2nid_nid_postfix(OBJ_nid2sn_nid_postfix(nid)) == nid);
    }
    RequireThrows([] { OBJ_sn2nid_nid_postfix(nullptr); });
    return TestAddedObjects() ? 0 : 1;
}
