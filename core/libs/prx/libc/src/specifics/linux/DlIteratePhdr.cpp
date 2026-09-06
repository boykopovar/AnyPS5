#include <link.h>
#include <cstddef>

extern "C" {

int dl_iterate_phdr_nid_postfix(int (*callback)(dl_phdr_info*, std::size_t, void*), void* data) {
    return dl_iterate_phdr(callback, data);
}

}
