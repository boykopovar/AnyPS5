#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::ContainerNode;
using NKWebKit::WebCoreAbi::ExceptionOrNode;
using NKWebKit::WebCoreAbi::Node;
using NKWebKit::WebCoreAbi::NodeFilter;
using NKWebKit::WebCoreAbi::NodeFilterAcceptResult;
using NKWebKit::WebCoreAbi::NodeFilterVtable;
using NKWebKit::WebCoreAbi::NodeFlagIsContainerNode;
using NKWebKit::WebCoreAbi::NodeVtable;
using NKWebKit::WebCoreAbi::TreeWalker;

constexpr std::int32_t ElementNode = 1;
constexpr std::int32_t TextNode = 3;
constexpr std::int32_t CommentNode = 8;
constexpr std::uint32_t ShowAll = 0xFFFFFFFFu;
constexpr std::uint32_t ShowElement = 0x1u;
constexpr std::uint32_t ShowComment = 0x80u;
constexpr std::uint16_t Accept = 1;
constexpr std::uint16_t Reject = 2;
constexpr std::uint16_t Skip = 3;
constexpr std::int32_t Success = 0;
constexpr std::int32_t ExceptionThrown = 1;
constexpr std::int32_t UnableToExecute = 2;
constexpr std::int32_t InvalidStateError = 8;
constexpr std::int32_t ExistingExceptionError = 36;
constexpr std::uint32_t OneRef = 2;
constexpr std::uint32_t ParentBit = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "tree walker check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename Exception, typename Fn>
void RequireThrows(Fn&& fn, std::source_location location = std::source_location::current()) {
    bool caught = false;
    try {
        fn();
    } catch (const Exception&) {
        caught = true;
    }
    Require(caught, location);
}

struct TestNode {
    ContainerNode node;
    std::int32_t type;
    const char* name;
};

std::int32_t APS5_VABI TestNodeType(const Node* self) {
    return reinterpret_cast<const TestNode*>(self)->type;
}

const NodeVtable testNodeVtable = [] {
    NodeVtable vtable{};
    vtable.nodeType = TestNodeType;
    return vtable;
}();

class Tree {
    std::deque<TestNode> nodes;

public:
    Node* Make(const char* name, std::int32_t type = ElementNode) {
        TestNode& created = nodes.emplace_back();
        std::memset(&created, 0, sizeof(created));
        created.node.vtable = const_cast<NodeVtable*>(&testNodeVtable);
        created.node.refCountAndParentBit = OneRef;
        if (type != TextNode && type != CommentNode) created.node.nodeFlags = NodeFlagIsContainerNode;
        created.type = type;
        created.name = name;
        return &created.node;
    }

    static void Append(Node* parent, Node* child) {
        auto* container = static_cast<ContainerNode*>(parent);
        child->parentNode = container;
        child->refCountAndParentBit |= ParentBit;
        child->previous = container->lastChild;
        if (container->lastChild) container->lastChild->next = child;
        else container->firstChild = child;
        container->lastChild = child;
    }
};

const char* NameOf(const Node* node) {
    return node ? reinterpret_cast<const TestNode*>(node)->name : "null";
}

struct TestFilter {
    NodeFilter filter;
    std::vector<std::string> rejected;
    std::vector<std::string> skipped;
    std::string throwsOn;
    std::string cannotRunOn;
    TreeWalker* reenter = nullptr;
    ExceptionOrNode reentered{};
    std::vector<std::string> calls;
};

bool Contains(const std::vector<std::string>& names, const char* name) {
    for (const auto& value : names) {
        if (value == name) return true;
    }
    return false;
}

NodeFilterAcceptResult APS5_VABI TestAcceptNode(NodeFilter* self, Node* node) {
    auto* filter = reinterpret_cast<TestFilter*>(self);
    const char* name = NameOf(node);
    filter->calls.emplace_back(name);
    NodeFilterAcceptResult result{};
    if (filter->reenter) {
        _ZN7WebCore10TreeWalker8nextNodeEv(&filter->reentered, filter->reenter);
        filter->reenter = nullptr;
    }
    if (filter->throwsOn == name) {
        result.type = ExceptionThrown;
        result.hasValue = false;
        return result;
    }
    if (filter->cannotRunOn == name) {
        result.type = UnableToExecute;
        result.hasValue = false;
        return result;
    }
    result.value = Contains(filter->rejected, name) ? Reject : Contains(filter->skipped, name) ? Skip : Accept;
    result.hasValue = true;
    return result;
}

