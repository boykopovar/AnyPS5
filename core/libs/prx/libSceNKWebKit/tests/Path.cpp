#include "prx/libSceNKWebKit/include/PathExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::PathAbi::Path;
using NKWebKit::PathAbi::PathElement;
using NKWebKit::PathAbi::PathElementFunction;
using NKWebKit::PathAbi::PathElementFunctionCallableWrapper;
using NKWebKit::PathAbi::PathElementFunctionCallableWrapperVtable;

void Require(const bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore::Path check failed at line %u\n", location.line());
        std::abort();
    }
}

struct OwnedPath {
    OwnedPath() {
        _ZN7WebCore4PathC1Ev(&path);
    }

    Path path{};

    ~OwnedPath() {
        _ZN7WebCore4PathD1Ev(&path);
    }

    OwnedPath(const OwnedPath&) = delete;
    OwnedPath& operator=(const OwnedPath&) = delete;
};

struct Collector {
    PathElementFunctionCallableWrapperVtable const* vtable;
    std::vector<PathElement>* elements;
};

void APS5_VABI Destroy(PathElementFunctionCallableWrapper*) {}

void APS5_VABI Collect(PathElementFunctionCallableWrapper* wrapper, const PathElement* element) {
    auto* self = reinterpret_cast<Collector*>(wrapper);
    self->elements->push_back(*element);
}

const PathElementFunctionCallableWrapperVtable CollectorVtable{Destroy, Destroy, Collect};

void APS5_VABI ThrowFromApply(PathElementFunctionCallableWrapper*, const PathElement*) {
    throw std::runtime_error("WebCore::Path::apply callback");
}

const PathElementFunctionCallableWrapperVtable ThrowerVtable{Destroy, Destroy, ThrowFromApply};

std::vector<PathElement> ElementsOf(const Path& path) {
    std::vector<PathElement> elements;
    Collector collector{&CollectorVtable, &elements};
    PathElementFunction function{reinterpret_cast<PathElementFunctionCallableWrapper*>(&collector)};
    _ZNK7WebCore4Path5applyERKN3WTF8FunctionIFvRKNS_11PathElementEEEE(&path, &function);
    return elements;
}

bool Throws(void (*function)()) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void NullPathMoveTo() {
    WebCore::FloatPoint point{1.0f, 2.0f};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(nullptr, &point);
}

void NullPathApply() {
    PathElementFunction function{nullptr};
    _ZNK7WebCore4Path5applyERKN3WTF8FunctionIFvRKNS_11PathElementEEEE(nullptr, &function);
}

void TestEmpty() {
    OwnedPath path;
    Require(_ZNK7WebCore4Path12elementCountEv(&path.path) == 0);
    const auto bounds = _ZNK7WebCore4Path16fastBoundingRectEv(&path.path);
    Require(bounds.width() == 0 && bounds.height() == 0);
    Require(ElementsOf(path.path).empty());
}

