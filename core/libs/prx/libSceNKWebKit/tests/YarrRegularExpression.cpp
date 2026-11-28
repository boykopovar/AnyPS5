#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::StringImpl;
using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::RegularExpressionPrivate;
using Jsc::YarrAbi::StringReference;

namespace {

const auto Construct = &_ZN3JSC4Yarr17RegularExpressionC1ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE;
const auto Match = &_ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi;
const auto Destroy = &_ZN3JSC4Yarr17RegularExpressionD1Ev;
const auto DestroyBase = &_ZN3JSC4Yarr17RegularExpressionD2Ev;
const auto Assign = &_ZN3JSC4Yarr17RegularExpressionaSERKS1_;
const auto MatchedLength = &_ZNK3JSC4Yarr17RegularExpression13matchedLengthEv;

std::set<void*> live;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (pointer) live.insert(pointer);
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (!pointer) return;
    if (live.erase(pointer) != 1) std::abort();
    std::free(pointer);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = TestAllocate(count * size);
    if (pointer) std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return *pointer ? 0 : 12;
}

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Yarr RegularExpression lifetime check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

RegularExpression Make(const std::string& pattern) {
    StringImpl impl{2, static_cast<unsigned>(pattern.size()), pattern.data(), 4};
    const StringReference reference{&impl};
    RegularExpression expression{};
    Construct(&expression, &reference, 0, 0, 0);
    return expression;
}

int MatchIn(const RegularExpression& expression, const std::string& text) {
    StringImpl impl{2, static_cast<unsigned>(text.size()), text.data(), 4};
    const StringReference reference{&impl};
    return Match(&expression, &reference, 0, nullptr);
}

template<typename TCall>
bool ThrowsInvalid(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

void TestDestructors() {
    RegularExpression first = Make("a+");
    RegularExpressionPrivate* const d = first.d;
    Require(live.count(d) == 1 && d->refCount == 1);
    d->refCount = 2;
    Destroy(&first);
    Require(first.d == d && d->refCount == 1 && live.count(d) == 1);
    DestroyBase(&first);
    Require(live.empty());

    RegularExpression empty{};
    Destroy(&empty);
    DestroyBase(&empty);
    Require(empty.d == nullptr);
    Require(ThrowsInvalid([] { Destroy(nullptr); }));
    Require(ThrowsInvalid([] { DestroyBase(nullptr); }));
}

void TestAssignment() {
    RegularExpression a = Make("a+");
    RegularExpression b = Make("b+");
    RegularExpressionPrivate* const first = a.d;
    RegularExpressionPrivate* const second = b.d;
    Require(live.size() == 2 && MatchIn(b, "xab") == 2);

    Require(Assign(&b, &a) == &b);
    Require(b.d == first && a.d == first && first->refCount == 2);
    Require(live.count(second) == 0 && live.size() == 1);
    Require(MatchIn(b, "xab") == 1);
    Require(first->lastMatchLength == 1);

    Require(Assign(&a, &a) == &a);
    Require(a.d == first && first->refCount == 2 && live.size() == 1);

    RegularExpression empty{};
    Require(Assign(&b, &empty) == &b);
    Require(b.d == nullptr && first->refCount == 1 && live.size() == 1);
    Require(Assign(&b, &a) == &b);
    Require(b.d == first && first->refCount == 2);

    Require(ThrowsInvalid([&] { Assign(nullptr, &a); }));
    Require(ThrowsInvalid([&] { Assign(&a, nullptr); }));
    Require(first->refCount == 2);

    Destroy(&a);
    Require(first->refCount == 1 && live.size() == 1);
    Destroy(&b);
    Require(live.empty());
}

void TestMatchedLength() {
    RegularExpression expression = Make("a+");
    Require(MatchedLength(&expression) == -1);
    Require(MatchIn(expression, "xaa") == 1);
    Require(MatchedLength(&expression) == 2);
    Require(MatchIn(expression, "zzz") == -1);
    Require(MatchedLength(&expression) == -1);
    Require(MatchIn(expression, "aa") == 0);
    Require(MatchedLength(&expression) == 2);
    Require(MatchIn(expression, "a") == 0);
    Require(MatchedLength(&expression) == 1);
    Destroy(&expression);

    RegularExpression empty{};
    Require(ThrowsInvalid([&] { MatchedLength(nullptr); }));
    Require(ThrowsInvalid([&] { MatchedLength(&empty); }));
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestDestructors();
    TestAssignment();
    TestMatchedLength();
    Require(live.empty());
    return 0;
}
