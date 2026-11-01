// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>
#include <vector>

namespace {
void Check(bool ok) {
    if (!ok) std::abort();
}

struct Range {
    std::int32_t start;
    std::int32_t end;
    std::uint32_t value;
};

std::int8_t APS5_VABI CollectRange(const void* context, std::int32_t start, std::int32_t end, std::uint32_t value) {
    auto* list = const_cast<std::vector<Range>*>(static_cast<const std::vector<Range>*>(context));
    list->push_back({start, end, value});
    return 1;
}

std::uint32_t APS5_VABI TransformValue(const void*, std::uint32_t value) {
    return value + 1;
}
}

int main() {
    std::int32_t status = 0;
    auto* trie = utrie2_openDummy_nid_postfix(1, 0x1234, 0x9999, &status);
    Check(trie != nullptr && status == 0);

    std::vector<Range> ranges;
    utrie2_enum_nid_postfix(trie, nullptr, CollectRange, &ranges);
    Check(ranges.size() == 1);
    Check(ranges[0].start == 0 && ranges[0].end == 0x10ffff && ranges[0].value == 0x1234);

    ranges.clear();
    utrie2_enum_nid_postfix(trie, TransformValue, CollectRange, &ranges);
    Check(ranges.size() == 1);
    Check(ranges[0].start == 0 && ranges[0].end == 0x10ffff && ranges[0].value == 0x1235);

    ranges.clear();
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0xd800, nullptr, CollectRange, &ranges);
    Check(ranges.size() == 1);
    Check(ranges[0].start == 0x10000 && ranges[0].end == 0x103ff && ranges[0].value == 0x1234);

    ranges.clear();
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0xdc00, nullptr, CollectRange, &ranges);
    Check(ranges.empty());

    utrie2_close_nid_postfix(trie);
    return 0;
}
