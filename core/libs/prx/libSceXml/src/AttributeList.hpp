#pragma once

#include "prx/libSceXml/include/Api.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace XmlDom {

constexpr int NotInitialized = static_cast<int>(0x80850002);
constexpr int AlreadyInitialized = 0x00850003;

struct Attribute {
    std::string name;
    std::string value;
};

struct AttributeContainer {
    XmlMemAllocator* allocator = nullptr;
    void* userData = nullptr;
    std::vector<Attribute> attributes;
    std::string name;
    std::string value;
    bool available = false;
};

int ContainerInitialize(XmlHandle* self, const XmlHandle* initializer);
int ContainerTerminate(XmlHandle* self);
void ContainerDestroy(XmlHandle* self);
void ContainerCopy(XmlHandle* self, const XmlHandle* other);
XmlHandle* ContainerAssign(XmlHandle* self, const XmlHandle* other);
bool ContainerIsAvailable(const XmlHandle* self);
int ContainerAddAttribute(XmlHandle* self, const XmlString* name, const XmlString* value);
int ContainerClear(XmlHandle* self);
int ContainerGetLength(const XmlHandle* self);
XmlString* ContainerGetAttributeByName(XmlString* result, const XmlHandle* self, const XmlString* name);
XmlString* ContainerGetAttributeByIndex(XmlString* result, const XmlHandle* self, std::uint32_t index);
int ContainerSetName(XmlHandle* self, const XmlString* name);
int ContainerSetValue(XmlHandle* self, const XmlString* value);
XmlString* ContainerGetName(XmlString* result, const XmlHandle* self);
XmlString* ContainerGetValue(XmlString* result, const XmlHandle* self);

}
