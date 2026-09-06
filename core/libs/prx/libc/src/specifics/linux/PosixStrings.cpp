#include <strings.h>
#include <cstdlib>
#include <cstring>

extern "C" {

int strcasecmp_nid_postfix(const char* s1, const char* s2) {
    return ::strcasecmp(s1, s2);
}

int strncasecmp_nid_postfix(const char* s1, const char* s2, size_t n) {
    return ::strncasecmp(s1, s2, n);
}

char* strdup_nid_postfix(const char* s) {
    return ::strdup(s);
}

}
