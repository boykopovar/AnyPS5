#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

using namespace Icu::Abi;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU edits check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Check(false);
}

struct Record {
    Edits edits{};
    Record() { edits.array = edits.stack; edits.capacity = 100; }
    ~Record() { _ZN6icu_685EditsD1Ev_nid_postfix(&edits); }
};

void Encoding() {
    Record record;
    auto& edits = record.edits;
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 0);
    Check(edits.length == 0);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 1);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 4096);
    Check(edits.length == 2 && edits.array[0] == 0xfff && edits.array[1] == 0);
    _ZN6icu_685Edits5resetEv_nid_postfix(&edits);
    for (int count = 0; count < 513; ++count) _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 1, 2);
    Check(edits.length == 2 && edits.array[0] == 0x15ff && edits.array[1] == 0x1400);
    Check(edits.changes == 513 && edits.delta == 513);
    _ZN6icu_685Edits5resetEv_nid_postfix(&edits);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 0, 61);
    Check(edits.length == 2 && edits.array[0] == 0x703d && edits.array[1] == 0x803d);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 32768, 0);
    Check(edits.length == 5 && edits.array[2] == 0x7f80 && edits.array[3] == 0x8001 && edits.array[4] == 0x8000);
    _ZN6icu_685Edits5resetEv_nid_postfix(&edits);
    const auto maximum = std::numeric_limits<std::int32_t>::max();
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, maximum, maximum);
    Check(edits.length == 5 && edits.array[0] == 0x7fff && edits.delta == 0);
    for (int index = 1; index < 5; ++index) Check(edits.array[index] == 0xffff);
    _ZN6icu_685Edits5resetEv_nid_postfix(&edits);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 0, maximum);
    const auto length = edits.length;
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 0, 1);
    Check(edits.error == 8 && edits.length == length && edits.changes == 2 && edits.delta == maximum);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 12);
    Check(edits.length == length);
    std::int32_t status = 4;
    Check(_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(&edits, &status) && status == 4);
    status = -1;
    Check(_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(&edits, &status) && status == 8);
    _ZN6icu_685Edits5resetEv_nid_postfix(&edits);
    Check(edits.error == 0 && edits.changes == 0 && edits.length == 0 && edits.delta == 0);
    status = -1;
    Check(!_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(&edits, &status) && status == -1);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, -1);
    Check(edits.error == 1);
    _ZN6icu_685Edits5resetEv_nid_postfix(&edits);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, -1, 0);
    Check(edits.error == 1 && edits.length == 0);
    _ZN6icu_685Edits6appendEi_nid_postfix(&edits, 0x12345);
    Check(edits.array[0] == 0x2345 && edits.error == 1);
    Invalid([] { _ZN6icu_685Edits5resetEv_nid_postfix(nullptr); });
}

