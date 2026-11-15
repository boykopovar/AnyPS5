#include "prx/libSceJsc/libSceJsc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace {

using Jsc::JscAbi::ClassInfoPrefix;
using Jsc::JscAbi::HashTablePrefix;
using Query = bool (APS5_VABI*)(const ClassInfoPrefix*);
static_assert(std::is_same_v<decltype(&_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv), Query>);

struct alignas(8) Metadata {
    std::array<unsigned char, 64> bytes;

    Metadata() { bytes.fill(0xA5); }
    template<typename T> void Write(std::size_t offset, T value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }
    const ClassInfoPrefix* Info() const { return reinterpret_cast<const ClassInfoPrefix*>(bytes.data()); }
    const HashTablePrefix* Table() const { return reinterpret_cast<const HashTablePrefix*>(bytes.data()); }
};

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ClassInfo check failed at line %d\n", line);
        std::abort();
    }
}

void TestChain(const std::array<int, 6>& tableKinds, bool expected) {
    std::array<Metadata, 6> infos;
    std::array<Metadata, 6> tables;
    for (std::size_t i = 0; i < infos.size(); ++i) {
        infos[i].Write(8, i + 1 < infos.size() ? infos[i + 1].Info() : nullptr);
        infos[i].Write(16, tableKinds[i] < 0 ? nullptr : tables[i].Table());
        tables[i].Write(8, tableKinds[i] == 1);
    }
    const auto oldInfos = infos;
    const auto oldTables = tables;
    Check(_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(infos[0].Info()) == expected, __LINE__);
    Check(std::memcmp(infos.data(), oldInfos.data(), sizeof(infos)) == 0, __LINE__);
    Check(std::memcmp(tables.data(), oldTables.data(), sizeof(tables)) == 0, __LINE__);
}

} // namespace

int main() {
    TestChain({-1, -1, -1, -1, -1, -1}, false);
    TestChain({0, -1, -1, -1, -1, -1}, false);
    TestChain({0, 0, 0, 0, 0, 0}, false);
    TestChain({1, -1, -1, -1, -1, -1}, true);
    TestChain({0, -1, 0, -1, 0, 1}, true);
    TestChain({-1, 0, -1, 0, 1, -1}, true);

    Metadata info;
    info.Write(8, static_cast<const ClassInfoPrefix*>(nullptr));
    info.Write(16, static_cast<const HashTablePrefix*>(nullptr));
    const auto oldInfo = info;
    Check(!_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(info.Info()), __LINE__);
    Check(info.bytes == oldInfo.bytes, __LINE__);
}
