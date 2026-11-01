#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct XmlHandle {
    void* impl;
};

struct XmlString {
    const char* buffer;
    std::size_t size;
};

struct XmlSimpleData {
    const char* data;
    std::size_t size;
};

struct XmlSerializeParameter {
    std::uint64_t first;
    std::uint64_t second;
    std::uint32_t third;
};

struct XmlNodeId {
    void* first;
    void* second;
    std::uint8_t flag;
};

struct XmlNode {
    XmlNodeId id;
    void* document;
    void* node;
    std::uint64_t state[3];
};

struct XmlMemAllocator;

struct XmlMemAllocatorVtable {
    void (APS5_VABI *destroy)(XmlMemAllocator* self);
    void (APS5_VABI *deleteObject)(XmlMemAllocator* self);
    void* (APS5_VABI *allocate)(XmlMemAllocator* self, std::size_t size, void* userData);
    void (APS5_VABI *deallocate)(XmlMemAllocator* self, void* pointer, void* userData);
};

struct XmlMemAllocator {
    const XmlMemAllocatorVtable* vtable;
};

struct XmlInitParameter {
    XmlMemAllocator* allocator;
    void* userData;
    std::uint64_t stored[2];
};

static_assert(sizeof(XmlNodeId) == 0x18);
static_assert(sizeof(XmlNode) == 0x40);
static_assert(offsetof(XmlMemAllocatorVtable, allocate) == 0x10);
static_assert(offsetof(XmlMemAllocatorVtable, deallocate) == 0x18);
static_assert(sizeof(XmlInitParameter) == 0x20);

