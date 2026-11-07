#pragma once

#include "prx/libSceXml/include/Api.hpp"
#include "prx/libSceXml/src/DomParser.hpp"

#include <memory>
#include <vector>

namespace XmlDom {

struct Generation {
    Buffers buffers;
    std::vector<Buffers> lentBuffers;
    bool lent = false;
    int status = 0;
    bool alive = true;
};

void DocumentCreate(XmlHandle* document, XmlMemAllocator* allocator, void* userData, std::shared_ptr<Generation> generation);
void DocumentCopy(XmlHandle* document, const XmlHandle* other);
XmlHandle* DocumentAssign(XmlHandle* document, const XmlHandle* other);
void DocumentDestroy(XmlHandle* document);
int DocumentTerminate(XmlHandle* document);
bool DocumentIsAvailable(const XmlHandle* document);
int DocumentGetStatus(const XmlHandle* document);
bool DocumentIsReadOnly(const XmlHandle* document);

enum class NodeQuery { Root, DocRoot, FirstChild, Sibling, Parent, LastChild, FirstAttribute, NextAttribute };
enum class StringQuery { NodeName, Text, SkippedText, AttributeName, AttributeValue, Entity };
enum class NumberQuery { NodeType, EntityType, HasChildNodes, HasAttributes };

XmlNodeId* DocumentNode(XmlNodeId* result, const XmlHandle* document, NodeQuery query, XmlNodeId node);
XmlString* DocumentString(XmlString* result, const XmlHandle* document, StringQuery query, XmlNodeId node);
XmlString* DocumentAttribute(XmlString* result, const XmlHandle* document, XmlNodeId node, const XmlString* name);
std::uint32_t DocumentNumber(const XmlHandle* document, NumberQuery query, XmlNodeId node);

}
