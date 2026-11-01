#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void AssertTrue(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Assertion failed: %s\n", message);
        std::abort();
    }
}

struct ParserEvents {
    bool startedDoc = false;
    bool endedDoc = false;
    int elementCount = 0;
    int endElementCount = 0;
    std::string textContent;
    std::string cdataContent;
    std::string commentContent;
    std::vector<std::string> localNames;
    std::vector<std::string> prefixes;
    std::vector<std::string> uris;
    bool errorOccurred = false;
};

static void APS5_VABI SaxStartDoc(void* ctx) {
    auto* ev = static_cast<ParserEvents*>(ctx);
    if (ev) ev->startedDoc = true;
}

static void APS5_VABI SaxEndDoc(void* ctx) {
    auto* ev = static_cast<ParserEvents*>(ctx);
    if (ev) ev->endedDoc = true;
}

static void APS5_VABI SaxStartElementNs(void* ctx, const uint8_t* localname, const uint8_t* prefix, const uint8_t* URI, int32_t nb_namespaces, const uint8_t** namespaces, int32_t nb_attributes, int32_t nb_defaulted, const uint8_t** attributes) {
    (void)nb_namespaces;
    (void)namespaces;
    (void)nb_attributes;
    (void)nb_defaulted;
    (void)attributes;
    auto* ev = static_cast<ParserEvents*>(ctx);
    if (!ev) return;
    ev->elementCount++;
    ev->localNames.push_back(localname ? reinterpret_cast<const char*>(localname) : "");
    ev->prefixes.push_back(prefix ? reinterpret_cast<const char*>(prefix) : "");
    ev->uris.push_back(URI ? reinterpret_cast<const char*>(URI) : "");
}

static void APS5_VABI SaxEndElementNs(void* ctx, const uint8_t* localname, const uint8_t* prefix, const uint8_t* URI) {
    (void)localname;
    (void)prefix;
    (void)URI;
    auto* ev = static_cast<ParserEvents*>(ctx);
    if (ev) ev->endElementCount++;
}

static void APS5_VABI SaxCharacters(void* ctx, const uint8_t* ch, int32_t len) {
    auto* ev = static_cast<ParserEvents*>(ctx);
    if (ev && ch && len > 0) {
        ev->textContent.append(reinterpret_cast<const char*>(ch), len);
    }
}

static void APS5_VABI SaxCdataBlock(void* ctx, const uint8_t* value, int32_t len) {
    auto* ev = static_cast<ParserEvents*>(ctx);
    if (ev && value && len > 0) {
        ev->cdataContent.append(reinterpret_cast<const char*>(value), len);
    }
}

static void APS5_VABI SaxComment(void* ctx, const uint8_t* value) {
    auto* ev = static_cast<ParserEvents*>(ctx);
    if (ev && value) {
        ev->commentContent.append(reinterpret_cast<const char*>(value));
    }
}

static void APS5_VABI SaxError(void* ctx, const char* msg, ...) {
    (void)msg;
    auto* ev = static_cast<ParserEvents*>(ctx);
    if (ev) ev->errorOccurred = true;
}

static void APS5_VABI SaxSerror(void* userData, NKWebKit::GuestXmlError* error) {
    (void)error;
    auto* ev = static_cast<ParserEvents*>(userData);
    if (ev) ev->errorOccurred = true;
}

void TestBasicStringsAndDict() {
    xmlInitParser_nid_postfix();

    const char hello[] = "Hello libxml2";
    uint8_t* dup = xmlStrdup_nid_postfix(reinterpret_cast<const uint8_t*>(hello));
    AssertTrue(dup != nullptr, "xmlStrdup returned null");
    AssertTrue(std::strcmp(reinterpret_cast<char*>(dup), hello) == 0, "xmlStrdup mismatch");
    std::free(dup);

    uint8_t* ndup = xmlStrndup_nid_postfix(reinterpret_cast<const uint8_t*>(hello), 5);
    AssertTrue(ndup != nullptr, "xmlStrndup returned null");
    AssertTrue(std::strcmp(reinterpret_cast<char*>(ndup), "Hello") == 0, "xmlStrndup mismatch");
    std::free(ndup);
}

