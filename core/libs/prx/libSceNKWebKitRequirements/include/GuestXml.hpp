#ifndef CORE_LIBS_PRX_LIBSCE_NKWEBKIT_REQUIREMENTS_INCLUDE_GUEST_XML_HPP
#define CORE_LIBS_PRX_LIBSCE_NKWEBKIT_REQUIREMENTS_INCLUDE_GUEST_XML_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit {

struct GuestDoc {
    void* _private;
    int32_t type;
    uint32_t pad1;
    char* name;
    void* children;
    void* last;
    void* parent;
    void* next;
    void* prev;
    GuestDoc* doc;

    int32_t compression;
    int32_t standalone;
    void* intSubset;
    void* extSubset;
    void* oldNs;
    const uint8_t* version;
    const uint8_t* encoding;
    void* ids;
    void* refs;
    const uint8_t* URL;
    int32_t charset;
    uint32_t pad2;
    void* dict;
    void* psvi;
    int32_t parseFlags;
    int32_t properties;
};

static_assert(sizeof(GuestDoc) == 176);
static_assert(offsetof(GuestDoc, _private) == 0);
static_assert(offsetof(GuestDoc, type) == 8);
static_assert(offsetof(GuestDoc, name) == 16);
static_assert(offsetof(GuestDoc, children) == 24);
static_assert(offsetof(GuestDoc, doc) == 64);
static_assert(offsetof(GuestDoc, compression) == 72);
static_assert(offsetof(GuestDoc, standalone) == 76);
static_assert(offsetof(GuestDoc, intSubset) == 80);
static_assert(offsetof(GuestDoc, extSubset) == 88);
static_assert(offsetof(GuestDoc, encoding) == 112);
static_assert(offsetof(GuestDoc, dict) == 152);
static_assert(offsetof(GuestDoc, properties) == 172);

struct GuestEntity {
    void* _private;
    int32_t type;
    uint32_t pad1;
    const uint8_t* name;
    void* children;
    void* last;
    void* parent;
    void* next;
    void* prev;
    GuestDoc* doc;

    uint8_t* orig;
    uint8_t* content;
    int32_t length;
    int32_t etype;
    const uint8_t* ExternalID;
    const uint8_t* SystemID;

    GuestEntity* nexte;
    const uint8_t* URI;
    int32_t owner;
    int32_t checked;
};

static_assert(sizeof(GuestEntity) == 136);
static_assert(offsetof(GuestEntity, _private) == 0);
static_assert(offsetof(GuestEntity, type) == 8);
static_assert(offsetof(GuestEntity, name) == 16);
static_assert(offsetof(GuestEntity, children) == 24);
static_assert(offsetof(GuestEntity, doc) == 64);
static_assert(offsetof(GuestEntity, orig) == 72);
static_assert(offsetof(GuestEntity, content) == 80);
static_assert(offsetof(GuestEntity, length) == 88);
static_assert(offsetof(GuestEntity, etype) == 92);
static_assert(offsetof(GuestEntity, ExternalID) == 96);
static_assert(offsetof(GuestEntity, SystemID) == 104);
static_assert(offsetof(GuestEntity, nexte) == 112);
static_assert(offsetof(GuestEntity, URI) == 120);
static_assert(offsetof(GuestEntity, owner) == 128);
static_assert(offsetof(GuestEntity, checked) == 132);

struct GuestXmlError {
    int32_t domain;
    int32_t code;
    char* message;
    int32_t level;
    uint32_t pad1;
    char* file;
    int32_t line;
    uint32_t pad2;
    char* str1;
    char* str2;
    char* str3;
    int32_t int1;
    int32_t int2;
    void* ctxt;
    void* node;
};

