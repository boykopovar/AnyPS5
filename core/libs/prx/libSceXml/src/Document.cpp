#include "prx/libSceXml/src/Document.hpp"

#include "prx/libSceXml/src/AttributeList.hpp"
#include "prx/libSceXml/src/DomReader.hpp"
#include "prx/libSceXml/src/NodeList.hpp"
#include "prx/libSceXml/src/Serializer.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace XmlDom {

namespace {

constexpr int ErrorNoMemory = static_cast<int>(0x80850001);
constexpr int ErrorNotInitialized = static_cast<int>(0x80850002);
constexpr int ErrorInvalidParameter = static_cast<int>(0x80850003);
constexpr int DocumentAlreadyInitialized = 0x00850003;
constexpr std::size_t ImplSize = 72;
constexpr std::size_t XmlNodeIdFieldsSize = offsetof(XmlNodeId, flag) + sizeof(std::uint8_t);
constexpr std::array<std::size_t, 7> ScratchSizes{1240, 96, 2088, 5304, 2088, 1752, 1752};

struct DocumentRef {
    std::shared_ptr<Generation> generation;
    XmlMemAllocator* allocator = nullptr;
    void* userData = nullptr;
    std::string serialized;
};

template <typename T>
bool CopyInGuest(T& destination, const T* source, std::size_t bytes = sizeof(T)) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&destination, source, bytes);
    errno = savedErrno;
    return copied;
}

template <typename T>
bool CopyOutGuest(T* destination, const T& source, std::size_t bytes = sizeof(T)) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &source, bytes);
    errno = savedErrno;
    return copied;
}

XmlHandle ReadGuestHandle(const XmlHandle* guest, const char* function) {
    XmlHandle local{};
    if (!CopyInGuest(local, guest)) {
        throw std::logic_error(std::string(function) + ": reading an unreadable object");
    }
    return local;
}

void WriteGuestNodeId(XmlNodeId* result, XmlNodeId value, const char* function) {
    if (!CopyOutGuest(result, value, XmlNodeIdFieldsSize)) {
        throw std::logic_error(std::string(function) + ": writing to an unwritable NodeId result");
    }
}

void WriteGuestInvalid(XmlNodeId* result, const char* function) {
    WriteGuestNodeId(result, XmlNodeId{nullptr, nullptr, 0}, function);
}

void WriteGuestWritableNode(XmlNodeId* result, const Generation* generation, const DomNode* node, const char* function) {
    if (node == nullptr) {
        WriteGuestInvalid(result, function);
        return;
    }
    WriteGuestNodeId(result, XmlNodeId{const_cast<Generation*>(generation), const_cast<DomNode*>(node), 0}, function);
}

enum class GuestStringState { Null, Empty, Value };

struct GuestStringRead {
    GuestStringState state = GuestStringState::Null;
    std::string value;
};

GuestStringRead ReadGuestString(const XmlString* guest, const char* function) {
    if (guest == nullptr) return {GuestStringState::Null, {}};
    XmlString local{};
    if (!CopyInGuest(local, guest)) {
        throw std::logic_error(std::string(function) + ": reading an unreadable string descriptor");
    }
    if (local.buffer == nullptr) {
        if (local.size != 0) throw std::invalid_argument(std::string(function) + ": null string buffer with non-zero size");
        return {GuestStringState::Null, {}};
    }
    if (local.size == 0) return {GuestStringState::Empty, {}};
    constexpr std::size_t Chunk = 64 * 1024;
    std::string out;
    for (std::size_t done = 0; done < local.size;) {
        const std::size_t step = std::min(Chunk, local.size - done);
        out.resize(done + step);
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(out.data() + done, local.buffer + done, step);
        errno = savedErrno;
        if (!ok) throw std::logic_error(std::string(function) + ": reading an unreadable string buffer");
        done += step;
    }
    return {GuestStringState::Value, std::move(out)};
}

DocumentRef* RefOf(const XmlHandle* document) {
    DocumentRef* ref = nullptr;
    std::memcpy(&ref, document->impl, sizeof(ref));
    return ref;
}

[[noreturn]] void AllocationFailed() {
    throw std::logic_error("sce::Xml::Dom::Document: an allocation failure while building a document was not measured");
}

void Attach(XmlHandle* document, XmlMemAllocator* allocator, void* userData, std::shared_ptr<Generation> generation) {
    auto ref = std::make_unique<DocumentRef>(DocumentRef{std::move(generation), allocator, userData});
    void* impl = allocator->vtable->allocate(allocator, ImplSize, userData);
    if (impl == nullptr) AllocationFailed();
    std::array<void*, ScratchSizes.size()> scratch{};
    for (std::size_t k = 0; k < ScratchSizes.size(); ++k) {
        scratch[k] = allocator->vtable->allocate(allocator, ScratchSizes[k], userData);
        if (scratch[k] != nullptr) continue;
        for (std::size_t j = k; j-- > 0;) allocator->vtable->deallocate(allocator, scratch[j], userData);
        allocator->vtable->deallocate(allocator, impl, userData);
        AllocationFailed();
    }
    for (std::size_t k = ScratchSizes.size(); k-- > 0;) allocator->vtable->deallocate(allocator, scratch[k], userData);
    DocumentRef* owned = ref.release();
    std::memcpy(impl, &owned, sizeof(owned));
    document->impl = impl;
}

