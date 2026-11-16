#include "prx/libSceXml/include/Api.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

struct TestAllocator {
    XmlMemAllocator base;
    std::vector<void*> blocks;
    std::string log;
    std::size_t allocations = 0;
    std::size_t failAt = 0;
    std::size_t first = 0;
};

void* APS5_VABI TestAllocate(XmlMemAllocator* self, std::size_t size, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == reinterpret_cast<void*>(0x1234), "allocate receives the user data");
    ++allocator->allocations;
    allocator->log += "a" + std::to_string(size) + " ";
    if (allocator->allocations == allocator->failAt) return nullptr;
    void* block = std::malloc(size);
    allocator->blocks.push_back(block);
    return block;
}

void APS5_VABI TestDeallocate(XmlMemAllocator* self, void* pointer, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == reinterpret_cast<void*>(0x1234), "deallocate receives the user data");
    std::size_t index = 0;
    while (index < allocator->blocks.size() && allocator->blocks[index] != pointer) ++index;
    Require(index < allocator->blocks.size(), "deallocate receives a live block");
    allocator->log += "f" + std::to_string(index - allocator->first) + " ";
    std::free(pointer);
    allocator->blocks[index] = nullptr;
}

void APS5_VABI TestDestroy(XmlMemAllocator*) {}

const XmlMemAllocatorVtable g_allocatorVtable{TestDestroy, TestDestroy, TestAllocate, TestDeallocate};

struct Handler;

struct HandlerVtable {
    void* destructor;
    void* deletingDestructor;
    int(APS5_VABI* startDocument)(Handler*, void*);
    int(APS5_VABI* startElement)(Handler*, void*, const XmlString*, const XmlHandle*);
    int(APS5_VABI* characters)(Handler*, void*, const XmlString*);
    int(APS5_VABI* endElement)(Handler*, void*, const XmlString*);
    int(APS5_VABI* endDocument)(Handler*, void*);
    int(APS5_VABI* node)(Handler*, void*, int, const XmlString*);
    int(APS5_VABI* reference)(Handler*, void*, int, const XmlString*);
    void(APS5_VABI* error)(Handler*, void*, int);
};

struct Handler {
    const HandlerVtable* vtable;
    std::string events;
    void* expectedUserData = nullptr;
    int failOn = 0;
    int calls = 0;
};

std::string Text(const XmlString* string) {
    return string->size == 0 ? std::string() : std::string(string->buffer, string->size);
}

int Record(Handler* h, void* userData, const std::string& event) {
    Require(userData == h->expectedUserData, "the callback receives the user data");
    h->events += event + "|";
    ++h->calls;
    return h->failOn == h->calls ? 1 : 0;
}

int APS5_VABI OnStartDocument(Handler* h, void* u) {
    return Record(h, u, "SD");
}

int APS5_VABI OnStartElement(Handler* h, void* u, const XmlString* name, const XmlHandle* attributes) {
    std::string event = "SE:" + Text(name);
    const int count = _ZNK3sce3Xml13AttributeList9getLengthEv(attributes);
    for (int i = 0; i < count; ++i) {
        XmlString value{};
        _ZNK3sce3Xml13AttributeList12getAttributeEj(&value, attributes, static_cast<std::uint32_t>(i));
        event += "," + std::to_string(i) + "=" + Text(&value);
    }
    return Record(h, u, event);
}

int APS5_VABI OnCharacters(Handler* h, void* u, const XmlString* text) {
    return Record(h, u, "CH:" + Text(text));
}

int APS5_VABI OnEndElement(Handler* h, void* u, const XmlString* name) {
    return Record(h, u, "EE:" + Text(name));
}

int APS5_VABI OnEndDocument(Handler* h, void* u) {
    return Record(h, u, "ED");
}

int APS5_VABI OnNode(Handler* h, void* u, int kind, const XmlString* text) {
    return Record(h, u, "N" + std::to_string(kind) + ":" + Text(text));
}

int APS5_VABI OnReference(Handler* h, void* u, int kind, const XmlString* text) {
    return Record(h, u, "R" + std::to_string(kind) + ":" + Text(text));
}

void APS5_VABI OnError(Handler* h, void* u, int status) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "ERR:%x", static_cast<unsigned>(status));
    Record(h, u, buffer);
}

