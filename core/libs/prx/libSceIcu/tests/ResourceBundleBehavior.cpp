#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/ResourceBundleOracle.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

using IcuRecoveredTest::Require;

struct Package {
    std::vector<std::max_align_t> storage;
    explicit Package(const char* path) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        Require(bool(input), "open official resource fixture package");
        const auto size = input.tellg();
        Require(size > 0, "fixture package has data");
        storage.resize((static_cast<std::size_t>(size) + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(storage.data()), size);
        Require(bool(input), "read complete aligned resource package");
        std::int32_t status = 0;
        ResourceOracleRegister(storage.data(), &status);
        Require(status <= 0, "native oracle registers the actual package");
        status = 0;
        udata_setCommonData(storage.data(), &status);
        Require(status <= 0, "guest data registration installs the actual package");
        udata_setFileAccess(ResourceOracleNoFiles(), nullptr);
    }
    ~Package() { u_cleanup_nid_postfix(); ResourceOracleCleanup(); }
};

struct Bundle {
    UResourceBundle* guest;
    void* native;
    explicit Bundle(const char* name = "aps5ResourceFixture", bool direct = true, const char* path = nullptr) {
        std::int32_t status = 0;
        std::int32_t expected = 0;
        native = ResourceOracleOpen(path, name, direct, &expected);
        std::fputs("resource fixture: valid bundle open\n", stderr);
        try { guest = direct ? ures_openDirect_nid_postfix(path, name, &status) : ures_open(path, name, &status); }
        catch (...) { ResourceOracleClose(native); throw; }
        Require(guest && native && status == expected && status <= 0, "explicit resource bundle opens with native status");
    }
    Bundle(const Bundle& parent, const char* key, std::int32_t index = 0, bool byKey = true) {
        std::int32_t status = 0;
        std::int32_t expected = 0;
        native = ResourceOracleChild(parent.native, key, index, byKey, nullptr, &expected);
        try { guest = byKey ? ures_getByKey(parent.guest, key, nullptr, &status) : ures_getByIndex_nid_postfix(parent.guest, index, nullptr, &status); }
        catch (...) { ResourceOracleClose(native); throw; }
        Require(bool(guest) == bool(native) && status == expected, "key/index child creation matches native result and status");
    }
    ~Bundle() { ures_close(guest); ResourceOracleClose(native); }
    void Close() { ures_close(guest); ResourceOracleClose(native); guest = nullptr; native = nullptr; }
    void Metadata() const {
        Require(ures_getSize_nid_postfix(guest) == ResourceOracleSize(native), "resource size matches native");
        Require(ures_getType_nid_postfix(guest) == ResourceOracleType(native), "resource type matches native");
        const auto* actual = ures_getKey_nid_postfix(guest);
        const auto* expected = ResourceOracleKey(native);
        Require(bool(actual) == bool(expected), "resource key nullability matches native");
        if (actual) Require(std::string_view(actual) == expected, "resource key text matches native");
    }
};

template<class Function>
void Refuses(Function&& function) {
    bool refused = false;
    try { function(); } catch (const std::exception&) { refused = true; }
    Require(refused, "unusable memory, foreign or stale handle is refused");
}

const char16_t* GuestString(const UResourceBundle* handle, int mode, const char* key, int index, std::int32_t* length, std::int32_t* status) {
    if (mode == 0) return ures_getString_nid_postfix(handle, length, status);
    if (mode == 1) return ures_getStringByKey(handle, key, length, status);
    return ures_getStringByIndex_nid_postfix(handle, index, length, status);
}

void String(const Bundle& bundle, int mode = 0, const char* key = nullptr, int index = 0, std::int32_t incoming = 0, bool nullLength = false) {
    auto status = incoming;
    auto expected = incoming;
    std::int32_t length = 0x55;
    std::int32_t nativeLength = 0x55;
    const auto* wanted = ResourceOracleString(bundle.native, mode, key, index, nullLength ? nullptr : &nativeLength, &expected);
    const auto* actual = GuestString(bundle.guest, mode, key, index, nullLength ? nullptr : &length, &status);
    Require(bool(actual) == bool(wanted) && length == nativeLength && status == expected, "string result, optional length and status match native");
    if (actual) Require((nullLength ? std::u16string_view(actual) : std::u16string_view(actual, length)) ==
        (nullLength ? std::u16string_view(wanted) : std::u16string_view(wanted, nativeLength)), "entire resource string matches native");
}

