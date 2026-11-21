#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/putil.h"
#include "unicode/utypes.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu udata_setAppData: " + message);
}

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename Call>
bool NotImplemented(Call call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct Item {
    std::string name;
    std::string format;
    std::size_t length;
    std::uint8_t seed;
};

struct Package {
    std::uint8_t* bytes = nullptr;
    std::vector<const std::uint8_t*> payloads;
};

void Put16(std::uint8_t* at, std::uint16_t value) {
    std::memcpy(at, &value, sizeof(value));
}

void Put32(std::uint8_t* at, std::uint32_t value) {
    std::memcpy(at, &value, sizeof(value));
}

void PutHeader(std::uint8_t* at, const std::string& format) {
    Put16(at, 32);
    at[2] = 0xda;
    at[3] = 0x27;
    Put16(at + 4, 20);
    at[8] = U_IS_BIG_ENDIAN;
    at[9] = U_CHARSET_FAMILY;
    at[10] = 2;
    std::memcpy(at + 12, format.data(), 4);
    const std::uint8_t versions[] = {1, 2, 3, 4, 9, 8, 7, 6};
    std::memcpy(at + 16, versions, sizeof(versions));
}

Package Build(const std::vector<Item>& items, const std::string& format = "CmnD") {
    static std::vector<std::unique_ptr<std::uint64_t[]>> storage;
    constexpr std::size_t toc = 32;
    std::size_t cursor = toc + 4 + 8 * items.size();
    for (const auto& item : items) cursor += item.name.size() + 1;
    std::vector<std::size_t> offsets;
    for (const auto& item : items) {
        cursor = (cursor + 15) / 16 * 16;
        offsets.push_back(cursor);
        cursor += 32 + item.length;
    }
    const std::size_t words = cursor / 8 + 2;
    storage.push_back(std::make_unique<std::uint64_t[]>(words));
    Package package;
    package.bytes = reinterpret_cast<std::uint8_t*>(storage.back().get());
    PutHeader(package.bytes, format);
    Put32(package.bytes + toc, static_cast<std::uint32_t>(items.size()));
    std::size_t name = toc + 4 + 8 * items.size();
    for (std::size_t index = 0; index != items.size(); ++index) {
        const auto& item = items[index];
        Put32(package.bytes + toc + 4 + 8 * index, static_cast<std::uint32_t>(name - toc));
        Put32(package.bytes + toc + 8 + 8 * index, static_cast<std::uint32_t>(offsets[index] - toc));
        std::memcpy(package.bytes + name, item.name.c_str(), item.name.size() + 1);
        name += item.name.size() + 1;
        PutHeader(package.bytes + offsets[index], item.format);
        auto* payload = package.bytes + offsets[index] + 32;
        for (std::size_t byte = 0; byte != item.length; ++byte) payload[byte] = static_cast<std::uint8_t>(item.seed * 31 + byte);
        package.payloads.push_back(payload);
    }
    return package;
}

Package Single(const std::string& name, std::uint8_t seed) {
    return Build({{name + "/a.res", "Tst1", 8, seed}});
}

std::int32_t Set(const char* name, const void* data, std::int32_t status) {
    udata_setAppData_nid_postfix(name, data, &status);
    return status;
}

struct Opened {
    std::int32_t status = 0;
    const std::uint8_t* memory = nullptr;
    Icu::DataInfo info{};
};

Opened Open(const char* path, const char* type, const char* name) {
    Opened opened;
    UDataMemory* data = udata_open_nid_postfix(path, type, name, &opened.status);
    if (!data) return opened;
    opened.info.size = sizeof(opened.info);
    udata_getInfo_nid_postfix(reinterpret_cast<Icu::DataMemory*>(data), &opened.info);
    opened.memory = static_cast<const std::uint8_t*>(udata_getMemory_nid_postfix(reinterpret_cast<Icu::DataMemory*>(data)));
    udata_close_nid_postfix(data);
    return opened;
}

void RequireServed(const char* path, const char* type, const char* name, const std::uint8_t* payload, const char* format,
                   const std::string& what) {
    const auto opened = Open(path, type, name);
    Require(opened.status == U_ZERO_ERROR, what + ": udata_open status " + std::to_string(opened.status));
    Require(opened.memory == payload, what + ": udata_open served another item");
    Require(std::memcmp(opened.info.dataFormat, format, 4) == 0, what + ": udata_getInfo data format");
    const std::uint8_t versions[] = {1, 2, 3, 4, 9, 8, 7, 6};
    Require(std::memcmp(opened.info.formatVersion, versions, sizeof(versions)) == 0, what + ": udata_getInfo versions");
    Require(opened.info.size == 20 && opened.info.sizeofUChar == 2, what + ": udata_getInfo size fields");
}

void RequireMissing(const char* path, const char* type, const char* name, const std::string& what) {
    const auto opened = Open(path, type, name);
    Require(!opened.memory && opened.status == U_FILE_ACCESS_ERROR, what + ": udata_open status " + std::to_string(opened.status));
}

void CheckRegistration() {
    const auto package = Build({{"appdata/a.res", "Tst1", 8, 1}, {"appdata/b.dat", "Tst2", 16, 2}, {"appdata/c.xyz", "Tst3", 4, 3}});
    Require(Set("appdata", package.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "a valid package was refused");
    RequireServed("appdata", "res", "a", package.payloads[0], "Tst1", "a.res");
    RequireServed("appdata", "dat", "b", package.payloads[1], "Tst2", "b.dat");
    RequireServed("appdata", "xyz", "c", package.payloads[2], "Tst3", "c.xyz");
    RequireServed("appdata", nullptr, "a.res", package.payloads[0], "Tst1", "a.res without a type");
    RequireMissing("appdata", "res", "z", "an absent item");
    RequireMissing("appdata", "dat", "a", "a wrong type");
    const auto other = Build({{"appdata/a.res", "Tst9", 8, 9}});
    Require(Set("appdata", other.bytes, U_ZERO_ERROR) == U_USING_DEFAULT_WARNING, "a second package of the same name");
    RequireServed("appdata", "res", "a", package.payloads[0], "Tst1", "the first package of a name");
    Require(Set("appdata", other.bytes, U_USING_FALLBACK_WARNING) == U_USING_DEFAULT_WARNING, "a second package after a warning");
    Require(Set("appdata", other.bytes, U_ILLEGAL_ARGUMENT_ERROR) == U_ILLEGAL_ARGUMENT_ERROR, "a second package after a failure");
}

void CheckIncomingStatus() {
    const auto failed = Single("failed", 4);
    for (const std::int32_t status : {1, 3, 7, 27, 65536})
        Require(Set("failed", failed.bytes, status) == status, "a failing status changed: " + std::to_string(status));
    RequireMissing("failed", "res", "a", "a package passed with a failing status");
    const auto warned = Single("warned", 5);
    Require(Set("warned", warned.bytes, U_USING_FALLBACK_WARNING) == U_USING_FALLBACK_WARNING, "a warning was not kept");
    RequireServed("warned", "res", "a", warned.payloads[0], "Tst1", "a package passed with a warning");
    const auto below = Single("below", 6);
    Require(Set("below", below.bytes, -129) == -129, "a status below the warnings was not kept");
    RequireServed("below", "res", "a", below.payloads[0], "Tst1", "a package passed with a status below the warnings");
    Require(Set("nulldata", nullptr, U_ZERO_ERROR) == U_ILLEGAL_ARGUMENT_ERROR, "null data");
    Require(Set("nulldata", nullptr, U_USING_DEFAULT_WARNING) == U_ILLEGAL_ARGUMENT_ERROR, "null data after a warning");
    Require(Set("nulldata", nullptr, U_INVALID_FORMAT_ERROR) == U_INVALID_FORMAT_ERROR, "null data after a failure");
}

void CheckHeaders() {
    struct Change {
        const char* label;
        std::size_t offset;
        std::uint8_t value;
        bool accepted;
    };
    const Change changes[] = {{"magic1", 2, 0, false}, {"magic2", 3, 0, false}, {"isBigEndian", 8, 1, false},
                              {"charsetFamily", 9, 1, false}, {"dataFormat[0]", 12, 'X', false}, {"dataFormat[3]", 15, 'X', false},
                              {"formatVersion 0", 16, 0, false}, {"formatVersion 2", 16, 2, false},
                              {"formatVersion[1]", 17, 9, true}, {"sizeofUChar", 10, 4, true}, {"info size", 4, 0, true},
                              {"reservedByte", 11, 5, true}, {"dataVersion", 20, 0, true}};
    int index = 0;
    for (const auto& change : changes) {
        const std::string name = "header" + std::to_string(index++);
        const auto package = Single(name, static_cast<std::uint8_t>(10 + index));
        package.bytes[change.offset] = change.value;
        const auto status = Set(name.c_str(), package.bytes, U_ZERO_ERROR);
        if (change.accepted) {
            Require(status == U_ZERO_ERROR, std::string(change.label) + " was refused");
            RequireServed(name.c_str(), "res", "a", package.payloads[0], "Tst1", change.label);
        } else {
            Require(status == U_INVALID_FORMAT_ERROR, std::string(change.label) + " gave status " + std::to_string(status));
            RequireMissing(name.c_str(), "res", "a", change.label);
        }
    }
    const auto empty = Build({});
    Require(Set("emptytoc", empty.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "an empty table of contents was refused");
    RequireMissing("emptytoc", "res", "a", "an empty table of contents");
    const auto pointers = Build({}, "ToCP");
    Require(Set("pointertoc", pointers.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "a ToCP package was refused");
    RequireMissing("pointertoc", "res", "a", "an empty ToCP package");
}

void CheckNames() {
    const auto first = Single("basename", 21);
    const auto second = Single("basename", 22);
    const auto third = Single("basename", 23);
    Require(Set("dir/basename", first.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "a name with a directory");
    RequireServed("basename", "res", "a", first.payloads[0], "Tst1", "a name with a directory");
    Require(Set("dir/basename", second.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "the same name with a directory again");
    RequireServed("basename", "res", "a", second.payloads[0], "Tst1", "the same name with a directory again");
    Require(Set("basename", third.bytes, U_ZERO_ERROR) == U_USING_DEFAULT_WARNING, "the base name of a cached name");
    RequireServed("basename", "res", "a", second.payloads[0], "Tst1", "the base name of a cached name");
    RequireMissing("dir/basename", "res", "a", "a name with a directory");
    const auto empty = Build({{"/a.res", "Tst1", 8, 24}});
    Require(Set("", empty.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "an empty name");
    Require(Set("", empty.bytes, U_ZERO_ERROR) == U_USING_DEFAULT_WARNING, "an empty name again");
    const auto trailing = Single("trailing", 25);
    Require(Set("trailing/", trailing.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "a name ending in a separator");
    Require(Set("trailing/", trailing.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "a name ending in a separator again");
    const auto unnamed = Single("unnamed", 26);
    std::int32_t status = U_ZERO_ERROR;
    Require(Throws([&] { udata_setAppData_nid_postfix(nullptr, unnamed.bytes, &status); }), "a null name with a valid package");
    Require(status == U_ZERO_ERROR, "a null name with a valid package wrote the status");
    status = U_USING_FALLBACK_WARNING;
    Require(Throws([&] { udata_setAppData_nid_postfix(nullptr, unnamed.bytes, &status); }), "a null name after a warning");
    Require(Set(nullptr, nullptr, U_ZERO_ERROR) == U_ILLEGAL_ARGUMENT_ERROR, "a null name with null data");
    const auto invalid = Single("unnamed", 27);
    invalid.bytes[2] = 0;
    Require(Set(nullptr, invalid.bytes, U_ZERO_ERROR) == U_INVALID_FORMAT_ERROR, "a null name with an invalid package");
    Require(Set(nullptr, unnamed.bytes, U_ILLEGAL_ARGUMENT_ERROR) == U_ILLEGAL_ARGUMENT_ERROR, "a null name after a failure");
    udata_setAppData_nid_postfix("nullstatus", Single("nullstatus", 28).bytes, nullptr);
    RequireMissing("nullstatus", "res", "a", "a null status");
    const auto backslash = Single("back\\slash", 29);
    if constexpr (U_FILE_SEP_CHAR == '/') {
        Require(Set("back\\slash", backslash.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "a name with a backslash");
        RequireServed("back\\slash", "res", "a", backslash.payloads[0], "Tst1", "a name with a backslash");
        RequireMissing("slash", "res", "a", "the part of a name after a backslash");
    } else {
        std::int32_t backslashStatus = U_ZERO_ERROR;
        Require(NotImplemented([&] { udata_setAppData_nid_postfix("back\\slash", backslash.bytes, &backslashStatus); }),
                "a name with a backslash on a host that separates paths with it");
    }
}

void CheckIcuDataNames() {
    const std::string names[] = {"icudt68l", "dir/icudt68l", "a/b/icudt68l", U_ICUDATA_NAME, std::string("dir/") + U_ICUDATA_NAME};
    for (const auto& name : names) {
        const auto package = Build({{name + "/zzz.res", "Tst1", 8, 30}});
        std::int32_t status = U_ZERO_ERROR;
        Require(NotImplemented([&] { udata_setAppData_nid_postfix(name.c_str(), package.bytes, &status); }), name + " was registered");
        Require(status == U_ZERO_ERROR, name + " wrote the status");
        const auto base = name.substr(name.rfind('/') == std::string::npos ? 0 : name.rfind('/') + 1);
        RequireMissing(base.c_str(), "res", "zzz", name + " after it was refused");
        Require(Set(name.c_str(), nullptr, U_ZERO_ERROR) == U_ILLEGAL_ARGUMENT_ERROR, name + " with null data");
        Require(Set(name.c_str(), package.bytes, U_ILLEGAL_ARGUMENT_ERROR) == U_ILLEGAL_ARGUMENT_ERROR, name + " after a failure");
        const auto invalid = Build({{name + "/zzz.res", "Tst1", 8, 31}});
        invalid.bytes[3] = 0;
        Require(Set(name.c_str(), invalid.bytes, U_ZERO_ERROR) == U_INVALID_FORMAT_ERROR, name + " with an invalid package");
    }
    const auto alias = Build({{"ICUDATA/zzz.res", "Tst1", 8, 32}});
    Require(Set("ICUDATA", alias.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "the name ICUDATA");
    const auto prefix = Single("icudt68", 33);
    Require(Set("icudt68", prefix.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "the name icudt68");
    RequireServed("icudt68", "res", "a", prefix.payloads[0], "Tst1", "the name icudt68");
    std::vector<std::string> ordinary = {"ICUDT68L", "icudt68lx"};
    if constexpr (U_FILE_SEP_CHAR == '/') ordinary.push_back("x\\icudt68l");
    for (const auto& name : ordinary) {
        const auto package = Single(name, 34);
        Require(Set(name.c_str(), package.bytes, U_ZERO_ERROR) == U_ZERO_ERROR, "the name " + name);
        RequireServed(name.c_str(), "res", "a", package.payloads[0], "Tst1", "the name " + name);
    }
}

struct ReadOnlyPage {
    std::int32_t* status = nullptr;
    std::size_t size = 0;

    explicit ReadOnlyPage(std::int32_t value) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        status = static_cast<std::int32_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(status != nullptr, "VirtualAlloc failed");
        *status = value;
        DWORD previous = 0;
        Require(VirtualProtect(status, size, PAGE_READONLY, &previous) != 0, "VirtualProtect failed");
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "mmap failed");
        status = static_cast<std::int32_t*>(mapped);
        *status = value;
        Require(mprotect(mapped, size, PROT_READ) == 0, "mprotect failed");
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(status, 0, MEM_RELEASE);
#else
        munmap(status, size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;
};

void CheckStatusMemory() {
    const auto fresh = Single("readonly", 40);
    {
        ReadOnlyPage page(U_ZERO_ERROR);
        Require(!Throws([&] { udata_setAppData_nid_postfix("readonly", fresh.bytes, page.status); }), "a read-only status ICU does not write");
        RequireServed("readonly", "res", "a", fresh.payloads[0], "Tst1", "a package passed with a read-only status");
        Require(Throws([&] { udata_setAppData_nid_postfix("readonly", fresh.bytes, page.status); }), "a read-only status ICU writes");
    }
    {
        ReadOnlyPage page(U_USING_DEFAULT_WARNING);
        Require(Throws([&] { udata_setAppData_nid_postfix("readonly", fresh.bytes, page.status); }), "a read-only warning ICU writes again");
        const auto warned = Single("readonlywarned", 41);
        Require(!Throws([&] { udata_setAppData_nid_postfix("readonlywarned", warned.bytes, page.status); }), "a read-only warning ICU keeps");
        RequireServed("readonlywarned", "res", "a", warned.payloads[0], "Tst1", "a package passed with a read-only warning");
    }
    {
        ReadOnlyPage page(U_ZERO_ERROR);
        const auto invalid = Single("readonlyinvalid", 42);
        invalid.bytes[2] = 0;
        Require(Throws([&] { udata_setAppData_nid_postfix("readonlyinvalid", invalid.bytes, page.status); }), "a read-only status with an invalid package");
        Require(Throws([&] { udata_setAppData_nid_postfix("readonlynull", nullptr, page.status); }), "a read-only status with null data");
    }
    {
        ReadOnlyPage page(U_ILLEGAL_ARGUMENT_ERROR);
        const auto failed = Single("readonlyfailed", 43);
        Require(!Throws([&] { udata_setAppData_nid_postfix("readonlyfailed", failed.bytes, page.status); }), "a read-only failing status");
        RequireMissing("readonlyfailed", "res", "a", "a package passed with a read-only failing status");
    }
    auto* unreadable = reinterpret_cast<std::int32_t*>(std::uintptr_t{8});
    Require(Throws([&] { udata_setAppData_nid_postfix("unreadable", fresh.bytes, unreadable); }), "an unreadable status");
}

void CheckConcurrentRegistration() {
    constexpr int Threads = 32;
    for (int round = 0; round != 5; ++round) {
        const std::string name = "race" + std::to_string(round);
        std::vector<Package> packages;
        for (int thread = 0; thread != Threads; ++thread) packages.push_back(Single(name, static_cast<std::uint8_t>(100 + thread)));
        std::vector<std::int32_t> statuses(Threads, -1);
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        for (int thread = 0; thread != Threads; ++thread) {
            threads.emplace_back([&, thread] {
                while (!start.load()) std::this_thread::yield();
                statuses[thread] = Set(name.c_str(), packages[thread].bytes, U_ZERO_ERROR);
            });
        }
        start = true;
        for (auto& thread : threads) thread.join();
        int winner = -1;
        int warnings = 0;
        for (int thread = 0; thread != Threads; ++thread) {
            if (statuses[thread] == U_ZERO_ERROR) {
                Require(winner < 0, "two threads registered " + name);
                winner = thread;
            } else if (statuses[thread] == U_USING_DEFAULT_WARNING) {
                ++warnings;
            }
        }
        Require(winner >= 0 && warnings == Threads - 1, "concurrent registration of " + name);
        RequireServed(name.c_str(), "res", "a", packages[winner].payloads[0], "Tst1", "the package of the registering thread");
    }
}

}

int main() {
    CheckRegistration();
    CheckIncomingStatus();
    CheckHeaders();
    CheckNames();
    CheckIcuDataNames();
    CheckStatusMemory();
    CheckConcurrentRegistration();
    return 0;
}
