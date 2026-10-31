#include "prx/libSceXml/include/InventoryExports.hpp"
#include "prx/libSceXml/include/Api.hpp"
#include "prx/libSceXml/src/Document.hpp"
#include "prx/libSceXml/src/DocumentBuilder.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <atomic>
#include <stdexcept>

namespace {

constexpr int XmlErrorNoMemory = static_cast<int>(0x80850001);
constexpr int XmlErrorNotInitialized = static_cast<int>(0x80850002);
constexpr int XmlErrorInvalidParameter = static_cast<int>(0x80850003);
constexpr int XmlAlreadyInitialized = 0x00850003;
constexpr int XmlErrorEngineNoMemory = static_cast<int>(0x80851000);
constexpr std::size_t XmlInitializerImplSize = 0x20;
constexpr std::size_t XmlAllocatorWrapperSize = 0x10;
constexpr std::size_t XmlEngineStateSize = 0xc8;

struct XmlAllocatorWrapper {
    void* reserved;
    XmlMemAllocator* allocator;
};

static_assert(sizeof(XmlAllocatorWrapper) == XmlAllocatorWrapperSize);

int g_xmlActiveInitializers = 0;
XmlAllocatorWrapper* g_xmlAllocatorWrapper = nullptr;
std::atomic<int> g_xmlEngineUsers = 0;
void* g_xmlEngineState = nullptr;
XmlMemAllocator* g_xmlEngineAllocator = nullptr;
void* g_xmlEngineUserData = nullptr;

void* XmlAllocate(XmlMemAllocator* allocator, std::size_t size, void* userData) {
    return allocator->vtable->allocate(allocator, size, userData);
}

void XmlDeallocate(XmlMemAllocator* allocator, void* pointer, void* userData) {
    allocator->vtable->deallocate(allocator, pointer, userData);
}

int XmlEngineAcquire() {
    if (g_xmlEngineUsers.fetch_add(1) != 0) return 0;
    g_xmlEngineState = XmlAllocate(g_xmlEngineAllocator, XmlEngineStateSize, g_xmlEngineUserData);
    if (g_xmlEngineState == nullptr) return XmlErrorEngineNoMemory;
    std::memset(g_xmlEngineState, 0, XmlEngineStateSize);
    return 0;
}

void XmlEngineRelease() {
    if (g_xmlEngineUsers.fetch_sub(1) != 1) return;
    if (g_xmlEngineState != nullptr) XmlDeallocate(g_xmlEngineAllocator, g_xmlEngineState, g_xmlEngineUserData);
    g_xmlEngineState = nullptr;
}

void XmlTerminate(XmlHandle* self) {
    const auto* impl = static_cast<const XmlInitParameter*>(self->impl);
    XmlEngineRelease();
    if (g_xmlActiveInitializers > 0) --g_xmlActiveInitializers;
    if (g_xmlAllocatorWrapper != nullptr) {
        XmlDeallocate(impl->allocator, g_xmlAllocatorWrapper, impl->userData);
        g_xmlAllocatorWrapper = nullptr;
    }
    XmlDeallocate(impl->allocator, self->impl, impl->userData);
    self->impl = nullptr;
}

void APS5_VABI XmlMemAllocatorDestroy_nid_no_patch(XmlMemAllocator*) {
}

void APS5_VABI XmlMemAllocatorDelete_nid_no_patch(XmlMemAllocator*) {
    throw std::logic_error("sce::Xml::MemAllocator: deleting destructor of the abstract base");
}

void* APS5_VABI XmlMemAllocatorPureAllocate_nid_no_patch(XmlMemAllocator*, std::size_t, void*) {
    __cxa_pure_virtual_nid_postfix();
}

void APS5_VABI XmlMemAllocatorPureDeallocate_nid_no_patch(XmlMemAllocator*, void*, void*) {
    __cxa_pure_virtual_nid_postfix();
}

const XmlMemAllocatorVtable g_xmlMemAllocatorVtable{XmlMemAllocatorDestroy_nid_no_patch, XmlMemAllocatorDelete_nid_no_patch, XmlMemAllocatorPureAllocate_nid_no_patch, XmlMemAllocatorPureDeallocate_nid_no_patch};

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

int APS5_VABI _ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(XmlHandle* self, const XmlInitParameter* parameter) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Initializer::initialize: null object");
    if (self->impl != nullptr) return XmlAlreadyInitialized;
    if (parameter == nullptr) return XmlErrorInvalidParameter;
    if (g_xmlActiveInitializers > 0) return XmlAlreadyInitialized;
    if (parameter->allocator == nullptr) throw std::invalid_argument("sce::Xml::Initializer::initialize: null allocator");
    void* impl = XmlAllocate(parameter->allocator, XmlInitializerImplSize, parameter->userData);
    if (impl == nullptr) return XmlErrorNoMemory;
    std::memcpy(impl, parameter, XmlInitializerImplSize);
    self->impl = impl;
    auto* wrapper = static_cast<XmlAllocatorWrapper*>(XmlAllocate(parameter->allocator, XmlAllocatorWrapperSize, parameter->userData));
    if (wrapper == nullptr) {
        XmlTerminate(self);
        return XmlErrorNoMemory;
    }
    *wrapper = {nullptr, parameter->allocator};
    g_xmlAllocatorWrapper = wrapper;
    g_xmlEngineAllocator = parameter->allocator;
    g_xmlEngineUserData = parameter->userData;
    const int result = XmlEngineAcquire();
    if (result < 0) return result;
    ++g_xmlActiveInitializers;
    return result;
}

