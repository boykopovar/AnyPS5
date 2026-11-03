#include "strcodec.h"
#include "JXRGlue.h"
#include <stdint.h>
#include <string.h>

void JxrMetadataRelease_nid_no_patch(DPKPROPVARIANT* value);

_Static_assert(sizeof(DPKVARTYPE) == 4 && sizeof(DPKPROPVARIANT) == 16, "metadata layout");
_Static_assert(offsetof(DPKPROPVARIANT, VT) == 8 && sizeof(((DPKPROPVARIANT*)0)->VT) == 8, "metadata payload");

void JxrMetadataCleanup_nid_no_patch(int32_t type, void* payload) {
    DPKPROPVARIANT value;
    value.vt = (DPKVARTYPE)type;
    memcpy(&value.VT, payload, sizeof(value.VT));
    JxrMetadataRelease_nid_no_patch(&value);
    memcpy(payload, &value.VT, sizeof(value.VT));
}
