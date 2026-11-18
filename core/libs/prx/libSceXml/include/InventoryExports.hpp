#pragma once

#include "SceTypes.hpp"
#include "prx/libSceXml/include/Api.hpp"

extern "C" {

int APS5_VABI _ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE(const XmlHandle* self, XmlNodeId node, XmlHandle* list);
int APS5_VABI _ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE(const XmlHandle* self, XmlNodeId node, const XmlString* name, XmlHandle* list);
XmlString* APS5_VABI _ZNK3sce3Xml4Attr7getNameEv(XmlString* result, const XmlHandle* self);
int APS5_VABI _ZNK3sce3Xml3Dom8NodeList8findItemENS1_6NodeIdE();
int APS5_VABI _ZNK3sce3Xml3Dom8Document13getAttributesENS1_6NodeIdEPNS1_8NodeListE(const XmlHandle* self, XmlNodeId node, XmlHandle* list);
XmlNodeId APS5_VABI _ZNK3sce3Xml3Dom8NodeList4itemEj(const XmlHandle* self, std::uint32_t index);
XmlString* APS5_VABI _ZNK3sce3Xml4Attr8getValueEv(XmlString* result, const XmlHandle* self);
XmlNodeId APS5_VABI _ZNK3sce3Xml3Dom8NodeListixEj(const XmlHandle* self, std::uint32_t index);
int APS5_VABI _ZNK3sce3Xml3Dom8NodeList8findItemEPKNS0_6StringE();
bool APS5_VABI _ZNK3sce3Xml3Dom8NodeList11isAvailableEv(const XmlHandle* self);
int APS5_VABI _ZNK3sce3Xml3Dom8Document10getXmlMetaEv();
std::uint32_t APS5_VABI _ZNK3sce3Xml3Dom8NodeList9getLengthEv(const XmlHandle* self);
bool APS5_VABI _ZNK3sce3Xml4Attr11isAvailableEv(const XmlHandle* self);

}