const Generation& Live(const XmlHandle* document) {
    const Generation& generation = *RefOf(document)->generation;
    if (!generation.alive) throw std::logic_error("sce::Xml::Dom::Document: reading a document after its builder parsed again or terminated reads freed memory in 13.42");
    return generation;
}

bool IsNullNodeId(XmlNodeId node) {
    return node.first == nullptr && node.second == nullptr && node.flag == 0;
}

void SetNode(XmlNodeId* result, const Generation* generation, std::uint64_t offset) {
    result->first = const_cast<Generation*>(generation);
    result->second = reinterpret_cast<void*>(offset);
    result->flag = 0;
}

void SetWritableNode(XmlNodeId* result, const Generation* generation, const DomNode* node) {
    if (node == nullptr) {
        result->first = nullptr;
        result->second = nullptr;
        result->flag = 0;
        return;
    }
    result->first = const_cast<Generation*>(generation);
    result->second = const_cast<DomNode*>(node);
    result->flag = 0;
}

XmlNodeId MakeNode(const Generation* generation, std::uint64_t offset) {
    XmlNodeId id{};
    SetNode(&id, generation, offset);
    return id;
}

XmlNodeId MakeWritableNode(const Generation* generation, const DomNode* node) {
    XmlNodeId id{};
    SetWritableNode(&id, generation, node);
    return id;
}

void SetInvalid(XmlNodeId* result) {
    result->first = nullptr;
    result->second = nullptr;
    result->flag = 0;
}

static_assert(std::is_trivially_copyable_v<XmlNodeId>);

void SetString(XmlString* result, const char* text) {
    result->buffer = text;
    result->size = text != nullptr ? std::strlen(text) : 0;
}

std::uint64_t OffsetOf(XmlNodeId node) {
    return reinterpret_cast<std::uint64_t>(node.second);
}

DomNode* ResolveWritable(const Generation& generation, XmlNodeId node) {
    if (!generation.writable || generation.tree == nullptr) return nullptr;
    if (node.first != &generation || node.flag != 0) return nullptr;
    return generation.tree->Resolve(node.second);
}

int SetAttributeImpl(XmlHandle* document, XmlNodeId node, const XmlString* name, const XmlString* value, bool create, const char* function) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, function);
    if (localDoc.impl == nullptr) return ErrorNotInitialized;
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        return ErrorReadOnly;
    }
    DomNode* target = ResolveWritable(generation, node);
    if (target == nullptr) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    if (!generation.tree->IsElement(target)) {
        generation.status = ErrorInvalidNodeType;
        return ErrorInvalidNodeType;
    }
    const GuestStringRead nameRead = ReadGuestString(name, function);
    if (nameRead.state != GuestStringState::Value) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    if (value == nullptr) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    const GuestStringRead valueRead = ReadGuestString(value, function);
    const int status = generation.tree->SetAttribute(target, nameRead.value, valueRead.value, create);
    generation.status = status;
    return status;
}

const std::vector<Attribute>* ReadOptionalAttributeList(const XmlHandle* attrs, const char* function) {
    if (attrs == nullptr) return nullptr;
    XmlHandle local{};
    if (!CopyInGuest(local, attrs)) {
        throw std::logic_error(std::string(function) + ": reading an unreadable AttributeList");
    }
    return ContainerAttributes(&local);
}

}

int DocumentInitialize(XmlHandle* document, const XmlHandle* initializer) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::initialize: null object");
    XmlHandle localDoc{};
    if (!CopyInGuest(localDoc, document)) throw std::logic_error("sce::Xml::Dom::Document::initialize: unreadable document");
    if (localDoc.impl != nullptr) return DocumentAlreadyInitialized;
    if (initializer == nullptr) return ErrorNotInitialized;
    XmlHandle localInit{};
    if (!CopyInGuest(localInit, initializer)) throw std::logic_error("sce::Xml::Dom::Document::initialize: unreadable initializer");
    if (localInit.impl == nullptr) return ErrorNotInitialized;
    const auto* parameter = static_cast<const XmlInitParameter*>(localInit.impl);
    if (parameter->allocator == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::initialize: null allocator");
    auto generation = std::make_shared<Generation>();
    generation->writable = true;
    generation->alive = true;
    generation->tree = std::make_unique<WritableTree>();
    auto ref = std::make_unique<DocumentRef>(DocumentRef{std::move(generation), parameter->allocator, parameter->userData});
    void* impl = parameter->allocator->vtable->allocate(parameter->allocator, ImplSize, parameter->userData);
    if (impl == nullptr) return ErrorNoMemory;
    DocumentRef* owned = ref.release();
    std::memcpy(impl, &owned, sizeof(owned));
    localDoc.impl = impl;
    if (!CopyOutGuest(document, localDoc)) {
        parameter->allocator->vtable->deallocate(parameter->allocator, impl, parameter->userData);
        delete owned;
        throw std::logic_error("sce::Xml::Dom::Document::initialize: unwritable document");
    }
    return 0;
}

