#include "prx/libSceXml/src/Node.hpp"

#include "prx/libSceXml/src/Document.hpp"
#include "prx/libSceXml/src/DomReader.hpp"

#include <stdexcept>

namespace XmlDom {

namespace {

constexpr std::uint32_t InvalidNodeType = 0xffffffff;

const Generation* GenerationOf(const XmlNode* self) {
    if (self->id.first == nullptr) return nullptr;
    const Generation* generation = static_cast<const Generation*>(self->id.first);
    if (!generation->alive) throw std::logic_error("sce::Xml::Dom::Node: reading a node after its builder parsed again or terminated reads freed memory in 13.42");
    return generation;
}

std::uint64_t OffsetOf(const XmlNode* self) {
    return reinterpret_cast<std::uint64_t>(self->id.second);
}

}

void NodeCreate(XmlNode* self, const XmlNodeId& id) {
    self->id.first = id.first;
    self->id.second = id.second;
    self->id.flag = id.flag;
    self->document = id.first;
    self->node = id.second;
    for (int i = 0; i < 3; ++i) self->state[i] = 0;
}

void NodeDestroy(XmlNode*) {
}

std::uint32_t NodeTypeOf(const XmlNode* self) {
    const Generation* generation = GenerationOf(self);
    if (generation == nullptr) return InvalidNodeType;
    return Reader(generation->buffers).NodeType(OffsetOf(self));
}

bool NodeHasChildNodes(const XmlNode* self) {
    const Generation* generation = GenerationOf(self);
    if (generation == nullptr) return false;
    return Reader(generation->buffers).HasChildNodes(OffsetOf(self));
}

bool NodeHasAttributes(const XmlNode* self) {
    const Generation* generation = GenerationOf(self);
    if (generation == nullptr) return false;
    return Reader(generation->buffers).HasAttributes(OffsetOf(self));
}

XmlNodeId* NodeRelative(XmlNodeId* result, const XmlNode* self, NodeQuery query) {
    if (self == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Node: null object");
    if (self->document == nullptr) {
        result->first = nullptr;
        result->second = nullptr;
        result->flag = 0;
        return result;
    }
    Generation* generation = static_cast<Generation*>(self->document);
    if (!generation->alive) throw std::logic_error("sce::Xml::Dom::Node: reading a node after its builder parsed again or terminated reads freed memory in 13.42");
    return GenerationNode(result, *generation, query, reinterpret_cast<std::uint64_t>(self->node));
}

namespace {

constexpr std::uint32_t DeclarationNode = 0;
constexpr std::uint32_t AttributeNode = 1;
constexpr std::uint32_t ElementNode = 4;
constexpr char DeclarationName[] = "xml";
constexpr char DeclarationValue[] = "version=\"1.0\"";

Generation* LiveGeneration(XmlString* result, const XmlNode* self) {
    if (self == nullptr || result == nullptr) throw std::invalid_argument("sce::Xml::Dom::Node: null object");
    if (self->document == nullptr) return nullptr;
    Generation* generation = static_cast<Generation*>(self->document);
    if (!generation->alive) throw std::logic_error("sce::Xml::Dom::Node: reading a node after its builder parsed again or terminated reads freed memory in 13.42");
    return generation;
}

XmlString* SetConstant(XmlString* result, const char* text, std::size_t size) {
    result->buffer = text;
    result->size = size;
    return result;
}

}

XmlString* NodeName(XmlString* result, const XmlNode* self) {
    Generation* generation = LiveGeneration(result, self);
    if (generation == nullptr) return SetConstant(result, nullptr, 0);
    const std::uint64_t offset = reinterpret_cast<std::uint64_t>(self->node);
    switch (Reader(generation->buffers).NodeType(offset)) {
    case DeclarationNode: return SetConstant(result, DeclarationName, sizeof(DeclarationName) - 1);
    case ElementNode: return GenerationString(result, *generation, StringQuery::NodeName, offset);
    case AttributeNode: return GenerationString(result, *generation, StringQuery::AttributeName, offset);
    default: return SetConstant(result, nullptr, 0);
    }
}

XmlString* NodeValue(XmlString* result, const XmlNode* self) {
    Generation* generation = LiveGeneration(result, self);
    if (generation == nullptr) return SetConstant(result, nullptr, 0);
    const std::uint64_t offset = reinterpret_cast<std::uint64_t>(self->node);
    switch (Reader(generation->buffers).NodeType(offset)) {
    case DeclarationNode: return SetConstant(result, DeclarationValue, sizeof(DeclarationValue) - 1);
    case AttributeNode: return GenerationString(result, *generation, StringQuery::AttributeValue, offset);
    default: return GenerationString(result, *generation, StringQuery::Text, offset);
    }
}

}
