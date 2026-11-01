#include "prx/libSceNKWebKitRequirements/include/GuestXml.hpp"
#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"

#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlstring.h>
#include <libxml/dict.h>
#include <libxml/entities.h>
#include <libxml/xmlIO.h>
#include <libxml/SAX2.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace {

struct XmlBinding {
    NKWebKit::GuestParserCtxt guest_ctxt{};
    xmlParserCtxtPtr host_ctxt = nullptr;
    NKWebKit::GuestSAXHandler* guest_sax = nullptr;
    xmlSAXHandler host_sax{};
    void* guest_user_data = nullptr;
    NKWebKit::GuestXmlError guest_error{};
    std::string error_message;
    std::string error_file;
    std::string error_str1, error_str2, error_str3;
    NKWebKit::GuestDoc guest_doc{};
    NKWebKit::GuestParserInput guest_input{};
};

static std::mutex g_xmlMutex;
static std::unordered_map<NKWebKit::GuestParserCtxt*, std::unique_ptr<XmlBinding>> g_xmlBindings;
static std::unordered_map<const NKWebKit::GuestDoc*, xmlDocPtr> g_docMap;

static NKWebKit::GuestInputMatchCallback s_guestInputMatch = nullptr;
static NKWebKit::GuestInputOpenCallback s_guestInputOpen = nullptr;
static NKWebKit::GuestInputReadCallback s_guestInputRead = nullptr;
static NKWebKit::GuestInputCloseCallback s_guestInputClose = nullptr;

static NKWebKit::GuestOutputMatchCallback s_guestOutputMatch = nullptr;
static NKWebKit::GuestOutputOpenCallback s_guestOutputOpen = nullptr;
static NKWebKit::GuestOutputWriteCallback s_guestOutputWrite = nullptr;
static NKWebKit::GuestOutputCloseCallback s_guestOutputClose = nullptr;

static XmlBinding* FindBinding(NKWebKit::GuestParserCtxt* ctxt) {
    if (!ctxt) return nullptr;
    std::lock_guard<std::mutex> lock(g_xmlMutex);
    auto it = g_xmlBindings.find(ctxt);
    if (it != g_xmlBindings.end()) {
        return it->second.get();
    }
    return nullptr;
}

static void SyncHostErrorToGuest(xmlErrorPtr h, NKWebKit::GuestXmlError* g, XmlBinding* b) {
    if (!h || !g) return;
    g->domain = h->domain;
    g->code = h->code;
    g->level = static_cast<int32_t>(h->level);
    g->line = h->line;
    g->int1 = h->int1;
    g->int2 = h->int2;
    g->ctxt = h->ctxt;
    g->node = h->node;

    if (b) {
        b->error_message = h->message ? h->message : "";
        b->error_file = h->file ? h->file : "";
        b->error_str1 = h->str1 ? h->str1 : "";
        b->error_str2 = h->str2 ? h->str2 : "";
        b->error_str3 = h->str3 ? h->str3 : "";
        g->message = b->error_message.empty() ? nullptr : b->error_message.data();
        g->file = b->error_file.empty() ? nullptr : b->error_file.data();
        g->str1 = b->error_str1.empty() ? nullptr : b->error_str1.data();
        g->str2 = b->error_str2.empty() ? nullptr : b->error_str2.data();
        g->str3 = b->error_str3.empty() ? nullptr : b->error_str3.data();
    } else {
        g->message = h->message;
        g->file = h->file;
        g->str1 = h->str1;
        g->str2 = h->str2;
        g->str3 = h->str3;
    }
}