void TestSegments() {
    OwnedPath path;
    const WebCore::FloatPoint move{10.0f, 20.0f};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(&path.path, &move);
    Require(_ZNK7WebCore4Path12elementCountEv(&path.path) == 1);
    auto elements = ElementsOf(path.path);
    Require(elements.size() == 1);
    Require(elements[0].type == PathElement::Type::MoveToPoint);
    Require(elements[0].points[0].m_x == 10.0f && elements[0].points[0].m_y == 20.0f);
    const auto moved = _ZNK7WebCore4Path16fastBoundingRectEv(&path.path);
    Require(moved.width() == 0 && moved.height() == 0);

    const WebCore::FloatPoint line{30.0f, 40.0f};
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&path.path, &line);
    Require(_ZNK7WebCore4Path12elementCountEv(&path.path) == 2);
    elements = ElementsOf(path.path);
    Require(elements[1].type == PathElement::Type::AddLineToPoint);
    Require(elements[1].points[0].m_x == 30.0f && elements[1].points[0].m_y == 40.0f);
    const auto lineBounds = _ZNK7WebCore4Path16fastBoundingRectEv(&path.path);
    Require(lineBounds.x() == 10.0f && lineBounds.y() == 20.0f);
    Require(lineBounds.width() == 20.0f && lineBounds.height() == 20.0f);

    const WebCore::FloatPoint control{40.0f, 10.0f};
    const WebCore::FloatPoint quadEnd{50.0f, 20.0f};
    _ZN7WebCore4Path14addQuadCurveToERKNS_10FloatPointES3_(&path.path, &control, &quadEnd);
    Require(_ZNK7WebCore4Path12elementCountEv(&path.path) == 3);
    elements = ElementsOf(path.path);
    Require(elements[2].type == PathElement::Type::AddQuadCurveToPoint);
    Require(elements[2].points[0].m_x == 40.0f && elements[2].points[1].m_y == 20.0f);

    const WebCore::FloatPoint control1{50.0f, 30.0f};
    const WebCore::FloatPoint control2{60.0f, 10.0f};
    const WebCore::FloatPoint bezierEnd{70.0f, 20.0f};
    _ZN7WebCore4Path16addBezierCurveToERKNS_10FloatPointES3_S3_(&path.path, &control1, &control2, &bezierEnd);
    Require(_ZNK7WebCore4Path12elementCountEv(&path.path) == 4);
    elements = ElementsOf(path.path);
    Require(elements[3].type == PathElement::Type::AddCurveToPoint);
    Require(elements[3].points[0].m_x == 50.0f && elements[3].points[2].m_y == 20.0f);

    _ZN7WebCore4Path12closeSubpathEv(&path.path);
    elements = ElementsOf(path.path);
    Require(elements.size() == 6);
    Require(elements[4].type == PathElement::Type::CloseSubpath);
    Require(elements[5].type == PathElement::Type::MoveToPoint);
}

void TestCopyAndMove() {
    OwnedPath source;
    const WebCore::FloatPoint move{1.0f, 2.0f};
    const WebCore::FloatPoint line{3.0f, 4.0f};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(&source.path, &move);
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&source.path, &line);

    OwnedPath copy;
    _ZN7WebCore4PathC1ERKS0_(&copy.path, &source.path);
    Require(_ZNK7WebCore4Path12elementCountEv(&copy.path) == 2);
    Require(ElementsOf(copy.path).size() == 2);
    const auto copyBounds = _ZNK7WebCore4Path16fastBoundingRectEv(&copy.path);
    Require(copyBounds.width() == 2.0f && copyBounds.height() == 2.0f);

    const WebCore::FloatPoint extra{10.0f, 10.0f};
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&copy.path, &extra);
    Require(_ZNK7WebCore4Path12elementCountEv(&copy.path) == 3);
    Require(_ZNK7WebCore4Path12elementCountEv(&source.path) == 2);

    OwnedPath assigned;
    _ZN7WebCore4PathaSERKS0_(&assigned.path, &source.path);
    Require(_ZNK7WebCore4Path12elementCountEv(&assigned.path) == 2);
    const auto assignedBounds = _ZNK7WebCore4Path16fastBoundingRectEv(&assigned.path);
    Require(assignedBounds.width() == 2.0f && assignedBounds.height() == 2.0f);

    OwnedPath moved;
    _ZN7WebCore4PathaSEOS0_(&moved.path, &source.path);
    Require(_ZNK7WebCore4Path12elementCountEv(&moved.path) == 2);
    Require(source.path.m_path == nullptr);
    Require(_ZNK7WebCore4Path12elementCountEv(&source.path) == 0);

    OwnedPath constructorSource;
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(&constructorSource.path, &move);
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&constructorSource.path, &line);
    alignas(8) unsigned char storage[sizeof(Path)];
    auto* constructed = reinterpret_cast<Path*>(storage);
    _ZN7WebCore4PathC1EOS0_(constructed, &constructorSource.path);
    Require(_ZNK7WebCore4Path12elementCountEv(constructed) == 2);
    Require(ElementsOf(*constructed).size() == 2);
    Require(constructorSource.path.m_path == nullptr);
    Require(constructorSource.path.m_elements.buffer == nullptr);
    Require(_ZNK7WebCore4Path12elementCountEv(&constructorSource.path) == 0);
    _ZN7WebCore4PathD1Ev(constructed);
}

