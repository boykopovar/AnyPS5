#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "prx/libc/include/FilesystemError.hpp"

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using GuestWhat = const char* (APS5_VABI *)(const LibcException::ExceptionObject*);
using Thrower = void (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const TypeRecord _ZTISt12system_error_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

static unsigned failures = 0;

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

static std::array<void*, 128> heapBlocks{};
static std::size_t allocationAttempt = 0;
static std::size_t failAllocationAt = 0;

static void** HeapSlot(void* pointer) {
    for (auto& block : heapBlocks)
        if (block == pointer) return &block;
    return nullptr;
}

static void* TrackAllocation(void* pointer) {
    if (!pointer) return nullptr;
    auto* slot = HeapSlot(nullptr);
    if (!slot || HeapSlot(pointer)) std::abort();
    *slot = pointer;
    return pointer;
}

static bool FailAllocation() {
    return failAllocationAt != 0 && ++allocationAttempt == failAllocationAt;
}

static void* APS5_VABI TestAllocate(std::size_t bytes) {
    if (FailAllocation()) return nullptr;
    return TrackAllocation(std::malloc(bytes ? bytes : 1));
}

static void APS5_VABI TestFree(void* pointer) {
    if (pointer) {
        auto* slot = HeapSlot(pointer);
        if (!slot) std::abort();
        *slot = nullptr;
    }
    std::free(pointer);
}

static void* APS5_VABI TestCalloc(std::size_t count, std::size_t bytes) {
    if (FailAllocation()) return nullptr;
    return TrackAllocation(std::calloc(count ? count : 1, bytes ? bytes : 1));
}

static void* APS5_VABI TestReallocate(void* pointer, std::size_t bytes) {
    if (!pointer) return TestAllocate(bytes);
    if (bytes == 0) {
        TestFree(pointer);
        return nullptr;
    }
    auto* slot = HeapSlot(pointer);
    if (!slot) std::abort();
    if (FailAllocation()) return nullptr;
    void* result = std::realloc(pointer, bytes);
    if (result) *slot = result;
    return result;
}

static void* APS5_VABI TestAlign(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
static void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
static int APS5_VABI TestPosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

static std::string Text(const GuestLocale::String& string) {
    return std::string(string.capacity > 15 ? string.pointer : string.buffer, string.size);
}

struct GuestSystemError {
    LibcException::SystemErrorObject base;
    const LibcException::ErrorCode& code() const noexcept { return base.code; }
};

struct Inspected {
    std::string type;
    std::string what;
    LibcException::ErrorCode systemCode;
    LibcException::ErrorCode code;
    std::string path1;
    std::string path2;
    std::uint64_t path2Capacity;
};

static Inspected inspected;

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* exception = static_cast<const LibcException::ExceptionObject*>(object);
    const auto* system = static_cast<const GuestSystemError*>(object);
    const auto* vtable = static_cast<const void* const*>(exception->vtable);
    const auto* bytes = static_cast<const unsigned char*>(object);
    inspected.type = static_cast<const TypeRecord*>(vtable[-1])->name;
    inspected.what = reinterpret_cast<GuestWhat>(const_cast<void*>(vtable[2]))(exception);
    inspected.systemCode = system->code();
    std::memcpy(&inspected.code, bytes + 0x30, sizeof(inspected.code));
    const auto* path1 = reinterpret_cast<const GuestLocale::String*>(bytes + 0x40);
    const auto* path2 = reinterpret_cast<const GuestLocale::String*>(bytes + 0x68);
    inspected.path1 = Text(*path1);
    inspected.path2 = Text(*path2);
    inspected.path2Capacity = path2->capacity;
}

struct Throw {
    const char* message;
    const GuestLocale::String* path;
    LibcException::ErrorCode code;
    const GuestLocale::String* path2 = nullptr;
};

static void APS5_VABI ThrowFilesystemError(const void* argument) {
    const auto* request = static_cast<const Throw*>(argument);
    if (request->path && request->path2) _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathES4_St10error_code_nid_postfix(request->message, request->path, request->path2, request->code);
    if (request->path) _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathESt10error_code_nid_postfix(request->message, request->path, request->code);
    _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix(request->message, request->code);
}

static void APS5_VABI ThrowSceV2FilesystemError(const void* argument) {
    const auto* request = static_cast<const Throw*>(argument);
    if (request->path && request->path2) _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathES5_NS_10error_codeE_nid_postfix(request->message, request->path, request->path2, request->code);
    if (request->path) _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathENS_10error_codeE_nid_postfix(request->message, request->path, request->code);
    _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcNS_10error_codeE_nid_postfix(request->message, request->code);
}

static GuestLocale::String GuestString(const std::string& text) {
    GuestLocale::String string {};
    string.capacity = 15;
    char* data = string.buffer;
    if (text.size() > 15) {
        string.capacity = text.size() | 15;
        const std::size_t bytes = string.capacity + 1;
        if (bytes < 0x1000) {
            data = static_cast<char*>(_Znwm_nid_postfix(bytes));
        } else {
            auto* raw = static_cast<unsigned char*>(_Znwm_nid_postfix(bytes + 0x27));
            const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + 0x27) & ~std::uintptr_t {0x1f};
            reinterpret_cast<void**>(aligned)[-1] = raw;
            data = reinterpret_cast<char*>(aligned);
        }
        string.pointer = data;
    }
    std::memcpy(data, text.data(), text.size());
    data[text.size()] = 0;
    string.size = text.size();
    return string;
}

