#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace {

std::string events;
int poolDepth = 0;
int pools = 0;
char processInfoObject = 0;
char activityObject = 0;
char processInfoClass = 0;
char stringClass = 0;

void Record(const std::string& event) {
    if (!events.empty()) events += ",";
    events += event;
}

}

extern "C" {

void* objc_getClass(const char* name) {
#ifdef FIXTURE_NO_CLASS
    static_cast<void>(name);
    return nullptr;
#else
    if (std::strcmp(name, "NSProcessInfo") == 0) return &processInfoClass;
    if (std::strcmp(name, "NSString") == 0) return &stringClass;
    return nullptr;
#endif
}

void* sel_registerName(const char* name) {
    static std::map<std::string, std::string> interned;
    return const_cast<char*>(interned.emplace(name, name).first->second.c_str());
}

#ifndef FIXTURE_OMIT_POOL
void* objc_autoreleasePoolPush() {
    ++poolDepth;
    ++pools;
    return &poolDepth;
}

void objc_autoreleasePoolPop(void* pool) {
    if (pool != &poolDepth) Record("bad-pool");
    --poolDepth;
}
#endif

void* objc_msgSend(void* self, void* selector, void* first, void* second) {
    const std::string name = static_cast<const char*>(selector);
    if (name == "processInfo") return self == &processInfoClass ? &processInfoObject : nullptr;
    if (name == "stringWithUTF8String:") return self == &stringClass ? first : nullptr;
    if (name == "beginActivityWithOptions:reason:") {
        char text[32];
        std::snprintf(text, sizeof(text), "begin:%llx:", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(first)));
        Record(text + std::string(static_cast<const char*>(second)));
#ifdef FIXTURE_NIL_ACTIVITY
        return nullptr;
#else
        return &activityObject;
#endif
    }
    if (name == "retain") {
        Record("retain");
        return self;
    }
    if (name == "release") {
        Record("release");
        return nullptr;
    }
    if (name == "endActivity:") {
        Record(self == &processInfoObject && first == &activityObject ? "end" : "end-wrong-target");
        return nullptr;
    }
    Record("unexpected:" + name);
    return nullptr;
}

const char* fixture_events() {
    return events.c_str();
}

int fixture_pool_depth() {
    return poolDepth;
}

int fixture_pools() {
    return pools;
}

}