void Value(const Bundle& bundle, int operation, std::int32_t incoming = 0, bool nullLength = false) {
    auto status = incoming;
    auto expected = incoming;
    std::int32_t length = 0x66;
    std::int32_t nativeLength = 0x66;
    if (operation == 0) {
        const auto actual = ures_getInt_nid_postfix(bundle.guest, &status);
        const auto wanted = ResourceOracleInt(bundle.native, &expected);
        Require(actual == wanted && status == expected, "signed integer result and type errors match native");
    } else if (operation == 1) {
        const auto* actual = ures_getBinary_nid_postfix(bundle.guest, nullLength ? nullptr : &length, &status);
        const auto* wanted = ResourceOracleBinary(bundle.native, nullLength ? nullptr : &nativeLength, &expected);
        Require(bool(actual) == bool(wanted) && length == nativeLength && status == expected, "binary result, length and status match native");
        if (actual && !nullLength) Require(std::equal(actual, actual + length, wanted), "all binary bytes match native");
    } else {
        const auto* actual = ures_getIntVector_nid_postfix(bundle.guest, nullLength ? nullptr : &length, &status);
        const auto* wanted = ResourceOracleVector(bundle.native, nullLength ? nullptr : &nativeLength, &expected);
        Require(bool(actual) == bool(wanted) && length == nativeLength && status == expected, "integer-vector result, length and status match native");
        if (actual && !nullLength) Require(std::equal(actual, actual + length, wanted), "entire integer vector matches native");
    }
}

void Literals() {
    Bundle root;
    root.Metadata();
    Require(ures_getSize_nid_postfix(root.guest) == 9 && ures_getType_nid_postfix(root.guest) == 2, "fixed real resource-table metadata");
    std::int32_t status = 0;
    std::int32_t length = -1;
    const auto* greeting = ures_getStringByKey(root.guest, "greeting", &length, &status);
    Require(greeting && status <= 0 && std::u16string_view(greeting, length) == u"h\u00e9llo \u4e16\u754c", "fixed Unicode string");
    Bundle number(root, "number");
    Require(ures_getInt_nid_postfix(number.guest, &status) == 1234 && status <= 0, "fixed signed integer");
    Bundle negative(root, "negative");
    Require(ures_getInt_nid_postfix(negative.guest, &status) == -7 && status <= 0, "fixed negative resource integer sign extension");
    Bundle binary(root, "bytes");
    const auto* bytes = ures_getBinary_nid_postfix(binary.guest, &length, &status);
    Require(bytes && status <= 0 && length == 4 && bytes[0] == 0 && bytes[1] == 0x10 && bytes[2] == 0xff && bytes[3] == 0, "fixed binary bytes including embedded zero");
    Bundle vector(root, "vector");
    const auto* integers = ures_getIntVector_nid_postfix(vector.guest, &length, &status);
    Require(integers && status <= 0 && length == 3 && integers[0] == 1 && integers[1] == -2 && integers[2] == 17, "fixed integer vector");
    Bundle systems("numberingSystems");
    Bundle table(systems, "numberingSystems");
    Bundle latin(table, "latn");
    const auto* digits = ures_getStringByKey(latin.guest, "desc", &length, &status);
    Require(digits && status <= 0 && std::u16string_view(digits, length) == u"0123456789", "real production CLDR resource remains available");
    Bundle locale("en_US", false);
    Bundle elements(locale, "NumberElements");
    Require(ures_getType_nid_postfix(elements.guest) == 2, "normal locale fallback resolves actual NumberElements table");
    for (const auto* key : {"greeting", "values", "nested", "number", "negative", "bytes", "vector", "empty", "alias", "missing"}) {
        Bundle child(root, key);
        child.Metadata();
        for (auto incoming : {0, -1, -124, 7}) {
            String(child, 0, nullptr, 0, incoming);
            for (auto operation : {0, 1, 2}) Value(child, operation, incoming);
        }
        String(root, 1, key);
        String(root, 1, key, 0, 0, true);
    }
    Bundle empty(root, "empty");
    Value(empty, 1, 0, true);
    Bundle values(root, "values");
    for (int index : {-1, 0, 1, 2, 3, 0x7fffffff}) {
        String(values, 2, nullptr, index);
        String(values, 2, nullptr, index, 0, true);
    }
}