void DocumentCreate(XmlHandle* document, XmlMemAllocator* allocator, void* userData, std::shared_ptr<Generation> generation) {
    document->impl = nullptr;
    Attach(document, allocator, userData, std::move(generation));
}

void DocumentCopy(XmlHandle* document, const XmlHandle* other) {
    if (document == nullptr || other == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    document->impl = nullptr;
    if (other->impl == nullptr) return;
    const DocumentRef* source = RefOf(other);
    Attach(document, source->allocator, source->userData, source->generation);
}

XmlHandle* DocumentAssign(XmlHandle* document, const XmlHandle* other) {
    if (document == nullptr || other == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document == other) return document;
    if (other->impl == nullptr) {
        if (document->impl != nullptr) throw std::logic_error("sce::Xml::Dom::Document: assigning an empty document over a document faults in 13.42");
        return document;
    }
    const DocumentRef* source = RefOf(other);
    if (document->impl == nullptr) {
        Attach(document, source->allocator, source->userData, source->generation);
    } else {
        RefOf(document)->generation = source->generation;
    }
    return document;
}

void DocumentDestroy(XmlHandle* document) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) return;
    DocumentRef* ref = RefOf(document);
    ref->allocator->vtable->deallocate(ref->allocator, document->impl, ref->userData);
    delete ref;
    document->impl = nullptr;
}

int DocumentTerminate(XmlHandle* document) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) return ErrorNotInitialized;
    DocumentDestroy(document);
    return 0;
}

bool DocumentIsAvailable(const XmlHandle* document) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    return document->impl != nullptr;
}

int DocumentGetStatus(const XmlHandle* document) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) return ErrorNotInitialized;
    return Live(document).status;
}

void DocumentResetStatus(const XmlHandle* document) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) return;
    Live(document);
    RefOf(document)->generation->status = 0;
}

bool DocumentIsReadOnly(const XmlHandle* document) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) return true;
    return !Live(document).writable;
}

XmlNodeId* DocumentSetWritable(XmlNodeId* result, XmlHandle* document) {
    constexpr const char* fn = "sce::Xml::Dom::Document::setWritable";
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::setWritable: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) {
        WriteGuestInvalid(result, fn);
        return result;
    }
    Live(&localDoc);
    DocumentRef* ref = RefOf(&localDoc);
    if (ref->generation->writable && ref->generation->tree != nullptr) {
        ref->generation->status = 0;
        WriteGuestWritableNode(result, ref->generation.get(), ref->generation->tree->DocRoot(), fn);
        return result;
    }
    auto writable = std::make_shared<Generation>();
    writable->writable = true;
    writable->alive = true;
    writable->status = 0;
    writable->tree = WritableTree::FromBuffers(ref->generation->buffers);
    WriteGuestWritableNode(result, writable.get(), writable->tree->DocRoot(), fn);
    ref->generation = std::move(writable);
    return result;
}

XmlNodeId* DocumentCreateElement(XmlNodeId* result, XmlHandle* document, const XmlString* name, const XmlHandle* attrs, const XmlString* text) {
    constexpr const char* fn = "sce::Xml::Dom::Document::createElement";
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::createElement: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) {
        WriteGuestInvalid(result, fn);
        return result;
    }
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        WriteGuestInvalid(result, fn);
        return result;
    }
    const GuestStringRead nameRead = ReadGuestString(name, fn);
    if (nameRead.state != GuestStringState::Value) {
        generation.status = ErrorInvalidParameter;
        WriteGuestInvalid(result, fn);
        return result;
    }
    const std::vector<Attribute>* attrList = ReadOptionalAttributeList(attrs, fn);
    const GuestStringRead textRead = ReadGuestString(text, fn);
    const std::string* textPtr = textRead.state == GuestStringState::Value ? &textRead.value : nullptr;
    int status = 0;
    DomNode* created = generation.tree->CreateElement(nameRead.value, attrList, textPtr, status);
    generation.status = status;
    WriteGuestWritableNode(result, &generation, created, fn);
    return result;
}