const NodeFilterVtable testFilterVtable = [] {
    NodeFilterVtable vtable{};
    vtable.acceptNode = TestAcceptNode;
    return vtable;
}();

void InitFilter(TestFilter& filter) {
    filter.filter.vtable = &testFilterVtable;
    filter.filter.scriptExecutionContext = nullptr;
    filter.filter.refCount = 1;
}

struct Walker {
    TreeWalker walker{};

    Walker(Node* root, std::uint32_t whatToShow, NodeFilter* filter = nullptr) {
        walker.refCount = 1;
        walker.root = root;
        walker.filter = filter;
        walker.whatToShow = whatToShow;
        walker.isActive = false;
        walker.current = root;
        root->refCountAndParentBit += 2 * OneRef;
    }
};

using Step = ExceptionOrNode* (APS5_VABI*)(ExceptionOrNode*, TreeWalker*);

Node* Call(Step step, TreeWalker& walker, std::source_location location = std::source_location::current()) {
    ExceptionOrNode result{};
    std::memset(&result, 0xCD, sizeof(result));
    Require(step(&result, &walker) == &result, location);
    Require(result.hasValue, location);
    Require(!walker.isActive, location);
    return result.value;
}

void RequireSequence(Step step, TreeWalker& walker, std::initializer_list<const char*> names, std::source_location location = std::source_location::current()) {
    for (const char* name : names) {
        Node* node = Call(step, walker, location);
        Require(std::strcmp(NameOf(node), name) == 0, location);
        Require(walker.current == node, location);
    }
    Node* last = walker.current;
    Require(Call(step, walker, location) == nullptr, location);
    Require(walker.current == last, location);
}

void SetCurrent(TreeWalker& walker, Node* node) {
    _ZN7WebCore10TreeWalker14setCurrentNodeERNS_4NodeE(&walker, node);
}

struct Sample {
    Tree tree;
    Node* root = tree.Make("root");
    Node* a = tree.Make("A");
    Node* a1 = tree.Make("A1");
    Node* a2 = tree.Make("A2", TextNode);
    Node* b = tree.Make("B");
    Node* b1 = tree.Make("B1");
    Node* b1a = tree.Make("B1a");
    Node* c = tree.Make("C", CommentNode);

    Sample() {
        Tree::Append(root, a);
        Tree::Append(a, a1);
        Tree::Append(a, a2);
        Tree::Append(root, b);
        Tree::Append(b, b1);
        Tree::Append(b1, b1a);
        Tree::Append(root, c);
    }

    std::vector<Node*> All() const { return {root, a, a1, a2, b, b1, b1a, c}; }
};

constexpr Step NextNode = _ZN7WebCore10TreeWalker8nextNodeEv;
constexpr Step PreviousNode = _ZN7WebCore10TreeWalker12previousNodeEv;
constexpr Step FirstChild = _ZN7WebCore10TreeWalker10firstChildEv;
constexpr Step LastChild = _ZN7WebCore10TreeWalker9lastChildEv;
constexpr Step ParentNode = _ZN7WebCore10TreeWalker10parentNodeEv;
constexpr Step NextSibling = _ZN7WebCore10TreeWalker11nextSiblingEv;
constexpr Step PreviousSibling = _ZN7WebCore10TreeWalker15previousSiblingEv;

void TestDocumentOrder() {
    Sample sample;
    Walker walker(sample.root, ShowAll);
    RequireSequence(NextNode, walker.walker, {"A", "A1", "A2", "B", "B1", "B1a", "C"});
    RequireSequence(PreviousNode, walker.walker, {"B1a", "B1", "B", "A2", "A1", "A", "root"});
}

