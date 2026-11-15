#pragma once

#include "SceTypes.hpp"
#include "prx/libSceXml/include/Api.hpp"

extern "C" {

int APS5_VABI _ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE();
int APS5_VABI _ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE();
XmlString* APS5_VABI _ZNK3sce3Xml4Attr7getNameEv(XmlString* result, const XmlHandle* self);
int APS5_VABI _ZNK3sce3Xml3Dom8NodeList8findItemENS1_6NodeIdE();
int APS5_VABI _ZNK3sce3Xml3Dom8Document13getAttributesENS1_6NodeIdEPNS1_8NodeListE();
int APS5_VABI _ZNK3sce3Xml3Dom8NodeList4itemEj();
XmlString* APS5_VABI _ZNK3sce3Xml4Attr8getValueEv(XmlString* result, const XmlHandle* self);
int APS5_VABI _ZNK3sce3Xml3Dom8NodeListixEj();
int APS5_VABI _ZNK3sce3Xml3Dom8NodeList8findItemEPKNS0_6StringE();
int APS5_VABI _ZNK3sce3Xml3Dom8NodeList11isAvailableEv();
int APS5_VABI _ZNK3sce3Xml3Dom8Document10getXmlMetaEv();
int APS5_VABI _ZNK3sce3Xml3Dom8NodeList9getLengthEv();
bool APS5_VABI _ZNK3sce3Xml4Attr11isAvailableEv(const XmlHandle* self);

}