static void ReleaseGuestString(GuestLocale::String& string) {
    if (string.capacity <= 15) return;
    _ZdlPv_nid_postfix(string.capacity + 1 >= 0x1000 ? reinterpret_cast<void**>(string.pointer)[-1] : string.pointer);
}

static std::string categoryText;

static const char* APS5_VABI GuestCategoryName(const LibcException::CategoryObject*) {
    return "guest";
}

static GuestLocale::String* APS5_VABI CategoryMessage(GuestLocale::String* result, const LibcException::CategoryObject*, int value) {
    *result = GuestString(categoryText + std::to_string(value));
    return result;
}

static const void* const CategoryVtable[] {
    nullptr, nullptr, nullptr, nullptr, reinterpret_cast<const void*>(GuestCategoryName), reinterpret_cast<const void*>(CategoryMessage)
};
static const LibcException::CategoryObject GuestCategory {CategoryVtable + 2};

static TypeRecord Named(const char* name) {
    return {_ZTISt9exception_nid_postfix.vtable, name, nullptr};
}

static void Catch(Thrower thrower, const Throw& request, const void* catchType, const void* otherType, int selector, const char* expectedType, const char* label) {
    SceV2CatchType = catchType;
    SceV2OtherType = otherType;
    SceV2Selector = 0;
    inspected = {};
    SceV2Catch(thrower, &request);
    Require(SceV2Selector == selector, label);
    Require(inspected.type == expectedType, label);
    if (!request.path2) {
        Require(inspected.path2.empty() && inspected.path2Capacity == 15, label);
    }
}

static void CheckMessages() {
    const TypeRecord unrelated = Named("St11range_error");
    const TypeRecord filesystemError = Named("NSt10filesystem16filesystem_errorE");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();

    Catch(ThrowFilesystemError, {"remove", nullptr, {0, 2, generic}}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "NSt10filesystem16filesystem_errorE", "caught as std::system_error");
    Require(inspected.what == "remove: no such file or directory", "message, separator and category text");
    Require(inspected.systemCode.value == 2 && inspected.systemCode.category == generic, "system_error base code() matches generic category");
    Require(inspected.code.value == 2 && inspected.code.category == generic, "error code at 0x30");
    Require(inspected.path1.empty(), "no path");

    Catch(ThrowFilesystemError, {"", nullptr, {0, 2, generic}}, &unrelated, &filesystemError, 1, "NSt10filesystem16filesystem_errorE", "caught by name as filesystem_error");
    Require(inspected.what == "no such file or directory", "empty message has no separator");

    categoryText = "short ";
    Catch(ThrowFilesystemError, {"copy", nullptr, {0, 7, &GuestCategory}}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "NSt10filesystem16filesystem_errorE", "guest category");
    Require(inspected.what == "copy: short 7", "guest category message in the inline buffer");

    categoryText = std::string(5000, 'x');
    Catch(ThrowFilesystemError, {"copy", nullptr, {0, 9, &GuestCategory}}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "NSt10filesystem16filesystem_errorE", "large guest category message");
    Require(inspected.what == "copy: " + categoryText + "9", "large guest category message is copied and released");
}

