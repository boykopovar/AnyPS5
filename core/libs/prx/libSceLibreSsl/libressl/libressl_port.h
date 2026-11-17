#ifndef LIBRESSL_PORT_H
#define LIBRESSL_PORT_H
#include <stddef.h>
#include <stdarg.h>
#ifndef __BEGIN_HIDDEN_DECLS
#define __BEGIN_HIDDEN_DECLS
#define __END_HIDDEN_DECLS
#endif
#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#ifndef __DECONST
#define __DECONST(type, var) ((type)(unsigned long)(var))
#endif
#ifndef __arraycount
#define __arraycount(a) (sizeof(a) / sizeof((a)[0]))
#endif
void *reallocarray(void *ptr, size_t nmemb, size_t size);
void *recallocarray(void *ptr, size_t oldnmemb, size_t nmemb, size_t size);
size_t strlcpy(char *dst, const char *src, size_t siz);
size_t strlcat(char *dst, const char *src, size_t siz);
void explicit_bzero(void *s, size_t n);
void freezero(void *s, size_t n);
int vasprintf(char **strp, const char *fmt, va_list ap);
char *strndup(const char *s, size_t n);
const char *getprogname(void);
unsigned int getuid(void);
#endif
