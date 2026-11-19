#include "prx/libSceJsc/include/ClassInfoExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace {

using Jsc::JscAbi::ClassInfo;
using Jsc::JscAbi::HashTable;
using QueryFunction = bool (APS5_VABI*)(const ClassInfo*);

static_assert(std::is_same_v<decltype(&_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv), QueryFunction>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ClassInfo check failed at line %d\n", line);
        std::abort();
    }
}

bool Query(const ClassInfo& info) {
    return _ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(&info);
}

void TestNoTablesIsFalse() {
    const ClassInfo base{"Base", nullptr, nullptr};
    const ClassInfo derived{"Derived", &base, nullptr};
    Check(!Query(base), __LINE__);
    Check(!Query(derived), __LINE__);
}

void TestOwnTableFlag() {
    const HashTable plain{4, 7, false};
    const HashTable flagged{4, 7, true};
    const ClassInfo withPlain{"Plain", nullptr, &plain};
    const ClassInfo withFlagged{"Flagged", nullptr, &flagged};
    Check(!Query(withPlain), __LINE__);
    Check(Query(withFlagged), __LINE__);
}

void TestFlagInheritedFromAncestor() {
    const HashTable plain{2, 3, false};
    const HashTable flagged{1, 1, true};
    const ClassInfo root{"Root", nullptr, &flagged};
    const ClassInfo middle{"Middle", &root, nullptr};
    const ClassInfo leaf{"Leaf", &middle, &plain};
    Check(Query(leaf), __LINE__);
    Check(Query(middle), __LINE__);
    Check(Query(root), __LINE__);
}

void TestAncestorsAboveTheQueryAreIgnored() {
    const HashTable flagged{1, 1, true};
    const HashTable plain{1, 1, false};
    const ClassInfo leaf{"Leaf", nullptr, &plain};
    const ClassInfo unrelated{"Unrelated", nullptr, &flagged};
    Check(!Query(leaf), __LINE__);
    Check(Query(unrelated), __LINE__);
}

void TestEveryPlainTableInChainIsFalse() {
    const HashTable a{0, 0, false};
    const HashTable b{8, 15, false};
    const ClassInfo root{"Root", nullptr, &a};
    const ClassInfo middle{"Middle", &root, &b};
    const ClassInfo leaf{"Leaf", &middle, &a};
    Check(!Query(leaf), __LINE__);
}

void TestNullThisThrows() {
    bool threw = false;
    try {
        _ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw, __LINE__);
}

} // namespace

int main() {
    TestNoTablesIsFalse();
    TestOwnTableFlag();
    TestFlagInheritedFromAncestor();
    TestAncestorsAboveTheQueryAreIgnored();
    TestEveryPlainTableInChainIsFalse();
    TestNullThisThrows();
    return 0;
}
