#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    if (size && nmemb > (size_t)-1 / size) return NULL;
    return realloc(ptr, nmemb * size);
}

size_t strlcpy(char *dst, const char *src, size_t siz) {
    size_t len = strlen(src);
    if (siz) {
        size_t copy = len >= siz ? siz - 1 : len;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return len;
}

size_t strlcat(char *dst, const char *src, size_t siz) {
    size_t dlen = strnlen(dst, siz);
    size_t slen = strlen(src);
    if (dlen == siz) return siz + slen;
    if (slen > siz - dlen - 1) slen = siz - dlen - 1;
    memcpy(dst + dlen, src, slen);
    dst[dlen + slen] = '\0';
    return dlen + slen;
}

void explicit_bzero(void *s, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)s;
    while (n--) *p++ = 0;
}

const char *getprogname(void) { return "libSceLibreSsl"; }
unsigned int getuid(void) { return 0; }

void *recallocarray(void *ptr, size_t oldnmemb, size_t nmemb, size_t size) {
    if ((size && (nmemb > (size_t)-1 / size || oldnmemb > (size_t)-1 / size)) ||
        (!size && (nmemb || oldnmemb)))
        return NULL;
    size_t oldtotal = oldnmemb * size, total = nmemb * size;
    void *p = realloc(ptr, total);
    if (p && total > oldtotal)
        memset((char *)p + oldtotal, 0, total - oldtotal);
    return p;
}

void freezero(void *s, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)s;
    while (n--) *p++ = 0;
    free(s);
}

int vasprintf(char **strp, const char *fmt, va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n < 0) return -1;
    char *buf = malloc((size_t)n + 1);
    if (!buf) return -1;
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    *strp = buf;
    return n;
}

char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    char *p = malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}
