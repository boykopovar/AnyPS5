#include "prx/libSceBemp2sys/libSceBemp2sys.hpp"

int main() {
    for (int iter = 0; iter < 3; ++iter) {
        bemp2sys_psdecoder_release_nid_postfix();
        bemp2sys_tsdecoder_release_nid_postfix();
    }
    for (int iter = 0; iter < 3; ++iter) {
        bemp2sys_psdecoder_release_nid_postfix();
        bemp2sys_tsdecoder_release_nid_postfix();
    }
}