extern "C" {

void APS5_VABI _ZN3sce3Xml10SimpleDataC1EPKcm(XmlSimpleData* self, const char* data, std::size_t size);
void APS5_VABI _ZN3sce3Xml10SimpleDataC1Ev(XmlSimpleData* self);
void APS5_VABI _ZN3sce3Xml10SimpleDataC2EPKcm(XmlSimpleData* self, const char* data, std::size_t size);
void APS5_VABI _ZN3sce3Xml10SimpleDataC2Ev(XmlSimpleData* self);
int APS5_VABI _ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(XmlHandle* self, const XmlInitParameter* parameter);
int APS5_VABI _ZN3sce3Xml11Initializer9terminateEv(XmlHandle* self);
void APS5_VABI _ZN3sce3Xml11InitializerC1Ev(XmlHandle* self);
void APS5_VABI _ZN3sce3Xml11InitializerC2Ev(XmlHandle* self);
void APS5_VABI _ZN3sce3Xml11InitializerD1Ev(XmlHandle* self);
void APS5_VABI _ZN3sce3Xml11InitializerD2Ev(XmlHandle* self);
void APS5_VABI _ZN3sce3Xml12MemAllocatorC2Ev(XmlMemAllocator* self);
[[noreturn]] void APS5_VABI _ZN3sce3Xml12MemAllocatorD0Ev(XmlMemAllocator* self);
void APS5_VABI _ZN3sce3Xml12MemAllocatorD1Ev(void*);
void APS5_VABI _ZN3sce3Xml12MemAllocatorD2Ev(void*);
int APS5_VABI _ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE();
int APS5_VABI _ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_();
int APS5_VABI _ZN3sce3Xml13AttributeList5clearEv();
int APS5_VABI _ZN3sce3Xml13AttributeList9terminateEv();
int APS5_VABI _ZN3sce3Xml13AttributeListC1ERKS1_();
void APS5_VABI _ZN3sce3Xml13AttributeListC1Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml13AttributeListC2ERKS1_();
void APS5_VABI _ZN3sce3Xml13AttributeListC2Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml13AttributeListD1Ev();
int APS5_VABI _ZN3sce3Xml13AttributeListD2Ev();
void APS5_VABI _ZN3sce3Xml18SerializeParameterC1Ev(XmlSerializeParameter* self);
void APS5_VABI _ZN3sce3Xml18SerializeParameterC2Ev(XmlSerializeParameter* self);
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder10initializeEPKNS0_11InitializerE();
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv();
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder16setResolveEntityEb();
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder20setSkipIgnorableTextEb();
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder26setSkipIgnorableWhiteSpaceEb();
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb();
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder9terminateEv();
void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderC1Ev(XmlHandle* self);
void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderC2Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderD1Ev();
int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderD2Ev();
int APS5_VABI _ZN3sce3Xml3Dom4Node11appendChildENS1_6NodeIdE();
int APS5_VABI _ZN3sce3Xml3Dom4Node11removeChildENS1_6NodeIdE();
int APS5_VABI _ZN3sce3Xml3Dom4Node12insertBeforeENS1_6NodeIdES3_();
int APS5_VABI _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE();
void APS5_VABI _ZN3sce3Xml3Dom4NodeC1ERKS2_(XmlNode* self, const XmlNode* other);
int APS5_VABI _ZN3sce3Xml3Dom4NodeC2ENS1_6NodeIdE();
void APS5_VABI _ZN3sce3Xml3Dom4NodeC2ERKS2_(XmlNode* self, const XmlNode* other);
int APS5_VABI _ZN3sce3Xml3Dom4NodeD1Ev();
int APS5_VABI _ZN3sce3Xml3Dom4NodeD2Ev();
XmlNode* APS5_VABI _ZN3sce3Xml3Dom4NodeaSERKS2_(XmlNode* self, const XmlNode* other);
void APS5_VABI _ZN3sce3Xml3Dom6NodeIdC1EPvS3_b(XmlNodeId* self, void* first, void* second, std::uint8_t flag);
void APS5_VABI _ZN3sce3Xml3Dom6NodeIdC1Ev(XmlNodeId* self);
void APS5_VABI _ZN3sce3Xml3Dom6NodeIdC2EPvS3_b(XmlNodeId* self, void* first, void* second, std::uint8_t flag);
void APS5_VABI _ZN3sce3Xml3Dom6NodeIdC2Ev(XmlNodeId* self);
XmlNodeId APS5_VABI _ZN3sce3Xml3Dom6NodeIdaSES2_(XmlNodeId* self, XmlNodeId value);
void* APS5_VABI _ZN3sce3Xml3Dom6NodeIdcvPvEv(XmlNodeId*);
bool APS5_VABI _ZN3sce3Xml3Dom6NodeIdeqES2_(const XmlNodeId* self, XmlNodeId other);
int APS5_VABI _ZN3sce3Xml3Dom6NodeIdeqEi();
bool APS5_VABI _ZN3sce3Xml3Dom6NodeIdneES2_(const XmlNodeId* self, XmlNodeId other);
int APS5_VABI _ZN3sce3Xml3Dom6NodeIdneEi();
int APS5_VABI _ZN3sce3Xml3Dom6NodeIdntEv();
int APS5_VABI _ZN3sce3Xml3Dom8Document10importNodeENS1_6NodeIdES3_PKS2_S3_();
int APS5_VABI _ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE();
int APS5_VABI _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_();
int APS5_VABI _ZN3sce3Xml3Dom8Document11removeChildENS1_6NodeIdES3_();
int APS5_VABI _ZN3sce3Xml3Dom8Document11resetStatusEv();
int APS5_VABI _ZN3sce3Xml3Dom8Document11setWritableEv();
int APS5_VABI _ZN3sce3Xml3Dom8Document12importParentEPKS2_NS1_6NodeIdE();
int APS5_VABI _ZN3sce3Xml3Dom8Document12setAttrValueENS1_6NodeIdEPKNS0_6StringES6_();
int APS5_VABI _ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_();
int APS5_VABI _ZN3sce3Xml3Dom8Document13createElementEPKNS0_6StringEPKNS0_13AttributeListES5_();
int APS5_VABI _ZN3sce3Xml3Dom8Document13recurseDeleteENS1_6NodeIdE();
int APS5_VABI _ZN3sce3Xml3Dom8Document14createTextNodeEPKNS0_6StringE();
int APS5_VABI _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_();
int APS5_VABI _ZN3sce3Xml3Dom8Document15removeAttributeENS1_6NodeIdEPKNS0_6StringE();
int APS5_VABI _ZN3sce3Xml3Dom8Document16removeAttributesENS1_6NodeIdE();
int APS5_VABI _ZN3sce3Xml3Dom8Document16setAttributeListENS1_6NodeIdEPKNS0_13AttributeListE();
int APS5_VABI _ZN3sce3Xml3Dom8Document7setTextENS1_6NodeIdEPKNS0_6StringE();
int APS5_VABI _ZN3sce3Xml3Dom8Document9serializeEPKNS0_18SerializeParameterEPNS0_6StringE();
int APS5_VABI _ZN3sce3Xml3Dom8Document9terminateEv();
int APS5_VABI _ZN3sce3Xml3Dom8DocumentC1ERKS2_();
void APS5_VABI _ZN3sce3Xml3Dom8DocumentC1Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml3Dom8DocumentC2ERKS2_();
void APS5_VABI _ZN3sce3Xml3Dom8DocumentC2Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml3Dom8DocumentD1Ev();
int APS5_VABI _ZN3sce3Xml3Dom8DocumentD2Ev();
int APS5_VABI _ZN3sce3Xml3Dom8DocumentaSERKS2_();
int APS5_VABI _ZN3sce3Xml3Dom8NodeList10initializeEPKNS0_11InitializerE();
int APS5_VABI _ZN3sce3Xml3Dom8NodeList10insertLastENS1_6NodeIdE();
int APS5_VABI _ZN3sce3Xml3Dom8NodeList10removeItemENS1_6NodeIdE();
int APS5_VABI _ZN3sce3Xml3Dom8NodeList11insertFirstENS1_6NodeIdE();
int APS5_VABI _ZN3sce3Xml3Dom8NodeList5clearEv();
int APS5_VABI _ZN3sce3Xml3Dom8NodeList9terminateEv();
int APS5_VABI _ZN3sce3Xml3Dom8NodeListC1ERKS2_();
void APS5_VABI _ZN3sce3Xml3Dom8NodeListC1Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml3Dom8NodeListC2ERKS2_();
void APS5_VABI _ZN3sce3Xml3Dom8NodeListC2Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml3Dom8NodeListD1Ev();
int APS5_VABI _ZN3sce3Xml3Dom8NodeListD2Ev();
int APS5_VABI _ZN3sce3Xml3Sax6Parser10initializeEPKNS0_11InitializerE();
int APS5_VABI _ZN3sce3Xml3Sax6Parser11setUserDataEPv();
int APS5_VABI _ZN3sce3Xml3Sax6Parser16setResolveEntityEb();
int APS5_VABI _ZN3sce3Xml3Sax6Parser18setDocumentHandlerEPNS1_15DocumentHandlerE();
int APS5_VABI _ZN3sce3Xml3Sax6Parser26setSkipIgnorableWhiteSpaceEb();
int APS5_VABI _ZN3sce3Xml3Sax6Parser5parseEPKNS0_6StringEb();
int APS5_VABI _ZN3sce3Xml3Sax6Parser5resetEv();
int APS5_VABI _ZN3sce3Xml3Sax6Parser9terminateEv();
void APS5_VABI _ZN3sce3Xml3Sax6ParserC1Ev(XmlHandle* self);
void APS5_VABI _ZN3sce3Xml3Sax6ParserC2Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml3Sax6ParserD1Ev();
int APS5_VABI _ZN3sce3Xml3Sax6ParserD2Ev();
int APS5_VABI _ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE();
int APS5_VABI _ZN3sce3Xml4Attr7setNameEPKNS0_6StringE();
int APS5_VABI _ZN3sce3Xml4Attr8setValueEPKNS0_6StringE();
int APS5_VABI _ZN3sce3Xml4Attr9terminateEv();
int APS5_VABI _ZN3sce3Xml4AttrC1ERKS1_();
void APS5_VABI _ZN3sce3Xml4AttrC1Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml4AttrC2ERKS1_();
void APS5_VABI _ZN3sce3Xml4AttrC2Ev(XmlHandle* self);
int APS5_VABI _ZN3sce3Xml4AttrD1Ev();
int APS5_VABI _ZN3sce3Xml4AttrD2Ev();
int APS5_VABI _ZN3sce3Xml4AttraSERKS1_();
int APS5_VABI _ZN3sce3Xml4Util9strResultEi();
void APS5_VABI _ZN3sce3Xml6StringC1EPKc(XmlString* self, const char* buffer);
void APS5_VABI _ZN3sce3Xml6StringC1EPKcm(XmlString* self, const char* buffer, std::size_t size);
void APS5_VABI _ZN3sce3Xml6StringC1ERKS1_(XmlString* self, const XmlString* other);
void APS5_VABI _ZN3sce3Xml6StringC1Ev(XmlString* self);
void APS5_VABI _ZN3sce3Xml6StringC2EPKc(XmlString* self, const char* buffer);
void APS5_VABI _ZN3sce3Xml6StringC2EPKcm(XmlString* self, const char* buffer, std::size_t size);
void APS5_VABI _ZN3sce3Xml6StringC2ERKS1_(XmlString* self, const XmlString* other);
void APS5_VABI _ZN3sce3Xml6StringC2Ev(XmlString* self);
XmlString* APS5_VABI _ZN3sce3Xml6StringaSERKS1_(XmlString* self, const XmlString* other);
int APS5_VABI _ZNK3sce3Xml13AttributeList11isAvailableEv();
int APS5_VABI _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE();
int APS5_VABI _ZNK3sce3Xml13AttributeList12getAttributeEj();
int APS5_VABI _ZNK3sce3Xml13AttributeList9getLengthEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node11getNodeNameEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node11getNodeTypeEv();
bool APS5_VABI _ZNK3sce3Xml3Dom4Node11isAvailableEv(const XmlNode* self);
int APS5_VABI _ZNK3sce3Xml3Dom4Node12getLastChildEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node12getNodeValueEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node13getAttributesEPNS1_8NodeListE();
int APS5_VABI _ZNK3sce3Xml3Dom4Node13getAttributesEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node13getChildNodesEPNS1_8NodeListE();
int APS5_VABI _ZNK3sce3Xml3Dom4Node13getChildNodesEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node13getFirstChildEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node13getParentNodeEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node13hasAttributesEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node13hasChildNodesEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node14getNextSiblingEv();
int APS5_VABI _ZNK3sce3Xml3Dom4Node16getOwnerDocumentEv();
int APS5_VABI _ZNK3sce3Xml3Dom8Document10getDocRootEv();
int APS5_VABI _ZNK3sce3Xml3Dom8Document10getSiblingENS1_6NodeIdE();

}