void StorageTests() {
    Record first;
    Record second;
    auto& source = first.edits;
    auto& target = second.edits;
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&source, 4);
    Check(_ZN6icu_685EditsaSERKS0__nid_postfix(&target, &source) == &target);
    Check(target.array == target.stack && target.length == 1 && target.array[0] == 3);
    Check(_ZN6icu_685EditsaSEOS0__nid_postfix(&target, &source) == &target);
    Check(source.length == 1 && target.array[0] == 3);
    Invalid([&] { _ZN6icu_685EditsaSEOS0__nid_postfix(&source, &source); });
    _ZN6icu_685Edits5resetEv_nid_postfix(&source);
    for (int index = 0; index < 250; ++index) {
        _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&source, 1);
        _ZN6icu_685Edits10addReplaceEii_nid_postfix(&source, 1, 2);
    }
    Check(source.length == 500 && source.capacity == 2000 && source.array != source.stack);
    _ZN6icu_685EditsaSERKS0__nid_postfix(&target, &source);
    Check(target.capacity == 500 && target.length == 500 && target.array != source.array);
    for (int index = 0; index < 500; ++index) Check(target.array[index] == source.array[index]);
    auto* array = source.array;
    _ZN6icu_685EditsaSEOS0__nid_postfix(&target, &source);
    Check(target.array == array && source.array == source.stack && source.capacity == 100 && source.length == 0);
    Check(_ZN6icu_685Edits9growArrayEv_nid_postfix(&target) && target.capacity == 4000);
    source.length = target.length;
    source.delta = target.delta;
    source.changes = target.changes;
    Check(_ZN6icu_685Edits9copyArrayERKS0__nid_postfix(&source, &target) == &source);
    Check(source.capacity == 500 && source.array[1] == 0x1400);
    _ZN6icu_685Edits5resetEv_nid_postfix(&source);
    source.error = 3;
    source.length = source.delta = source.changes = 8;
    _ZN6icu_685Edits9copyArrayERKS0__nid_postfix(&source, &target);
    Check(source.length == 0 && source.delta == 0 && source.changes == 0 && source.error == 3);
    _ZN6icu_685Edits9moveArrayERS0__nid_postfix(&source, &target);
    Check(source.length == 0 && target.length == 500);
    Edits limit{};
    limit.capacity = std::numeric_limits<std::int32_t>::max();
    Check(!_ZN6icu_685Edits9growArrayEv_nid_postfix(&limit) && limit.error == 8);
    _ZN6icu_685EditsD2Ev_nid_postfix(&limit);
    Edits empty{};
    empty.array = empty.stack;
    empty.capacity = 100;
    _ZN6icu_685Edits12releaseArrayEv_nid_postfix(&empty);
}

void Iteration() {
    Record record;
    auto& edits = record.edits;
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 3);
    for (int count = 0; count < 4; ++count) _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 1, 2);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 61, 32768);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 1);
    std::int32_t status = 0;
    for (std::int8_t coarse : {0, -3}) {
        EditsIterator iterator;
        _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(&iterator, edits.array, edits.length, -7, coarse);
        Check(iterator.onlyChanges == -7 && iterator.coarse == coarse);
        std::vector<std::array<int, 3>> spans;
        while (_ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 0, &status))
            spans.push_back({iterator.changed, iterator.oldLength, iterator.newLength});
        if (coarse) Check(spans == std::vector<std::array<int, 3>>{{0, 3, 3}, {1, 65, 32776}, {0, 1, 1}});
        else Check(spans == std::vector<std::array<int, 3>>{{0, 3, 3}, {1, 1, 2}, {1, 1, 2}, {1, 1, 2}, {1, 1, 2}, {1, 61, 32768}, {0, 1, 1}});
        Check(iterator.sourceIndex == 69 && iterator.destinationIndex == 32780 && !iterator.changed);
        for (auto index = spans.size(); index > 0; --index) {
            Check(_ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(&iterator, &status));
            Check((std::array<int, 3>{iterator.changed, iterator.oldLength, iterator.newLength} == spans[index - 1]));
        }
        Check(!_ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(&iterator, &status));
        Check(iterator.sourceIndex == 0 && iterator.destinationIndex == 0);
        _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(&iterator, edits.array, edits.length, 1, coarse);
        Check(_ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, -8, &status));
        Check(iterator.changed && iterator.sourceIndex == 3 && iterator.destinationIndex == 3);
        Check(_ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(&iterator, &status));
        Check(_ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 1, &status));
        Check(iterator.sourceIndex == 3);
    }
    EditsIterator iterator;
    _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(&iterator, nullptr, 1, 0, 0);
    status = 3;
    Check(!_ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 0, &status) && iterator.index == 0);
    status = 0;
    Invalid([&] { _ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 0, &status); });
    const std::uint16_t lengths[]{0xffff, 0xffff};
    _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(&iterator, lengths, 2, 0, 0);
    Check(_ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(&iterator, 60) == 60 && iterator.index == 0);
    Check(_ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(&iterator, 63) == std::numeric_limits<std::int32_t>::max());
    Invalid([&] { _ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(&iterator, 61); });
    iterator.oldLength = 2;
    iterator.newLength = 3;
    iterator.changed = -1;
    _ZN6icu_685Edits8Iterator17updateNextIndexesEv_nid_postfix(&iterator);
    Check(iterator.sourceIndex == 2 && iterator.destinationIndex == 3 && iterator.replacementIndex == 3);
    _ZN6icu_685Edits8Iterator21updatePreviousIndexesEv_nid_postfix(&iterator);
    Check(iterator.sourceIndex == 0 && iterator.destinationIndex == 0 && iterator.replacementIndex == 0);
    Check(!_ZN6icu_685Edits8Iterator6noNextEv_nid_postfix(&iterator) && iterator.oldLength == 0 && iterator.newLength == 0);
}