XmlNodeId* DocumentCreateTextNode(XmlNodeId* result, XmlHandle* document, const XmlString* text) {
    constexpr const char* fn = "sce::Xml::Dom::Document::createTextNode";
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::createTextNode: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) {
        WriteGuestInvalid(result, fn);
        return result;
    }
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        WriteGuestInvalid(result, fn);
        return result;
    }
    const GuestStringRead textRead = ReadGuestString(text, fn);
    if (textRead.state != GuestStringState::Value) {
        generation.status = ErrorInvalidParameter;
        WriteGuestInvalid(result, fn);
        return result;
    }
    DomNode* created = generation.tree->CreateText(textRead.value);
    generation.status = 0;
    WriteGuestWritableNode(result, &generation, created, fn);
    return result;
}

XmlNodeId* DocumentAddElementChild(XmlNodeId* result, XmlHandle* document, XmlNodeId parent, const XmlString* name, const XmlHandle* attrs, const XmlString* text) {
    constexpr const char* fn = "sce::Xml::Dom::Document::addElementChild";
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::addElementChild: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) {
        WriteGuestInvalid(result, fn);
        return result;
    }
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        WriteGuestInvalid(result, fn);
        return result;
    }
    DomNode* parentNode = ResolveWritable(generation, parent);
    if (parentNode == nullptr) {
        generation.status = ErrorInvalidParameter;
        WriteGuestInvalid(result, fn);
        return result;
    }
    if (!generation.tree->IsContainer(parentNode)) {
        generation.status = ErrorInvalidNodeType;
        WriteGuestInvalid(result, fn);
        return result;
    }
    if (parentNode == generation.tree->DocRoot() && generation.tree->Root() != nullptr) {
        generation.status = ErrorInvalidParameter;
        WriteGuestInvalid(result, fn);
        return result;
    }
    const GuestStringRead nameRead = ReadGuestString(name, fn);
    if (nameRead.state != GuestStringState::Value) {
        generation.status = ErrorInvalidParameter;
        WriteGuestInvalid(result, fn);
        return result;
    }
    const std::vector<Attribute>* attrList = ReadOptionalAttributeList(attrs, fn);
    const GuestStringRead textRead = ReadGuestString(text, fn);
    const std::string* textPtr = textRead.state == GuestStringState::Value ? &textRead.value : nullptr;
    int status = 0;
    DomNode* created = generation.tree->CreateElement(nameRead.value, attrList, textPtr, status);
    if (created == nullptr) {
        generation.status = status;
        WriteGuestInvalid(result, fn);
        return result;
    }
    if (!generation.tree->AppendChild(parentNode, created, status)) {
        generation.tree->DeleteSubtree(created);
        generation.status = status;
        WriteGuestInvalid(result, fn);
        return result;
    }
    generation.status = 0;
    WriteGuestWritableNode(result, &generation, created, fn);
    return result;
}

XmlNodeId* DocumentInsertNode(XmlNodeId* result, XmlHandle* document, XmlNodeId parent, XmlNodeId refChild, XmlNodeId child) {
    constexpr const char* fn = "sce::Xml::Dom::Document::insertNode";
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::insertNode: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) {
        WriteGuestInvalid(result, fn);
        return result;
    }
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        WriteGuestInvalid(result, fn);
        return result;
    }
    DomNode* parentNode = ResolveWritable(generation, parent);
    DomNode* childNode = ResolveWritable(generation, child);
    DomNode* refNode = nullptr;
    if (!IsNullNodeId(refChild)) {
        refNode = ResolveWritable(generation, refChild);
        if (refNode == nullptr) {
            generation.status = StatusNodeNotFound;
            WriteGuestInvalid(result, fn);
            return result;
        }
    }
    int status = 0;
    if (!generation.tree->InsertAfter(parentNode, refNode, childNode, status)) {
        generation.status = status;
        WriteGuestInvalid(result, fn);
        return result;
    }
    generation.status = 0;
    WriteGuestWritableNode(result, &generation, childNode, fn);
    return result;
}

XmlNodeId* DocumentRemoveChild(XmlNodeId* result, XmlHandle* document, XmlNodeId child, XmlNodeId parent) {
    constexpr const char* fn = "sce::Xml::Dom::Document::removeChild";
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::removeChild: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) {
        WriteGuestInvalid(result, fn);
        return result;
    }
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        WriteGuestInvalid(result, fn);
        return result;
    }
    DomNode* childNode = ResolveWritable(generation, child);
    DomNode* parentNode = ResolveWritable(generation, parent);
    int status = 0;
    if (!generation.tree->RemoveChild(childNode, parentNode, status)) {
        generation.status = status;
        WriteGuestInvalid(result, fn);
        return result;
    }
    generation.status = 0;
    WriteGuestWritableNode(result, &generation, childNode, fn);
    return result;
}

