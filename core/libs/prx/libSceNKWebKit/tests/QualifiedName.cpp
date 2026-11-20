#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WtfAbi::AtomString;
using NKWebKit::WtfAbi::String;
using WebCore::QualifiedName;
using WebCore::QualifiedNameImpl;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "QualifiedName check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

AtomString Atom(const char* text) {
    return AtomString { _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text))) };
}

QualifiedName Construct(const AtomString& prefix, const AtomString& localName, const AtomString& namespaceURI) {
    QualifiedName name { nullptr };
    _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(&name, &prefix, &localName, &namespaceURI);
    return name;
}

void Deref(QualifiedName& name) {
    QualifiedNameImpl* impl = std::exchange(name.m_impl, nullptr);
    if (--impl->m_refCount) return;
    _ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev(impl);
    _ZN3WTF8fastFreeEPv(impl);
}

template<typename Function>
void RequireThrows(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

void TestFields() {
    AtomString prefix = Atom("svg");
    AtomString localName = Atom("rect");
    AtomString namespaceURI = Atom("http://www.w3.org/2000/svg");
    QualifiedName name = Construct(prefix, localName, namespaceURI);
    Require(name.m_impl);
    Require(name.m_impl->m_refCount == 1);
    Require(name.m_impl->m_existingHash == 0);
    Require(name.m_impl->m_prefix == prefix.string.impl);
    Require(name.m_impl->m_localName == localName.string.impl);
    Require(name.m_impl->m_namespace == namespaceURI.string.impl);
    Require(!name.m_impl->m_localNameUpper);
    Require(localName.string.impl->refCount == 4);
    Deref(name);
    Require(localName.string.impl->refCount == 2);
    Require(prefix.string.impl->refCount == 2);
    Require(namespaceURI.string.impl->refCount == 2);
}

void TestInterning() {
    AtomString prefix;
    AtomString localName = Atom("div");
    AtomString otherLocalName = Atom("span");
    AtomString namespaceURI = Atom("http://www.w3.org/1999/xhtml");
    QualifiedName first = Construct(prefix, localName, namespaceURI);
    QualifiedName second = Construct(prefix, localName, namespaceURI);
    QualifiedName other = Construct(prefix, otherLocalName, namespaceURI);
    Require(first.m_impl == second.m_impl);
    Require(first.m_impl->m_refCount == 2);
    Require(first.m_impl != other.m_impl);
    Require(!first.m_impl->m_prefix);
    Deref(other);
    Deref(second);
    Deref(first);
    Require(localName.string.impl->refCount == 2);
}

void TestEmptyNamespace() {
    AtomString prefix;
    AtomString localName = Atom("lang");
    AtomString nullNamespace;
    AtomString emptyNamespace = Atom("");
    const unsigned emptyReferences = emptyNamespace.string.impl->refCount;
    QualifiedName withNull = Construct(prefix, localName, nullNamespace);
    QualifiedName withEmpty = Construct(prefix, localName, emptyNamespace);
    Require(withNull.m_impl == withEmpty.m_impl);
    Require(!withNull.m_impl->m_namespace);
    Require(emptyNamespace.string.impl->refCount == emptyReferences);
    Deref(withEmpty);
    Deref(withNull);
}

void TestDestroyedEntryIsReplaced() {
    AtomString prefix;
    AtomString localName = Atom("href");
    AtomString namespaceURI = Atom("http://www.w3.org/1999/xlink");
    QualifiedName dying = Construct(prefix, localName, namespaceURI);
    QualifiedNameImpl* dyingImpl = dying.m_impl;
    dyingImpl->m_refCount = 0;
    QualifiedName replacement = Construct(prefix, localName, namespaceURI);
    Require(replacement.m_impl != dyingImpl);
    Require(replacement.m_impl->m_refCount == 1);
    _ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev(dyingImpl);
    _ZN3WTF8fastFreeEPv(dyingImpl);
    QualifiedName again = Construct(prefix, localName, namespaceURI);
    Require(again.m_impl == replacement.m_impl);
    Require(again.m_impl->m_refCount == 2);
    Deref(again);
    Deref(replacement);
    Require(localName.string.impl->refCount == 2);
}

void TestBaseObjectEntryPoints() {
    AtomString prefix;
    AtomString localName = Atom("title");
    AtomString namespaceURI;
    QualifiedName complete = Construct(prefix, localName, namespaceURI);
    QualifiedName base { nullptr };
    _ZN7WebCore13QualifiedNameC2ERKN3WTF10AtomStringES4_S4_(&base, &prefix, &localName, &namespaceURI);
    Require(base.m_impl == complete.m_impl);
    Deref(base);
    QualifiedNameImpl* impl = std::exchange(complete.m_impl, nullptr);
    Require(impl->m_refCount == 1);
    impl->m_refCount = 0;
    _ZN7WebCore13QualifiedName17QualifiedNameImplD2Ev(impl);
    _ZN3WTF8fastFreeEPv(impl);
    Require(localName.string.impl->refCount == 2);
}

void TestNullArguments() {
    AtomString atom = Atom("a");
    QualifiedName name { nullptr };
    RequireThrows([&] { _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(nullptr, &atom, &atom, &atom); });
    RequireThrows([&] { _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(&name, nullptr, &atom, &atom); });
    RequireThrows([&] { _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(&name, &atom, nullptr, &atom); });
    RequireThrows([&] { _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(&name, &atom, &atom, nullptr); });
    RequireThrows([] { _ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev(nullptr); });
    Require(!name.m_impl);
}

}

int main() {
    TestFields();
    TestInterning();
    TestEmptyNamespace();
    TestDestroyedEntryIsReplaced();
    TestBaseObjectEntryPoints();
    TestNullArguments();
    return 0;
}