void Mapping() {
    Record record;
    auto& edits = record.edits;
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 3);
    for (int index = 0; index < 40; ++index) _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 1, 2);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 1);
    std::int32_t status = 0;
    EditsIterator iterator;
    _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(&iterator, edits.array, edits.length, 0, 0);
    for (int index : {0, 2, 3, 20, 39, 43, 44, 42, 40, 25, 4, 1, 10, 30, 44, 80, -1}) {
        const auto expected = index < 0 ? 0 : index < 3 ? index : index <= 43 ? 3 + (index - 3) * 2 : 84;
        Check(_ZN6icu_685Edits8Iterator31destinationIndexFromSourceIndexEiR10UErrorCode_nid_postfix(&iterator, index, &status) == expected);
    }
    for (int index : {0, 2, 3, 4, 22, 24, 80, 83, 84, 82, 81, 60, 20, 3, 1, 30, 84, 90, -1}) {
        const auto expected = index < 0 ? 0 : index < 3 ? index : index <= 83 ? 3 + (index - 3 + 1) / 2 : 44;
        Check(_ZN6icu_685Edits8Iterator31sourceIndexFromDestinationIndexEiR10UErrorCode_nid_postfix(&iterator, index, &status) == expected);
    }
    Check(_ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(&iterator, 44, 1, &status) == 1);
    Check(_ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(&iterator, 2, -7, &status) == 0);
    Check(_ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(&iterator, -1, 0, &status) == -1);
    status = 3;
    Check(_ZN6icu_685Edits8Iterator31destinationIndexFromSourceIndexEiR10UErrorCode_nid_postfix(&iterator, 1, &status) == 0);
}

void Merge() {
    Record first;
    Record second;
    Record result;
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&first.edits, 3);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&first.edits, 2, 4);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&first.edits, 2);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&second.edits, 4);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&second.edits, 2, 1);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&second.edits, 3);
    std::int32_t status = 0;
    Check(_ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&result.edits, &first.edits, &second.edits, &status) == &result.edits);
    Check(status == 0 && result.edits.delta == 1 && result.edits.changes == 1);
    EditsIterator iterator;
    _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(&iterator, result.edits.array, result.edits.length, 0, 0);
    for (auto span : std::vector<std::array<int, 3>>{{0, 3, 3}, {1, 2, 3}, {0, 2, 2}}) {
        Check(_ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 0, &status));
        Check((std::array<int, 3>{iterator.changed, iterator.oldLength, iterator.newLength} == span));
    }
    Check(!_ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 0, &status));
    _ZN6icu_685Edits5resetEv_nid_postfix(&second.edits);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&second.edits, 10);
    _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&result.edits, &first.edits, &second.edits, &status);
    Check(status == 1);
    status = 3;
    const auto length = result.edits.length;
    _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&result.edits, nullptr, nullptr, &status);
    Check(status == 3 && result.edits.length == length);
}

}

int main() {
    Encoding();
    StorageTests();
    Iteration();
    Mapping();
    Merge();
    std::puts("ICU edits tests passed");
}