void FillIn() {
    Bundle root;
    Bundle fill(root, "number");
    std::set<std::string> keys;
    for (int index = -1; index <= 9; ++index) {
        std::int32_t status = 0;
        std::int32_t expected = 0;
        auto* wanted = ResourceOracleChild(root.native, nullptr, index, false, fill.native, &expected);
        auto* actual = ures_getByIndex_nid_postfix(root.guest, index, fill.guest, &status);
        Require(actual == fill.guest && wanted == fill.native && status == expected, "index success and errors preserve supplied fillIn identity");
        fill.Metadata();
        if (status <= 0) {
            const auto* key = ures_getKey_nid_postfix(fill.guest);
            Require(key && keys.emplace(key).second, "full indexed table walk returns distinct keys");
        }
    }
    Require(keys.size() == 9, "every actual table key is visited");
    for (const auto* key : {"greeting", "nested", "missing", "number"}) {
        std::int32_t status = 0;
        std::int32_t expected = 0;
        auto* wanted = ResourceOracleChild(root.native, key, 0, true, fill.native, &expected);
        auto* actual = ures_getByKey(root.guest, key, fill.guest, &status);
        Require(actual == fill.guest && wanted == fill.native && status == expected, "key success and errors preserve supplied fillIn identity");
        fill.Metadata();
    }
    std::int32_t status = 0;
    std::int32_t expected = 0;
    Require(ures_getByKey(root.guest, "nested", root.guest, &status) == root.guest &&
        ResourceOracleChild(root.native, "nested", 0, true, root.native, &expected) == root.native && status == expected,
        "parent can be its own fillIn without identity change or deadlock");
    root.Metadata();
    String(root, 1, "word");
    Bundle scalar("aps5ResourceFixture");
    Bundle number(scalar, "number");
    for (int index : {-1, 0, 1}) {
        Bundle child(number, nullptr, index, false);
        child.Metadata();
        String(number, 2, nullptr, index);
    }
}

void Status() {
    auto* invalid = reinterpret_cast<UResourceBundle*>(std::uintptr_t{1});
    auto* fill = reinterpret_cast<UResourceBundle*>(std::uintptr_t{2});
    const auto* key = reinterpret_cast<const char*>(std::uintptr_t{1});
    auto* length = reinterpret_cast<std::int32_t*>(std::uintptr_t{1});
    for (int incoming : {0, 1, 7, 15}) {
        auto failed = incoming;
        auto* status = incoming ? &failed : nullptr;
        Require(ures_open(key, key, status) == nullptr, "null/failed status precedes open inputs");
        Require(ures_openDirect_nid_postfix(key, key, status) == nullptr, "null/failed status precedes direct-open inputs");
        Require(ures_getByKey(invalid, key, fill, status) == fill, "null/failed status returns untouched foreign fillIn for key lookup");
        Require(ures_getByIndex_nid_postfix(invalid, -1, fill, status) == fill, "null/failed status returns untouched foreign fillIn for index lookup");
        Require(ures_getString_nid_postfix(invalid, length, status) == nullptr && ures_getStringByKey(invalid, key, length, status) == nullptr &&
            ures_getStringByIndex_nid_postfix(invalid, -1, length, status) == nullptr, "null/failed status precedes string handles and lengths");
        Require(ures_getBinary_nid_postfix(invalid, length, status) == nullptr && ures_getIntVector_nid_postfix(invalid, length, status) == nullptr &&
            ures_getInt_nid_postfix(invalid, status) == -1, "null/failed status precedes scalar/binary/vector access");
        Require(failed == incoming, "failed status remains untouched");
    }
    Require(ures_getSize_nid_postfix(nullptr) == 0 && ures_getType_nid_postfix(nullptr) == -1 && ures_getKey_nid_postfix(nullptr) == nullptr,
        "stateless null-resource getters preserve native return shapes");
    std::int32_t status = 0;
    std::int32_t outputLength = 0x55;
    Require(ures_getByKey(nullptr, key, fill, &status) == fill && status == 1, "null parent reports argument error before key/fillIn access");
    status = 0;
    Require(ures_getByIndex_nid_postfix(nullptr, -1, fill, &status) == fill && status == 1, "null index parent reports argument error before fillIn access");
    status = 0;
    Require(ures_getString_nid_postfix(nullptr, &outputLength, &status) == nullptr && status == 1 && outputLength == 0x55, "null value source leaves unused length untouched");
    Bundle root;
    for (auto incoming : {-1, -124, -128}) String(root, 1, "greeting", 0, incoming);
    status = 0;
    Refuses([&] { ures_open(nullptr, nullptr, &status); });
    status = 0;
    Refuses([&] { ures_openDirect_nid_postfix(nullptr, nullptr, &status); });
    Refuses([&] { ures_getByKey(root.guest, "number", invalid, &status); });
    ures_close(nullptr);
}