int DocumentRecurseDelete(XmlHandle* document, XmlNodeId node) {
    constexpr const char* fn = "sce::Xml::Dom::Document::recurseDelete";
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::recurseDelete: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) return ErrorNotInitialized;
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        return ErrorReadOnly;
    }
    if (IsNullNodeId(node)) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    DomNode* target = ResolveWritable(generation, node);
    if (target == nullptr) {
        generation.status = StatusNodeNotFound;
        return StatusNodeNotFound;
    }
    if (target != generation.tree->DocRoot() && target->parent == nullptr) {
        generation.tree->DeleteSubtree(target);
    }
    generation.status = 0;
    return 0;
}

int DocumentSetText(XmlHandle* document, XmlNodeId node, const XmlString* text) {
    constexpr const char* fn = "sce::Xml::Dom::Document::setText";
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::setText: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) return ErrorNotInitialized;
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        return ErrorReadOnly;
    }
    DomNode* target = ResolveWritable(generation, node);
    if (target == nullptr || text == nullptr) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    const GuestStringRead textRead = ReadGuestString(text, fn);
    const int status = generation.tree->SetText(target, textRead.value);
    generation.status = status;
    return status;
}

int DocumentSetAttribute(XmlHandle* document, XmlNodeId node, const XmlString* name, const XmlString* value) {
    return SetAttributeImpl(document, node, name, value, true, "sce::Xml::Dom::Document::setAttribute");
}

int DocumentSetAttrValue(XmlHandle* document, XmlNodeId node, const XmlString* name, const XmlString* value) {
    return SetAttributeImpl(document, node, name, value, false, "sce::Xml::Dom::Document::setAttrValue");
}

int DocumentSetAttributeList(XmlHandle* document, XmlNodeId node, const XmlHandle* attrs) {
    constexpr const char* fn = "sce::Xml::Dom::Document::setAttributeList";
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::setAttributeList: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) return ErrorNotInitialized;
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        return ErrorReadOnly;
    }
    DomNode* target = ResolveWritable(generation, node);
    if (target == nullptr) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    if (!generation.tree->IsElement(target)) {
        generation.status = ErrorInvalidNodeType;
        return ErrorInvalidNodeType;
    }
    const std::vector<Attribute>* attrList = ReadOptionalAttributeList(attrs, fn);
    if (attrList == nullptr) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    const int status = generation.tree->SetAttributeList(target, *attrList);
    generation.status = status;
    return status;
}

int DocumentRemoveAttribute(XmlHandle* document, XmlNodeId node, const XmlString* name) {
    constexpr const char* fn = "sce::Xml::Dom::Document::removeAttribute";
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::removeAttribute: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) return ErrorNotInitialized;
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        return ErrorReadOnly;
    }
    DomNode* target = ResolveWritable(generation, node);
    if (target == nullptr) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    if (!generation.tree->IsElement(target)) {
        generation.status = ErrorInvalidNodeType;
        return ErrorInvalidNodeType;
    }
    const GuestStringRead nameRead = ReadGuestString(name, fn);
    if (nameRead.state != GuestStringState::Value) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    const int status = generation.tree->RemoveAttribute(target, nameRead.value);
    generation.status = status;
    return status;
}

int DocumentRemoveAttributes(XmlHandle* document, XmlNodeId node) {
    constexpr const char* fn = "sce::Xml::Dom::Document::removeAttributes";
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document::removeAttributes: null object");
    const XmlHandle localDoc = ReadGuestHandle(document, fn);
    if (localDoc.impl == nullptr) return ErrorNotInitialized;
    Live(&localDoc);
    Generation& generation = *RefOf(&localDoc)->generation;
    if (!generation.writable || generation.tree == nullptr) {
        generation.status = ErrorReadOnly;
        return ErrorReadOnly;
    }
    DomNode* target = ResolveWritable(generation, node);
    if (target == nullptr) {
        generation.status = ErrorInvalidParameter;
        return ErrorInvalidParameter;
    }
    const int status = generation.tree->RemoveAttributes(target);
    generation.status = status;
    return status;
}