const HandlerVtable g_handlerVtable{nullptr, nullptr, OnStartDocument, OnStartElement, OnCharacters, OnEndElement, OnEndDocument, OnNode, OnReference, OnError};

struct Fixture {
    TestAllocator allocator{};
    XmlInitParameter parameter{};
    XmlHandle initializer{nullptr};
    Handler handler{&g_handlerVtable};
    XmlHandle parser{nullptr};

    Fixture() {
        allocator.base.vtable = &g_allocatorVtable;
        parameter = {&allocator.base, reinterpret_cast<void*>(0x1234), {0, 0}};
        Require(_ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(&initializer, &parameter) == 0, "the initializer starts");
    }

    ~Fixture() {
        _ZN3sce3Xml3Sax6ParserD1Ev(&parser);
        _ZN3sce3Xml11Initializer9terminateEv(&initializer);
        for (void* block : allocator.blocks) {
            if (block != nullptr) std::free(block);
        }
    }

    void Mark() {
        allocator.log.clear();
        allocator.first = allocator.blocks.size();
    }

    void Start() {
        Require(_ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE(&parser, &initializer) == 0, "parser initialize");
        Require(_ZN3sce3Xml3Sax6Parser18setDocumentHandlerEPNS1_15DocumentHandlerE(&parser, &handler) == 0, "set the handler");
    }

    int Parse(const char* text, bool last) {
        const XmlString string{text, std::strlen(text)};
        return _ZN3sce3Xml3Sax6Parser5parseEPKNS0_6StringEb(&parser, &string, last);
    }

    std::string Take() {
        std::string events = std::move(handler.events);
        handler.events.clear();
        return events;
    }
};

