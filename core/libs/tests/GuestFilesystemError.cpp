#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

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

static std::string Text(const GuestLocale::String& string) {
    return std::string(string.capacity > 15 ? string.pointer : string.buffer, string.size);
}

struct Inspected {
    std::string type;
    std::string what;
    LibcException::ErrorCode code;
    std::string path1;
    std::string path2;
    std::uint64_t path2Capacity;
};

static Inspected inspected;

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* exception = static_cast<const LibcException::ExceptionObject*>(object);
    const auto* vtable = static_cast<const void* const*>(exception->vtable);
    const auto* bytes = static_cast<const unsigned char*>(object);
    inspected.type = static_cast<const TypeRecord*>(vtable[-1])->name;
    inspected.what = reinterpret_cast<GuestWhat>(const_cast<void*>(vtable[2]))(exception);
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
};

static void APS5_VABI ThrowFilesystemError(const void* argument) {
    const auto* request = static_cast<const Throw*>(argument);
    if (request->path) _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathESt10error_code_nid_postfix(request->message, request->path, request->code);
    _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix(request->message, request->code);
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

static GuestLocale::String* APS5_VABI CategoryMessage(GuestLocale::String* result, const LibcException::CategoryObject*, int value) {
    *result = GuestString(categoryText + std::to_string(value));
    return result;
}

static const void* const CategoryVtable[] {
    nullptr, nullptr, nullptr, nullptr, nullptr, reinterpret_cast<const void*>(CategoryMessage)
};
static const LibcException::CategoryObject GuestCategory {CategoryVtable + 2};

static TypeRecord Named(const char* name) {
    return {_ZTISt9exception_nid_postfix.vtable, name, nullptr};
}

static void Catch(const Throw& request, const void* catchType, const void* otherType, int selector, const char* label) {
    SceV2CatchType = catchType;
    SceV2OtherType = otherType;
    SceV2Selector = 0;
    inspected = {};
    SceV2Catch(ThrowFilesystemError, &request);
    Require(SceV2Selector == selector, label);
    Require(inspected.type == "NSt10filesystem16filesystem_errorE", label);
    Require(inspected.path2.empty() && inspected.path2Capacity == 15, label);
}

static void CheckMessages() {
    const TypeRecord unrelated = Named("St11range_error");
    const TypeRecord filesystemError = Named("NSt10filesystem16filesystem_errorE");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();

    Catch({"remove", nullptr, {0, 2, generic}}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "caught as std::system_error");
    Require(inspected.what == "remove: no such file or directory", "message, separator and category text");
    Require(inspected.code.value == 2 && inspected.code.category == generic, "error code at 0x30");
    Require(inspected.path1.empty(), "no path");

    Catch({"", nullptr, {0, 2, generic}}, &unrelated, &filesystemError, 1, "caught by name as filesystem_error");
    Require(inspected.what == "no such file or directory", "empty message has no separator");

    categoryText = "short ";
    Catch({"copy", nullptr, {0, 7, &GuestCategory}}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "guest category");
    Require(inspected.what == "copy: short 7", "guest category message in the inline buffer");

    categoryText = std::string(5000, 'x');
    Catch({"copy", nullptr, {0, 9, &GuestCategory}}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "large guest category message");
    Require(inspected.what == "copy: " + categoryText + "9", "large guest category message is copied and released");
}

static void CheckPaths() {
    const TypeRecord unrelated = Named("St11range_error");
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    for (const std::string text : {std::string("a.txt"), std::string("/app0/data/save/slot01.bin"), std::string(4200, 'p')}) {
        GuestLocale::String path = GuestString(text);
        Catch({"open", &path, {0, 2, generic}}, &_ZTISt12system_error_nid_postfix, &unrelated, 2, "path overload");
        Require(inspected.path1 == text, "path1 is a copy of the path");
        Require(inspected.what == "open: no such file or directory", "path overload message leaves the path out");
        Require(Text(path) == text, "the caller's path is unchanged");
        ReleaseGuestString(path);
    }
}

static void CheckNullArguments() {
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    GuestLocale::String path = GuestString("a");
    auto throws = [](auto call) {
        try { call(); } catch (const std::invalid_argument&) { return true; }
        return false;
    };
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix(nullptr, {0, 2, generic}); }), "null message throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix("x", {0, 2, nullptr}); }), "null category throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathESt10error_code_nid_postfix("x", nullptr, {0, 2, generic}); }), "null path throws");
    Require(throws([&] { _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathESt10error_code_nid_postfix(nullptr, &path, {0, 2, generic}); }), "null message with a path throws");
}

int main() {
    std::array<void*, 10> defaultHeap {};
    ApplicationHeapRegister_nid_no_patch(defaultHeap.data());
    CheckMessages();
    CheckPaths();
    CheckNullArguments();
    if (failures) return 1;
    std::puts("guest std::filesystem::_Xfilesystem_error: PASS");
    return 0;
}