static void SyncHostDocToGuest(xmlDocPtr h, NKWebKit::GuestDoc* g) {
    if (!h || !g) return;
    g->_private = h->_private;
    g->type = static_cast<int32_t>(h->type);
    g->name = h->name;
    g->children = h->children;
    g->last = h->last;
    g->parent = h->parent;
    g->next = h->next;
    g->prev = h->prev;
    g->doc = g;
    g->compression = h->compression;
    g->standalone = h->standalone;
    g->intSubset = h->intSubset;
    g->extSubset = h->extSubset;
    g->oldNs = h->oldNs;
    g->version = h->version;
    g->encoding = h->encoding;
    g->ids = h->ids;
    g->refs = h->refs;
    g->URL = h->URL;
    g->charset = h->charset;
    g->dict = h->dict;
    g->psvi = h->psvi;
    g->parseFlags = h->parseFlags;
    g->properties = h->properties;

    std::lock_guard<std::mutex> lock(g_xmlMutex);
    g_docMap[g] = h;
}

static void SyncHostInputToGuest(xmlParserInputPtr h, NKWebKit::GuestParserInput* g) {
    if (!h || !g) return;
    g->buf = h->buf;
    g->filename = h->filename;
    g->directory = h->directory;
    g->base = h->base;
    g->cur = h->cur;
    g->end = h->end;
    g->length = h->length;
    g->line = h->line;
    g->col = h->col;
    g->consumed = static_cast<uint64_t>(h->consumed);
    g->free = reinterpret_cast<void*>(h->free);
    g->encoding = h->encoding;
    g->version = h->version;
    g->standalone = h->standalone;
    g->id = h->id;
}

static void SyncHostToGuest(XmlBinding* b) {
    if (!b || !b->host_ctxt) return;
    auto* h = b->host_ctxt;
    auto* g = &b->guest_ctxt;
    g->wellFormed = h->wellFormed;
    g->errNo = h->errNo;
    g->disableSAX = h->disableSAX;
    g->options = h->options;
    g->loadsubset = h->loadsubset;
    g->linenumbers = h->linenumbers;
    g->recovery = h->recovery;
    g->progressive = h->progressive;
    g->instate = static_cast<int32_t>(h->instate);
    g->token = h->token;
    g->nbChars = static_cast<int64_t>(h->nbChars);
    g->checkIndex = static_cast<int64_t>(h->checkIndex);
    g->dict = h->dict;
    g->sax2 = h->sax2;
    g->replaceEntities = h->replaceEntities;
    g->spaceNr = h->spaceNr;
    g->spaceMax = h->spaceMax;
    g->spaceTab = h->spaceTab;
    g->depth = h->depth;
    g->charset = h->charset;
    g->nodelen = h->nodelen;
    g->nodemem = h->nodemem;
    g->pedantic = h->pedantic;
    g->_private = h->_private;

    if (h->myDoc) {
        SyncHostDocToGuest(h->myDoc, &b->guest_doc);
        g->myDoc = &b->guest_doc;
    } else {
        g->myDoc = nullptr;
    }

    if (h->input) {
        SyncHostInputToGuest(h->input, &b->guest_input);
        g->input = &b->guest_input;
    } else {
        g->input = nullptr;
    }

    SyncHostErrorToGuest(&h->lastError, &g->lastError, b);
}

static void SyncGuestToHost(XmlBinding* b) {
    if (!b || !b->host_ctxt) return;
    auto* h = b->host_ctxt;
    auto* g = &b->guest_ctxt;
    h->wellFormed = g->wellFormed;
    h->errNo = g->errNo;
    h->disableSAX = g->disableSAX;
    h->options = g->options;
    h->loadsubset = g->loadsubset;
    h->linenumbers = g->linenumbers;
    h->recovery = g->recovery;
    h->progressive = g->progressive;
    h->instate = static_cast<xmlParserInputState>(g->instate);
    h->token = g->token;
    h->nbChars = static_cast<long>(g->nbChars);
    h->checkIndex = static_cast<unsigned long>(g->checkIndex);
    h->replaceEntities = g->replaceEntities;
    h->spaceNr = g->spaceNr;
    h->spaceMax = g->spaceMax;
    h->spaceTab = g->spaceTab;
    h->depth = g->depth;
    h->charset = h->charset;
    h->nodelen = g->nodelen;
    h->nodemem = g->nodemem;
    h->pedantic = g->pedantic;
    h->_private = g->_private;
}