void TestWhatToShow() {
    Sample sample;
    Walker elements(sample.root, ShowElement);
    RequireSequence(NextNode, elements.walker, {"A", "A1", "B", "B1", "B1a"});
    RequireSequence(PreviousNode, elements.walker, {"B1", "B", "A1", "A", "root"});

    Walker comments(sample.root, ShowComment);
    RequireSequence(NextNode, comments.walker, {"C"});
    Require(Call(FirstChild, comments.walker) == nullptr);
    RequireSequence(PreviousNode, comments.walker, {});

    Walker nothing(sample.root, 0);
    RequireSequence(NextNode, nothing.walker, {});
    Require(Call(FirstChild, nothing.walker) == nullptr);
    Require(Call(LastChild, nothing.walker) == nullptr);
    Require(nothing.walker.current == sample.root);
}

void TestStructuralSteps() {
    Sample sample;
    Walker walker(sample.root, ShowAll);
    TreeWalker& w = walker.walker;
    Require(Call(ParentNode, w) == nullptr);
    Require(Call(NextSibling, w) == nullptr);
    Require(Call(PreviousSibling, w) == nullptr);
    Require(Call(FirstChild, w) == sample.a);
    Require(Call(NextSibling, w) == sample.b);
    Require(Call(NextSibling, w) == sample.c);
    Require(Call(NextSibling, w) == nullptr);
    Require(w.current == sample.c);
    Require(Call(FirstChild, w) == nullptr);
    Require(Call(PreviousSibling, w) == sample.b);
    Require(Call(LastChild, w) == sample.b1);
    Require(Call(LastChild, w) == sample.b1a);
    Require(Call(ParentNode, w) == sample.b1);
    Require(Call(ParentNode, w) == sample.b);
    Require(Call(ParentNode, w) == sample.root);
    Require(Call(ParentNode, w) == nullptr);
    Require(Call(LastChild, w) == sample.c);
    Require(Call(PreviousSibling, w) == sample.b);
    Require(Call(PreviousSibling, w) == sample.a);
    Require(Call(PreviousSibling, w) == nullptr);
    Require(w.current == sample.a);

    SetCurrent(w, sample.a2);
    Require(Call(NextSibling, w) == nullptr);
    Require(w.current == sample.a2);
    SetCurrent(w, sample.a1);
    Require(Call(PreviousSibling, w) == nullptr);
    Require(w.current == sample.a1);
}

void TestFilterResults() {
    Sample sample;
    TestFilter filter;
    InitFilter(filter);
    filter.rejected = {"B"};
    filter.skipped = {"A"};
    Walker walker(sample.root, ShowAll, &filter.filter);
    TreeWalker& w = walker.walker;

    RequireSequence(NextNode, w, {"A1", "A2", "C"});
    RequireSequence(PreviousNode, w, {"A2", "A1", "root"});

    Require(Call(FirstChild, w) == sample.a1);
    SetCurrent(w, sample.root);
    Require(Call(LastChild, w) == sample.c);
    Require(Call(PreviousSibling, w) == sample.a2);
    Require(Call(NextSibling, w) == sample.c);
    SetCurrent(w, sample.a1);
    Require(Call(NextSibling, w) == sample.a2);
    Require(Call(ParentNode, w) == sample.root);

    SetCurrent(w, sample.b1);
    Require(Call(ParentNode, w) == sample.root);

    SetCurrent(w, sample.a2);
    filter.calls.clear();
    Require(Call(PreviousSibling, w) == sample.a1);
    Require(filter.calls == std::vector<std::string>{"A1"});

    SetCurrent(w, sample.a1);
    filter.calls.clear();
    Require(Call(PreviousSibling, w) == nullptr);
    Require(filter.calls == std::vector<std::string>{"A"});
    Require(w.current == sample.a1);

    filter.rejected = {"A1", "A2"};
    filter.skipped = {};
    SetCurrent(w, sample.a);
    Require(Call(FirstChild, w) == nullptr);
    Require(Call(LastChild, w) == nullptr);
    Require(w.current == sample.a);

    filter.rejected = {"A"};
    SetCurrent(w, sample.root);
    Require(Call(FirstChild, w) == sample.b);
    SetCurrent(w, sample.a2);
    Require(Call(ParentNode, w) == sample.root);
}

