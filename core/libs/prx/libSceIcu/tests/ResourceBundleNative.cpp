#include "unicode/ures.h"
#include "unicode/udata.h"
#include "unicode/uclean.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string_view>
#include <vector>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "native resource control failed: %s\n", message);
    std::abort();
}

void Check(const char* path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    Require(bool(input), "open official test-only package");
    const auto size = input.tellg();
    Require(size > 0, "read package size");
    std::vector<std::max_align_t> storage((static_cast<std::size_t>(size) + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(storage.data()), size);
    Require(bool(input), "read complete aligned package");
    UErrorCode status = U_ZERO_ERROR;
    udata_setCommonData(storage.data(), &status);
    Require(U_SUCCESS(status), "register real packaged data before resource use");
    udata_setFileAccess(UDATA_NO_FILES, &status);
    Require(U_SUCCESS(status), "use registered packages without external files");
    auto* root = ures_openDirect(nullptr, "aps5ResourceFixture", &status);
    Require(root && U_SUCCESS(status) && ures_getType(root) == URES_TABLE && ures_getSize(root) == 9, "official genrb/icupkg fixture is a real resource table");
    std::int32_t length = -1;
    const auto* greeting = ures_getStringByKey(root, "greeting", &length, &status);
    Require(greeting && U_SUCCESS(status) && std::u16string_view(greeting, length) == u"h\u00e9llo \u4e16\u754c", "Unicode string data");
    auto* child = ures_getByKey(root, "number", nullptr, &status);
    Require(child && ures_getType(child) == URES_INT && ures_getInt(child, &status) == 1234, "positive integer resource");
    Require(ures_getByKey(root, "negative", child, &status) == child && ures_getInt(child, &status) == -7, "signed integer and fillIn identity");
    Require(ures_getByKey(root, "bytes", child, &status) == child, "binary child lookup");
    const auto* bytes = ures_getBinary(child, &length, &status);
    Require(bytes && length == 4 && bytes[0] == 0 && bytes[1] == 0x10 && bytes[2] == 0xff && bytes[3] == 0, "binary byte resource");
    ures_getByKey(root, "vector", child, &status);
    const auto* vector = ures_getIntVector(child, &length, &status);
    Require(vector && length == 3 && vector[0] == 1 && vector[1] == -2 && vector[2] == 17, "integer vector resource");
    ures_getByKey(root, "empty", child, &status);
    bytes = ures_getBinary(child, &length, &status);
    Require(bytes && length == 0 && U_SUCCESS(status), "empty binary resource");
    const auto* alias = ures_getStringByKey(root, "alias", &length, &status);
    Require(alias && std::u16string_view(alias, length) == u"nested" && U_SUCCESS(status), "real alias resolution");
    ures_getByKey(root, "values", child, &status);
    Require(ures_getType(child) == URES_ARRAY && ures_getSize(child) == 3, "array resource");
    const auto* second = ures_getStringByIndex(child, 1, &length, &status);
    Require(second && std::u16string_view(second, length) == u"\u4e8c", "indexed Unicode resource");
    ures_close(root);
    const auto* first = ures_getStringByIndex(child, 0, &length, &status);
    Require(first && std::u16string_view(first, length) == u"first", "child stays usable after parent close");
    ures_close(child);
    auto* systems = ures_openDirect(nullptr, "numberingSystems", &status);
    auto* table = ures_getByKey(systems, "numberingSystems", nullptr, &status);
    auto* latin = ures_getByKey(table, "latn", nullptr, &status);
    const auto* digits = ures_getStringByKey(latin, "desc", &length, &status);
    Require(digits && U_SUCCESS(status) && std::u16string_view(digits, length) == u"0123456789", "full production CLDR data retained beside fixture");
    ures_close(latin);
    ures_close(table);
    ures_close(systems);
    u_cleanup();
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "test-only official package path");
    Check(argv[1]);
}
