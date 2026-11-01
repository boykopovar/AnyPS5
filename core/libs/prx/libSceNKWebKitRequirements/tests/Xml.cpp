#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

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

static int32_t APS5_VABI DummyInputMatch(const char* filename) {
    (void)filename;
    return 0;
}
static void* APS5_VABI DummyInputOpen(const char* filename) {
    (void)filename;
    return nullptr;
}
static int32_t APS5_VABI DummyInputRead(void* context, char* buffer, int32_t len) {
    (void)context;
    (void)buffer;
    (void)len;
    return -1;
}
static int32_t APS5_VABI DummyInputClose(void* context) {
    (void)context;
    return 0;
}

static int32_t APS5_VABI DummyOutputMatch(const char* filename) {
    (void)filename;
    return 0;
}
static void* APS5_VABI DummyOutputOpen(const char* filename) {
    (void)filename;
    return nullptr;
}
static int32_t APS5_VABI DummyOutputWrite(void* context, const char* buffer, int32_t len) {
    (void)context;
    (void)buffer;
    (void)len;
    return -1;
}
static int32_t APS5_VABI DummyOutputClose(void* context) {
    (void)context;
    return 0;
}

void TestIoCallbacks() {
    int inRet = xmlRegisterInputCallbacks_nid_postfix(DummyInputMatch, DummyInputOpen, DummyInputRead, DummyInputClose);
    AssertTrue(inRet >= 0, "xmlRegisterInputCallbacks failed");

    int outRet = xmlRegisterOutputCallbacks_nid_postfix(DummyOutputMatch, DummyOutputOpen, DummyOutputWrite, DummyOutputClose);
    AssertTrue(outRet >= 0, "xmlRegisterOutputCallbacks failed");
}

} // namespace

int main() {
    TestBasicStringsAndDict();
    TestPushParserChunks();
    TestErrorHandling();
    TestMemoryParserAndEntities();
    TestIoCallbacks();
    std::printf("Xml tests passed.\n");
    return 0;
}