XmlNodeId* DocumentNode(XmlNodeId* result, const XmlHandle* document, NodeQuery query, XmlNodeId node) {
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) {
        SetInvalid(result);
        return result;
    }
    Generation& generation = *RefOf(document)->generation;
    Live(document);
    if (generation.writable && generation.tree != nullptr) {
        const WritableTree& tree = *generation.tree;
        DomNode* resolved = ResolveWritable(generation, node);
        DomNode* found = nullptr;
        switch (query) {
        case NodeQuery::Root:
            found = tree.Root();
            break;
        case NodeQuery::DocRoot:
            found = tree.DocRoot();
            break;
        case NodeQuery::FirstChild:
            found = tree.FirstChild(resolved);
            break;
        case NodeQuery::Sibling:
            found = tree.NextSibling(resolved);
            break;
        case NodeQuery::Parent:
            if (IsNullNodeId(node)) {
                generation.status = StatusInvalidNode;
                SetInvalid(result);
                return result;
            }
            found = tree.Parent(resolved, generation.status);
            break;
        case NodeQuery::LastChild:
            found = tree.LastChild(resolved);
            break;
        case NodeQuery::FirstAttribute:
            found = tree.FirstAttribute(resolved);
            break;
        case NodeQuery::NextAttribute:
            found = tree.NextAttribute(resolved);
            break;
        }
        SetWritableNode(result, &generation, found);
        return result;
    }
    const Reader reader(generation.buffers);
    const std::uint64_t offset = OffsetOf(node);
    std::uint64_t found = 0;
    switch (query) {
    case NodeQuery::Root:
        found = reader.Root();
        break;
    case NodeQuery::DocRoot:
        found = generation.buffers.nodes.empty() ? 0 : 1;
        break;
    case NodeQuery::FirstChild:
        found = reader.FirstChild(offset);
        break;
    case NodeQuery::Sibling:
        found = reader.Sibling(offset);
        break;
    case NodeQuery::Parent:
        found = reader.Parent(offset, generation.status);
        break;
    case NodeQuery::LastChild:
        SetNode(result, &generation, reader.LastChild(offset));
        return result;
    case NodeQuery::FirstAttribute:
        found = reader.FirstAttribute(offset);
        break;
    case NodeQuery::NextAttribute:
        found = reader.NextAttribute(offset);
        break;
    }
    if (found == 0) {
        SetInvalid(result);
    } else {
        SetNode(result, &generation, found);
    }
    return result;
}

XmlString* DocumentString(XmlString* result, const XmlHandle* document, StringQuery query, XmlNodeId node) {
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) {
        SetString(result, nullptr);
        return result;
    }
    Live(document);
    Generation& generation = *RefOf(document)->generation;
    const char* text = nullptr;
    if (generation.writable && generation.tree != nullptr) {
        const WritableTree& tree = *generation.tree;
        const DomNode* resolved = ResolveWritable(generation, node);
        switch (query) {
        case StringQuery::NodeName:
            text = tree.NodeName(resolved);
            break;
        case StringQuery::Text:
            text = tree.Text(resolved);
            break;
        case StringQuery::SkippedText:
            text = tree.SkippedText(resolved);
            break;
        case StringQuery::AttributeName:
            text = tree.AttributeName(resolved);
            break;
        case StringQuery::AttributeValue:
            text = tree.AttributeValue(resolved);
            break;
        case StringQuery::Entity:
            text = tree.Entity(resolved);
            break;
        }
        SetString(result, text);
        return result;
    }
    const Reader reader(generation.buffers);
    const std::uint64_t offset = OffsetOf(node);
    switch (query) {
    case StringQuery::NodeName:
        text = reader.NodeName(offset);
        break;
    case StringQuery::Text:
        text = reader.Text(offset);
        break;
    case StringQuery::SkippedText:
        text = reader.SkippedText(offset);
        break;
    case StringQuery::AttributeName:
        text = reader.AttributeName(offset);
        break;
    case StringQuery::AttributeValue:
        text = reader.AttributeValue(offset);
        break;
    case StringQuery::Entity:
        text = reader.Entity(offset);
        break;
    }
    if (text != nullptr) generation.lent = true;
    SetString(result, text);
    return result;
}

XmlString* DocumentAttribute(XmlString* result, const XmlHandle* document, XmlNodeId node, const XmlString* name) {
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr || name == nullptr || name->buffer == nullptr || name->size == 0) {
        SetString(result, nullptr);
        return result;
    }
    Live(document);
    Generation& generation = *RefOf(document)->generation;
    std::string read;
    const auto nameByte = [&](std::size_t index) {
        while (read.size() <= index) {
            char byte = 0;
            if (!GuestCopyIn_nid_no_patch(&byte, name->buffer + read.size(), 1)) throw std::logic_error("sce::Xml::Dom::Document: getAttribute reading the name where it is not readable faults in 13.42");
            read.push_back(byte);
        }
        return read[index];
    };
    const auto matches = [&](const char* attributeName) {
        for (std::size_t i = 0;; ++i) {
            const char byte = nameByte(i);
            if (attributeName[i] != byte) return false;
            if (byte == 0) return true;
        }
    };
    const char* text = nullptr;
    if (generation.writable && generation.tree != nullptr) {
        const DomNode* resolved = ResolveWritable(generation, node);
        text = generation.tree->FindAttribute(resolved, matches);
    } else {
        const Reader reader(generation.buffers);
        text = reader.Attribute(OffsetOf(node), matches);
        if (text != nullptr) generation.lent = true;
    }
    SetString(result, text);
    return result;
}

