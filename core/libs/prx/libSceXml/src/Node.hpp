#pragma once

#include "prx/libSceXml/include/Api.hpp"
#include "prx/libSceXml/src/Document.hpp"

#include <cstdint>

namespace XmlDom {

void NodeCreate(XmlNode* self, const XmlNodeId& id);
void NodeDestroy(XmlNode* self);
std::uint32_t NodeTypeOf(const XmlNode* self);
bool NodeHasChildNodes(const XmlNode* self);
bool NodeHasAttributes(const XmlNode* self);
XmlNodeId* NodeRelative(XmlNodeId* result, const XmlNode* self, NodeQuery query);
XmlString* NodeName(XmlString* result, const XmlNode* self);
XmlString* NodeValue(XmlString* result, const XmlNode* self);

}