static NKWebKit::GuestEntity* SyncHostEntityToGuest(xmlEntityPtr h) {
    if (!h) return nullptr;
    static thread_local NKWebKit::GuestEntity s_guestEntity{};
    s_guestEntity._private = h->_private;
    s_guestEntity.type = static_cast<int32_t>(h->type);
    s_guestEntity.name = h->name;
    s_guestEntity.children = h->children;
    s_guestEntity.last = h->last;
    s_guestEntity.parent = h->parent;
    s_guestEntity.next = h->next;
    s_guestEntity.prev = h->prev;
    s_guestEntity.doc = nullptr;
    s_guestEntity.orig = h->orig;
    s_guestEntity.content = h->content;
    s_guestEntity.length = h->length;
    s_guestEntity.etype = static_cast<int32_t>(h->etype);
    s_guestEntity.ExternalID = h->ExternalID;
    s_guestEntity.SystemID = h->SystemID;
    s_guestEntity.nexte = nullptr;
    s_guestEntity.URI = h->URI;
    s_guestEntity.owner = h->owner;
    s_guestEntity.checked = h->checked;
    return &s_guestEntity;
}

static void HostStartElementNs(void* ctx, const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI, int nb_namespaces, const xmlChar** namespaces, int nb_attributes, int nb_defaulted, const xmlChar** attributes) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->startElementNs) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->startElementNs(guest_ctx,
        reinterpret_cast<const uint8_t*>(localname),
        reinterpret_cast<const uint8_t*>(prefix),
        reinterpret_cast<const uint8_t*>(URI),
        nb_namespaces,
        reinterpret_cast<const uint8_t**>(namespaces),
        nb_attributes,
        nb_defaulted,
        reinterpret_cast<const uint8_t**>(attributes));
}

static void HostEndElementNs(void* ctx, const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->endElementNs) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->endElementNs(guest_ctx,
        reinterpret_cast<const uint8_t*>(localname),
        reinterpret_cast<const uint8_t*>(prefix),
        reinterpret_cast<const uint8_t*>(URI));
}

static void HostCharacters(void* ctx, const xmlChar* ch, int len) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->characters) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->characters(guest_ctx, reinterpret_cast<const uint8_t*>(ch), len);
}

static void HostCdataBlock(void* ctx, const xmlChar* value, int len) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->cdataBlock) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->cdataBlock(guest_ctx, reinterpret_cast<const uint8_t*>(value), len);
}

static void HostComment(void* ctx, const xmlChar* value) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->comment) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->comment(guest_ctx, reinterpret_cast<const uint8_t*>(value));
}

static void HostProcessingInstruction(void* ctx, const xmlChar* target, const xmlChar* data) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->processingInstruction) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->processingInstruction(guest_ctx, reinterpret_cast<const uint8_t*>(target), reinterpret_cast<const uint8_t*>(data));
}

static void HostStartDocument(void* ctx) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->startDocument) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->startDocument(guest_ctx);
}

static void HostEndDocument(void* ctx) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->endDocument) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->endDocument(guest_ctx);
}

static void HostStartElement(void* ctx, const xmlChar* name, const xmlChar** atts) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->startElement) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->startElement(guest_ctx, reinterpret_cast<const uint8_t*>(name), reinterpret_cast<const uint8_t**>(atts));
}

static void HostEndElement(void* ctx, const xmlChar* name) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->endElement) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->endElement(guest_ctx, reinterpret_cast<const uint8_t*>(name));
}

static void HostInternalSubset(void* ctx, const xmlChar* name, const xmlChar* ExternalID, const xmlChar* SystemID) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->internalSubset) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->internalSubset(guest_ctx, reinterpret_cast<const uint8_t*>(name), reinterpret_cast<const uint8_t*>(ExternalID), reinterpret_cast<const uint8_t*>(SystemID));
}