void TestFilterOnlySeesShownNodes() {
    Sample sample;
    TestFilter filter;
    InitFilter(filter);
    Walker walker(sample.root, ShowElement, &filter.filter);
    RequireSequence(NextNode, walker.walker, {"A", "A1", "B", "B1", "B1a"});
    Require(filter.calls == std::vector<std::string>{"A", "A1", "B", "B1", "B1a"});
}

void TestFilterExceptions() {
    Sample sample;
    TestFilter filter;
    InitFilter(filter);
    filter.throwsOn = "B";
    Walker walker(sample.root, ShowAll, &filter.filter);
    TreeWalker& w = walker.walker;
    Require(Call(NextNode, w) == sample.a);
    Require(Call(NextNode, w) == sample.a1);
    Require(Call(NextNode, w) == sample.a2);

    for (Step step : {NextNode, NextSibling}) {
        SetCurrent(w, step == NextNode ? sample.a2 : sample.a);
        Node* before = w.current;
        ExceptionOrNode result{};
        Require(step(&result, &w) == &result);
        Require(!result.hasValue);
        Require(result.exception.code == ExistingExceptionError);
        Require(result.exception.message == nullptr);
        Require(w.current == before);
        Require(!w.isActive);
    }

    filter.throwsOn.clear();
    filter.cannotRunOn = "A";
    SetCurrent(w, sample.root);
    RequireThrows<std::runtime_error>([&] {
        ExceptionOrNode result{};
        _ZN7WebCore10TreeWalker10firstChildEv(&result, &w);
    });
    Require(!w.isActive);
    Require(w.current == sample.root);
}

void TestRecursiveFilter() {
    Sample sample;
    TestFilter filter;
    InitFilter(filter);
    Walker walker(sample.root, ShowAll, &filter.filter);
    TreeWalker& w = walker.walker;
    filter.reenter = &w;
    Require(Call(NextNode, w) == sample.a);
    Require(!filter.reentered.hasValue);
    Require(filter.reentered.exception.code == InvalidStateError);
    const auto* message = filter.reentered.exception.message;
    Require(message != nullptr);
    const std::string expected = "Recursive filters are not allowed";
    Require(message->length == expected.size());
    Require(std::memcmp(message->data, expected.data(), expected.size()) == 0);
    NKWebKit::WtfAbi::Release(filter.reentered.exception.message);
    Require(w.current == sample.a);
}

void TestReferenceCounts() {
    Sample sample;
    TestFilter filter;
    InitFilter(filter);
    filter.rejected = {"B"};
    filter.skipped = {"A"};
    Walker walker(sample.root, ShowAll, &filter.filter);
    TreeWalker& w = walker.walker;
    std::vector<std::uint32_t> before;
    for (Node* node : sample.All()) before.push_back(node->refCountAndParentBit);

    for (Step step : {NextNode, NextNode, NextNode, PreviousNode, PreviousSibling, ParentNode, LastChild, FirstChild, NextSibling}) {
        ExceptionOrNode result{};
        step(&result, &w);
        Require(result.hasValue);
    }
    Node* current = w.current;
    const auto all = sample.All();
    for (std::size_t i = 0; i < all.size(); ++i) {
        std::uint32_t expected = before[i];
        if (all[i] == sample.root) expected -= OneRef;
        if (all[i] == current) expected += OneRef;
        Require(all[i]->refCountAndParentBit == expected);
    }

    SetCurrent(w, sample.b1a);
    Require(sample.b1a->refCountAndParentBit == (2 * OneRef | ParentBit));
    SetCurrent(w, sample.root);
    Require(sample.b1a->refCountAndParentBit == (OneRef | ParentBit));
    Require(sample.root->refCountAndParentBit == 3 * OneRef);
}