static_assert(sizeof(GuestXmlError) == 88);
static_assert(offsetof(GuestXmlError, domain) == 0);
static_assert(offsetof(GuestXmlError, code) == 4);
static_assert(offsetof(GuestXmlError, message) == 8);
static_assert(offsetof(GuestXmlError, level) == 16);
static_assert(offsetof(GuestXmlError, file) == 24);
static_assert(offsetof(GuestXmlError, line) == 32);
static_assert(offsetof(GuestXmlError, str1) == 40);
static_assert(offsetof(GuestXmlError, str2) == 48);
static_assert(offsetof(GuestXmlError, str3) == 56);
static_assert(offsetof(GuestXmlError, int1) == 64);
static_assert(offsetof(GuestXmlError, int2) == 68);
static_assert(offsetof(GuestXmlError, ctxt) == 72);
static_assert(offsetof(GuestXmlError, node) == 80);

struct GuestParserNodeInfoSeq {
    uint64_t maximum;
    uint64_t length;
    void* buffer;
};

static_assert(sizeof(GuestParserNodeInfoSeq) == 24);
static_assert(offsetof(GuestParserNodeInfoSeq, maximum) == 0);
static_assert(offsetof(GuestParserNodeInfoSeq, length) == 8);
static_assert(offsetof(GuestParserNodeInfoSeq, buffer) == 16);

struct GuestValidCtxt {
    void* userData;
    void* error;
    void* warning;
    void* node;
    int32_t nodeNr;
    int32_t nodeMax;
    void* nodeTab;
    uint32_t finishDtd;
    uint32_t pad1;
    GuestDoc* doc;
    int32_t valid;
    uint32_t pad2;
    void* vstate;
    int32_t vstateNr;
    int32_t vstateMax;
    void* vstateTab;
    void* am;
    void* state;
};

static_assert(sizeof(GuestValidCtxt) == 112);
static_assert(offsetof(GuestValidCtxt, userData) == 0);
static_assert(offsetof(GuestValidCtxt, error) == 8);
static_assert(offsetof(GuestValidCtxt, warning) == 16);
static_assert(offsetof(GuestValidCtxt, node) == 24);
static_assert(offsetof(GuestValidCtxt, nodeNr) == 32);
static_assert(offsetof(GuestValidCtxt, nodeMax) == 36);
static_assert(offsetof(GuestValidCtxt, nodeTab) == 40);
static_assert(offsetof(GuestValidCtxt, finishDtd) == 48);
static_assert(offsetof(GuestValidCtxt, doc) == 56);
static_assert(offsetof(GuestValidCtxt, valid) == 64);
static_assert(offsetof(GuestValidCtxt, vstate) == 72);
static_assert(offsetof(GuestValidCtxt, am) == 96);
static_assert(offsetof(GuestValidCtxt, state) == 104);

struct GuestParserInput {
    void* buf;
    const char* filename;
    const char* directory;
    const uint8_t* base;
    const uint8_t* cur;
    const uint8_t* end;
    int32_t length;
    int32_t line;
    int32_t col;
    uint32_t pad1;
    uint64_t consumed;
    void* free;
    const uint8_t* encoding;
    const uint8_t* version;
    int32_t standalone;
    int32_t id;
};

static_assert(sizeof(GuestParserInput) == 104);
static_assert(offsetof(GuestParserInput, buf) == 0);
static_assert(offsetof(GuestParserInput, filename) == 8);
static_assert(offsetof(GuestParserInput, directory) == 16);
static_assert(offsetof(GuestParserInput, base) == 24);
static_assert(offsetof(GuestParserInput, cur) == 32);
static_assert(offsetof(GuestParserInput, end) == 40);
static_assert(offsetof(GuestParserInput, length) == 48);
static_assert(offsetof(GuestParserInput, line) == 52);
static_assert(offsetof(GuestParserInput, col) == 56);
static_assert(offsetof(GuestParserInput, consumed) == 64);
static_assert(offsetof(GuestParserInput, free) == 72);
static_assert(offsetof(GuestParserInput, encoding) == 80);
static_assert(offsetof(GuestParserInput, version) == 88);
static_assert(offsetof(GuestParserInput, standalone) == 96);
static_assert(offsetof(GuestParserInput, id) == 100);

using GuestInternalSubsetSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name, const uint8_t* ExternalID, const uint8_t* SystemID);
using GuestIsStandaloneSAXFunc = int32_t (APS5_VABI *)(void* ctx);
using GuestHasInternalSubsetSAXFunc = int32_t (APS5_VABI *)(void* ctx);
using GuestHasExternalSubsetSAXFunc = int32_t (APS5_VABI *)(void* ctx);
using GuestResolveEntitySAXFunc = void* (APS5_VABI *)(void* ctx, const uint8_t* publicId, const uint8_t* systemId);
using GuestGetEntitySAXFunc = GuestEntity* (APS5_VABI *)(void* ctx, const uint8_t* name);
using GuestEntityDeclSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name, int32_t type, const uint8_t* publicId, const uint8_t* systemId, const uint8_t* content);
using GuestNotationDeclSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name, const uint8_t* publicId, const uint8_t* systemId);
using GuestAttributeDeclSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* elem, const uint8_t* fullname, int32_t type, int32_t def, const uint8_t* defaultValue, void* tree);
using GuestElementDeclSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name, int32_t type, void* content);
using GuestUnparsedEntityDeclSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name, const uint8_t* publicId, const uint8_t* systemId, const uint8_t* notationName);
using GuestSetDocumentLocatorSAXFunc = void (APS5_VABI *)(void* ctx, void* loc);
using GuestStartDocumentSAXFunc = void (APS5_VABI *)(void* ctx);
using GuestEndDocumentSAXFunc = void (APS5_VABI *)(void* ctx);
using GuestStartElementSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name, const uint8_t** atts);
using GuestEndElementSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name);
using GuestReferenceSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name);
using GuestCharactersSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* ch, int32_t len);
using GuestIgnorableWhitespaceSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* ch, int32_t len);
using GuestProcessingInstructionSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* target, const uint8_t* data);
using GuestCommentSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* value);
using GuestWarningSAXFunc = void (APS5_VABI *)(void* ctx, const char* msg, ...);
using GuestErrorSAXFunc = void (APS5_VABI *)(void* ctx, const char* msg, ...);
using GuestFatalErrorSAXFunc = void (APS5_VABI *)(void* ctx, const char* msg, ...);
using GuestGetParameterEntitySAXFunc = GuestEntity* (APS5_VABI *)(void* ctx, const uint8_t* name);
using GuestCdataBlockSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* value, int32_t len);
using GuestExternalSubsetSAXFunc = void (APS5_VABI *)(void* ctx, const uint8_t* name, const uint8_t* ExternalID, const uint8_t* SystemID);
using GuestStartElementNsSAX2Func = void (APS5_VABI *)(void* ctx, const uint8_t* localname, const uint8_t* prefix, const uint8_t* URI, int32_t nb_namespaces, const uint8_t** namespaces, int32_t nb_attributes, int32_t nb_defaulted, const uint8_t** attributes);
using GuestEndElementNsSAX2Func = void (APS5_VABI *)(void* ctx, const uint8_t* localname, const uint8_t* prefix, const uint8_t* URI);
using GuestStructuredErrorFunc = void (APS5_VABI *)(void* userData, GuestXmlError* error);

struct GuestSAXHandler {
    GuestInternalSubsetSAXFunc internalSubset;
    GuestIsStandaloneSAXFunc isStandalone;
    GuestHasInternalSubsetSAXFunc hasInternalSubset;
    GuestHasExternalSubsetSAXFunc hasExternalSubset;
    GuestResolveEntitySAXFunc resolveEntity;
    GuestGetEntitySAXFunc getEntity;
    GuestEntityDeclSAXFunc entityDecl;
    GuestNotationDeclSAXFunc notationDecl;
    GuestAttributeDeclSAXFunc attributeDecl;
    GuestElementDeclSAXFunc elementDecl;
    GuestUnparsedEntityDeclSAXFunc unparsedEntityDecl;
    GuestSetDocumentLocatorSAXFunc setDocumentLocator;
    GuestStartDocumentSAXFunc startDocument;
    GuestEndDocumentSAXFunc endDocument;
    GuestStartElementSAXFunc startElement;
    GuestEndElementSAXFunc endElement;
    GuestReferenceSAXFunc reference;
    GuestCharactersSAXFunc characters;
    GuestIgnorableWhitespaceSAXFunc ignorableWhitespace;
    GuestProcessingInstructionSAXFunc processingInstruction;
    GuestCommentSAXFunc comment;
    GuestWarningSAXFunc warning;
    GuestErrorSAXFunc error;
    GuestFatalErrorSAXFunc fatalError;
    GuestGetParameterEntitySAXFunc getParameterEntity;
    GuestCdataBlockSAXFunc cdataBlock;
    GuestExternalSubsetSAXFunc externalSubset;
    uint32_t initialized;
    uint32_t pad;
    void* _private;
    GuestStartElementNsSAX2Func startElementNs;
    GuestEndElementNsSAX2Func endElementNs;
    GuestStructuredErrorFunc serror;
};