static void CheckPaths() {
    const TypeRecord unrelated = Named("St11range_error");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    for (const std::string text : {std::string("a.txt"), std::string("/app0/data/save/slot01.bin"), std::string(4200, 'p')}) {
        GuestLocale::String path = GuestString(text);
        Catch(ThrowFilesystemError, {"open", &path, {0, 2, generic}}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "NSt10filesystem16filesystem_errorE", "path overload");
        Require(inspected.path1 == text, "path1 is a copy of the path");
        Require(inspected.what == "open: no such file or directory", "path overload message leaves the path out");
        Require(Text(path) == text, "the caller's path is unchanged");
        ReleaseGuestString(path);
    }
}

static void CheckTwoPaths() {
    const TypeRecord unrelated = Named("St11range_error");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    GuestLocale::String path1 = GuestString("source.txt");
    GuestLocale::String path2 = GuestString(std::string(4500, 'q'));
    Catch(ThrowFilesystemError, {"copy_file", &path1, {0, 17, generic}, &path2}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "NSt10filesystem16filesystem_errorE", "two paths overload");
    Require(inspected.path1 == "source.txt", "path1 is source");
    Require(inspected.path2 == std::string(4500, 'q'), "path2 is destination");
    Require(inspected.what == "copy_file: file exists", "two paths message leaves paths out");
    Require(Text(path1) == "source.txt", "caller path1 unchanged");
    Require(Text(path2) == std::string(4500, 'q'), "caller path2 unchanged");
    ReleaseGuestString(path1);
    ReleaseGuestString(path2);
}

static void CheckNullArguments() {
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    GuestLocale::String path = GuestString("a");
    GuestLocale::String path2 = GuestString("b");
    auto throws = [](auto call) {
        try { call(); } catch (const std::invalid_argument&) { return true; }
        return false;
    };
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix(nullptr, {0, 2, generic}); }), "null message throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix("x", {0, 2, nullptr}); }), "null category throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathESt10error_code_nid_postfix("x", nullptr, {0, 2, generic}); }), "null path throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathESt10error_code_nid_postfix(nullptr, &path, {0, 2, generic}); }), "null message with a path throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathES4_St10error_code_nid_postfix(nullptr, &path, &path2, {0, 2, generic}); }), "null message with two paths throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathES4_St10error_code_nid_postfix("x", nullptr, &path2, {0, 2, generic}); }), "null path1 with two paths throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathES4_St10error_code_nid_postfix("x", &path, nullptr, {0, 2, generic}); }), "null path2 with two paths throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathES4_St10error_code_nid_postfix("x", &path, &path2, {0, 2, nullptr}); }), "null category with two paths throws");
    ReleaseGuestString(path);
    ReleaseGuestString(path2);
}

static void CheckSceV2Messages() {
    const TypeRecord unrelated = Named("St11range_error");
    const TypeRecord filesystemError = Named("NSt8__sce_v210filesystem16filesystem_errorE");
    const TypeRecord systemError = Named("NSt8__sce_v212system_errorE");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();

    Catch(ThrowSceV2FilesystemError, {"remove", nullptr, {0, 2, generic}}, &systemError, &unrelated, 2, "NSt8__sce_v210filesystem16filesystem_errorE", "sce_v2 caught as std::__sce_v2::system_error");
    Require(inspected.what == "remove: no such file or directory", "sce_v2 message, separator and category text");
    Require(inspected.systemCode.value == 2 && inspected.systemCode.category == generic, "sce_v2 system_error base code() at 16");
    Require(inspected.code.value == 2 && inspected.code.category == generic, "sce_v2 error code at 0x30");
    Require(inspected.path1.empty(), "sce_v2 no path");

    Catch(ThrowSceV2FilesystemError, {"", nullptr, {0, 2, generic}}, &unrelated, &filesystemError, 1, "NSt8__sce_v210filesystem16filesystem_errorE", "sce_v2 caught by name as filesystem_error");
    Require(inspected.what == "no such file or directory", "sce_v2 empty message has no separator");

    categoryText = "short ";
    Catch(ThrowSceV2FilesystemError, {"copy", nullptr, {0, 7, &GuestCategory}}, &systemError, &unrelated, 2, "NSt8__sce_v210filesystem16filesystem_errorE", "sce_v2 guest category");
    Require(inspected.what == "copy: short 7", "sce_v2 guest category message in the inline buffer");

    categoryText = std::string(5000, 'x');
    Catch(ThrowSceV2FilesystemError, {"copy", nullptr, {0, 9, &GuestCategory}}, &systemError, &unrelated, 2, "NSt8__sce_v210filesystem16filesystem_errorE", "sce_v2 large guest category message");
    Require(inspected.what == "copy: " + categoryText + "9", "sce_v2 large guest category message is copied and released");
}

