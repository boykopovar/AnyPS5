#ifndef LIBRESSL_SYSLOG_SHIM_H
#define LIBRESSL_SYSLOG_SHIM_H
#include <stdarg.h>
#define LOG_INFO 6
#define LOG_LOCAL2 (1<<13)
#define LOG_PRIMASK 0x07
struct syslog_data { int unused; };
#define SYSLOG_DATA_INIT { 0 }
static inline void vsyslog_r(int pri, struct syslog_data *data, const char *fmt, va_list ap) {
    (void)pri; (void)data;
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}
#endif