void TestDetachedCurrent() {
    Sample sample;
    Tree extra;
    Node* detached = extra.Make("detached");
    detached->refCountAndParentBit = 0;
    Walker walker(sample.root, ShowAll);
    TreeWalker& w = walker.walker;
    SetCurrent(w, detached);
    Require(detached->refCountAndParentBit == OneRef);
    Require(Call(ParentNode, w) == nullptr);
    Require(Call(NextNode, w) == nullptr);
    Require(Call(PreviousNode, w) == nullptr);
    Require(detached->refCountAndParentBit == OneRef);

    RequireThrows<std::runtime_error>([&] { SetCurrent(w, sample.a); });
    Require(w.current == detached);
    Require(sample.a->refCountAndParentBit == (OneRef | ParentBit));
    Require(detached->refCountAndParentBit == OneRef);

    Node* child = extra.Make("child");
    Tree::Append(detached, child);
    for (Step step : {NextNode, FirstChild, LastChild}) {
        RequireThrows<std::runtime_error>([&] {
            ExceptionOrNode result{};
            step(&result, &w);
        });
        Require(w.current == detached);
        Require(detached->refCountAndParentBit == OneRef);
        Require(child->refCountAndParentBit == (OneRef | ParentBit));
        Require(!w.isActive);
    }

    SetCurrent(w, detached);
    Require(w.current == detached);
    Require(detached->refCountAndParentBit == OneRef);
}

void TestRootBoundary() {
    Sample sample;
    Walker walker(sample.b, ShowAll);
    TreeWalker& w = walker.walker;
    RequireSequence(NextNode, w, {"B1", "B1a"});
    Require(Call(NextSibling, w) == nullptr);
    RequireSequence(PreviousNode, w, {"B1", "B"});
    Require(Call(ParentNode, w) == nullptr);
    Require(Call(NextSibling, w) == nullptr);
    Require(Call(PreviousSibling, w) == nullptr);

    SetCurrent(w, sample.a1);
    Require(Call(ParentNode, w) == sample.a);
    Require(Call(ParentNode, w) == sample.root);
    Require(Call(ParentNode, w) == nullptr);
    Require(w.current == sample.root);

    Walker leaf(sample.a1, ShowAll);
    RequireSequence(NextNode, leaf.walker, {});
    RequireSequence(PreviousNode, leaf.walker, {});

    TestFilter filter;
    InitFilter(filter);
    filter.rejected = {"A"};
    Walker rejectedRoot(sample.a, ShowAll, &filter.filter);
    SetCurrent(rejectedRoot.walker, sample.b);
    Require(Call(PreviousNode, rejectedRoot.walker) == nullptr);
    Require(rejectedRoot.walker.current == sample.b);
}

void TestUnexpectedNodeType() {
    Sample sample;
    reinterpret_cast<TestNode*>(sample.a)->type = 0;
    Walker walker(sample.root, ShowAll);
    RequireThrows<std::invalid_argument>([&] {
        ExceptionOrNode result{};
        _ZN7WebCore10TreeWalker8nextNodeEv(&result, &walker.walker);
    });
    Require(walker.walker.current == sample.root);
    Require(!walker.walker.isActive);
}

void TestNullArguments() {
    Sample sample;
    Walker walker(sample.root, ShowAll);
    ExceptionOrNode result{};
    for (Step step : {NextNode, PreviousNode, FirstChild, LastChild, ParentNode, NextSibling, PreviousSibling}) {
        RequireThrows<std::invalid_argument>([&] { step(&result, nullptr); });
        RequireThrows<std::invalid_argument>([&] { step(nullptr, &walker.walker); });
    }
    RequireThrows<std::invalid_argument>([&] { SetCurrent(walker.walker, nullptr); });
    RequireThrows<std::invalid_argument>([&] { _ZN7WebCore10TreeWalker14setCurrentNodeERNS_4NodeE(nullptr, sample.a); });
    Require(walker.walker.current == sample.root);
}

}

int main() {
    TestDocumentOrder();
    TestWhatToShow();
    TestStructuralSteps();
    TestFilterResults();
    TestFilterOnlySeesShownNodes();
    TestFilterExceptions();
    TestRecursiveFilter();
    TestReferenceCounts();
    TestDetachedCurrent();
    TestRootBoundary();
    TestUnexpectedNodeType();
    TestNullArguments();
    return 0;
}
