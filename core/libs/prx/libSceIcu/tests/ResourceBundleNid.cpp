#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using IcuRecoveredTest::Require;

void Check(const char* path, const char* package) {
    std::ifstream input(package, std::ios::binary | std::ios::ate);
    Require(bool(input), "open actual compiled resource package");
    const auto size = input.tellg();
    Require(size > 0, "resource package is nonempty");
    std::vector<std::max_align_t> storage((static_cast<std::size_t>(size) + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(storage.data()), size);
    Require(bool(input), "read complete aligned resource package");
#ifdef _WIN32
    auto module = LoadLibraryA(path);
#else
    auto module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    Require(module != nullptr, "load actual canonical NID-patched ICU artifact");
    const auto import = [&](const char* name) {
#ifdef _WIN32
        auto* address = GetProcAddress(module, name);
#else
        auto* address = dlsym(module, name);
#endif
        Require(address != nullptr, "resolve resource-bundle NID");
        return address;
    };
    const auto common = reinterpret_cast<decltype(&udata_setCommonData)>(import("wHZwCpZVcGI"));
    const auto fileAccess = reinterpret_cast<decltype(&udata_setFileAccess)>(import("mCyTdtewGpk"));
    const auto cleanup = reinterpret_cast<decltype(&u_cleanup_nid_postfix)>(import("1SsridDRBYQ"));
    const auto open = reinterpret_cast<decltype(&ures_open)>(import("bpD6xUk4QVI"));
    const auto direct = reinterpret_cast<decltype(&ures_openDirect_nid_postfix)>(import("lh5Sirif1Js"));
    const auto close = reinterpret_cast<decltype(&ures_close)>(import("--GQ9Ov+0mM"));
    const auto byKey = reinterpret_cast<decltype(&ures_getByKey)>(import("8WDJjbxNvCg"));
    const auto byIndex = reinterpret_cast<decltype(&ures_getByIndex_nid_postfix)>(import("CJkitLFH-II"));
    const auto count = reinterpret_cast<decltype(&ures_getSize_nid_postfix)>(import("JZ5yszZEFEY"));
    const auto type = reinterpret_cast<decltype(&ures_getType_nid_postfix)>(import("XUT0dUajUrA"));
    const auto key = reinterpret_cast<decltype(&ures_getKey_nid_postfix)>(import("m7ZpRaRI5LM"));
    const auto string = reinterpret_cast<decltype(&ures_getString_nid_postfix)>(import("Ei1Dn07XYs4"));
    const auto stringByKey = reinterpret_cast<decltype(&ures_getStringByKey)>(import("apt8-c9wFxQ"));
    const auto stringByIndex = reinterpret_cast<decltype(&ures_getStringByIndex_nid_postfix)>(import("iyv2v10Y3ho"));
    const auto integer = reinterpret_cast<decltype(&ures_getInt_nid_postfix)>(import("rGSY5kqmIQo"));
    const auto binary = reinterpret_cast<decltype(&ures_getBinary_nid_postfix)>(import("5AKCnEfGT+Y"));
    const auto vector = reinterpret_cast<decltype(&ures_getIntVector_nid_postfix)>(import("iMQarT42Hpk"));
    std::int32_t status = 0;
    common(storage.data(), &status);
    Require(status <= 0, "actual NID registers compiled fixture before resource use");
    fileAccess(3, &status);
    Require(status <= 0, "actual NID disables external resource files");
    auto* root = direct(nullptr, "aps5ResourceFixture", &status);
    Require(root && status <= 0 && type(root) == 2 && count(root) == 9, "actual NIDs open and describe the compiled table");
    std::int32_t length = -1;
    const auto* greeting = stringByKey(root, "greeting", &length, &status);
    Require(greeting && status <= 0 && std::u16string_view(greeting, length) == u"h\u00e9llo \u4e16\u754c", "actual NID reads Unicode string by key");
    auto* child = byKey(root, "number", nullptr, &status);
    Require(child && status <= 0 && type(child) == 7 && integer(child, &status) == 1234, "actual NIDs read integer resource");
    Require(byKey(root, "negative", child, &status) == child && integer(child, &status) == -7 && status <= 0, "actual NIDs reuse fillIn identity for signed integer");
    Require(byKey(root, "bytes", child, &status) == child && status <= 0, "actual NID selects binary resource");
    const auto* bytes = binary(child, &length, &status);
    Require(bytes && status <= 0 && length == 4 && bytes[0] == 0 && bytes[1] == 0x10 && bytes[2] == 0xff && bytes[3] == 0, "actual NID reads binary bytes");
    Require(byKey(root, "vector", child, &status) == child && status <= 0, "actual NID selects integer vector");
    const auto* values = vector(child, &length, &status);
    Require(values && status <= 0 && length == 3 && values[0] == 1 && values[1] == -2 && values[2] == 17, "actual NID reads signed integer vector");
    Require(byKey(root, "values", child, &status) == child && type(child) == 8 && count(child) == 3 && status <= 0, "actual NIDs select and describe array");
    const auto* second = stringByIndex(child, 1, &length, &status);
    Require(second && status <= 0 && std::u16string_view(second, length) == u"\u4e8c", "actual NID reads indexed Unicode string");
    auto* item = byIndex(child, 0, nullptr, &status);
    Require(item && status <= 0 && type(item) == 0, "actual NID opens array child by index");
    const auto* first = string(item, &length, &status);
    Require(first && status <= 0 && std::u16string_view(first, length) == u"first", "actual NID reads child string");
    close(item);
    Require(byKey(root, "greeting", child, &status) == child && status <= 0 && std::string_view(key(child)) == "greeting", "actual NIDs preserve fillIn identity and child key");
    close(root);
    greeting = string(child, &length, &status);
    Require(greeting && status <= 0 && std::u16string_view(greeting, length) == u"h\u00e9llo \u4e16\u754c", "actual NID child survives parent close");
    close(child);
    status = 0;
    root = open(nullptr, "en_US", &status);
    Require(root && status <= 0, "actual NID opens explicit locale with fallback");
    child = byKey(root, "NumberElements", nullptr, &status);
    Require(child && status <= 0 && type(child) == 2, "actual NID reads real production locale data beside fixture");
    close(child);
    close(root);
    close(nullptr);
    cleanup();
#ifdef _WIN32
    Require(FreeLibrary(module) != 0, "unload actual ICU artifact after cleanup");
#else
    Require(dlclose(module) == 0, "unload actual ICU artifact after cleanup");
#endif
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 3, "canonical patched ICU path and compiled resource package arguments");
    Check(argv[1], argv[2]);
}
