#include "prx/libSceXml/src/Document.hpp"

#include "prx/libSceXml/src/DomReader.hpp"

#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace XmlDom {

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x80850002);
constexpr std::size_t ImplSize = 72;
constexpr std::array<std::size_t, 7> ScratchSizes{1240, 96, 2088, 5304, 2088, 1752, 1752};

struct DocumentRef {
    std::shared_ptr<Generation> generation;
    XmlMemAllocator* allocator = nullptr;
    void* userData = nullptr;
};

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

void SetNode(XmlNodeId* result, const Generation* generation, std::uint64_t offset) {
    result->first = const_cast<Generation*>(generation);
    result->second = reinterpret_cast<void*>(offset);
    result->flag = 0;
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

bool DocumentIsReadOnly(const XmlHandle* document) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) return true;
    Live(document);
    return true;
}

XmlNodeId* DocumentNode(XmlNodeId* result, const XmlHandle* document, NodeQuery query, XmlNodeId node) {
    if (document == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) {
        SetInvalid(result);
        return result;
    }
    Generation& generation = *RefOf(document)->generation;
    Live(document);
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
    const Reader reader(generation.buffers);
    const std::uint64_t offset = OffsetOf(node);
    const char* text = nullptr;
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
    const Reader reader(generation.buffers);
    const char* text = reader.Attribute(OffsetOf(node), name->buffer);
    if (text != nullptr) generation.lent = true;
    SetString(result, text);
    return result;
}

std::uint32_t DocumentNumber(const XmlHandle* document, NumberQuery query, XmlNodeId node) {
    if (document == nullptr) throw std::invalid_argument("sce::Xml::Dom::Document: null object");
    if (document->impl == nullptr) return query == NumberQuery::NodeType ? 0xffffffff : 0;
    const Reader reader(Live(document).buffers);
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

}
