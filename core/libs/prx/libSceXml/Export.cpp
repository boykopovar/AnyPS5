#include "prx/libSceXml/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>

namespace {

void AssignNodeId(XmlNodeId* self, const XmlNodeId& value) {
    self->first = value.first;
    self->second = value.second;
    self->flag = value.flag;
}

void CopyNode(XmlNode* self, const XmlNode* other) {
    AssignNodeId(&self->id, other->id);
    self->document = other->document;
    self->node = other->node;
    for (int i = 0; i < 3; ++i) self->state[i] = other->state[i];
}

}

extern "C" {

void APS5_VABI _ZN3sce3Xml10SimpleDataC1EPKcm(XmlSimpleData* self, const char* data, std::size_t size) {
    self->data = data;
    self->size = size;
}

void APS5_VABI _ZN3sce3Xml10SimpleDataC1Ev(XmlSimpleData* self) {
    self->data = nullptr;
    self->size = 0;
}

void APS5_VABI _ZN3sce3Xml10SimpleDataC2EPKcm(XmlSimpleData* self, const char* data, std::size_t size) {
    self->data = data;
    self->size = size;
}

void APS5_VABI _ZN3sce3Xml10SimpleDataC2Ev(XmlSimpleData* self) {
    self->data = nullptr;
    self->size = 0;
}

int APS5_VABI _ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml11Initializer9terminateEv() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml11InitializerC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml11InitializerC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml11InitializerD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml11InitializerD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml12MemAllocatorC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml12MemAllocatorD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml12MemAllocatorD1Ev(void*) {
}

void APS5_VABI _ZN3sce3Xml12MemAllocatorD2Ev(void*) {
}

int APS5_VABI _ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml13AttributeList5clearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml13AttributeList9terminateEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml13AttributeListC1ERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml13AttributeListC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml13AttributeListC2ERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml13AttributeListC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml13AttributeListD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml13AttributeListD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml18SerializeParameterC1Ev(XmlSerializeParameter* self) {
    self->first = 0;
    self->second = 0;
    self->third = 0;
}

void APS5_VABI _ZN3sce3Xml18SerializeParameterC2Ev(XmlSerializeParameter* self) {
    self->first = 0;
    self->second = 0;
    self->third = 0;
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder10initializeEPKNS0_11InitializerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder16setResolveEntityEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder20setSkipIgnorableTextEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder26setSkipIgnorableWhiteSpaceEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder9terminateEv() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom4Node11appendChildENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom4Node11removeChildENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom4Node12insertBeforeENS1_6NodeIdES3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml3Dom4NodeC1ERKS2_(XmlNode* self, const XmlNode* other) {
    CopyNode(self, other);
}

int APS5_VABI _ZN3sce3Xml3Dom4NodeC2ENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml3Dom4NodeC2ERKS2_(XmlNode* self, const XmlNode* other) {
    CopyNode(self, other);
}

int APS5_VABI _ZN3sce3Xml3Dom4NodeD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom4NodeD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

XmlNode* APS5_VABI _ZN3sce3Xml3Dom4NodeaSERKS2_(XmlNode* self, const XmlNode* other) {
    if (self != other) CopyNode(self, other);
    return self;
}

void APS5_VABI _ZN3sce3Xml3Dom6NodeIdC1EPvS3_b(XmlNodeId* self, void* first, void* second, std::uint8_t flag) {
    self->first = first;
    self->second = second;
    self->flag = flag;
}

void APS5_VABI _ZN3sce3Xml3Dom6NodeIdC1Ev(XmlNodeId* self) {
    self->first = nullptr;
    self->second = nullptr;
    self->flag = 0;
}

void APS5_VABI _ZN3sce3Xml3Dom6NodeIdC2EPvS3_b(XmlNodeId* self, void* first, void* second, std::uint8_t flag) {
    self->first = first;
    self->second = second;
    self->flag = flag;
}

void APS5_VABI _ZN3sce3Xml3Dom6NodeIdC2Ev(XmlNodeId* self) {
    self->first = nullptr;
    self->second = nullptr;
    self->flag = 0;
}

XmlNodeId APS5_VABI _ZN3sce3Xml3Dom6NodeIdaSES2_(XmlNodeId* self, XmlNodeId value) {
    AssignNodeId(self, value);
    XmlNodeId result;
    std::memcpy(&result, self, sizeof(result));
    return result;
}

void* APS5_VABI _ZN3sce3Xml3Dom6NodeIdcvPvEv(XmlNodeId*) {
    return nullptr;
}

bool APS5_VABI _ZN3sce3Xml3Dom6NodeIdeqES2_(const XmlNodeId* self, XmlNodeId other) {
    return other.first == self->first && other.second == self->second && other.flag == self->flag;
}

int APS5_VABI _ZN3sce3Xml3Dom6NodeIdeqEi() {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI _ZN3sce3Xml3Dom6NodeIdneES2_(const XmlNodeId* self, XmlNodeId other) {
    return other.first != self->first || other.second != self->second || other.flag != self->flag;
}

int APS5_VABI _ZN3sce3Xml3Dom6NodeIdneEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom6NodeIdntEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document10importNodeENS1_6NodeIdES3_PKS2_S3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document11removeChildENS1_6NodeIdES3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document11resetStatusEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document11setWritableEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document12importParentEPKS2_NS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document12setAttrValueENS1_6NodeIdEPKNS0_6StringES6_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document13createElementEPKNS0_6StringEPKNS0_13AttributeListES5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document13recurseDeleteENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document14createTextNodeEPKNS0_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document15removeAttributeENS1_6NodeIdEPKNS0_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document16removeAttributesENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document16setAttributeListENS1_6NodeIdEPKNS0_13AttributeListE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document7setTextENS1_6NodeIdEPKNS0_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document9serializeEPKNS0_18SerializeParameterEPNS0_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8Document9terminateEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8DocumentC1ERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml3Dom8DocumentC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml3Dom8DocumentC2ERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml3Dom8DocumentC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml3Dom8DocumentD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8DocumentD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8DocumentaSERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeList10initializeEPKNS0_11InitializerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeList10insertLastENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeList10removeItemENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeList11insertFirstENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeList5clearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeList9terminateEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeListC1ERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml3Dom8NodeListC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeListC2ERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml3Dom8NodeListC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeListD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Dom8NodeListD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6Parser11setUserDataEPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6Parser16setResolveEntityEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6Parser18setDocumentHandlerEPNS1_15DocumentHandlerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6Parser26setSkipIgnorableWhiteSpaceEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6Parser5parseEPKNS0_6StringEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6Parser5resetEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6Parser9terminateEv() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml3Sax6ParserC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml3Sax6ParserC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml3Sax6ParserD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml3Sax6ParserD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml4Attr7setNameEPKNS0_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml4Attr8setValueEPKNS0_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml4Attr9terminateEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml4AttrC1ERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml4AttrC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml4AttrC2ERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml4AttrC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

int APS5_VABI _ZN3sce3Xml4AttrD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml4AttrD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml4AttraSERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce3Xml4Util9strResultEi() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce3Xml6StringC1EPKc(XmlString* self, const char* buffer) {
    self->buffer = buffer;
    self->size = buffer != nullptr ? std::strlen(buffer) : 0;
}

void APS5_VABI _ZN3sce3Xml6StringC1EPKcm(XmlString* self, const char* buffer, std::size_t size) {
    self->buffer = buffer;
    self->size = size;
}

void APS5_VABI _ZN3sce3Xml6StringC1ERKS1_(XmlString* self, const XmlString* other) {
    self->buffer = other->buffer;
    self->size = other->size;
}

void APS5_VABI _ZN3sce3Xml6StringC1Ev(XmlString* self) {
    self->buffer = nullptr;
    self->size = 0;
}

void APS5_VABI _ZN3sce3Xml6StringC2EPKc(XmlString* self, const char* buffer) {
    self->buffer = buffer;
    self->size = buffer != nullptr ? std::strlen(buffer) : 0;
}

void APS5_VABI _ZN3sce3Xml6StringC2EPKcm(XmlString* self, const char* buffer, std::size_t size) {
    self->buffer = buffer;
    self->size = size;
}

void APS5_VABI _ZN3sce3Xml6StringC2ERKS1_(XmlString* self, const XmlString* other) {
    self->buffer = other->buffer;
    self->size = other->size;
}

void APS5_VABI _ZN3sce3Xml6StringC2Ev(XmlString* self) {
    self->buffer = nullptr;
    self->size = 0;
}

XmlString* APS5_VABI _ZN3sce3Xml6StringaSERKS1_(XmlString* self, const XmlString* other) {
    self->buffer = other->buffer;
    self->size = other->size;
    return self;
}

int APS5_VABI _ZNK3sce3Xml13AttributeList11isAvailableEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml13AttributeList12getAttributeEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml13AttributeList9getLengthEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node11getNodeNameEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node11getNodeTypeEv() {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI _ZNK3sce3Xml3Dom4Node11isAvailableEv(const XmlNode* self) {
    return self->document != nullptr && self->node != nullptr;
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node12getLastChildEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node12getNodeValueEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node13getAttributesEPNS1_8NodeListE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node13getAttributesEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node13getChildNodesEPNS1_8NodeListE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node13getChildNodesEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node13getFirstChildEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node13getParentNodeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node13hasAttributesEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node13hasChildNodesEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node14getNextSiblingEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom4Node16getOwnerDocumentEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8Document10getDocRootEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8Document10getSiblingENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

}
