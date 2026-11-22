#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <type_traits>

namespace {

constexpr std::int32_t IllegalArgument = 1;

static_assert(std::is_same_v<decltype(&ures_getVersionNumberInternal_nid_postfix), const char* (APS5_VABI*)(const UResourceBundle*)>);
static_assert(std::is_same_v<decltype(&ures_getVersionNumber_nid_postfix), const char* (APS5_VABI*)(const UResourceBundle*)>);
static_assert(std::is_same_v<decltype(&ures_getVersion_nid_postfix), void (APS5_VABI*)(const UResourceBundle*, std::uint8_t*)>);
static_assert(std::is_same_v<decltype(&ures_getVersionByKey_nid_postfix), void (APS5_VABI*)(const UResourceBundle*, const char*, std::uint8_t*, std::int32_t*)>);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU resource version: %s\n", message);
    std::abort();
}

template <typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

void CheckNullArguments() {
    Require(ures_getVersionNumberInternal_nid_postfix(nullptr) == nullptr, "null bundle internal version");
    Require(ures_getVersionNumber_nid_postfix(nullptr) == nullptr, "null bundle version number");

    std::uint8_t version[4] = {0xaa, 0xbb, 0xcc, 0xdd};
    errno = 73;
    ures_getVersion_nid_postfix(nullptr, version);
    Require(version[0] == 0xaa && version[1] == 0xbb && version[2] == 0xcc && version[3] == 0xdd, "null bundle version untouched");
    Require(errno == 73, "null bundle version errno");

    ures_getVersion_nid_postfix(nullptr, nullptr);
    Require(errno == 73, "null both version errno");

    auto* unmapped = reinterpret_cast<std::uint8_t*>(std::uintptr_t{0x10});
    ures_getVersion_nid_postfix(nullptr, unmapped);
    Require(errno == 73, "unmapped version pointer errno");

    std::int32_t status = 0;
    ures_getVersionByKey_nid_postfix(nullptr, "Version", version, &status);
    Require(status == IllegalArgument, "null bundle version by key status");
    Require(version[0] == 0xaa, "null bundle version by key output untouched");

    status = 0;
    ures_getVersionByKey_nid_postfix(nullptr, nullptr, nullptr, &status);
    Require(status == IllegalArgument, "null bundle null args status");

    status = 17;
    ures_getVersionByKey_nid_postfix(nullptr, "Version", version, &status);
    Require(status == 17, "pre-failed status preserved");
    Require(version[0] == 0xaa, "pre-failed output untouched");

    RequireThrows([&] { ures_getVersionByKey_nid_postfix(nullptr, "Version", version, nullptr); }, "null status throws");
    auto* unmappedStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    RequireThrows([&] { ures_getVersionByKey_nid_postfix(nullptr, "Version", version, unmappedStatus); }, "unmapped status throws");
}

void CheckWildAndNullVersionHandling() {
    std::uint8_t version[4] = {0x11, 0x22, 0x33, 0x44};
    auto* wildBundle = reinterpret_cast<const UResourceBundle*>(std::uintptr_t{0x10});

    RequireThrows([&] { ures_getVersionNumberInternal_nid_postfix(wildBundle); }, "wild bundle internal throws");
    RequireThrows([&] { ures_getVersionNumber_nid_postfix(wildBundle); }, "wild bundle number throws");
    RequireThrows([&] { ures_getVersion_nid_postfix(wildBundle, version); }, "wild bundle version throws");
    RequireThrows([&] {
        std::int32_t status = 0;
        ures_getVersionByKey_nid_postfix(wildBundle, "Version", version, &status);
    }, "wild bundle get by key throws");

    std::array<std::uint8_t, 200> bundleStorage{};
    auto* bundle = reinterpret_cast<UResourceBundle*>(bundleStorage.data());
    ures_initStackObject_nid_postfix(bundle);

    RequireThrows([&] { ures_getVersionNumberInternal_nid_postfix(bundle); }, "stack bundle null fVersion internal throws");
    RequireThrows([&] { ures_getVersionNumber_nid_postfix(bundle); }, "stack bundle null fVersion number throws");
    RequireThrows([&] { ures_getVersion_nid_postfix(bundle, version); }, "stack bundle null fVersion version throws");
    RequireThrows([&] {
        std::int32_t status = 0;
        ures_getVersionByKey_nid_postfix(bundle, "Version", version, &status);
    }, "stack bundle get by key throws");

    std::int32_t preFailedStatus = 17;
    ures_getVersionByKey_nid_postfix(bundle, "Version", version, &preFailedStatus);
    Require(preFailedStatus == 17, "pre-failed status preserved on non-null bundle");
    Require(version[0] == 0x11, "pre-failed output untouched on non-null bundle");

    const char versionStr[] = "68.2.0.0";
    const char* verPtr = versionStr;
    std::memcpy(bundleStorage.data() + 16, &verPtr, sizeof(verPtr));

    const char* internalNum = ures_getVersionNumberInternal_nid_postfix(bundle);
    Require(internalNum != nullptr, "valid bundle internal version");
    Require(std::strcmp(internalNum, "68.2.0.0") == 0, "valid bundle internal version string matches");

    const char* verNum = ures_getVersionNumber_nid_postfix(bundle);
    Require(verNum != nullptr, "valid bundle version number");
    Require(std::strcmp(verNum, "68.2.0.0") == 0, "valid bundle version number matches");

    std::uint8_t parsedVer[4] = {0xff, 0xff, 0xff, 0xff};
    ures_getVersion_nid_postfix(bundle, parsedVer);
    Require(parsedVer[0] == 68 && parsedVer[1] == 2 && parsedVer[2] == 0 && parsedVer[3] == 0, "parsed version fields");

    auto* unmapped = reinterpret_cast<std::uint8_t*>(std::uintptr_t{0x10});
    RequireThrows([&] { ures_getVersion_nid_postfix(bundle, nullptr); }, "null versionInfo throws");
    RequireThrows([&] { ures_getVersion_nid_postfix(bundle, unmapped); }, "unmapped versionInfo throws");

    std::array<std::uint8_t, 200> badBundleStorage{};
    auto* badBundle = reinterpret_cast<UResourceBundle*>(badBundleStorage.data());
    const char* badStr = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    std::memcpy(badBundleStorage.data() + 16, &badStr, sizeof(badStr));
    RequireThrows([&] { ures_getVersion_nid_postfix(badBundle, parsedVer); }, "unmapped version string throws");
}

}

int main() {
    CheckNullArguments();
    CheckWildAndNullVersionHandling();
}