static_assert(sizeof(GuestSAXHandler) == 256);
static_assert(offsetof(GuestSAXHandler, internalSubset) == 0);
static_assert(offsetof(GuestSAXHandler, setDocumentLocator) == 88);
static_assert(offsetof(GuestSAXHandler, startDocument) == 96);
static_assert(offsetof(GuestSAXHandler, endDocument) == 104);
static_assert(offsetof(GuestSAXHandler, startElement) == 112);
static_assert(offsetof(GuestSAXHandler, endElement) == 120);
static_assert(offsetof(GuestSAXHandler, characters) == 136);
static_assert(offsetof(GuestSAXHandler, cdataBlock) == 200);
static_assert(offsetof(GuestSAXHandler, initialized) == 216);
static_assert(offsetof(GuestSAXHandler, startElementNs) == 232);
static_assert(offsetof(GuestSAXHandler, endElementNs) == 240);
static_assert(offsetof(GuestSAXHandler, serror) == 248);

struct GuestParserCtxt {
    GuestSAXHandler* sax;
    void* userData;
    GuestDoc* myDoc;
    int32_t wellFormed;
    int32_t replaceEntities;
    const uint8_t* version;
    const uint8_t* encoding;
    int32_t standalone;
    int32_t html;

    GuestParserInput* input;
    int32_t inputNr;
    int32_t inputMax;
    GuestParserInput** inputTab;

    void* node;
    int32_t nodeNr;
    int32_t nodeMax;
    void** nodeTab;

    int32_t record_info;
    uint32_t pad1;
    GuestParserNodeInfoSeq node_seq;

    int32_t errNo;
    int32_t hasExternalSubset;
    int32_t hasPErefs;
    int32_t external;

    int32_t valid;
    int32_t validate;
    GuestValidCtxt vctxt;

    int32_t instate;
    int32_t token;

    char* directory;

    const uint8_t* name;
    int32_t nameNr;
    int32_t nameMax;
    const uint8_t** nameTab;

    int64_t nbChars;
    int64_t checkIndex;
    int32_t keepBlanks;
    int32_t disableSAX;
    int32_t inSubset;
    uint32_t pad2;
    const uint8_t* intSubName;
    uint8_t* extSubURI;
    uint8_t* extSubSystem;

    int32_t* space;
    int32_t spaceNr;
    int32_t spaceMax;
    int32_t* spaceTab;

    int32_t depth;
    uint32_t pad3;
    GuestParserInput* entity;
    int32_t charset;
    int32_t nodelen;
    int32_t nodemem;
    int32_t pedantic;
    void* _private;

    int32_t loadsubset;
    int32_t linenumbers;
    void* catalogs;
    int32_t recovery;
    int32_t progressive;
    void* dict;
    const uint8_t** atts;
    int32_t maxatts;
    int32_t docdict;

    const uint8_t* str_xml;
    const uint8_t* str_xmlns;
    const uint8_t* str_xml_ns;

    int32_t sax2;
    int32_t nsNr;
    int32_t nsMax;
    uint32_t pad4;
    const uint8_t** nsTab;
    int32_t* attallocs;
    void* pushTab;
    void* attsDefault;
    void* attsSpecial;
    int32_t nsWellFormed;
    int32_t options;