void Lifetime() {
    Bundle root;
    Bundle child(root, "greeting");
    root.Close();
    String(child);
    std::int32_t status = 0;
    auto* closed = ures_openDirect_nid_postfix(nullptr, "aps5ResourceFixture", &status);
    for (int phase = 0; phase < 2; ++phase) {
        auto* enumeration = reinterpret_cast<UEnumeration*>(closed);
        Refuses([&] { uenum_count_nid_postfix(enumeration, &status); });
        Refuses([&] { uenum_next_nid_postfix(enumeration, nullptr, &status); });
        Refuses([&] { uenum_unext_nid_postfix(enumeration, nullptr, &status); });
        Refuses([&] { uenum_close_nid_postfix(enumeration); });
        if (phase == 0) ures_close(closed);
    }
    for (int index = 0; index < 128; ++index) {
        auto* fresh = ures_openDirect_nid_postfix(nullptr, "aps5ResourceFixture", &status);
        Require(fresh && fresh != closed, "closed resource identities are not reused");
        Refuses([&] { ures_getType_nid_postfix(closed); });
        ures_close(fresh);
    }
    auto* numbering = unumsys_openByName_nid_postfix("latn", &status);
    Refuses([&] { ures_getSize_nid_postfix(reinterpret_cast<UResourceBundle*>(numbering)); });
    Refuses([&] { unumsys_getName_nid_postfix(reinterpret_cast<UNumberingSystem*>(child.guest)); });
    unumsys_close_nid_postfix(numbering);
}

void Memory() {
    Bundle root;
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 3;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* address = nullptr;
    Require(sceKernelMapDirectMemory(&address, bytes, 3, 0, physical, 0) == 0);
    auto* data = static_cast<char*>(address);
    Require(sceKernelMprotect(data, page, 0) == 0);
    Require(sceKernelMprotect(data + page * 2, page, 0) == 0);
    auto* key = data + page * 2 - 7;
    std::memcpy(key, "number", 7);
    auto* status = reinterpret_cast<std::int32_t*>(data + page);
    *status = 0;
    auto* number = ures_getByKey(root.guest, key, nullptr, status);
    Require(number && *status <= 0 && ures_getInt_nid_postfix(number, status) == 1234, "NUL key ends at readable page boundary");
    ures_close(number);
    std::fill_n(key, 7, 'x');
    Refuses([&] { ures_getByKey(root.guest, key, nullptr, status); });
    Refuses([&] { ures_getType_nid_postfix(reinterpret_cast<UResourceBundle*>(data)); });
    auto* length = reinterpret_cast<std::int32_t*>(data + page + 4);
    *length = 0x55;
    const auto* text = ures_getStringByKey(root.guest, "greeting", length, status);
    Require(text && *length == 8 && *status <= 0, "mapped exact resultLength write");
    Require(sceKernelMprotect(data + page, page, 1) == 0);
    Refuses([&] { ures_getStringByKey(root.guest, "greeting", length, status); });
    Refuses([&] { ures_getInt_nid_postfix(root.guest, status); });
    Require(sceKernelMprotect(data + page, page, 3) == 0);
    *status = 7;
    Require(sceKernelMprotect(data + page, page, 1) == 0);
    auto* fill = reinterpret_cast<UResourceBundle*>(std::uintptr_t{2});
    Require(ures_getByKey(reinterpret_cast<UResourceBundle*>(std::uintptr_t{1}), data, fill, status) == fill && *status == 7,
        "read-only failed status returns fillIn before guarded memory");
    Require(sceKernelMprotect(data + page, page, 3) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
}

} // namespace

int main(int argc, char** argv) {
    try {
        Require(argc == 3, "fixture package and resource test mode");
        Package package(argv[1]);
        const std::string_view mode = argv[2];
        if (mode == "literals") Literals();
        else if (mode == "fillin") FillIn();
        else if (mode == "status") Status();
        else if (mode == "lifetime") Lifetime();
        else if (mode == "memory") Memory();
        else Require(false, "unknown resource test mode");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "resource fixture failed: %s\n", error.what());
        return 1;
    }
}
