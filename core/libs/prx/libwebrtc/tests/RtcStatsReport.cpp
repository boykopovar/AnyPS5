#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RTCStatsReport::ConstIterator check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

WebRtcRtcStatsReportConstIterator* Next(WebRtcRtcStatsReportConstIterator* iterator) {
    return _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(iterator);
}

WebRtcRtcStatsReportConstIterator* NextPostfix(WebRtcRtcStatsReportConstIterator* iterator) {
    return _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(iterator, 0);
}

bool NotEqual(const WebRtcRtcStatsReportConstIterator& left, const WebRtcRtcStatsReportConstIterator& right) {
    return _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&left, &right);
}

const void* Dereference(const WebRtcRtcStatsReportConstIterator& iterator) {
    return _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&iterator);
}

const void* Arrow(const WebRtcRtcStatsReportConstIterator& iterator) {
    return _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&iterator);
}

WebRtcStatsMapNode* End(WebRtcRtcStatsReport& report) {
    return reinterpret_cast<WebRtcStatsMapNode*>(&report.stats.root);
}

struct Fixture {
    WebRtcRtcStatsReport report{};
    std::array<WebRtcStatsMapNode, 5> nodes{};
    std::array<int, 5> stats{};

    Fixture() {
        auto& [a, b, c, d, e] = nodes;
        report.refCount = 1;
        report.timestampUs = 1234;
        report.stats.root = &b;
        report.stats.beginNode = &a;
        report.stats.size = nodes.size();
        b.links = {&a, &d, End(report), true};
        a.links = {nullptr, nullptr, &b, true};
        d.links = {&c, &e, &b, false};
        c.links = {nullptr, nullptr, &d, true};
        e.links = {nullptr, nullptr, &d, true};
        for (std::size_t index = 0; index < nodes.size(); ++index)
            nodes[index].stats = &stats[index];
    }

    WebRtcRtcStatsReportConstIterator Begin() { return {&report, report.stats.beginNode}; }
    WebRtcRtcStatsReportConstIterator EndIterator() { return {&report, End(report)}; }
};

void TestWalksInKeyOrder() {
    Fixture fixture;
    auto iterator = fixture.Begin();
    const auto end = fixture.EndIterator();
    std::size_t index = 0;
    while (NotEqual(iterator, end)) {
        Require(index < fixture.stats.size());
        Require(Arrow(iterator) == &fixture.stats[index]);
        Require(Dereference(iterator) == &fixture.stats[index]);
        Require((index % 2 == 0 ? Next(&iterator) : NextPostfix(&iterator)) == &iterator);
        Require(iterator.report == &fixture.report);
        ++index;
    }
    Require(index == fixture.stats.size());
    Require(iterator.node == end.node);
    Require(!NotEqual(iterator, end));
}

void TestSingleEntryReachesEnd() {
    WebRtcRtcStatsReport report{};
    WebRtcStatsMapNode node{};
    int stats = 0;
    node.links = {nullptr, nullptr, End(report), true};
    node.stats = &stats;
    report.stats = {&node, &node, 1};
    WebRtcRtcStatsReportConstIterator iterator{&report, &node};
    Require(Next(&iterator) == &iterator);
    Require(iterator.node == End(report));
}

void TestNotEqualComparesPositionsOnly() {
    Fixture fixture;
    Fixture other;
    const WebRtcRtcStatsReportConstIterator first{&fixture.report, &fixture.nodes[2]};
    const WebRtcRtcStatsReportConstIterator second{&other.report, &fixture.nodes[2]};
    const WebRtcRtcStatsReportConstIterator third{&fixture.report, &fixture.nodes[3]};
    Require(!NotEqual(first, second));
    Require(NotEqual(first, third));
}

void TestArrowReturnsANullEntry() {
    Fixture fixture;
    fixture.nodes[1].stats = nullptr;
    const WebRtcRtcStatsReportConstIterator iterator{&fixture.report, &fixture.nodes[1]};
    Require(Arrow(iterator) == nullptr);
    RequireInvalidArgument([&] { Dereference(iterator); });
}

void TestEndAndInvalidIteratorsThrow() {
    Fixture fixture;
    auto end = fixture.EndIterator();
    RequireInvalidArgument([&] { Next(&end); });
    RequireInvalidArgument([&] { NextPostfix(&end); });
    RequireInvalidArgument([&] { Dereference(end); });
    RequireInvalidArgument([&] { Arrow(end); });
    Require(end.node == End(fixture.report));

    WebRtcRtcStatsReportConstIterator noReport{nullptr, &fixture.nodes[0]};
    RequireInvalidArgument([&] { Next(&noReport); });
    RequireInvalidArgument([&] { Arrow(noReport); });
    WebRtcRtcStatsReportConstIterator noNode{&fixture.report, nullptr};
    RequireInvalidArgument([&] { Next(&noNode); });
    RequireInvalidArgument([&] { Dereference(noNode); });

    const auto begin = fixture.Begin();
    RequireInvalidArgument([&] { Next(nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&begin, nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(nullptr, &begin); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(nullptr); });
}

void TestMalformedTreesThrow() {
    Fixture orphan;
    orphan.nodes[4].links.parent = nullptr;
    WebRtcRtcStatsReportConstIterator last{&orphan.report, &orphan.nodes[4]};
    RequireInvalidArgument([&] { Next(&last); });
    Require(last.node == &orphan.nodes[4]);

    Fixture detached;
    detached.report.stats.root = &detached.nodes[3];
    WebRtcRtcStatsReportConstIterator lastDetached{&detached.report, &detached.nodes[4]};
    RequireInvalidArgument([&] { Next(&lastDetached); });

    Fixture cycle;
    cycle.nodes[3].links.left = &cycle.nodes[3];
    WebRtcRtcStatsReportConstIterator second{&cycle.report, &cycle.nodes[1]};
    RequireInvalidArgument([&] { Next(&second); });

    Fixture undersized;
    undersized.report.stats.size = 1;
    WebRtcRtcStatsReportConstIterator first{&undersized.report, &undersized.nodes[1]};
    RequireInvalidArgument([&] { Next(&first); });
}

}

int main() {
    TestWalksInKeyOrder();
    TestSingleEntryReachesEnd();
    TestNotEqualComparesPositionsOnly();
    TestArrowReturnsANullEntry();
    TestEndAndInvalidIteratorsThrow();
    TestMalformedTreesThrow();
    return 0;
}
