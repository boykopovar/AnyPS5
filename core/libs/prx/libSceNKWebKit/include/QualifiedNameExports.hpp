#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WebCore {

struct QualifiedNameImpl {
    std::uint32_t m_refCount;
    std::uint32_t m_existingHash;
    NKWebKit::WtfAbi::StringImpl* m_prefix;
    NKWebKit::WtfAbi::StringImpl* m_localName;
    NKWebKit::WtfAbi::StringImpl* m_namespace;
    NKWebKit::WtfAbi::StringImpl* m_localNameUpper;
};

struct QualifiedName {
    QualifiedNameImpl* m_impl;
};

static_assert(std::is_trivially_copyable_v<QualifiedNameImpl>);
static_assert(sizeof(QualifiedNameImpl) == 40);
static_assert(offsetof(QualifiedNameImpl, m_refCount) == 0);
static_assert(offsetof(QualifiedNameImpl, m_existingHash) == 4);
static_assert(offsetof(QualifiedNameImpl, m_prefix) == 8);
static_assert(offsetof(QualifiedNameImpl, m_localName) == 16);
static_assert(offsetof(QualifiedNameImpl, m_namespace) == 24);
static_assert(offsetof(QualifiedNameImpl, m_localNameUpper) == 32);
static_assert(sizeof(QualifiedName) == 8);

}

extern "C" {

void APS5_VABI _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(WebCore::QualifiedName* self, const NKWebKit::WtfAbi::AtomString* prefix, const NKWebKit::WtfAbi::AtomString* localName, const NKWebKit::WtfAbi::AtomString* namespaceURI);
void APS5_VABI _ZN7WebCore13QualifiedNameC2ERKN3WTF10AtomStringES4_S4_(WebCore::QualifiedName* self, const NKWebKit::WtfAbi::AtomString* prefix, const NKWebKit::WtfAbi::AtomString* localName, const NKWebKit::WtfAbi::AtomString* namespaceURI);
void APS5_VABI _ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev(WebCore::QualifiedNameImpl* self);
void APS5_VABI _ZN7WebCore13QualifiedName17QualifiedNameImplD2Ev(WebCore::QualifiedNameImpl* self);

}