static void HostExternalSubset(void* ctx, const xmlChar* name, const xmlChar* ExternalID, const xmlChar* SystemID) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->externalSubset) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->externalSubset(guest_ctx, reinterpret_cast<const uint8_t*>(name), reinterpret_cast<const uint8_t*>(ExternalID), reinterpret_cast<const uint8_t*>(SystemID));
}

static int HostIsStandalone(void* ctx) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->isStandalone) return 0;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    return b->guest_sax->isStandalone(guest_ctx);
}

static int HostHasInternalSubset(void* ctx) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->hasInternalSubset) return 0;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    return b->guest_sax->hasInternalSubset(guest_ctx);
}

static int HostHasExternalSubset(void* ctx) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->hasExternalSubset) return 0;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    return b->guest_sax->hasExternalSubset(guest_ctx);
}

static xmlEntityPtr HostGetEntity(void* ctx, const xmlChar* name) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->getEntity) return nullptr;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    auto* gent = b->guest_sax->getEntity(guest_ctx, reinterpret_cast<const uint8_t*>(name));
    if (!gent) return nullptr;
    static thread_local xmlEntity s_hostEntity{};
    s_hostEntity._private = gent->_private;
    s_hostEntity.type = static_cast<xmlElementType>(gent->type);
    s_hostEntity.name = const_cast<xmlChar*>(reinterpret_cast<const xmlChar*>(gent->name));
    s_hostEntity.children = static_cast<xmlNodePtr>(gent->children);
    s_hostEntity.last = static_cast<xmlNodePtr>(gent->last);
    s_hostEntity.parent = reinterpret_cast<struct _xmlDtd*>(gent->parent);
    s_hostEntity.next = static_cast<xmlNodePtr>(gent->next);
    s_hostEntity.prev = static_cast<xmlNodePtr>(gent->prev);
    s_hostEntity.doc = nullptr;
    s_hostEntity.orig = gent->orig;
    s_hostEntity.content = gent->content;
    s_hostEntity.length = gent->length;
    s_hostEntity.etype = static_cast<xmlEntityType>(gent->etype);
    s_hostEntity.ExternalID = reinterpret_cast<const xmlChar*>(gent->ExternalID);
    s_hostEntity.SystemID = reinterpret_cast<const xmlChar*>(gent->SystemID);
    s_hostEntity.nexte = nullptr;
    s_hostEntity.URI = reinterpret_cast<const xmlChar*>(gent->URI);
    s_hostEntity.owner = gent->owner;
    s_hostEntity.checked = gent->checked;
    return &s_hostEntity;
}

static xmlEntityPtr HostGetParameterEntity(void* ctx, const xmlChar* name) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->getParameterEntity) return nullptr;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    auto* gent = b->guest_sax->getParameterEntity(guest_ctx, reinterpret_cast<const uint8_t*>(name));
    if (!gent) return nullptr;
    static thread_local xmlEntity s_hostPE{};
    s_hostPE._private = gent->_private;
    s_hostPE.type = static_cast<xmlElementType>(gent->type);
    s_hostPE.name = const_cast<xmlChar*>(reinterpret_cast<const xmlChar*>(gent->name));
    s_hostPE.children = static_cast<xmlNodePtr>(gent->children);
    s_hostPE.last = static_cast<xmlNodePtr>(gent->last);
    s_hostPE.parent = reinterpret_cast<struct _xmlDtd*>(gent->parent);
    s_hostPE.next = static_cast<xmlNodePtr>(gent->next);
    s_hostPE.prev = static_cast<xmlNodePtr>(gent->prev);
    s_hostPE.doc = nullptr;
    s_hostPE.orig = gent->orig;
    s_hostPE.content = gent->content;
    s_hostPE.length = gent->length;
    s_hostPE.etype = static_cast<xmlEntityType>(gent->etype);
    s_hostPE.ExternalID = reinterpret_cast<const xmlChar*>(gent->ExternalID);
    s_hostPE.SystemID = reinterpret_cast<const xmlChar*>(gent->SystemID);
    s_hostPE.nexte = nullptr;
    s_hostPE.URI = reinterpret_cast<const xmlChar*>(gent->URI);
    s_hostPE.owner = gent->owner;
    s_hostPE.checked = gent->checked;
    return &s_hostPE;
}

