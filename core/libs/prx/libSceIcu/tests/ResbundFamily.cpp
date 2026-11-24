#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ures.h"
#include "unicode/uenum.h"
#include "uresimp.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("icu resbund family: " + message);
}

bool Throws(const std::function<void()>& body) {
    try {
        body();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

constexpr std::size_t HeaderSize = 32;
constexpr std::uint32_t TableType = 2;
constexpr std::uint32_t TypeShift = 28;

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MissingResource = 2;
constexpr std::int32_t TypeMismatch = 17;

void Put16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void PutHeader(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view format) {
    Put16(bytes, offset, HeaderSize);
    bytes[offset + 2] = 0xda;
    bytes[offset + 3] = 0x27;
    Put16(bytes, offset + 4, 20);
    bytes[offset + 8] = U_IS_BIG_ENDIAN;
    bytes[offset + 9] = U_CHARSET_FAMILY;
    bytes[offset + 10] = U_SIZEOF_UCHAR;
    std::memcpy(bytes.data() + offset + 12, format.data(), 4);
    bytes[offset + 16] = 1;
}

std::size_t PutKey(std::vector<std::uint8_t>& bytes, std::size_t root, std::size_t& cursor, std::string_view key) {
    const auto offset = cursor - root;
    std::memcpy(bytes.data() + cursor, key.data(), key.size());
    cursor += key.size() + 1;
    return offset;
}

std::vector<std::uint8_t> TestIndex(const std::vector<std::string_view>& locales) {
    const std::string name = U_ICUDATA_NAME "/coll/res_index.res";
    std::vector<std::uint8_t> bytes(1024);
    PutHeader(bytes, 0, "CmnD");
    const std::size_t toc = HeaderSize;
    const std::size_t item = (toc + 12 + name.size() + 1 + 15) / 16 * 16;
    Put32(bytes, toc, 1);
    Put32(bytes, toc + 4, 12);
    Put32(bytes, toc + 8, static_cast<std::uint32_t>(item - toc));
    std::memcpy(bytes.data() + toc + 12, name.data(), name.size());
    PutHeader(bytes, item, "ResB");
    const std::size_t root = item + HeaderSize;
    std::size_t cursor = root + 4;
    const auto installedKey = PutKey(bytes, root, cursor, "InstalledLocales");
    std::vector<std::size_t> keys;
    for (const auto locale : locales) keys.push_back(PutKey(bytes, root, cursor, locale));
    cursor = (cursor + 3) / 4 * 4;
    const auto installed = cursor;
    Put16(bytes, cursor, static_cast<std::uint16_t>(locales.size()));
    cursor += 2;
    for (const auto key : keys) {
        Put16(bytes, cursor, static_cast<std::uint16_t>(key));
        cursor += 2;
    }
    cursor = (cursor + 3) / 4 * 4 + 4 * locales.size();
    const auto top = cursor;
    Put16(bytes, cursor, 1);
    Put16(bytes, cursor + 2, static_cast<std::uint16_t>(installedKey));
    Put32(bytes, cursor + 4, (TableType << TypeShift) | static_cast<std::uint32_t>((installed - root) / 4));
    Put32(bytes, root, (TableType << TypeShift) | static_cast<std::uint32_t>((top - root) / 4));
    Require(cursor + 8 <= bytes.size(), "the test data does not fit its buffer");
    return bytes;
}

void* GuestPages = nullptr;
std::size_t PageSize = 0;

void SetUpPages() {
#ifdef _WIN32
    PageSize = 4096;
    GuestPages = VirtualAlloc(nullptr, PageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(GuestPages != nullptr, "reserve guest pages");
    DWORD oldProtection = 0;
    Require(VirtualProtect(static_cast<char*>(GuestPages) + PageSize, PageSize, PAGE_NOACCESS, &oldProtection) != 0, "protect the guard page");
#else
    PageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    GuestPages = mmap(nullptr, PageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(GuestPages != MAP_FAILED, "map guest pages");
    Require(mprotect(static_cast<char*>(GuestPages) + PageSize, PageSize, PROT_NONE) == 0, "protect the guard page");
#endif
}

void* GuestBuffer(std::size_t bytes) {
    return static_cast<std::uint8_t*>(GuestPages) + PageSize - bytes;
}

std::size_t GuestCursor = 0;

char* GuestString(const char* text) {
    auto* buffer = static_cast<char*>(GuestPages) + GuestCursor;
    std::strcpy(buffer, text);
    GuestCursor += std::strlen(buffer) + 1;
    return buffer;
}

std::string Utf8(const char16_t* text, std::int32_t length) {
    std::string out;
    for (std::int32_t i = 0; i < length; ++i) out += static_cast<char>(static_cast<unsigned char>(text[i]));
    return out;
}

const char* Package = U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll";
const char* Item = "res_index";

std::int32_t Status(int status) { return static_cast<std::int32_t>(status); }

void GuardAxis() {
    Require(Throws([&] {
        UResourceBundle fill{};
        ures_initStackObject(&fill);
        ures_openDirectFillIn_nid_postfix(&fill, nullptr, nullptr, nullptr);
    }),
        "openDirectFillIn throws on a null status (bundled ICU dereferences it)");
    Require(Throws([&] {
        ures_findResource_nid_postfix(nullptr, static_cast<UResourceBundle*>(GuestBuffer(sizeof(UResourceBundle))),
            static_cast<std::int32_t*>(GuestBuffer(4)));
    }),
        "findResource throws on a null path (bundled ICU dereferences it)");
    Require(Throws([&] {
        char path[] = "x";
        ures_findSubResource_nid_postfix(nullptr, path, static_cast<UResourceBundle*>(GuestBuffer(sizeof(UResourceBundle))),
            static_cast<std::int32_t*>(GuestBuffer(4)));
    }),
        "findSubResource throws on a null bundle (bundled ICU dereferences it)");
    Require(Throws([&] {
        UResourceBundle fill{};
        ures_initStackObject(&fill);
        ures_getStringByKeyWithFallback_nid_postfix(&fill, nullptr, static_cast<std::int32_t*>(GuestBuffer(4)),
            static_cast<std::int32_t*>(GuestBuffer(4)));
    }),
        "getStringByKeyWithFallback throws on a null key (bundled ICU dereferences it)");

    std::int32_t status = 0;
    std::uint8_t version[4] = { 9, 9, 9, 9 };
    ures_getVersionByKey_nid_postfix(nullptr, "InstalledLocales", version, &status);
    Require(status == IllegalArgument, "a null bundle passes through and ICU reports its documented error");
    Require(version[0] == 9 && version[1] == 9 && version[2] == 9 && version[3] == 9, "a rejected lookup leaves the buffer alone");
}

} // namespace

static_assert(std::is_same_v<decltype(&ures_openNoDefault_nid_postfix),
                             UResourceBundle* (APS5_VABI*)(const char*, const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ures_openFillIn_nid_postfix),
                             void (APS5_VABI*)(UResourceBundle*, const char*, const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ures_openDirectFillIn_nid_postfix),
                             void (APS5_VABI*)(UResourceBundle*, const char*, const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ures_findResource_nid_postfix),
                             UResourceBundle* (APS5_VABI*)(const char*, UResourceBundle*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ures_findSubResource_nid_postfix),
                             UResourceBundle* (APS5_VABI*)(const UResourceBundle*, char*, UResourceBundle*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ures_getKeywordValues_nid_postfix),
                             UEnumeration* (APS5_VABI*)(const char*, const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ures_getStringByKeyWithFallback_nid_postfix),
                             const char16_t* (APS5_VABI*)(const UResourceBundle*, const char*, std::int32_t*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ures_getVersionByKey_nid_postfix),
                             void (APS5_VABI*)(const UResourceBundle*, const char*, std::uint8_t*, std::int32_t*)>);

int main(int argc, char** argv) {
    Require(argc == 2, "expected one argument: empty or data");
    SetUpPages();

    alignas(16) static std::uint8_t storage[1024];
    bool installed = false;

    if (std::string_view(argv[1]) == "data") {
        auto image = TestIndex({ "de", "en_US" });
        std::memcpy(storage, image.data(), image.size());

        std::int32_t oursInstall = 0;
        udata_setCommonData_nid_postfix(storage, &oursInstall);
        Require(oursInstall == 0, "install through the export, status " + std::to_string(oursInstall));
        UErrorCode controlInstall = U_ZERO_ERROR;
        udata_setCommonData(storage, &controlInstall);
        Require(static_cast<int>(controlInstall) == 0, "install through bundled ICU, status " + std::to_string(static_cast<int>(controlInstall)));
        installed = true;
    }

    {
        UResourceBundle controlFill{};
        UResourceBundle oursFill{};
        ures_initStackObject(&controlFill);
        ures_initStackObject(&oursFill);
        std::int32_t oursStatus = 0;
        ures_openDirectFillIn_nid_postfix(&oursFill, GuestString(Package), GuestString(Item), &oursStatus);
        UErrorCode controlStatus = U_ZERO_ERROR;
        ures_openDirectFillIn(&controlFill, const_cast<char*>(Package), const_cast<char*>(Item), &controlStatus);
        const std::int32_t expected = installed ? 0 : MissingResource;
        Require(oursStatus == expected, "our openDirectFillIn status " + std::to_string(oursStatus) + ", expected " + std::to_string(expected));
        Require(static_cast<int>(controlStatus) == expected, "bundled openDirectFillIn status " + std::to_string(static_cast<int>(controlStatus)));
    }

    {
        UResourceBundle controlFill{};
        UResourceBundle oursFill{};
        ures_initStackObject(&controlFill);
        ures_initStackObject(&oursFill);
        std::int32_t oursStatus = 0;
        ures_openFillIn_nid_postfix(&oursFill, GuestString(Package), GuestString(Item), &oursStatus);
        UErrorCode controlStatus = U_ZERO_ERROR;
        ures_openFillIn(&controlFill, const_cast<char*>(Package), const_cast<char*>(Item), &controlStatus);
        Require(oursStatus == static_cast<int>(controlStatus), "the non-direct open paths report the same status on both sides");
    }

    {
        UResourceBundle oursBundle{};
        ures_initStackObject(&oursBundle);
        std::int32_t oursOpen = 0;
        ures_openDirectFillIn_nid_postfix(&oursBundle, GuestString(Package), GuestString(Item), &oursOpen);
        Require(oursOpen == (installed ? 0 : MissingResource), "our bundle for the key lookups, status " + std::to_string(oursOpen));

        std::int32_t oursLength = -1;
        std::int32_t oursStatus = 0;
        const char16_t* oursText = ures_getStringByKeyWithFallback_nid_postfix(&oursBundle, "InstalledLocales", &oursLength, &oursStatus);

        UErrorCode controlStatus = U_ZERO_ERROR;
        UResourceBundle controlBundle{};
        ures_initStackObject(&controlBundle);
        ures_openDirectFillIn(&controlBundle, const_cast<char*>(Package), const_cast<char*>(Item), &controlStatus);
        int controlLength = -1;
        const char16_t* controlText = ures_getStringByKey(&controlBundle, "InstalledLocales", &controlLength, &controlStatus);

        if (installed) {
            Require(oursStatus == TypeMismatch, "our table-key read status " + std::to_string(oursStatus));
            Require(controlStatus == TypeMismatch, "bundled table-key read status " + std::to_string(static_cast<int>(controlStatus)));
            Require(oursText == nullptr && controlText == nullptr, "a table key yields no string on either side");
        }
    }

    {
        char oursPath[] = "InstalledLocales/de";
        char controlPath[] = "InstalledLocales/de";
        UResourceBundle oursSub{};
        UResourceBundle controlSub{};
        ures_initStackObject(&oursSub);
        ures_initStackObject(&controlSub);
        UResourceBundle oursBundle{};
        UResourceBundle controlBundle{};
        ures_initStackObject(&oursBundle);
        ures_initStackObject(&controlBundle);
        std::int32_t oursStatus = 0;
        ures_openDirectFillIn_nid_postfix(&oursBundle, GuestString(Package), GuestString(Item), &oursStatus);
        UErrorCode controlStatus = U_ZERO_ERROR;
        ures_openDirectFillIn(&controlBundle, const_cast<char*>(Package), const_cast<char*>(Item), &controlStatus);

        if (installed) {
            ures_findSubResource_nid_postfix(&oursBundle, oursPath, &oursSub, &oursStatus);
            Require(oursStatus == 0, "our findSubResource status " + std::to_string(oursStatus));
            ures_findSubResource(&controlBundle, controlPath, &controlSub, &controlStatus);
            Require(static_cast<int>(controlStatus) == 0, "bundled findSubResource status " + std::to_string(static_cast<int>(controlStatus)));
            Require(std::string_view(oursPath) == std::string_view(controlPath), "both sides see the same truncation");
            Require(oursPath[16] == 0 && controlPath[16] == 0, "the path buffer is terminated in place at the separator");
        }
    }

    {
        std::uint8_t oursVersion[4] = { 9, 9, 9, 9 };
        std::uint8_t controlVersion[4] = { 9, 9, 9, 9 };
        std::int32_t oursStatus = 0;
        UErrorCode controlStatus = U_ZERO_ERROR;
        ures_getVersionByKey_nid_postfix(nullptr, "InstalledLocales", oursVersion, &oursStatus);
        ures_getVersionByKey(nullptr, "InstalledLocales", controlVersion, &controlStatus);
        Require(oursStatus == static_cast<int>(controlStatus), "the version lookup reports the same status on both sides");
        Require(std::memcmp(oursVersion, controlVersion, 4) == 0, "the version buffer is written the same way");
    }

    {
        std::int32_t oursStatus = 0;
        UErrorCode controlStatus = U_ZERO_ERROR;
        UEnumeration* oursEnum = ures_getKeywordValues_nid_postfix(GuestString(Package), "InstalledLocales", &oursStatus);
        UEnumeration* controlEnum = ures_getKeywordValues(Package, "InstalledLocales", &controlStatus);
        Require((oursEnum != nullptr) == (controlEnum != nullptr), "both sides agree on the enumeration");
        Require(oursStatus == static_cast<int>(controlStatus), "both sides report the same enumeration status");
    }

    GuardAxis();

    return 0;
}