    int32_t dictNames;
    int32_t freeElemsNr;
    void* freeElems;
    int32_t freeAttrsNr;
    uint32_t pad5;
    void* freeAttrs;

    GuestXmlError lastError;
    int32_t parseMode;
    uint32_t pad6;
    uint64_t nbentities;
    uint64_t sizeentities;

    void* nodeInfo;
    int32_t nodeInfoNr;
    int32_t nodeInfoMax;
    void* nodeInfoTab;

    int32_t input_id;
    uint32_t pad7;
    uint64_t sizeentcopy;
};

static_assert(sizeof(GuestParserCtxt) == 752);
static_assert(offsetof(GuestParserCtxt, sax) == 0);
static_assert(offsetof(GuestParserCtxt, userData) == 8);
static_assert(offsetof(GuestParserCtxt, myDoc) == 16);
static_assert(offsetof(GuestParserCtxt, wellFormed) == 24);
static_assert(offsetof(GuestParserCtxt, replaceEntities) == 28);
static_assert(offsetof(GuestParserCtxt, version) == 32);
static_assert(offsetof(GuestParserCtxt, encoding) == 40);
static_assert(offsetof(GuestParserCtxt, standalone) == 48);
static_assert(offsetof(GuestParserCtxt, html) == 52);
static_assert(offsetof(GuestParserCtxt, input) == 56);
static_assert(offsetof(GuestParserCtxt, inputNr) == 64);
static_assert(offsetof(GuestParserCtxt, inputMax) == 68);
static_assert(offsetof(GuestParserCtxt, inputTab) == 72);
static_assert(offsetof(GuestParserCtxt, node) == 80);
static_assert(offsetof(GuestParserCtxt, nodeNr) == 88);
static_assert(offsetof(GuestParserCtxt, nodeMax) == 92);
static_assert(offsetof(GuestParserCtxt, nodeTab) == 96);
static_assert(offsetof(GuestParserCtxt, record_info) == 104);
static_assert(offsetof(GuestParserCtxt, node_seq) == 112);
static_assert(offsetof(GuestParserCtxt, errNo) == 136);
static_assert(offsetof(GuestParserCtxt, hasExternalSubset) == 140);
static_assert(offsetof(GuestParserCtxt, hasPErefs) == 144);
static_assert(offsetof(GuestParserCtxt, external) == 148);
static_assert(offsetof(GuestParserCtxt, valid) == 152);
static_assert(offsetof(GuestParserCtxt, validate) == 156);
static_assert(offsetof(GuestParserCtxt, vctxt) == 160);
static_assert(offsetof(GuestParserCtxt, instate) == 272);
static_assert(offsetof(GuestParserCtxt, token) == 276);
static_assert(offsetof(GuestParserCtxt, directory) == 280);
static_assert(offsetof(GuestParserCtxt, name) == 288);
static_assert(offsetof(GuestParserCtxt, nameNr) == 296);
static_assert(offsetof(GuestParserCtxt, nameMax) == 300);
static_assert(offsetof(GuestParserCtxt, nameTab) == 304);
static_assert(offsetof(GuestParserCtxt, nbChars) == 312);
static_assert(offsetof(GuestParserCtxt, checkIndex) == 320);
static_assert(offsetof(GuestParserCtxt, keepBlanks) == 328);
static_assert(offsetof(GuestParserCtxt, disableSAX) == 332);
static_assert(offsetof(GuestParserCtxt, inSubset) == 336);
static_assert(offsetof(GuestParserCtxt, intSubName) == 344);
static_assert(offsetof(GuestParserCtxt, extSubURI) == 352);
static_assert(offsetof(GuestParserCtxt, extSubSystem) == 360);
static_assert(offsetof(GuestParserCtxt, space) == 368);
static_assert(offsetof(GuestParserCtxt, spaceNr) == 376);
static_assert(offsetof(GuestParserCtxt, spaceMax) == 380);
static_assert(offsetof(GuestParserCtxt, spaceTab) == 384);
static_assert(offsetof(GuestParserCtxt, depth) == 392);
static_assert(offsetof(GuestParserCtxt, entity) == 400);
static_assert(offsetof(GuestParserCtxt, charset) == 408);
static_assert(offsetof(GuestParserCtxt, nodelen) == 412);
static_assert(offsetof(GuestParserCtxt, nodemem) == 416);
static_assert(offsetof(GuestParserCtxt, pedantic) == 420);
static_assert(offsetof(GuestParserCtxt, _private) == 424);
static_assert(offsetof(GuestParserCtxt, loadsubset) == 432);
static_assert(offsetof(GuestParserCtxt, linenumbers) == 436);
static_assert(offsetof(GuestParserCtxt, catalogs) == 440);
static_assert(offsetof(GuestParserCtxt, recovery) == 448);
static_assert(offsetof(GuestParserCtxt, progressive) == 452);
static_assert(offsetof(GuestParserCtxt, dict) == 456);
static_assert(offsetof(GuestParserCtxt, atts) == 464);
static_assert(offsetof(GuestParserCtxt, maxatts) == 472);
static_assert(offsetof(GuestParserCtxt, docdict) == 476);
static_assert(offsetof(GuestParserCtxt, str_xml) == 480);
static_assert(offsetof(GuestParserCtxt, str_xmlns) == 488);
static_assert(offsetof(GuestParserCtxt, str_xml_ns) == 496);
static_assert(offsetof(GuestParserCtxt, sax2) == 504);
static_assert(offsetof(GuestParserCtxt, nsNr) == 508);
static_assert(offsetof(GuestParserCtxt, nsMax) == 512);
static_assert(offsetof(GuestParserCtxt, nsTab) == 520);
static_assert(offsetof(GuestParserCtxt, attallocs) == 528);
static_assert(offsetof(GuestParserCtxt, pushTab) == 536);
static_assert(offsetof(GuestParserCtxt, attsDefault) == 544);
static_assert(offsetof(GuestParserCtxt, attsSpecial) == 552);
static_assert(offsetof(GuestParserCtxt, nsWellFormed) == 560);
static_assert(offsetof(GuestParserCtxt, options) == 564);
static_assert(offsetof(GuestParserCtxt, dictNames) == 568);
static_assert(offsetof(GuestParserCtxt, freeElemsNr) == 572);
static_assert(offsetof(GuestParserCtxt, freeElems) == 576);
static_assert(offsetof(GuestParserCtxt, freeAttrsNr) == 584);
static_assert(offsetof(GuestParserCtxt, freeAttrs) == 592);
static_assert(offsetof(GuestParserCtxt, lastError) == 600);
static_assert(offsetof(GuestParserCtxt, parseMode) == 688);
static_assert(offsetof(GuestParserCtxt, nbentities) == 696);
static_assert(offsetof(GuestParserCtxt, sizeentities) == 704);
static_assert(offsetof(GuestParserCtxt, nodeInfo) == 712);
static_assert(offsetof(GuestParserCtxt, nodeInfoNr) == 720);
static_assert(offsetof(GuestParserCtxt, nodeInfoMax) == 724);
static_assert(offsetof(GuestParserCtxt, nodeInfoTab) == 728);
static_assert(offsetof(GuestParserCtxt, input_id) == 736);
static_assert(offsetof(GuestParserCtxt, sizeentcopy) == 744);

using GuestInputMatchCallback = int32_t (APS5_VABI *)(const char* filename);
using GuestInputOpenCallback = void* (APS5_VABI *)(const char* filename);
using GuestInputReadCallback = int32_t (APS5_VABI *)(void* context, char* buffer, int32_t len);
using GuestInputCloseCallback = int32_t (APS5_VABI *)(void* context);

using GuestOutputMatchCallback = int32_t (APS5_VABI *)(const char* filename);
using GuestOutputOpenCallback = void* (APS5_VABI *)(const char* filename);
using GuestOutputWriteCallback = int32_t (APS5_VABI *)(void* context, const char* buffer, int32_t len);
using GuestOutputCloseCallback = int32_t (APS5_VABI *)(void* context);

} // namespace NKWebKit

#endif