static void HostEntityDecl(void* ctx, const xmlChar* name, int type, const xmlChar* publicId, const xmlChar* systemId, xmlChar* content) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->entityDecl) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->entityDecl(guest_ctx,
        reinterpret_cast<const uint8_t*>(name),
        type,
        reinterpret_cast<const uint8_t*>(publicId),
        reinterpret_cast<const uint8_t*>(systemId),
        reinterpret_cast<const uint8_t*>(content));
}

static void HostIgnorableWhitespace(void* ctx, const xmlChar* ch, int len) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->ignorableWhitespace) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->ignorableWhitespace(guest_ctx, reinterpret_cast<const uint8_t*>(ch), len);
}

static void HostReference(void* ctx, const xmlChar* name) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->reference) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    b->guest_sax->reference(guest_ctx, reinterpret_cast<const uint8_t*>(name));
}

static void HostSerror(void* userData, xmlErrorPtr error) {
    auto* b = static_cast<XmlBinding*>(userData);
    if (!b || !b->guest_sax || !b->guest_sax->serror) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    SyncHostErrorToGuest(error, &b->guest_error, b);
    b->guest_sax->serror(guest_ctx, &b->guest_error);
}

static void HostWarning(void* ctx, const char* msg, ...) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->warning) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    char buf[1024];
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);
    b->guest_sax->warning(guest_ctx, "%s", buf);
}

static void HostError(void* ctx, const char* msg, ...) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->error) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    char buf[1024];
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);
    b->guest_sax->error(guest_ctx, "%s", buf);
}

static void HostFatalError(void* ctx, const char* msg, ...) {
    auto* b = static_cast<XmlBinding*>(ctx);
    if (!b || !b->guest_sax || !b->guest_sax->fatalError) return;
    void* guest_ctx = b->guest_user_data ? b->guest_user_data : &b->guest_ctxt;
    char buf[1024];
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);
    b->guest_sax->fatalError(guest_ctx, "%s", buf);
}

static void InitHostSax(NKWebKit::GuestSAXHandler* guest_sax, xmlSAXHandler* host_sax) {
    if (!guest_sax || !host_sax) return;
    host_sax->initialized = XML_SAX2_MAGIC;
    if (guest_sax->startElementNs) host_sax->startElementNs = HostStartElementNs;
    if (guest_sax->endElementNs) host_sax->endElementNs = HostEndElementNs;
    if (guest_sax->characters) host_sax->characters = HostCharacters;
    if (guest_sax->cdataBlock) host_sax->cdataBlock = HostCdataBlock;
    if (guest_sax->comment) host_sax->comment = HostComment;
    if (guest_sax->processingInstruction) host_sax->processingInstruction = HostProcessingInstruction;
    if (guest_sax->startDocument) host_sax->startDocument = HostStartDocument;
    if (guest_sax->endDocument) host_sax->endDocument = HostEndDocument;
    if (guest_sax->startElement) host_sax->startElement = HostStartElement;
    if (guest_sax->endElement) host_sax->endElement = HostEndElement;
    if (guest_sax->internalSubset) host_sax->internalSubset = HostInternalSubset;
    if (guest_sax->externalSubset) host_sax->externalSubset = HostExternalSubset;
    if (guest_sax->isStandalone) host_sax->isStandalone = HostIsStandalone;
    if (guest_sax->hasInternalSubset) host_sax->hasInternalSubset = HostHasInternalSubset;
    if (guest_sax->hasExternalSubset) host_sax->hasExternalSubset = HostHasExternalSubset;
    if (guest_sax->getEntity) host_sax->getEntity = HostGetEntity;
    if (guest_sax->getParameterEntity) host_sax->getParameterEntity = HostGetParameterEntity;
    if (guest_sax->entityDecl) host_sax->entityDecl = HostEntityDecl;
    if (guest_sax->ignorableWhitespace) host_sax->ignorableWhitespace = HostIgnorableWhitespace;
    if (guest_sax->reference) host_sax->reference = HostReference;
    if (guest_sax->serror) host_sax->serror = HostSerror;
    if (guest_sax->warning) host_sax->warning = HostWarning;
    if (guest_sax->error) host_sax->error = HostError;
    if (guest_sax->fatalError) host_sax->fatalError = HostFatalError;
}