void TestPushParserChunks() {
    const char xmlDoc[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!-- test-comment-data -->\n"
        "<root xmlns:test=\"http://example.org/test\">\n"
        "    <test:item id=\"1\">Sample text</test:item>\n"
        "    <test:item id=\"2\"><![CDATA[cdata-payload]]></test:item>\n"
        "</root>\n";

    ParserEvents events;
    NKWebKit::GuestSAXHandler sax{};
    sax.startDocument = SaxStartDoc;
    sax.endDocument = SaxEndDoc;
    sax.startElementNs = SaxStartElementNs;
    sax.endElementNs = SaxEndElementNs;
    sax.characters = SaxCharacters;
    sax.cdataBlock = SaxCdataBlock;
    sax.comment = SaxComment;
    sax.error = SaxError;
    sax.serror = SaxSerror;

    NKWebKit::GuestParserCtxt* ctxt = xmlCreatePushParserCtxt_nid_postfix(&sax, &events, nullptr, 0, "test.xml");
    AssertTrue(ctxt != nullptr, "xmlCreatePushParserCtxt returned null");

    int optRet = xmlCtxtUseOptions_nid_postfix(ctxt, 1);
    AssertTrue(optRet == 0, "xmlCtxtUseOptions failed");

    size_t docLen = std::strlen(xmlDoc);
    size_t chunkSize = 7;
    for (size_t offset = 0; offset < docLen; offset += chunkSize) {
        size_t size = (docLen - offset > chunkSize) ? chunkSize : (docLen - offset);
        int terminate = (offset + size == docLen) ? 1 : 0;
        int parseRet = xmlParseChunk_nid_postfix(ctxt, xmlDoc + offset, static_cast<int>(size), terminate);
        AssertTrue(parseRet == 0, "xmlParseChunk failed");
    }

    int64_t consumed = xmlByteConsumed_nid_postfix(ctxt);
    AssertTrue(consumed > 0, "xmlByteConsumed should be positive");

    AssertTrue(events.startedDoc, "startDocument not called");
    AssertTrue(events.endedDoc, "endDocument not called");
    AssertTrue(events.elementCount == 3, "expected 3 elements (<root>, <test:item>, <test:item>)");
    AssertTrue(events.endElementCount == 3, "expected 3 end elements");
    AssertTrue(events.commentContent.find("test-comment-data") != std::string::npos, "comment not captured");
    AssertTrue(events.cdataContent.find("cdata-payload") != std::string::npos, "cdata not captured");
    AssertTrue(events.textContent.find("Sample text") != std::string::npos, "text not captured");

    if (ctxt->dict) {
        const uint8_t* lookedUp = xmlDictLookup_nid_postfix(ctxt->dict, reinterpret_cast<const uint8_t*>("root"), 4);
        AssertTrue(lookedUp != nullptr, "xmlDictLookup returned null");
        AssertTrue(std::strcmp(reinterpret_cast<const char*>(lookedUp), "root") == 0, "xmlDictLookup mismatch");
    }

    xmlStopParser_nid_postfix(ctxt);
    xmlFreeParserCtxt_nid_postfix(ctxt);
}

void TestErrorHandling() {
    const char badXml[] = "<root><unclosed></root>";

    ParserEvents events;
    NKWebKit::GuestSAXHandler sax{};
    sax.serror = SaxSerror;
    sax.error = SaxError;

    NKWebKit::GuestParserCtxt* ctxt = xmlCreatePushParserCtxt_nid_postfix(&sax, &events, nullptr, 0, "bad.xml");
    AssertTrue(ctxt != nullptr, "xmlCreatePushParserCtxt returned null");

    int ret = xmlParseChunk_nid_postfix(ctxt, badXml, static_cast<int>(std::strlen(badXml)), 1);
    AssertTrue(ret != 0, "malformed xml should produce non-zero return code");

    NKWebKit::GuestXmlError* err = xmlCtxtGetLastError_nid_postfix(ctxt);
    AssertTrue(err != nullptr, "xmlCtxtGetLastError returned null");
    AssertTrue(err->code != 0, "error code should be non-zero");

    xmlFreeParserCtxt_nid_postfix(ctxt);
}

void TestMemoryParserAndEntities() {
    const char memXml[] = "<greeting>Hello Memory Parser</greeting>";
    NKWebKit::GuestParserCtxt* ctxt = xmlCreateMemoryParserCtxt_nid_postfix(memXml, static_cast<int>(std::strlen(memXml)));
    AssertTrue(ctxt != nullptr, "xmlCreateMemoryParserCtxt returned null");

    int contentRet = xmlParseContent_nid_postfix(ctxt);
    AssertTrue(contentRet == 0, "xmlParseContent failed");

    int encRet = xmlSwitchEncoding_nid_postfix(ctxt, 1);
    (void)encRet;

    xmlSAX2StartDocument_nid_postfix(ctxt);
    xmlSAX2InternalSubset_nid_postfix(ctxt, reinterpret_cast<const uint8_t*>("greeting"), nullptr, nullptr);
    xmlSAX2EntityDecl_nid_postfix(ctxt, reinterpret_cast<const uint8_t*>("customEnt"), 1, nullptr, nullptr, reinterpret_cast<const uint8_t*>("customVal"));
    xmlSAX2EndDocument_nid_postfix(ctxt);

    NKWebKit::GuestEntity* ent = xmlGetPredefinedEntity_nid_postfix(reinterpret_cast<const uint8_t*>("amp"));
    AssertTrue(ent != nullptr, "xmlGetPredefinedEntity('amp') returned null");
    AssertTrue(ent->name != nullptr && std::strcmp(reinterpret_cast<const char*>(ent->name), "amp") == 0, "entity name mismatch");
    AssertTrue(ent->content != nullptr && std::strcmp(reinterpret_cast<const char*>(ent->content), "&") == 0, "entity content mismatch");

    NKWebKit::GuestEntity* docEnt = xmlGetDocEntity_nid_postfix(ctxt->myDoc, reinterpret_cast<const uint8_t*>("amp"));
    (void)docEnt;

    if (ctxt->myDoc) {
        xmlFreeDoc_nid_postfix(ctxt->myDoc);
        ctxt->myDoc = nullptr;
    }
    xmlFreeParserCtxt_nid_postfix(ctxt);
}

struct IoHandler {
    const char* scheme = "";
    const char* payload = "";
    int matches = 0;
    int opens = 0;
    int reads = 0;
    int closes = 0;
    int foreign = 0;
    void (*onFirstRead)() = nullptr;
};

struct IoStream {
    int handler = -1;
    std::size_t offset = 0;
};

IoHandler g_io[6];
IoStream g_streams[32];
int g_streamCount = 0;
int g_lateRegistration = -2;

template <int H>
int32_t APS5_VABI IoMatch(const char* name) {
    ++g_io[H].matches;
    const std::size_t length = std::strlen(g_io[H].scheme);
    return length != 0 && std::strncmp(name, g_io[H].scheme, length) == 0 ? 1 : 0;
}

template <int H>
void* APS5_VABI IoOpen(const char* name) {
    (void)name;
    ++g_io[H].opens;
    AssertTrue(g_streamCount < 32, "too many I/O streams");
    IoStream* stream = &g_streams[g_streamCount++];
    stream->handler = H;
    stream->offset = 0;
    return stream;
}

template <int H>
int32_t APS5_VABI IoRead(void* context, char* buffer, int32_t len) {
    auto* stream = static_cast<IoStream*>(context);
    ++g_io[H].reads;
    if (stream->handler != H) {
        ++g_io[H].foreign;
        return -1;
    }
    if (g_io[H].reads == 1 && g_io[H].onFirstRead) {
        g_io[H].onFirstRead();
    }
    const std::size_t length = std::strlen(g_io[H].payload);
    if (stream->offset >= length || len <= 0) {
        return 0;
    }
    buffer[0] = g_io[H].payload[stream->offset++];
    return 1;
}

template <int H>
int32_t APS5_VABI IoClose(void* context) {
    ++g_io[H].closes;
    if (static_cast<IoStream*>(context)->handler != H) {
        ++g_io[H].foreign;
    }
    return 0;
}

template <int H>
int RegisterInput() {
    return xmlRegisterInputCallbacks_nid_postfix(IoMatch<H>, IoOpen<H>, IoRead<H>, IoClose<H>);
}

template <int H>
int32_t APS5_VABI OutMatch(const char* name) {
    (void)name;
    ++g_io[H].matches;
    return 0;
}

template <int H>
void* APS5_VABI OutOpen(const char* name) {
    (void)name;
    ++g_io[H].opens;
    return nullptr;
}

template <int H>
int32_t APS5_VABI OutWrite(void* context, const char* buffer, int32_t len) {
    (void)context;
    (void)buffer;
    ++g_io[H].reads;
    return len;
}

template <int H>
int32_t APS5_VABI OutClose(void* context) {
    (void)context;
    ++g_io[H].closes;
    return 0;
}

template <int H>
int RegisterOutput() {
    return xmlRegisterOutputCallbacks_nid_postfix(OutMatch<H>, OutOpen<H>, OutWrite<H>, OutClose<H>);
}

void RegisterLateHandler() {
    g_lateRegistration = RegisterInput<2>();
}

void ResetIoCounters() {
    for (IoHandler& handler : g_io) {
        handler.matches = 0;
        handler.opens = 0;
        handler.reads = 0;
        handler.closes = 0;
        handler.foreign = 0;
        handler.onFirstRead = nullptr;
    }
}

int ParseWithExternalEntities() {
    const char doc[] =
        "<!DOCTYPE r [<!ENTITY a SYSTEM \"alpha:one\"><!ENTITY b SYSTEM \"beta:two\">]>"
        "<r>&a;|&b;</r>";
    NKWebKit::GuestParserCtxt* ctxt = xmlCreatePushParserCtxt_nid_postfix(nullptr, nullptr, nullptr, 0, "io.xml");
    AssertTrue(ctxt != nullptr, "xmlCreatePushParserCtxt returned null");
    AssertTrue(xmlCtxtUseOptions_nid_postfix(ctxt, 2) == 0, "xmlCtxtUseOptions(XML_PARSE_NOENT) failed");
    const int result = xmlParseChunk_nid_postfix(ctxt, doc, static_cast<int>(std::strlen(doc)), 1);
    if (ctxt->myDoc) {
        xmlFreeDoc_nid_postfix(ctxt->myDoc);
        ctxt->myDoc = nullptr;
    }
    xmlFreeParserCtxt_nid_postfix(ctxt);
    return result;
}

void ExpectServed(int handler, const char* message) {
    AssertTrue(g_io[handler].opens == 1, message);
    AssertTrue(g_io[handler].reads > static_cast<int>(std::strlen(g_io[handler].payload)), message);
    AssertTrue(g_io[handler].closes == 1, message);
    AssertTrue(g_io[handler].foreign == 0, message);
}

void ExpectUntouched(int handler, const char* message) {
    AssertTrue(g_io[handler].opens == 0 && g_io[handler].reads == 0 && g_io[handler].closes == 0, message);
}

void TestInputHandlersAndOpenContexts() {
    g_io[0].scheme = "alpha:";
    g_io[0].payload = "alpha-payload";
    g_io[1].scheme = "beta:";
    g_io[1].payload = "beta-payload";
    const int first = RegisterInput<0>();
    const int second = RegisterInput<1>();
    AssertTrue(first >= 0 && second == first + 1, "input registrations should take consecutive entries");

    ResetIoCounters();
    g_io[0].onFirstRead = RegisterLateHandler;
    AssertTrue(ParseWithExternalEntities() == 0, "parse through two guest input handlers failed");
    AssertTrue(g_lateRegistration == second + 1, "registration made while alpha was open should succeed");
    ExpectServed(0, "alpha entity should be read and closed through alpha's callbacks after a later registration");
    ExpectServed(1, "beta entity should be read and closed through beta's callbacks");
    ExpectUntouched(2, "the late handler matches nothing and must not receive alpha's or beta's stream");
    AssertTrue(g_io[2].matches >= 1, "the late handler should be consulted for beta");
}

void TestFailedRegistrations() {
    int inputResult = 0;
    for (int i = 0; i < 20 && inputResult >= 0; ++i) {
        inputResult = RegisterInput<3>();
    }
    AssertTrue(inputResult == -1, "input registrations should fail once libxml2's table is full");
    AssertTrue(RegisterInput<4>() == -1, "a registration into a full table should fail");

    ResetIoCounters();
    AssertTrue(ParseWithExternalEntities() == 0, "parse after failed registrations failed");
    ExpectServed(0, "alpha should still be served after failed registrations");
    ExpectServed(1, "beta should still be served after failed registrations");
    ExpectUntouched(3, "filler handlers match nothing");
    AssertTrue(g_io[4].matches == 0, "a rejected handler must never be called");
    ExpectUntouched(4, "a rejected handler must never be called");

    const int firstOutput = RegisterOutput<0>();
    const int secondOutput = RegisterOutput<1>();
    AssertTrue(firstOutput >= 0 && secondOutput == firstOutput + 1, "output registrations should take consecutive entries");
    int outputResult = 0;
    for (int i = 0; i < 20 && outputResult >= 0; ++i) {
        outputResult = RegisterOutput<3>();
    }
    AssertTrue(outputResult == -1, "output registrations should fail once libxml2's table is full");
    AssertTrue(RegisterOutput<5>() == -1, "an output registration into a full table should fail");
}

void TestDocumentOutlivesContext() {
    const char docXml[] = "<!DOCTYPE r [<!ENTITY kept \"kept value\">]><r>&kept;</r>";
    NKWebKit::GuestParserCtxt* ctxt = xmlCreatePushParserCtxt_nid_postfix(nullptr, nullptr, nullptr, 0, "kept.xml");
    AssertTrue(ctxt != nullptr, "xmlCreatePushParserCtxt returned null");
    AssertTrue(xmlParseChunk_nid_postfix(ctxt, docXml, static_cast<int>(std::strlen(docXml)), 1) == 0, "the document should parse");
    NKWebKit::GuestDoc* doc = ctxt->myDoc;
    AssertTrue(doc != nullptr, "the parser should build a document");
    ctxt->myDoc = nullptr;
    xmlFreeParserCtxt_nid_postfix(ctxt);

    const char otherXml[] = "<!DOCTYPE o [<!ENTITY other \"other value\">]><o>&other;</o>";
    NKWebKit::GuestParserCtxt* other = xmlCreatePushParserCtxt_nid_postfix(nullptr, nullptr, nullptr, 0, "other.xml");
    AssertTrue(other != nullptr, "xmlCreatePushParserCtxt returned null");
    AssertTrue(xmlParseChunk_nid_postfix(other, otherXml, static_cast<int>(std::strlen(otherXml)), 1) == 0, "the second document should parse");
    NKWebKit::GuestDoc* otherDoc = other->myDoc;
    AssertTrue(otherDoc != nullptr && otherDoc != doc, "a later context must not reuse a document that is still alive");
    other->myDoc = nullptr;
    xmlFreeParserCtxt_nid_postfix(other);
    AssertTrue(xmlGetDocEntity_nid_postfix(doc, reinterpret_cast<const uint8_t*>("other")) == nullptr, "the first document must not see the second document's entities");
    xmlFreeDoc_nid_postfix(otherDoc);

    AssertTrue(doc->type == 9, "the document should keep its type after the context is freed");
    AssertTrue(doc->doc == doc, "the document should still point to itself");
    AssertTrue(doc->version != nullptr && std::strcmp(reinterpret_cast<const char*>(doc->version), "1.0") == 0, "the document version should survive the context");
    NKWebKit::GuestEntity* kept = xmlGetDocEntity_nid_postfix(doc, reinterpret_cast<const uint8_t*>("kept"));
    AssertTrue(kept != nullptr && kept->content != nullptr, "the document entity should be found after the context is freed");
    AssertTrue(std::strcmp(reinterpret_cast<const char*>(kept->content), "kept value") == 0, "the document entity content mismatch");
    xmlFreeDoc_nid_postfix(doc);
}

} // namespace

int main() {
    TestBasicStringsAndDict();
    TestPushParserChunks();
    TestErrorHandling();
    TestMemoryParserAndEntities();
    TestDocumentOutlivesContext();
    TestInputHandlersAndOpenContexts();
    TestFailedRegistrations();
    std::printf("Xml tests passed.\n");
    return 0;
}