void TestClear() {
    OwnedPath path;
    const WebCore::FloatPoint move{1.0f, 2.0f};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(&path.path, &move);
    _ZN7WebCore4Path5clearEv(&path.path);
    Require(path.path.m_path != nullptr);
    Require(_ZNK7WebCore4Path12elementCountEv(&path.path) == 0);
    Require(ElementsOf(path.path).empty());
}

void TestEmptyCopyAndClear() {
    OwnedPath source;
    alignas(8) unsigned char storage[sizeof(Path)];
    auto* copy = reinterpret_cast<Path*>(storage);
    _ZN7WebCore4PathC1ERKS0_(copy, &source.path);
    Require(copy->m_elementsEngaged);
    const WebCore::FloatPoint copyMove{3.0f, 4.0f};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(copy, &copyMove);
    Require(_ZNK7WebCore4Path12elementCountEv(copy) == 1);
    auto elements = ElementsOf(*copy);
    Require(elements.size() == 1);
    Require(elements[0].type == PathElement::Type::MoveToPoint);
    _ZN7WebCore4PathD1Ev(copy);

    OwnedPath cleared;
    cleared.path.m_elementsEngaged = false;
    _ZN7WebCore4Path5clearEv(&cleared.path);
    Require(cleared.path.m_elementsEngaged);
    const WebCore::FloatPoint start{1.0f, 2.0f};
    const WebCore::FloatPoint end{5.0f, 6.0f};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(&cleared.path, &start);
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&cleared.path, &end);
    Require(_ZNK7WebCore4Path12elementCountEv(&cleared.path) == 2);
    elements = ElementsOf(cleared.path);
    Require(elements.size() == 2);
    Require(elements[1].type == PathElement::Type::AddLineToPoint);
}

void TestApplyException() {
    OwnedPath path;
    const WebCore::FloatPoint move{1.0f, 2.0f};
    const WebCore::FloatPoint line{3.0f, 4.0f};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(&path.path, &move);
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&path.path, &line);
    struct Thrower {
        PathElementFunctionCallableWrapperVtable const* vtable;
    } thrower{&ThrowerVtable};
    PathElementFunction function{reinterpret_cast<PathElementFunctionCallableWrapper*>(&thrower)};
    bool threw = false;
    try {
        _ZNK7WebCore4Path5applyERKN3WTF8FunctionIFvRKNS_11PathElementEEEE(&path.path, &function);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(_ZNK7WebCore4Path12elementCountEv(&path.path) == 2);
}

void TestRoundedRects() {
    OwnedPath path;
    const WebCore::FloatRect rect{{0.0f, 0.0f}, {100.0f, 50.0f}};
    const WebCore::FloatSize radius{10.0f, 10.0f};
    _ZN7WebCore4Path14addRoundedRectERKNS_9FloatRectERKNS_9FloatSizeENS0_19RoundedRectStrategyE(&path.path, &rect, &radius, 0);
    Require(_ZNK7WebCore4Path12elementCountEv(&path.path) == 11);
    const auto bounds = _ZNK7WebCore4Path16fastBoundingRectEv(&path.path);
    Require(bounds.x() == 0.0f && bounds.y() == 0.0f);
    Require(bounds.width() == 100.0f && bounds.height() == 50.0f);

    OwnedPath plain;
    const WebCore::FloatSize huge{1000.0f, 1000.0f};
    const WebCore::FloatRoundedRect unrenderable{rect, {huge, huge, huge, huge}};
    _ZN7WebCore4Path14addRoundedRectERKNS_16FloatRoundedRectENS0_19RoundedRectStrategyE(&plain.path, &unrenderable, 0);
    Require(_ZNK7WebCore4Path12elementCountEv(&plain.path) == 6);
    const auto plainBounds = _ZNK7WebCore4Path16fastBoundingRectEv(&plain.path);
    Require(plainBounds.width() == 100.0f && plainBounds.height() == 50.0f);
}

void TestFaults() {
    Require(Throws(NullPathMoveTo));
    Require(Throws(NullPathApply));
}

}

int main() {
    try {
        TestEmpty();
        TestSegments();
        TestCopyAndMove();
        TestClear();
        TestEmptyCopyAndClear();
        TestApplyException();
        TestRoundedRects();
        TestFaults();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "WebCore::Path test failed: %s\n", error.what());
        return 1;
    }
    std::puts("WebCore::Path tests passed.");
    return 0;
}
