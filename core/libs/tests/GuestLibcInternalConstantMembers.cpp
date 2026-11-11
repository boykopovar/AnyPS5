#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using CategoryName = const char* (APS5_VABI*)(const void*) noexcept;
using VtableName = const char* (APS5_VABI*)(const LibcException::CategoryObject*);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal constant members: %s\n", message);
    std::abort();
}

const char* LibcName(const LibcException::CategoryObject* category) {
    return std::bit_cast<VtableName>(static_cast<const void* const*>(category->vtable)[2])(category);
}

void CheckName(CategoryName name, const LibcException::CategoryObject* category, const char* expected, const char* message) {
    const int other = 0;
    const char* first = name(category);
    Require(first != nullptr && std::strcmp(first, expected) == 0, message);
    Require(name(nullptr) == first && name(&other) == first, "the name does not depend on the object");
    if (category) Require(std::strcmp(LibcName(category), first) == 0, "the name matches the libc category vtable");
}

void CheckCodecvtBase() {
    const int other = 0;
    Require(sceLibcInternalCodecvtBaseMaxLength_nid_no_patch(nullptr) == 1, "codecvt_base::do_max_length returns 1");
    Require(sceLibcInternalCodecvtBaseMaxLength_nid_no_patch(&other) == 1, "codecvt_base::do_max_length ignores the facet");
    Require(sceLibcInternalCodecvtBaseEncoding_nid_no_patch(nullptr) == 1, "codecvt_base::do_encoding returns 1");
    Require(sceLibcInternalCodecvtBaseEncoding_nid_no_patch(&other) == 1, "codecvt_base::do_encoding ignores the facet");
}

}

int main() {
    CheckName(sceLibcInternalSystemErrorCategoryName_nid_no_patch, _ZSt15system_categoryv_nid_postfix(), "system", "_System_error_category::name is system");
    CheckName(sceLibcInternalGenericErrorCategoryName_nid_no_patch, _ZSt16generic_categoryv_nid_postfix(), "generic", "_Generic_error_category::name is generic");
    CheckName(sceLibcInternalFutureErrorCategoryName_nid_no_patch, _ZSt15future_categoryv_nid_postfix(), "future", "_Future_error_category::name is future");
    CheckName(sceLibcInternalIostreamErrorCategoryName_nid_no_patch, nullptr, "iostream", "_Iostream_error_category::name is iostream");
    CheckCodecvtBase();
    std::puts("libc internal constant members passed");
    return 0;
}
