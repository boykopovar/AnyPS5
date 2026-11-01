#include <cstdlib>
#include <cstdint>

extern "C" int sceFiosOverlayGet(int handle, void *buf, std::size_t size);
extern "C" int sceFiosOverlayResolveSync(int handle, void *buf, std::size_t size);

void TestGuestFios2OverlayBounds() {
    int invalidHandle = -1;
    sceFiosOverlayGet(invalidHandle, nullptr, 0);
    sceFiosOverlayResolveSync(invalidHandle, nullptr, 0);
}
