/* explicit_bzero, getprogname, getuid, vasprintf and strndup are written for AnyPS5; they are not copied or derived from OpenBSD or LibreSSL. */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

void explicit_bzero(void *s, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)s;
    while (n--) *p++ = 0;
}

const char *getprogname(void) { return "libSceLibreSsl"; }
unsigned int getuid(void) { return 0; }

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