static int HostInputMatch(const char* filename) {
    return s_guestInputMatch ? s_guestInputMatch(filename) : 0;
}

static void* HostInputOpen(const char* filename) {
    return s_guestInputOpen ? s_guestInputOpen(filename) : nullptr;
}

static int HostInputRead(void* context, char* buffer, int len) {
    return s_guestInputRead ? s_guestInputRead(context, buffer, len) : -1;
}

static int HostInputClose(void* context) {
    return s_guestInputClose ? s_guestInputClose(context) : -1;
}

static int HostOutputMatch(const char* filename) {
    return s_guestOutputMatch ? s_guestOutputMatch(filename) : 0;
}

static void* HostOutputOpen(const char* filename) {
    return s_guestOutputOpen ? s_guestOutputOpen(filename) : nullptr;
}

static int HostOutputWrite(void* context, const char* buffer, int len) {
    return s_guestOutputWrite ? s_guestOutputWrite(context, buffer, len) : -1;
}

static int HostOutputClose(void* context) {
    return s_guestOutputClose ? s_guestOutputClose(context) : -1;
}

} // namespace

extern "C" {

int64_t APS5_VABI xmlByteConsumed_nid_postfix(NKWebKit::GuestParserCtxt* ctxt) {
    auto* b = FindBinding(ctxt);
    if (!b || !b->host_ctxt) return 0;
    return static_cast<int64_t>(xmlByteConsumed(b->host_ctxt));
}

NKWebKit::GuestParserCtxt* APS5_VABI xmlCreateMemoryParserCtxt_nid_postfix(const char* buffer, int size) {
    auto b = std::make_unique<XmlBinding>();
    b->host_ctxt = xmlCreateMemoryParserCtxt(buffer, size);
    if (!b->host_ctxt) {
        return nullptr;
    }
    SyncHostToGuest(b.get());
    auto* ret = &b->guest_ctxt;
    std::lock_guard<std::mutex> lock(g_xmlMutex);
    g_xmlBindings[ret] = std::move(b);
    return ret;
}

NKWebKit::GuestParserCtxt* APS5_VABI xmlCreatePushParserCtxt_nid_postfix(NKWebKit::GuestSAXHandler* sax, void* user_data, const char* chunk, int size, const char* filename) {
    auto b = std::make_unique<XmlBinding>();
    b->guest_sax = sax;
    b->guest_user_data = user_data;
    b->guest_ctxt.userData = user_data;
    b->guest_ctxt.sax = sax;

    if (sax) {
        InitHostSax(sax, &b->host_sax);
        b->host_ctxt = xmlCreatePushParserCtxt(&b->host_sax, b.get(), chunk, size, filename);
    } else {
        b->host_ctxt = xmlCreatePushParserCtxt(nullptr, b.get(), chunk, size, filename);
    }

    if (!b->host_ctxt) {
        return nullptr;
    }

    SyncHostToGuest(b.get());

    auto* ret = &b->guest_ctxt;
    std::lock_guard<std::mutex> lock(g_xmlMutex);
    g_xmlBindings[ret] = std::move(b);
    return ret;
}

NKWebKit::GuestXmlError* APS5_VABI xmlCtxtGetLastError_nid_postfix(void* ctx) {
    auto* b = FindBinding(reinterpret_cast<NKWebKit::GuestParserCtxt*>(ctx));
    xmlErrorPtr err = xmlCtxtGetLastError(b ? b->host_ctxt : ctx);
    if (!err) return nullptr;
    if (b) {
        SyncHostErrorToGuest(err, &b->guest_error, b);
        return &b->guest_error;
    }
    static thread_local NKWebKit::GuestXmlError s_staticError{};
    SyncHostErrorToGuest(err, &s_staticError, nullptr);
    return &s_staticError;
}

int APS5_VABI xmlCtxtUseOptions_nid_postfix(NKWebKit::GuestParserCtxt* ctxt, int options) {
    auto* b = FindBinding(ctxt);
    if (!b || !b->host_ctxt) return -1;
    SyncGuestToHost(b);
    int ret = xmlCtxtUseOptions(b->host_ctxt, options);
    SyncHostToGuest(b);
    return ret;
}

const uint8_t* APS5_VABI xmlDictLookup_nid_postfix(void* dict, const uint8_t* name, int len) {
    return reinterpret_cast<const uint8_t*>(xmlDictLookup(reinterpret_cast<xmlDictPtr>(dict), reinterpret_cast<const xmlChar*>(name), len));
}

void APS5_VABI xmlFreeDoc_nid_postfix(NKWebKit::GuestDoc* cur) {
    if (!cur) return;
    xmlDocPtr host_doc = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_xmlMutex);
        auto it = g_docMap.find(cur);
        if (it != g_docMap.end()) {
            host_doc = it->second;
            g_docMap.erase(it);
        }
    }
    if (host_doc) {
        xmlFreeDoc(host_doc);
    }
}