std::uint32_t DocumentNumber(const XmlHandle* document, NumberQuery query, XmlNodeId node) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) return query == NumberQuery::NodeType ? InvalidNodeTypeValue : 0;
    const Generation& generation = Live(document);
    if (generation.writable && generation.tree != nullptr) {
        const DomNode* resolved = ResolveWritable(generation, node);
        switch (query) {
        case NumberQuery::NodeType:
            return resolved != nullptr ? resolved->type : InvalidNodeTypeValue;
        case NumberQuery::EntityType:
            return resolved != nullptr && (resolved->type & 0x0fu) == NodeTypeEntity ? resolved->entityType : InvalidEntityTypeValue;
        case NumberQuery::HasChildNodes:
            return generation.tree->FirstChild(resolved) != nullptr ? 1 : 0;
        case NumberQuery::HasAttributes:
            return generation.tree->FirstAttribute(resolved) != nullptr ? 1 : 0;
        }
        return 0;
    }
    const Reader reader(generation.buffers);
    const std::uint64_t offset = OffsetOf(node);
    switch (query) {
    case NumberQuery::NodeType:
        return reader.NodeType(offset);
    case NumberQuery::EntityType:
        return reader.EntityType(offset);
    case NumberQuery::HasChildNodes:
        return reader.HasChildNodes(offset) ? 1 : 0;
    case NumberQuery::HasAttributes:
        return reader.HasAttributes(offset) ? 1 : 0;
    }
    return 0;
}

int DocumentChildNodes(const XmlHandle* document, XmlNodeId node, XmlHandle* list) {
    if (document == nullptr || list == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr || list->impl == nullptr) return ListNotInitialized;
    Live(document);
    Generation& generation = *RefOf(document)->generation;
    std::vector<XmlNodeId> items;
    if (generation.writable && generation.tree != nullptr) {
        const DomNode* resolved = ResolveWritable(generation, node);
        for (DomNode* child = generation.tree->FirstChild(resolved); child != nullptr; child = generation.tree->NextSibling(child)) {
            items.push_back(MakeWritableNode(&generation, child));
        }
        return NodeListReplace(list, items);
    }
    const Reader reader(generation.buffers);
    for (std::uint64_t child = reader.FirstChild(OffsetOf(node)); child != 0; child = reader.Sibling(child)) {
        items.push_back(MakeNode(&generation, child));
    }
    return NodeListReplace(list, items);
}

int DocumentAttributes(const XmlHandle* document, XmlNodeId node, XmlHandle* list) {
    if (document == nullptr || list == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr || list->impl == nullptr) return ListNotInitialized;
    Live(document);
    Generation& generation = *RefOf(document)->generation;
    std::vector<XmlNodeId> items;
    if (generation.writable && generation.tree != nullptr) {
        const DomNode* resolved = ResolveWritable(generation, node);
        for (DomNode* attr = generation.tree->FirstAttribute(resolved); attr != nullptr; attr = generation.tree->NextAttribute(attr)) {
            items.push_back(MakeWritableNode(&generation, attr));
        }
        return NodeListReplace(list, items);
    }
    const Reader reader(generation.buffers);
    for (std::uint64_t attribute = reader.FirstAttribute(OffsetOf(node)); attribute != 0; attribute = reader.NextAttribute(attribute)) {
        items.push_back(MakeNode(&generation, attribute));
    }
    return NodeListReplace(list, items);
}

int DocumentElementsByTagName(const XmlHandle* document, XmlNodeId node, const XmlString* name, XmlHandle* list) {
    if (document == nullptr || list == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr || list->impl == nullptr) return ListNotInitialized;
    if (name == nullptr || name->buffer == nullptr || name->size == 0) return NodeListReplace(list, {});
    Live(document);
    Generation& generation = *RefOf(document)->generation;
    constexpr std::size_t Chunk = 64 * 1024;
    std::string wanted;
    for (std::size_t done = 0; done < name->size;) {
        const std::size_t step = std::min(Chunk, name->size - done);
        wanted.resize(done + step);
        if (!GuestCopyIn_nid_no_patch(wanted.data() + done, name->buffer + done, step)) throw std::logic_error("sce::Xml::Dom::Document: getElementsByTagName reading a name where it is not readable faults in 13.42");
        done += step;
    }
    std::vector<XmlNodeId> items;
    if (generation.writable && generation.tree != nullptr) {
        const WritableTree& tree = *generation.tree;
        const DomNode* start = ResolveWritable(generation, node);
        if (start == nullptr) return NodeListReplace(list, items);
        std::vector<const DomNode*> pending;
        const auto pushChildren = [&](const DomNode* parent) {
            for (std::size_t index = parent->children.size(); index-- > 0;) pending.push_back(parent->children[index]);
        };
        pushChildren(start);
        while (!pending.empty()) {
            const DomNode* current = pending.back();
            pending.pop_back();
            if (tree.IsElement(current) && current->name == wanted) items.push_back(MakeWritableNode(&generation, current));
            pushChildren(current);
        }
        return NodeListReplace(list, items);
    }
    const Reader reader(generation.buffers);
    std::vector<std::uint64_t> pending;
    const auto pushChildren = [&](std::uint64_t parent) {
        std::vector<std::uint64_t> children;
        for (std::uint64_t child = reader.FirstChild(parent); child != 0; child = reader.Sibling(child)) children.push_back(child);
        for (std::size_t index = children.size(); index-- > 0;) pending.push_back(children[index]);
    };
    pushChildren(OffsetOf(node));
    while (!pending.empty()) {
        const std::uint64_t current = pending.back();
        pending.pop_back();
        const char* nodeName = reader.NodeName(current);
        if (nodeName != nullptr && wanted == nodeName) items.push_back(MakeNode(&generation, current));
        pushChildren(current);
    }
    return NodeListReplace(list, items);
}