static void CheckSceV2Paths() {
    const TypeRecord unrelated = Named("St11range_error");
    const TypeRecord systemError = Named("NSt8__sce_v212system_errorE");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    for (const std::string text : {std::string("a.txt"), std::string("/app0/data/save/slot01.bin"), std::string(4200, 'p')}) {
        GuestLocale::String path = GuestString(text);
        Catch(ThrowSceV2FilesystemError, {"open", &path, {0, 2, generic}}, &systemError, &unrelated, 2, "NSt8__sce_v210filesystem16filesystem_errorE", "sce_v2 path overload");
        Require(inspected.path1 == text, "sce_v2 path1 is a copy of the path");
        Require(inspected.what == "open: no such file or directory", "sce_v2 path overload message leaves the path out");
        Require(Text(path) == text, "sce_v2 caller's path is unchanged");
        ReleaseGuestString(path);
    }
}

static void CheckSceV2TwoPaths() {
    const TypeRecord unrelated = Named("St11range_error");
    const TypeRecord systemError = Named("NSt8__sce_v212system_errorE");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    GuestLocale::String path1 = GuestString("source.txt");
    GuestLocale::String path2 = GuestString(std::string(4500, 'q'));
    Catch(ThrowSceV2FilesystemError, {"copy_file", &path1, {0, 17, generic}, &path2}, &systemError, &unrelated, 2, "NSt8__sce_v210filesystem16filesystem_errorE", "sce_v2 two paths overload");
    Require(inspected.path1 == "source.txt", "sce_v2 path1 is source");
    Require(inspected.path2 == std::string(4500, 'q'), "sce_v2 path2 is destination");
    Require(inspected.what == "copy_file: file exists", "sce_v2 two paths message leaves paths out");
    Require(Text(path1) == "source.txt", "sce_v2 caller path1 unchanged");
    Require(Text(path2) == std::string(4500, 'q'), "sce_v2 caller path2 unchanged");
    ReleaseGuestString(path1);
    ReleaseGuestString(path2);
}

static void CheckSceV2NullArguments() {
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    GuestLocale::String path = GuestString("a");
    GuestLocale::String path2 = GuestString("b");
    auto throws = [](auto call) {
        try { call(); } catch (const std::invalid_argument&) { return true; }
        return false;
    };
    Require(throws([&] { _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcNS_10error_codeE_nid_postfix(nullptr, {0, 2, generic}); }), "sce_v2 null message throws");
    Require(throws([&] { _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcNS_10error_codeE_nid_postfix("x", {0, 2, nullptr}); }), "sce_v2 null category throws");
    Require(throws([&] { _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathENS_10error_codeE_nid_postfix("x", nullptr, {0, 2, generic}); }), "sce_v2 null path throws");
    Require(throws([&] { _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathENS_10error_codeE_nid_postfix(nullptr, &path, {0, 2, generic}); }), "sce_v2 null message with a path throws");
    Require(throws([&] { _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathES5_NS_10error_codeE_nid_postfix(nullptr, &path, &path2, {0, 2, generic}); }), "sce_v2 null message with two paths throws");
    Require(throws([&] { _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathES5_NS_10error_codeE_nid_postfix("x", nullptr, &path2, {0, 2, generic}); }), "sce_v2 null path1 with two paths throws");
    Require(throws([&] { _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathES5_NS_10error_codeE_nid_postfix("x", &path, nullptr, {0, 2, generic}); }), "sce_v2 null path2 with two paths throws");
    Require(throws([&] { _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathES5_NS_10error_codeE_nid_postfix("x", &path, &path2, {0, 2, nullptr}); }), "sce_v2 null category with two paths throws");
    ReleaseGuestString(path);
    ReleaseGuestString(path2);
}

