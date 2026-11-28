#pragma once

namespace IcuDataMemoryOwnership {

struct TrustedRecord {
    const void* pHeader;
    void* mapAddr;
    void* map;
};

void MarkFreed(void* pointer);
bool IsFreed(void* pointer);
void Register(void* pointer, const void* pHeader, void* mapAddr, void* map);
bool TryTake(void* pointer, TrustedRecord& out);

}