struct WritableSerializerView {
    const WritableTree& tree;

    const DomNode* Root() const { return tree.Root(); }
    const DomNode* FirstChild(const DomNode* node) const { return tree.FirstChild(node); }
    const DomNode* Sibling(const DomNode* node) const { return tree.NextSibling(node); }
    const DomNode* FirstAttribute(const DomNode* node) const { return tree.FirstAttribute(node); }
    const DomNode* NextAttribute(const DomNode* attribute) const { return tree.NextAttribute(attribute); }
    std::uint32_t NodeType(const DomNode* node) const { return node != nullptr ? node->type : InvalidNodeTypeValue; }
    std::uint32_t EntityType(const DomNode* node) const { return node != nullptr && (node->type & 0x0fu) == NodeTypeEntity ? node->entityType : InvalidEntityTypeValue; }
    const char* NodeName(const DomNode* node) const { return tree.NodeName(node); }
    const char* Text(const DomNode* node) const { return tree.Text(node); }
    const char* SkippedText(const DomNode* node) const { return tree.SkippedText(node); }
    const char* AttributeName(const DomNode* attribute) const { return tree.AttributeName(attribute); }
    const char* AttributeValue(const DomNode* attribute) const { return tree.AttributeValue(attribute); }
    const char* Entity(const DomNode* node) const { return tree.Entity(node); }
};

std::uint32_t GenerationNodeType(const Generation& generation, XmlNodeId id) {
    if (generation.writable && generation.tree != nullptr) {
        const DomNode* resolved = ResolveWritable(generation, id);
        return resolved != nullptr ? resolved->type : InvalidNodeTypeValue;
    }
    return Reader(generation.buffers).NodeType(OffsetOf(id));
}

bool GenerationHasChildNodes(const Generation& generation, XmlNodeId id) {
    if (generation.writable && generation.tree != nullptr) {
        const DomNode* resolved = ResolveWritable(generation, id);
        return generation.tree->FirstChild(resolved) != nullptr;
    }
    return Reader(generation.buffers).HasChildNodes(OffsetOf(id));
}

bool GenerationHasAttributes(const Generation& generation, XmlNodeId id) {
    if (generation.writable && generation.tree != nullptr) {
        const DomNode* resolved = ResolveWritable(generation, id);
        return generation.tree->FirstAttribute(resolved) != nullptr;
    }
    return Reader(generation.buffers).HasAttributes(OffsetOf(id));
}

const char* GenerationNodeName(const Generation& generation, XmlNodeId id) {
    if (generation.writable && generation.tree != nullptr) {
        const DomNode* resolved = ResolveWritable(generation, id);
        return generation.tree->NodeName(resolved);
    }
    return Reader(generation.buffers).NodeName(OffsetOf(id));
}

const std::string& DocumentSerialize(const XmlHandle* document, bool declaration) {
    if (document == nullptr || document->impl == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    const Generation& generation = Live(document);
    DocumentRef* ref = RefOf(document);
    if (generation.writable && generation.tree != nullptr) {
        const WritableSerializerView view{*generation.tree};
        ref->serialized = Serializer<WritableSerializerView>(view).Run(declaration);
    } else {
        const Reader reader(generation.buffers);
        ref->serialized = Serializer<Reader>(reader).Run(declaration);
    }
    return ref->serialized;
}

XmlNodeId NodeListFindByName(const XmlHandle* list, const std::string& name) {
    const std::uint32_t length = NodeListLength(list);
    for (std::uint32_t index = 0; index < length; ++index) {
        const XmlNodeId item = NodeListItem(list, index);
        if (item.first == nullptr) continue;
        const auto* generation = static_cast<const Generation*>(item.first);
        if (!generation->alive) throw std::logic_error("sce::Xml::Dom::NodeList::findItem: reading a node after its builder parsed again or terminated reads freed memory in 13.42");
        const char* itemName = GenerationNodeName(*generation, item);
        if (itemName != nullptr && name == itemName) return item;
    }
    return XmlNodeId{nullptr, nullptr, 0};
}

}