void APS5_VABI xmlFreeParserCtxt_nid_postfix(NKWebKit::GuestParserCtxt* ctxt) {
    if (!ctxt) return;
    std::unique_ptr<XmlBinding> b;
    {
        std::lock_guard<std::mutex> lock(g_xmlMutex);
        auto it = g_xmlBindings.find(ctxt);
        if (it != g_xmlBindings.end()) {
            b = std::move(it->second);
            g_xmlBindings.erase(it);
        }
    }
    if (b && b->host_ctxt) {
        xmlFreeParserCtxt(b->host_ctxt);
    }
}

NKWebKit::GuestEntity* APS5_VABI xmlGetDocEntity_nid_postfix(const NKWebKit::GuestDoc* doc, const uint8_t* name) {
    xmlDocPtr host_doc = nullptr;
    if (doc) {
        std::lock_guard<std::mutex> lock(g_xmlMutex);
        auto it = g_docMap.find(doc);
        if (it != g_docMap.end()) {
            host_doc = it->second;
        }
    }
    xmlEntityPtr ent = xmlGetDocEntity(host_doc, reinterpret_cast<const xmlChar*>(name));
    return SyncHostEntityToGuest(ent);
}

NKWebKit::GuestEntity* APS5_VABI xmlGetPredefinedEntity_nid_postfix(const uint8_t* name) {
    xmlEntityPtr ent = xmlGetPredefinedEntity(reinterpret_cast<const xmlChar*>(name));
    return SyncHostEntityToGuest(ent);
}

void APS5_VABI xmlInitParser_nid_postfix() {
    xmlInitParser();
}

int APS5_VABI xmlParseChunk_nid_postfix(NKWebKit::GuestParserCtxt* ctxt, const char* chunk, int size, int terminate) {
    auto* b = FindBinding(ctxt);
    if (!b || !b->host_ctxt) return -1;
    SyncGuestToHost(b);
    int ret = xmlParseChunk(b->host_ctxt, chunk, size, terminate);
    SyncHostToGuest(b);
    return ret;
}

int APS5_VABI xmlParseContent_nid_postfix(NKWebKit::GuestParserCtxt* ctxt) {
    auto* b = FindBinding(ctxt);
    if (!b || !b->host_ctxt) return -1;
    SyncGuestToHost(b);
    xmlParseContent(b->host_ctxt);
    SyncHostToGuest(b);
    return 0;
}

