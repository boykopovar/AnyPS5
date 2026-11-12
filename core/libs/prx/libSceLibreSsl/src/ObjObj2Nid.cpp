#include "prx/libSceLibreSsl/include/ObjectTable.hpp"

extern "C" {

int APS5_VABI OBJ_obj2nid_nid_postfix(const LibreSslAsn1Object* object) {
    return LibreSslObjectNid_nid_no_patch(object);
}

}
