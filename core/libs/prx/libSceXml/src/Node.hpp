#pragma once

#include "prx/libSceXml/include/Api.hpp"

#include <cstdint>

namespace XmlDom {

void NodeCreate(XmlNode* self, const XmlNodeId& id);
void NodeDestroy(XmlNode* self);
std::uint32_t NodeTypeOf(const XmlNode* self);
bool NodeHasChildNodes(const XmlNode* self);
bool NodeHasAttributes(const XmlNode* self);

}