static void CheckSceV2SystemErrorBase() {
    const TypeRecord unrelated = Named("St11range_error");
    const TypeRecord systemError = Named("NSt8__sce_v212system_errorE");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();

    using CategoryName = const char* (APS5_VABI *)(const LibcException::CategoryObject*);
    auto categoryName = [](const LibcException::CategoryObject* category) {
        const auto* vtable = static_cast<const void* const*>(category->vtable);
        return reinterpret_cast<CategoryName>(const_cast<void*>(vtable[2]))(category);
    };

    Catch(ThrowSceV2FilesystemError, {"read", nullptr, {0, 5, generic}}, &systemError, &unrelated, 2, "NSt8__sce_v210filesystem16filesystem_errorE", "catch as sce_v2 system_error base");
    Require(inspected.systemCode.value == 5, "system_error base code().value matches");
    Require(inspected.systemCode.category == generic, "system_error base code().category matches generic category");
    Require(std::strcmp(categoryName(inspected.systemCode.category), "generic") == 0, "system_error base category name is generic");
    Require(inspected.code.value == 5 && inspected.code.category == generic, "filesystem_error code at 0x30 preserved");

    categoryText = "custom ";
    Catch(ThrowSceV2FilesystemError, {"write", nullptr, {0, 13, &GuestCategory}}, &systemError, &unrelated, 2, "NSt8__sce_v210filesystem16filesystem_errorE", "catch with guest category");
    Require(inspected.systemCode.value == 13, "system_error base code().value matches guest code");
    Require(inspected.systemCode.category == &GuestCategory, "system_error base code().category matches guest category");
    Require(std::strcmp(categoryName(inspected.systemCode.category), "guest") == 0, "system_error base guest category name is guest");
    Require(inspected.code.value == 13 && inspected.code.category == &GuestCategory, "filesystem_error code at 0x30 preserved for guest category");
}

static void CheckConstructionFailures() {
    Require(heapBlocks == std::array<void*, 128>{}, "earlier filesystem checks leave no guest allocations");
    const std::string firstText(48, 'a');
    const std::string secondText(79, 'b');
    auto first = GuestString(firstText);
    auto second = GuestString(secondText);
    const auto before = heapBlocks;
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    const std::array<Thrower, 2> throwers{ThrowFilesystemError, ThrowSceV2FilesystemError};
    for (const auto thrower : throwers) {
        for (int paths = 1; paths <= 2; ++paths) {
            const Throw request{"copy", &first, {0, 0, generic}, paths == 2 ? &second : nullptr};
            for (std::size_t failing = 1; failing <= static_cast<std::size_t>(paths); ++failing) {
                allocationAttempt = 0;
                failAllocationAt = failing;
                bool caught = false;
                try {
                    thrower(&request);
                } catch (const std::bad_alloc&) {
                    caught = true;
                } catch (...) {
                    Require(false, "path allocation failure throws bad_alloc");
                }
                failAllocationAt = 0;
                Require(caught && allocationAttempt == failing, "the selected path copy allocation fails");
                Require(heapBlocks == before, "failed filesystem path copy releases all prior allocations");
                Require(Text(first) == firstText && Text(second) == secondText, "failed path copies preserve both source strings");
            }
        }
    }
    ReleaseGuestString(first);
    ReleaseGuestString(second);
    Require(heapBlocks == std::array<void*, 128>{}, "filesystem construction failures leave no guest allocations");
}