int APS5_VABI _ZN3sce3Xml11Initializer9terminateEv(XmlHandle* self) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Initializer::terminate: null object");
    if (self->impl == nullptr) return XmlErrorNotInitialized;
    XmlTerminate(self);
    return 0;
}

void APS5_VABI _ZN3sce3Xml11InitializerC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml11InitializerC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml11InitializerD1Ev(XmlHandle* self) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Initializer::~Initializer: null object");
    if (self->impl == nullptr) return;
    XmlTerminate(self);
}

void APS5_VABI _ZN3sce3Xml11InitializerD2Ev(XmlHandle* self) {
    _ZN3sce3Xml11InitializerD1Ev(self);
}

void APS5_VABI _ZN3sce3Xml12MemAllocatorC2Ev(XmlMemAllocator* self) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::MemAllocator::MemAllocator: null object");
    self->vtable = &g_xmlMemAllocatorVtable;
}

void APS5_VABI _ZN3sce3Xml12MemAllocatorD0Ev(XmlMemAllocator*) {
    throw std::logic_error("sce::Xml::MemAllocator: deleting destructor of the abstract base");
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

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder10initializeEPKNS0_11InitializerE(XmlHandle* self, const XmlHandle* initializer) {
    return XmlDom::BuilderInitialize(self, initializer);
}

XmlHandle* APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(XmlHandle* result, XmlHandle* self) {
    return XmlDom::BuilderGetDocument(result, self);
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder16setResolveEntityEb(XmlHandle* self, bool value) {
    XmlDom::BuilderSetOption(self, XmlDom::BuilderOption::ResolveEntity, value);
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder20setSkipIgnorableTextEb(XmlHandle* self, bool value) {
    XmlDom::BuilderSetOption(self, XmlDom::BuilderOption::SkipIgnorableText, value);
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder26setSkipIgnorableWhiteSpaceEb(XmlHandle* self, bool value) {
    XmlDom::BuilderSetOption(self, XmlDom::BuilderOption::SkipIgnorableWhiteSpace, value);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb(XmlHandle* self, const XmlString* text, bool last) {
    return XmlDom::BuilderParse(self, text, last);
}

int APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilder9terminateEv(XmlHandle* self) {
    return XmlDom::BuilderTerminate(self);
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderD1Ev(XmlHandle* self) {
    XmlDom::BuilderDestroy(self);
}

void APS5_VABI _ZN3sce3Xml3Dom15DocumentBuilderD2Ev(XmlHandle* self) {
    XmlDom::BuilderDestroy(self);
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

int APS5_VABI _ZN3sce3Xml3Dom8Document9terminateEv(XmlHandle* self) {
    return XmlDom::DocumentTerminate(self);
}

void APS5_VABI _ZN3sce3Xml3Dom8DocumentC1ERKS2_(XmlHandle* self, const XmlHandle* other) {
    XmlDom::DocumentCopy(self, other);
}

void APS5_VABI _ZN3sce3Xml3Dom8DocumentC1Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml3Dom8DocumentC2ERKS2_(XmlHandle* self, const XmlHandle* other) {
    XmlDom::DocumentCopy(self, other);
}

void APS5_VABI _ZN3sce3Xml3Dom8DocumentC2Ev(XmlHandle* self) {
    self->impl = nullptr;
}

void APS5_VABI _ZN3sce3Xml3Dom8DocumentD1Ev(XmlHandle* self) {
    XmlDom::DocumentDestroy(self);
}

void APS5_VABI _ZN3sce3Xml3Dom8DocumentD2Ev(XmlHandle* self) {
    XmlDom::DocumentDestroy(self);
}

XmlHandle* APS5_VABI _ZN3sce3Xml3Dom8DocumentaSERKS2_(XmlHandle* self, const XmlHandle* other) {
    return XmlDom::DocumentAssign(self, other);
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

XmlNodeId* APS5_VABI _ZNK3sce3Xml3Dom8Document10getDocRootEv(XmlNodeId* result, const XmlHandle* self) {
    return XmlDom::DocumentNode(result, self, XmlDom::NodeQuery::DocRoot, XmlNodeId{});
}

XmlNodeId* APS5_VABI _ZNK3sce3Xml3Dom8Document10getSiblingENS1_6NodeIdE(XmlNodeId* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentNode(result, self, XmlDom::NodeQuery::Sibling, node);
}


bool APS5_VABI _ZNK3sce3Xml3Dom8Document11isAvailableEv(const XmlHandle* self) {
    return XmlDom::DocumentIsAvailable(self);
}

int APS5_VABI _ZNK3sce3Xml3Dom8Document9getStatusEv(const XmlHandle* self) {
    return XmlDom::DocumentGetStatus(self);
}

bool APS5_VABI _ZNK3sce3Xml3Dom8Document10isReadOnlyEv(const XmlHandle* self) {
    return XmlDom::DocumentIsReadOnly(self);
}

XmlNodeId* APS5_VABI _ZNK3sce3Xml3Dom8Document7getRootEv(XmlNodeId* result, const XmlHandle* self) {
    return XmlDom::DocumentNode(result, self, XmlDom::NodeQuery::Root, XmlNodeId{});
}

XmlNodeId* APS5_VABI _ZNK3sce3Xml3Dom8Document13getFirstChildENS1_6NodeIdE(XmlNodeId* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentNode(result, self, XmlDom::NodeQuery::FirstChild, node);
}

XmlNodeId* APS5_VABI _ZNK3sce3Xml3Dom8Document9getParentENS1_6NodeIdE(XmlNodeId* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentNode(result, self, XmlDom::NodeQuery::Parent, node);
}

XmlNodeId* APS5_VABI _ZNK3sce3Xml3Dom8Document12getLastChildENS1_6NodeIdE(XmlNodeId* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentNode(result, self, XmlDom::NodeQuery::LastChild, node);
}

XmlNodeId* APS5_VABI _ZNK3sce3Xml3Dom8Document12getFirstAttrENS1_6NodeIdE(XmlNodeId* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentNode(result, self, XmlDom::NodeQuery::FirstAttribute, node);
}

XmlNodeId* APS5_VABI _ZNK3sce3Xml3Dom8Document11getNextAttrENS1_6NodeIdE(XmlNodeId* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentNode(result, self, XmlDom::NodeQuery::NextAttribute, node);
}

XmlString* APS5_VABI _ZNK3sce3Xml3Dom8Document11getNodeNameENS1_6NodeIdE(XmlString* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentString(result, self, XmlDom::StringQuery::NodeName, node);
}

XmlString* APS5_VABI _ZNK3sce3Xml3Dom8Document7getTextENS1_6NodeIdE(XmlString* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentString(result, self, XmlDom::StringQuery::Text, node);
}

XmlString* APS5_VABI _ZNK3sce3Xml3Dom8Document14getSkippedTextENS1_6NodeIdE(XmlString* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentString(result, self, XmlDom::StringQuery::SkippedText, node);
}

XmlString* APS5_VABI _ZNK3sce3Xml3Dom8Document11getAttrNameENS1_6NodeIdE(XmlString* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentString(result, self, XmlDom::StringQuery::AttributeName, node);
}

XmlString* APS5_VABI _ZNK3sce3Xml3Dom8Document12getAttrValueENS1_6NodeIdE(XmlString* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentString(result, self, XmlDom::StringQuery::AttributeValue, node);
}

XmlString* APS5_VABI _ZNK3sce3Xml3Dom8Document9getEntityENS1_6NodeIdE(XmlString* result, const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentString(result, self, XmlDom::StringQuery::Entity, node);
}

XmlString* APS5_VABI _ZNK3sce3Xml3Dom8Document12getAttributeENS1_6NodeIdEPKNS0_6StringE(XmlString* result, const XmlHandle* self, XmlNodeId node, const XmlString* name) {
    return XmlDom::DocumentAttribute(result, self, node, name);
}

int APS5_VABI _ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(const XmlHandle* self, XmlNodeId node) {
    return static_cast<int>(XmlDom::DocumentNumber(self, XmlDom::NumberQuery::NodeType, node));
}

int APS5_VABI _ZNK3sce3Xml3Dom8Document13getEntityTypeENS1_6NodeIdE(const XmlHandle* self, XmlNodeId node) {
    return static_cast<int>(XmlDom::DocumentNumber(self, XmlDom::NumberQuery::EntityType, node));
}

bool APS5_VABI _ZNK3sce3Xml3Dom8Document13hasChildNodesENS1_6NodeIdE(const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentNumber(self, XmlDom::NumberQuery::HasChildNodes, node) != 0;
}

bool APS5_VABI _ZNK3sce3Xml3Dom8Document13hasAttributesENS1_6NodeIdE(const XmlHandle* self, XmlNodeId node) {
    return XmlDom::DocumentNumber(self, XmlDom::NumberQuery::HasAttributes, node) != 0;
}

XmlNodeId _ZN3sce3Xml3Dom13invalidNodeIdE{};

}

extern "C" {

int APS5_VABI _ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml4Attr7getNameEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8NodeList8findItemENS1_6NodeIdE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8Document13getAttributesENS1_6NodeIdEPNS1_8NodeListE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8NodeList4itemEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml4Attr8getValueEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8NodeListixEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8NodeList8findItemEPKNS0_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8NodeList11isAvailableEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8Document10getXmlMetaEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml3Dom8NodeList9getLengthEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce3Xml4Attr11isAvailableEv() {
    NotImplemented_nid_no_patch(__func__);
}

}
