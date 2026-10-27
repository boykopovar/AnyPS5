#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static const Value& Front(const Value& value) {
    return *_ZNK3sce4Json5Array5frontEv(_ZNK3sce4Json5Value8getArrayEv(&value));
}
static void Tree(const Value& root) {
    Require(root.type == 6 && !root.parent, "A copied root must own an independent root value");
    const auto& child = Front(root);
    Require(child.parent == &root && child.type == 6, "An array element must refer to its owning value");
    const auto& leaf = Front(child);
    Require(leaf.parent == &child && leaf.type == 2 && leaf.payload == 42,
            "Nested children must refer to their immediate owner and preserve inline payloads");
}
int main() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize ownership tests");
    Value text{};
    _ZN3sce4Json5ValueC1EPKc(&text, "keep");
    const auto payload = text.payload;
    const auto* string = _ZNK3sce4Json5Value9getStringEv(&text);
    Require(reinterpret_cast<std::uintptr_t>(string) == payload, "A string payload must hold its String object address");
    _ZN3sce4Json5Value3setENS0_9ValueTypeE(&text, static_cast<ValueType>(5));
    Require(text.payload == payload && std::strcmp(_ZNK3sce4Json6String5c_strEv(string), "keep") == 0,
            "Setting an existing type must preserve its contents and identity");
    Value leaf{};
    _ZN3sce4Json5ValueC1El(&leaf, 42);
    Array inner{};
    _ZN3sce4Json5ArrayC1Ev(&inner);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&inner, &leaf);
    Require(!_ZNK3sce4Json5Array5frontEv(&inner)->parent, "A standalone array must not invent a value owner");
    Value child{};
    _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&child, &inner);
    Array outer{};
    _ZN3sce4Json5ArrayC1Ev(&outer);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&outer, &child);
    Value root{};
    _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&root, &outer);
    Tree(root);
    Require(reinterpret_cast<std::uintptr_t>(_ZNK3sce4Json5Value8getArrayEv(&root)) == root.payload,
            "An array payload must hold its Array object address");
    Value copy{};
    _ZN3sce4Json5ValueC1ERKS1_(&copy, &root);
    Tree(copy);
    Require(copy.payload != root.payload && Front(copy).payload != Front(root).payload,
            "Copies must not share owned containers");
    const auto rootPayload = root.payload;
    _ZN3sce4Json5Value3setENS0_9ValueTypeE(&root, static_cast<ValueType>(6));
    Require(root.payload == rootPayload && _ZNK3sce4Json5Array4sizeEv(_ZNK3sce4Json5Value8getArrayEv(&root)) == 1,
            "Setting the existing array type must retain elements and storage");
    auto* ownedArray = const_cast<Array*>(_ZNK3sce4Json5Value8getArrayEv(&copy));
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(ownedArray, &child);
    const auto* appended = _ZNK3sce4Json5Array4backEv(ownedArray);
    Require(appended->parent == &copy && Front(*appended).parent == appended,
            "Appending to an owned array must bind both new and nested children");
    _ZN3sce4Json5Value3setERKS1_(&copy, &Front(copy));
    Require(!copy.parent && Front(copy).parent == &copy && Front(copy).payload == 42,
            "Assignment from a descendant must finish copying before releasing the old tree");
    Value cross{};
    _ZN3sce4Json5ValueC1Ev(&cross);
    const char objectInput[] = "{\"a\":[1,2]}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&cross, objectInput, sizeof(objectInput) - 1) == 0, "Parse cross-type ownership tree");
    const auto* arrayMember = _ZNK3sce4Json5ValueixEPKc(&cross, "a");
    _ZN3sce4Json5ValueaSERKS1_(&cross, arrayMember);
    Require(cross.type == 6 && Front(cross).payload == 1 && Front(cross).parent == &cross,
            "Object-to-array descendant assignment must preserve the source before clearing its owner");
    _ZN3sce4Json5Value5clearEv(&cross);
    const char arrayInput[] = "[\"retained\"]";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&cross, arrayInput, sizeof(arrayInput) - 1) == 0, "Parse string descendant tree");
    _ZN3sce4Json5Value3setERKS1_(&cross, &Front(cross));
    Require(cross.type == 5 && std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&cross)), "retained") == 0,
            "Array-to-string descendant assignment must preserve text before clearing its owner");
    _ZN3sce4Json5Value5clearEv(&cross);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&cross, objectInput, sizeof(objectInput) - 1) == 0, "Parse raw Array descendant source");
    _ZN3sce4Json5Value3setERKNS0_5ArrayE(&cross, _ZNK3sce4Json5Value8getArrayEv(_ZNK3sce4Json5ValueixEPKc(&cross, "a")));
    Require(cross.type == TypeArray && Front(cross).payload == 1, "Array overload must retain a descendant before changing the destination type");
    _ZN3sce4Json5Value5clearEv(&cross);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&cross, arrayInput, sizeof(arrayInput) - 1) == 0, "Parse raw String descendant source");
    _ZN3sce4Json5Value3setERKNS0_6StringE(&cross, _ZNK3sce4Json5Value9getStringEv(&Front(cross)));
    Require(cross.type == TypeString && std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&cross)), "retained") == 0,
            "String overload must retain descendant text before changing the destination type");
    _ZN3sce4Json5Value5clearEv(&cross);
    const char nestedObject[] = "[{\"a\":7}]";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&cross, nestedObject, sizeof(nestedObject) - 1) == 0, "Parse raw Object descendant source");
    _ZN3sce4Json5Value3setERKNS0_6ObjectE(&cross, _ZNK3sce4Json5Value9getObjectEv(&Front(cross)));
    const auto* member = _ZNK3sce4Json5ValueixEPKc(&cross, "a");
    Require(cross.type == TypeObject && member->payload == 7 && member->parent == &cross,
            "Object overload must retain descendant contents and bind their new owner");
    for (bool nested : {false, true}) {
        _ZN3sce4Json5Value5clearEv(&cross);
        const char* document = nested ? "[{\"retained-key\":7}]" : "{\"retained-key\":7}";
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&cross, document, std::strlen(document)) == 0, "Parse object-key ownership tree");
        const auto* object = _ZNK3sce4Json5Value9getObjectEv(nested ? &Front(cross) : &cross);
        ObjectIterator iterator{};
        _ZNK3sce4Json6Object5beginEv(&iterator, object);
        const auto* pair = _ZNK3sce4Json6Object8iteratordeEv(&iterator);
        _ZN3sce4Json5Value3setERKNS0_6StringE(&cross, &pair->key);
        Require(cross.type == TypeString && std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&cross)), "retained-key") == 0,
                "String assignment from an owned object key must copy text before releasing its tree");
        _ZN3sce4Json6Object8iteratorD1Ev(&iterator);
    }
    _ZN3sce4Json5ValueD1Ev(&cross);
    _ZN3sce4Json5Value4swapERS1_(&root, &text);
    Require(root.type == 5 && text.type == 6, "Swap must exchange payloads");
    Tree(text);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&root)), "keep") == 0,
            "Swap must preserve string contents");
    _ZN3sce4Json5ValueD1Ev(&root);
    _ZN3sce4Json5ValueD1Ev(&copy);
    _ZN3sce4Json5ValueD1Ev(&child);
    _ZN3sce4Json5ValueD1Ev(&leaf);
    _ZN3sce4Json5ValueD1Ev(&text);
    _ZN3sce4Json5ArrayD1Ev(&inner);
    _ZN3sce4Json5ArrayD1Ev(&outer);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate ownership tests");
    return 0;
}
