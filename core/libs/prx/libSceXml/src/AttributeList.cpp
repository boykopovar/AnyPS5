#include "prx/libSceXml/src/AttributeList.hpp"

#include "prx/libc/include/General.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace XmlDom {

namespace {

constexpr std::size_t BlockSize = 72;

AttributeContainer* StateOf(const XmlHandle* self) {
    AttributeContainer* state = nullptr;
    std::memcpy(&state, self->impl, sizeof(state));
    return state;
}

void RequireHandle(const XmlHandle* self, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
}

void SetString(XmlString* result, const std::string& text) {
    if (text.empty()) {
        result->buffer = nullptr;
        result->size = 0;
        return;
    }
    result->buffer = text.c_str();
    result->size = text.size();
}

void SetEmpty(XmlString* result) {
    result->buffer = nullptr;
    result->size = 0;
}

std::string ReadGuest(const XmlString* string, const char* function) {
    if (string->size == 0) return {};
    std::string text(string->size, '\0');
    if (string->buffer == nullptr) throw std::invalid_argument(std::string(function) + ": a null string buffer with a non-zero size");
    if (!GuestCopyIn_nid_no_patch(text.data(), string->buffer, string->size)) {
        throw std::logic_error(std::string(function) + ": reading a string where it is not readable faults in 13.42");
    }
    return text;
}

void Attach(XmlHandle* self, XmlMemAllocator* allocator, void* userData) {
    auto state = std::make_unique<AttributeContainer>();
    state->allocator = allocator;
    state->userData = userData;
    state->available = true;
    void* block = allocator->vtable->allocate(allocator, BlockSize, userData);
    if (block == nullptr) throw std::logic_error("sce::Xml: an allocation failure while initializing an attribute container was not measured");
    AttributeContainer* owned = state.release();
    std::memcpy(block, &owned, sizeof(owned));
    self->impl = block;
}

}

int ContainerInitialize(XmlHandle* self, const XmlHandle* initializer) {
    RequireHandle(self, "sce::Xml: initialize");
    if (self->impl != nullptr) return AlreadyInitialized;
    if (initializer == nullptr || initializer->impl == nullptr) return NotInitialized;
    const auto* parameter = static_cast<const XmlInitParameter*>(initializer->impl);
    if (parameter->allocator == nullptr) throw std::invalid_argument("sce::Xml: initialize with a null allocator");
    Attach(self, parameter->allocator, parameter->userData);
    return 0;
}

void ContainerDestroy(XmlHandle* self) {
    RequireHandle(self, "sce::Xml: destructor");
    if (self->impl == nullptr) return;
    AttributeContainer* state = StateOf(self);
    state->allocator->vtable->deallocate(state->allocator, self->impl, state->userData);
    delete state;
    self->impl = nullptr;
}

int ContainerTerminate(XmlHandle* self) {
    RequireHandle(self, "sce::Xml: terminate");
    if (self->impl == nullptr) return NotInitialized;
    ContainerDestroy(self);
    return 0;
}

void ContainerCopy(XmlHandle* self, const XmlHandle* other) {
    RequireHandle(self, "sce::Xml: copy constructor");
    RequireHandle(other, "sce::Xml: copy constructor");
    self->impl = nullptr;
    if (other->impl == nullptr) return;
    const AttributeContainer* source = StateOf(other);
    Attach(self, source->allocator, source->userData);
    AttributeContainer* copy = StateOf(self);
    copy->attributes = source->attributes;
    copy->name = source->name;
    copy->value = source->value;
}

XmlHandle* ContainerAssign(XmlHandle* self, const XmlHandle* other) {
    RequireHandle(self, "sce::Xml: operator=");
    RequireHandle(other, "sce::Xml: operator=");
    if (self == other) return self;
    if (other->impl == nullptr) {
        if (self->impl != nullptr) throw std::logic_error("sce::Xml: assigning an empty attribute container over a live one was not measured");
        return self;
    }
    const AttributeContainer* source = StateOf(other);
    if (self->impl == nullptr) {
        Attach(self, source->allocator, source->userData);
    }
    AttributeContainer* target = StateOf(self);
    target->attributes = source->attributes;
    target->name = source->name;
    target->value = source->value;
    return self;
}