static void CheckSymbolicLinkPrefixErrors() {
    const std::string directoryName = "anyps5-symlink-prefix-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = std::filesystem::current_path() / directoryName;
    Require(std::filesystem::create_directory(directory), "create symlink prefix test directory");
    const auto regularFile = directory / "regular.txt";
    std::vector<std::filesystem::path> createdLinks;
    struct CleanupGuard {
        const std::filesystem::path& directory;
        const std::filesystem::path& regularFile;
        const std::vector<std::filesystem::path>& links;
        ~CleanupGuard() {
            for (auto it = links.rbegin(); it != links.rend(); ++it) {
#ifdef _WIN32
                DeleteFileW(it->c_str());
#else
                std::error_code removeError;
                std::filesystem::remove(*it, removeError);
#endif
            }
            std::error_code cleanupError;
            std::filesystem::remove(regularFile, cleanupError);
            std::filesystem::remove(directory, cleanupError);
        }
    } cleanup{directory, regularFile, createdLinks};
    {
        std::ofstream output(regularFile);
        output << "x";
    }
    Require(std::filesystem::exists(regularFile), "create regular file");
    bool linksAvailable = true;
    auto createLink = [&](const std::string& name, const std::string& target) {
        if (!linksAvailable) return false;
        const auto linkPath = directory / name;
#ifdef _WIN32
        const std::wstring wideTarget(target.begin(), target.end());
        if (CreateSymbolicLinkW(linkPath.c_str(), wideTarget.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0) {
            if (GetLastError() == ERROR_PRIVILEGE_NOT_HELD) {
                linksAvailable = false;
                return false;
            }
            Require(false, "create Windows symbolic link");
            return false;
        }
#else
        std::error_code linkError;
        std::filesystem::create_symlink(target, linkPath, linkError);
        if (linkError) {
            Require(false, "create POSIX symbolic link");
            return false;
        }
#endif
        createdLinks.push_back(linkPath);
        return true;
    };

    auto createChain = [&](const std::string& prefix, unsigned count, const std::string& leafTarget) {
        for (unsigned index = 1; index <= count; ++index) {
            const std::string linkName = prefix + "-" + std::to_string(index);
            const std::string target = index == 1 ? leafTarget : prefix + "-" + std::to_string(index - 1);
            if (!createLink(linkName, target)) return false;
        }
        return true;
    };

    bool setupComplete = createLink("loop-a", "loop-b") &&
        createLink("loop-b", "loop-a") &&
        createChain("chain41", 41, "missing-41");
#ifdef _WIN32
    setupComplete = setupComplete &&
        createChain("chain33", 33, "missing-33") &&
        createChain("chain32", 32, "missing-32") &&
        createChain("file32", 32, "regular.txt");
#endif

    auto checkNativeAndGuest = [&](const std::string& relativePath, int expectedNotFound, int expectedPrefix, bool expectedNonDir,
        const char* label) {
        const auto nativePath = directory / relativePath;
        const std::string guestPath = directoryName + "/" + relativePath;
        Require(PrefixError_nid_no_patch(nativePath) == expectedPrefix, label);
        Require(NotFoundError_nid_no_patch(nativePath) == expectedNotFound, label);
        Require(HasNonDirectoryPrefix_nid_no_patch(nativePath) == expectedNonDir, label);
        *__error_nid_postfix() = 0;
        Require(access_nid_postfix(guestPath.c_str(), 0) == -1 && *__error_nid_postfix() == expectedNotFound, label);
    };

    checkNativeAndGuest("regular.txt/child", 20, 20, true, "path below regular file returns ENOTDIR");
    checkNativeAndGuest("missing/child", 2, 0, false, "path below missing directory returns ENOENT");

    if (setupComplete && linksAvailable) {
        checkNativeAndGuest("loop-a/child", 62, 62, false, "path below symlink loop returns ELOOP");
        checkNativeAndGuest("chain41-41/child", 62, 62, false, "path below 41-link dangling chain returns ELOOP");
#ifdef _WIN32
        checkNativeAndGuest("chain33-33/child", 62, 62, false, "path below 33-link dangling chain returns ELOOP");
        checkNativeAndGuest("chain32-32/child", 2, 2, false, "path below 32-link dangling chain returns ENOENT");
        checkNativeAndGuest("file32-32/child", 20, 20, true, "path below 32-link chain to regular file returns ENOTDIR");
#endif
    } else if (!linksAvailable) {
        std::fprintf(stderr, "symbolic links unavailable: symlink prefix error checks skipped\n");
    }

}

int main() {
    std::array<void*, 10> heap{
        reinterpret_cast<void*>(TestAllocate), reinterpret_cast<void*>(TestFree), reinterpret_cast<void*>(TestCalloc),
        reinterpret_cast<void*>(TestReallocate), reinterpret_cast<void*>(TestAlign), reinterpret_cast<void*>(TestRealign),
        reinterpret_cast<void*>(TestPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    CheckMessages();
    CheckPaths();
    CheckTwoPaths();
    CheckNullArguments();
    CheckSceV2Messages();
    CheckSceV2Paths();
    CheckSceV2TwoPaths();
    CheckSceV2NullArguments();
    CheckSceV2SystemErrorBase();
    CheckConstructionFailures();
    CheckSymbolicLinkPrefixErrors();
    if (failures) return 1;
    std::puts("guest std::filesystem::_Xfilesystem_error and std::__sce_v2::filesystem::_Xfilesystem_error: PASS");
    return 0;
}