int APS5_VABI xmlRegisterInputCallbacks_nid_postfix(NKWebKit::GuestInputMatchCallback matchFunc, NKWebKit::GuestInputOpenCallback openFunc, NKWebKit::GuestInputReadCallback readFunc, NKWebKit::GuestInputCloseCallback closeFunc) {
    s_guestInputMatch = matchFunc;
    s_guestInputOpen = openFunc;
    s_guestInputRead = readFunc;
    s_guestInputClose = closeFunc;
    return xmlRegisterInputCallbacks(HostInputMatch, HostInputOpen, HostInputRead, HostInputClose);
}

int APS5_VABI xmlRegisterOutputCallbacks_nid_postfix(NKWebKit::GuestOutputMatchCallback matchFunc, NKWebKit::GuestOutputOpenCallback openFunc, NKWebKit::GuestOutputWriteCallback writeFunc, NKWebKit::GuestOutputCloseCallback closeFunc) {
    s_guestOutputMatch = matchFunc;
    s_guestOutputOpen = openFunc;
    s_guestOutputWrite = writeFunc;
    s_guestOutputClose = closeFunc;
    return xmlRegisterOutputCallbacks(HostOutputMatch, HostOutputOpen, HostOutputWrite, HostOutputClose);
}

void APS5_VABI xmlSAX2EndDocument_nid_postfix(void* ctx) {
    auto* b = FindBinding(reinterpret_cast<NKWebKit::GuestParserCtxt*>(ctx));
    xmlSAX2EndDocument(b ? b->host_ctxt : ctx);
}

void APS5_VABI xmlSAX2EntityDecl_nid_postfix(void* ctx, const uint8_t* name, int type, const uint8_t* publicId, const uint8_t* systemId, const uint8_t* content) {
    auto* b = FindBinding(reinterpret_cast<NKWebKit::GuestParserCtxt*>(ctx));
    xmlSAX2EntityDecl(b ? b->host_ctxt : ctx, reinterpret_cast<const xmlChar*>(name), type, reinterpret_cast<const xmlChar*>(publicId), reinterpret_cast<const xmlChar*>(systemId), const_cast<xmlChar*>(reinterpret_cast<const xmlChar*>(content)));
}

void APS5_VABI xmlSAX2InternalSubset_nid_postfix(void* ctx, const uint8_t* name, const uint8_t* ExternalID, const uint8_t* SystemID) {
    auto* b = FindBinding(reinterpret_cast<NKWebKit::GuestParserCtxt*>(ctx));
    xmlSAX2InternalSubset(b ? b->host_ctxt : ctx, reinterpret_cast<const xmlChar*>(name), reinterpret_cast<const xmlChar*>(ExternalID), reinterpret_cast<const xmlChar*>(SystemID));
}

void APS5_VABI xmlSAX2StartDocument_nid_postfix(void* ctx) {
    auto* b = FindBinding(reinterpret_cast<NKWebKit::GuestParserCtxt*>(ctx));
    xmlSAX2StartDocument(b ? b->host_ctxt : ctx);
}

void APS5_VABI xmlStopParser_nid_postfix(NKWebKit::GuestParserCtxt* ctxt) {
    auto* b = FindBinding(ctxt);
    if (!b || !b->host_ctxt) return;
    xmlStopParser(b->host_ctxt);
    SyncHostToGuest(b);
}

uint8_t* APS5_VABI xmlStrdup_nid_postfix(const uint8_t* cur) {
    return reinterpret_cast<uint8_t*>(xmlStrdup(reinterpret_cast<const xmlChar*>(cur)));
}

uint8_t* APS5_VABI xmlStrndup_nid_postfix(const uint8_t* cur, int len) {
    return reinterpret_cast<uint8_t*>(xmlStrndup(reinterpret_cast<const xmlChar*>(cur), len));
}

int APS5_VABI xmlSwitchEncoding_nid_postfix(NKWebKit::GuestParserCtxt* ctxt, int enc) {
    auto* b = FindBinding(ctxt);
    if (!b || !b->host_ctxt) return -1;
    SyncGuestToHost(b);
    int ret = xmlSwitchEncoding(b->host_ctxt, static_cast<xmlCharEncoding>(enc));
    SyncHostToGuest(b);
    return ret;
}

}
