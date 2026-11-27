#pragma once

#include "prx/libSceXml/include/Api.hpp"
#include "prx/libSceXml/src/DomParser.hpp"
#include "prx/libSceXml/src/WritableTree.hpp"

#include <memory>
#include <string>
#include <vector>

namespace XmlDom {

struct Generation {
    Buffers buffers;
    std::vector<Buffers> lentBuffers;
    bool lent = false;
    int status = 0;
    bool alive = true;
    bool writable = false;
    std::unique_ptr<WritableTree> tree;
    Generation* retiredNext = nullptr;
};

std::shared_ptr<Generation> NewGeneration();
int DocumentInitialize(XmlHandle* document, const XmlHandle* initializer);
void DocumentCreate(XmlHandle* document, XmlMemAllocator* allocator, void* userData, std::shared_ptr<Generation> generation);
void DocumentCopy(XmlHandle* document, const XmlHandle* other);
XmlHandle* DocumentAssign(XmlHandle* document, const XmlHandle* other);
void DocumentDestroy(XmlHandle* document);
int DocumentTerminate(XmlHandle* document);
bool DocumentIsAvailable(const XmlHandle* document);
int DocumentGetStatus(const XmlHandle* document);
void DocumentResetStatus(const XmlHandle* document);
bool DocumentIsReadOnly(const XmlHandle* document);

XmlNodeId* DocumentSetWritable(XmlNodeId* result, XmlHandle* document);
XmlNodeId* DocumentCreateElement(XmlNodeId* result, XmlHandle* document, const XmlString* name, const XmlHandle* attrs, const XmlString* text);
XmlNodeId* DocumentCreateTextNode(XmlNodeId* result, XmlHandle* document, const XmlString* text);
XmlNodeId* DocumentAddElementChild(XmlNodeId* result, XmlHandle* document, XmlNodeId parent, const XmlString* name, const XmlHandle* attrs, const XmlString* text);
XmlNodeId* DocumentInsertNode(XmlNodeId* result, XmlHandle* document, XmlNodeId parent, XmlNodeId refChild, XmlNodeId child);
XmlNodeId* DocumentRemoveChild(XmlNodeId* result, XmlHandle* document, XmlNodeId child, XmlNodeId parent);
int DocumentRecurseDelete(XmlHandle* document, XmlNodeId node);
int DocumentSetText(XmlHandle* document, XmlNodeId node, const XmlString* text);
int DocumentSetAttribute(XmlHandle* document, XmlNodeId node, const XmlString* name, const XmlString* value);
int DocumentSetAttrValue(XmlHandle* document, XmlNodeId node, const XmlString* name, const XmlString* value);
int DocumentSetAttributeList(XmlHandle* document, XmlNodeId node, const XmlHandle* attrs);
int DocumentRemoveAttribute(XmlHandle* document, XmlNodeId node, const XmlString* name);
int DocumentRemoveAttributes(XmlHandle* document, XmlNodeId node);

enum class NodeQuery { Root, DocRoot, FirstChild, Sibling, Parent, LastChild, FirstAttribute, NextAttribute };
enum class StringQuery { NodeName, Text, SkippedText, AttributeName, AttributeValue, Entity };
enum class NumberQuery { NodeType, EntityType, HasChildNodes, HasAttributes };

XmlNodeId* DocumentNode(XmlNodeId* result, const XmlHandle* document, NodeQuery query, XmlNodeId node);
XmlString* DocumentString(XmlString* result, const XmlHandle* document, StringQuery query, XmlNodeId node);
XmlString* DocumentAttribute(XmlString* result, const XmlHandle* document, XmlNodeId node, const XmlString* name);
std::uint32_t DocumentNumber(const XmlHandle* document, NumberQuery query, XmlNodeId node);
int DocumentChildNodes(const XmlHandle* document, XmlNodeId node, XmlHandle* list);
int DocumentAttributes(const XmlHandle* document, XmlNodeId node, XmlHandle* list);
XmlNodeId NodeListFindByName(const XmlHandle* list, const std::string& name);
const std::string& DocumentSerialize(const XmlHandle* document, bool declaration);
int DocumentElementsByTagName(const XmlHandle* document, XmlNodeId node, const XmlString* name, XmlHandle* list);

std::uint32_t GenerationNodeType(const Generation& generation, XmlNodeId id);
bool GenerationHasChildNodes(const Generation& generation, XmlNodeId id);
bool GenerationHasAttributes(const Generation& generation, XmlNodeId id);
const char* GenerationNodeName(const Generation& generation, XmlNodeId id);

}