bool ContainerIsAvailable(const XmlHandle* self) {
    RequireHandle(self, "sce::Xml: isAvailable");
    return self->impl != nullptr;
}

int ContainerAddAttribute(XmlHandle* self, const XmlString* name, const XmlString* value) {
    RequireHandle(self, "sce::Xml: addAttribute");
    if (self->impl == nullptr) throw std::logic_error("sce::Xml: addAttribute on a container that was not initialized was not measured");
    if (name == nullptr || value == nullptr) throw std::invalid_argument("sce::Xml: addAttribute with a null string");
    AttributeContainer* state = StateOf(self);
    state->attributes.push_back(Attribute{ReadGuest(name, "sce::Xml: addAttribute"), ReadGuest(value, "sce::Xml: addAttribute")});
    return 0;
}

int ContainerClear(XmlHandle* self) {
    RequireHandle(self, "sce::Xml: clear");
    if (self->impl == nullptr) throw std::logic_error("sce::Xml: clear on a container that was not initialized was not measured");
    AttributeContainer* state = StateOf(self);
    state->attributes.clear();
    return 0;
}

int ContainerGetLength(const XmlHandle* self) {
    RequireHandle(self, "sce::Xml: getLength");
    if (self->impl == nullptr) return 0;
    return static_cast<int>(StateOf(self)->attributes.size());
}

XmlString* ContainerGetAttributeByName(XmlString* result, const XmlHandle* self, const XmlString* name) {
    RequireHandle(self, "sce::Xml: getAttribute");
    if (result == nullptr) throw std::invalid_argument("sce::Xml: getAttribute with a null result");
    if (self->impl == nullptr || name == nullptr) {
        SetEmpty(result);
        return result;
    }
    const std::string wanted = ReadGuest(name, "sce::Xml: getAttribute");
    for (const Attribute& attribute : StateOf(self)->attributes) {
        if (attribute.name == wanted) {
            SetString(result, attribute.value);
            return result;
        }
    }
    SetEmpty(result);
    return result;
}

XmlString* ContainerGetAttributeByIndex(XmlString* result, const XmlHandle* self, std::uint32_t index) {
    RequireHandle(self, "sce::Xml: getAttribute");
    if (result == nullptr) throw std::invalid_argument("sce::Xml: getAttribute with a null result");
    if (self->impl == nullptr) {
        SetEmpty(result);
        return result;
    }
    const AttributeContainer* state = StateOf(self);
    if (index >= state->attributes.size()) {
        SetEmpty(result);
        return result;
    }
    SetString(result, state->attributes[index].value);
    return result;
}

int ContainerSetName(XmlHandle* self, const XmlString* name) {
    RequireHandle(self, "sce::Xml: setName");
    if (self->impl == nullptr) throw std::logic_error("sce::Xml: setName on an object that was not initialized was not measured");
    if (name == nullptr) throw std::invalid_argument("sce::Xml: setName with a null string");
    StateOf(self)->name = ReadGuest(name, "sce::Xml: setName");
    return 0;
}

int ContainerSetValue(XmlHandle* self, const XmlString* value) {
    RequireHandle(self, "sce::Xml: setValue");
    if (self->impl == nullptr) throw std::logic_error("sce::Xml: setValue on an object that was not initialized was not measured");
    if (value == nullptr) throw std::invalid_argument("sce::Xml: setValue with a null string");
    StateOf(self)->value = ReadGuest(value, "sce::Xml: setValue");
    return 0;
}

XmlString* ContainerGetName(XmlString* result, const XmlHandle* self) {
    RequireHandle(self, "sce::Xml: getName");
    if (result == nullptr) throw std::invalid_argument("sce::Xml: getName with a null result");
    if (self->impl == nullptr) {
        SetEmpty(result);
        return result;
    }
    SetString(result, StateOf(self)->name);
    return result;
}

XmlString* ContainerGetValue(XmlString* result, const XmlHandle* self) {
    RequireHandle(self, "sce::Xml: getValue");
    if (result == nullptr) throw std::invalid_argument("sce::Xml: getValue with a null result");
    if (self->impl == nullptr) {
        SetEmpty(result);
        return result;
    }
    SetString(result, StateOf(self)->value);
    return result;
}

}