bool Throws(Fixture& f, const char* text, bool last) {
    try {
        f.Parse(text, last);
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

void TestLifecycle() {
    Fixture f;
    f.Mark();
    XmlHandle parser{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml3Sax6ParserC1Ev(&parser);
    Require(parser.impl == nullptr, "the constructor clears the object");
    XmlHandle never{nullptr};
    Require(_ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE(&parser, nullptr) == static_cast<int>(0x80850002), "a null initializer");
    Require(_ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE(&parser, &never) == static_cast<int>(0x80850002), "an initializer without an implementation");
    const XmlString text{"<a/>", 4};
    Require(_ZN3sce3Xml3Sax6Parser5parseEPKNS0_6StringEb(&parser, &text, true) == static_cast<int>(0x80850002), "parse before initialize");
    Require(_ZN3sce3Xml3Sax6Parser5resetEv(&parser) == static_cast<int>(0x80850002), "reset before initialize");
    Require(_ZN3sce3Xml3Sax6Parser9terminateEv(&parser) == static_cast<int>(0x80850002), "terminate before initialize");
    _ZN3sce3Xml3Sax6Parser11setUserDataEPv(&parser, nullptr);
    _ZN3sce3Xml3Sax6Parser16setResolveEntityEb(&parser, true);
    _ZN3sce3Xml3Sax6Parser26setSkipIgnorableWhiteSpaceEb(&parser, false);
    _ZN3sce3Xml3Sax6ParserD2Ev(&parser);
    Require(f.allocator.log.empty(), "nothing is allocated before initialize");

    Require(_ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE(&parser, &f.initializer) == 0 && parser.impl != nullptr, "initialize");
    Require(f.allocator.log == "a16 a32 a360 ", "initialize allocates the 13.42 blocks in order");
    f.allocator.log.clear();
    Require(_ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE(&parser, &f.initializer) == 0x00850003 && f.allocator.log.empty(), "a second initialize is refused");
    const XmlString nullBuffer{nullptr, 4};
    const XmlString empty{"", 0};
    Require(_ZN3sce3Xml3Sax6Parser5parseEPKNS0_6StringEb(&parser, nullptr, true) == static_cast<int>(0x80850003), "a null string");
    Require(_ZN3sce3Xml3Sax6Parser5parseEPKNS0_6StringEb(&parser, &nullBuffer, true) == static_cast<int>(0x80850003), "a null buffer");
    Require(_ZN3sce3Xml3Sax6Parser5parseEPKNS0_6StringEb(&parser, &empty, true) == static_cast<int>(0x80850003), "an empty string");
    Require(_ZN3sce3Xml3Sax6Parser5parseEPKNS0_6StringEb(&parser, &text, true) == static_cast<int>(0x80850016), "parse without a handler");
    Require(_ZN3sce3Xml3Sax6Parser5resetEv(&parser) == 0, "reset");
    Require(_ZN3sce3Xml3Sax6Parser9terminateEv(&parser) == 0 && parser.impl == nullptr, "terminate");
    Require(f.allocator.log == "f2 f1 f0 ", "terminate frees the core, the engine and the object in order");
    f.allocator.log.clear();
    Require(_ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE(&parser, &f.initializer) == 0, "initialize again");
    f.allocator.log.clear();
    Handler handler{&g_handlerVtable};
    Require(_ZN3sce3Xml3Sax6Parser18setDocumentHandlerEPNS1_15DocumentHandlerE(&parser, &handler) == 0, "set the handler");
    Require(f.allocator.log == "a32 ", "setDocumentHandler allocates the adapter");
    f.allocator.log.clear();
    Require(_ZN3sce3Xml3Sax6Parser18setDocumentHandlerEPNS1_15DocumentHandlerE(&parser, &handler) == 0, "set the handler again");
    Require(f.allocator.log == "f6 a32 ", "a second handler releases the first adapter before allocating");
    f.allocator.log.clear();
    _ZN3sce3Xml3Sax6ParserD1Ev(&parser);
    Require(parser.impl == nullptr && f.allocator.log == "f7 f5 f4 f3 ", "the destructor frees the adapter, the core, the engine and the object");
    Require(_ZN3sce3Xml3Sax6Parser9terminateEv(&parser) == static_cast<int>(0x80850002), "terminate again");
}

void TestAllocationFailure() {
    Fixture f;
    f.allocator.failAt = f.allocator.allocations + 1;
    Require(_ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE(&f.parser, &f.initializer) == static_cast<int>(0x80850001) && f.parser.impl == nullptr, "the first allocation fails");
    f.allocator.failAt = f.allocator.allocations + 2;
    bool threw = false;
    try {
        _ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE(&f.parser, &f.initializer);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw, "a later allocation failure is not measured");
}

void TestDocument() {
    Fixture f;
    f.handler.expectedUserData = reinterpret_cast<void*>(0x77);
    f.Start();
    _ZN3sce3Xml3Sax6Parser11setUserDataEPv(&f.parser, f.handler.expectedUserData);
    Require(f.Parse("<?xml version=\"1.0\"?>\n<root a=\"1\" b='two'>\n  <item/>  text <b>x</b>\n</root>\n", true) == 0, "a document parses");
    Require(f.Take() == "SD|N7:xml version=\"1.0\"?>|SE:root,0=1,1=two|SE:item|EE:item|CH:text |SE:b|CH:x|EE:b|EE:root|ED|", "events of a document with the default white space skipping");
    Require(f.Parse("<a><!-- c --><![CDATA[<z>]]></a>", true) == 0, "comments and CDATA");
    Require(f.Take() == "SD|SE:a|N11: c -->|N9:<z>]]>|EE:a|ED|", "comment and CDATA events");
    Require(f.Parse("\xEF\xBB\xBF<a/>", true) == 0, "a byte order mark");
    Require(f.Take() == "SD|SE:a|EE:a|ED|", "the byte order mark is skipped");
    _ZN3sce3Xml3Sax6Parser26setSkipIgnorableWhiteSpaceEb(&f.parser, false);
    Require(f.Parse("<a> <b/> t </a>", true) == 0, "white space kept");
    Require(f.Take() == "SD|SE:a|CH: |SE:b|EE:b|CH: t |EE:a|ED|", "white space is delivered when skipping is off");
}

void TestStreaming() {
    Fixture f;
    f.Start();
    Require(f.Parse("<a>", false) == 0x00850001, "a chunk that is not the last is in progress");
    Require(f.Take() == "SD|SE:a|", "the first chunk");
    Require(f.Parse("<b/>", false) == 0x00850001, "another chunk");
    Require(f.Parse("</a>", true) == 0, "the last chunk completes the document");
    Require(f.Take() == "SE:b|EE:b|EE:a|ED|", "the rest of the document");
    Require(f.Parse("<a/>", false) == 0x00850001, "a complete document that is not marked last");
    Require(f.Take() == "SD|SE:a|EE:a|", "no end of document yet");
}

void TestErrors() {
    Fixture f;
    f.Start();
    Require(f.Parse("<a></b>", true) == static_cast<int>(0x8085000a), "an end tag mismatch");
    Require(f.Take() == "SD|SE:a|ERR:8085000a|", "the error is reported to the handler");
    Require(f.Parse("<a>", true) == static_cast<int>(0x80850008), "an unclosed element at the end");
    Require(f.Take() == "SD|SE:a|ERR:80850008|", "the truncation is reported");
    Require(f.Parse("<!-- only -->", true) == static_cast<int>(0x80850019), "no document element");
    Require(f.Take() == "SD|N11: only -->|ERR:80850019|", "the missing root is reported");
    f.handler.failOn = 2;
    f.handler.calls = 0;
    Require(f.Parse("<a/>", true) == 0x00850002, "a callback that fails aborts the parse");
    Require(f.Take() == "SD|SE:a|ERR:850002|", "the abort is reported");
    f.handler.failOn = 0;
    Require(f.Parse("<a/>", true) == 0, "a parse after an abort starts a new document");
}

void TestUnmeasured() {
    Fixture f;
    f.Start();
    Require(Throws(f, "<!DOCTYPE a><a/>", true), "a document type declaration");
    f.Take();
    Require(f.Parse("<a x=\"&amp;\">&amp; &lt;</a>", true) == 0, "references stay raw without the resolve-entity option");
    Require(f.Take() == "SD|SE:a,0=&amp;|CH:&amp; &lt;|EE:a|ED|", "raw references");
    Require(f.Parse("<a x=\"a<b\tc\"/>", true) == 0, "an attribute value is taken raw");
    Require(f.Take() == "SD|SE:a,0=a<b\tc|EE:a|ED|", "raw attribute value");
    _ZN3sce3Xml3Sax6Parser16setResolveEntityEb(&f.parser, true);
    Require(Throws(f, "<a>&amp;</a>", true), "a reference with the resolve-entity option on");
    Require(Throws(f, "<a x=\"&amp;\"/>", true), "a reference in an attribute with the resolve-entity option on");
    _ZN3sce3Xml3Sax6Parser16setResolveEntityEb(&f.parser, false);
    Require(Throws(f, "<a>text", false), "character data cut by the chunk end");
    Require(Throws(f, "<a", false), "a start tag cut by the chunk end");
    Require(Throws(f, "<!-- x", false), "a comment cut by the chunk end");
    Require(Throws(f, "text<a/>", true), "character data before the document element");
    Require(Throws(f, "<a/><b/>", true), "an element after the document element");
    Require(Throws(f, "</a>", true), "an end tag outside the document element");
    Require(Throws(f, "<a x=\"1\"y=\"2\"/>", true), "attributes not separated by white space");
    f.Take();
}

void TestTagErrors() {
    Fixture f;
    f.Start();
    Require(f.Parse("<a x >", true) == static_cast<int>(0x8085000f), "an attribute without an equals sign");
    Require(f.Take() == "SD|ERR:8085000f|", "reported before the element starts");
    Require(f.Parse("<a x=1>", true) == static_cast<int>(0x80850011), "an unquoted attribute value");
    Require(f.Take() == "SD|ERR:80850011|", "reported before the element starts");
    Require(f.Parse("<a x=\"1\" x=\"2\"/>", true) == static_cast<int>(0x8085001a), "a duplicate attribute name");
    Require(f.Take() == "SD|ERR:8085001a|", "reported before the element starts");
    Require(f.Parse("<ab></abc>", true) == static_cast<int>(0x8085000e), "an end tag longer than the open element");
    Require(f.Take() == "SD|SE:ab|ERR:8085000e|", "reported as markup");
    Require(f.Parse("<ab></a2>", true) == static_cast<int>(0x8085000a), "an end tag that differs from the open element");
    Require(f.Parse("<a></a >", true) == 0, "white space before the closing bracket of an end tag");
    f.Take();
}

}

int main() {
    TestLifecycle();
    TestAllocationFailure();
    TestDocument();
    TestStreaming();
    TestErrors();
    TestUnmeasured();
    TestTagErrors();
    std::puts("ok");
    return 0;
}
