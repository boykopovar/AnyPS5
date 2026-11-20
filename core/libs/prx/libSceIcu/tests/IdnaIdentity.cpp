#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "unicode/uidna.h"
#include <cstdlib>
#include <string_view>

namespace {

using IcuRecoveredTest::Require;

struct ReusedBlock {
    void* storage = nullptr;
    std::size_t size = 0;
    bool useForNextAllocation = false;
    int releases = 0;
};

void* APS5_VABI Allocate(const void* context, std::size_t size) {
    auto& block = *static_cast<ReusedBlock*>(const_cast<void*>(context));
    if (!block.useForNextAllocation) return std::malloc(size);
    block.useForNextAllocation = false;
    if (block.storage) {
        Require(size == block.size && block.releases == 1, "reopen requests the released native block");
    } else {
        block.storage = std::malloc(size);
        block.size = size;
    }
    return block.storage;
}

void* APS5_VABI Reallocate(const void* context, void* pointer, std::size_t size) {
    const auto& block = *static_cast<const ReusedBlock*>(context);
    Require(!pointer || pointer != block.storage, "the pinned UIDNA object has a stable allocation size");
    return std::realloc(pointer, size);
}

void APS5_VABI Release(const void* context, void* pointer) {
    auto& block = *static_cast<ReusedBlock*>(const_cast<void*>(context));
    if (pointer && pointer == block.storage) ++block.releases;
    else std::free(pointer);
}

}

void CheckIcuIdnaIdentity() {
    ReusedBlock block;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&block, Allocate, Reallocate, Release, &status);
    Require(status == 0, "install the controlled native ICU allocator");
    auto* warm = uidna_openUTS46_nid_postfix(0, &status);
    Require(warm && status == 0, "warm the UTS46 normalization data");
    uidna_close_nid_postfix(warm);

    block.useForNextAllocation = true;
    auto* first = uidna_openUTS46_nid_postfix(0, &status);
    Require(first && status == 0 && block.storage, "capture the native allocation for the first open");
    uidna_close_nid_postfix(first);
    Require(block.releases == 1, "closing the first handle releases its native allocation");

    block.useForNextAllocation = true;
    auto* second = uidna_openUTS46_nid_postfix(UIDNA_NONTRANSITIONAL_TO_ASCII, &status);
    Require(second && status == 0 && !block.useForNextAllocation, "reuse the native allocation on reopen");
    Require(first != second, "distinct guest handles when ICU reuses its allocation");
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    char16_t output[32]{};
    IcuRecoveredTest::RequireThrows([&] { uidna_nameToASCII_nid_postfix(first, u"abc", -1, output, 32, &info, &status); });
    IcuRecoveredTest::RequireThrows([&] { uidna_close_nid_postfix(first); });
    Require(block.releases == 1, "stale close leaves the new backend allocation live");
    Require(uidna_nameToASCII_nid_postfix(second, u"faß.de", -1, output, 32, &info, &status) == 13 &&
            std::u16string_view(output) == u"xn--fa-hia.de" && status == 0, "the new handle keeps its own options");
    uidna_close_nid_postfix(second);
    Require(block.releases == 2, "closing the second handle releases its native allocation");
    u_cleanup_nid_postfix();
    std::free(block.storage);
}
