#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

[[noreturn]] void abort(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
 (void)arg0;
 (void)arg1;
 (void)arg2;
 (void)arg3;
 (void)arg4;
 (void)arg5;
 __builtin_unreachable();
}

int atexit(atexit_func_t func) {
 (void)func;
 return 0;
}

int atoi(const char* str) {
 (void)str;
 return 0;
}

double cos(double x) {
 (void)x;
 return 0;
}

float cosf(float x) {
 (void)x;
 return 0;
}

void Exit(int code) {
 (void)code;
}

void exit(int code) {
 (void)code;
}

int fflush(FILE* stream) {
 (void)stream;
 return 0;
}

void init_env(const InitEnvParams* params) {
 (void)params;
}

int memcmp(const void* s1, const void* s2, size_t n) {
 (void)s1;
 (void)s2;
 (void)n;
 return 0;
}

void* memcpy(void* dest, const void* src, size_t n) {
 (void)dest;
 (void)src;
 (void)n;
 return nullptr;
}

void* memmove(void* dest, const void* src, size_t n) {
 (void)dest;
 (void)src;
 (void)n;
 return nullptr;
}

void* memset(void* s, int c, size_t n) {
 (void)s;
 (void)c;
 (void)n;
 return nullptr;
}

int puts(const char* s) {
 (void)s;
 return 0;
}

void sceLibcHeapGetTraceInfo(Info* info) {
 (void)info;
}

int setenv(const char* name, const char* value, int overwrite) {
 (void)name;
 (void)value;
 (void)overwrite;
 return 0;
}

double Sin(double x) {
 (void)x;
 return 0;
}

double sin(double x) {
 (void)x;
 return 0;
}

void Sincos(double x, double* sinp, double* cosp) {
 (void)x;
 (void)sinp;
 (void)cosp;
}

void sincos(double x, double* sinp, double* cosp) {
 (void)x;
 (void)sinp;
 (void)cosp;
}

void sincosf(float x, float* sinp, float* cosp) {
 (void)x;
 (void)sinp;
 (void)cosp;
}

float sinf(float x) {
 (void)x;
 return 0;
}

int snprintf(VA_ARGS) {
 return 0;
}

char* strcat(char* dest, const char* src) {
 (void)dest;
 (void)src;
 return nullptr;
}

const char* strchr(const char* s, int c) {
 (void)s;
 (void)c;
 return nullptr;
}

int strcmp(const char* s1, const char* s2) {
 (void)s1;
 (void)s2;
 return 0;
}

char* strcpy(char* dest, const char* src) {
 (void)dest;
 (void)src;
 return nullptr;
}

size_t strlen(const char* s) {
 (void)s;
 return 0;
}

int strncmp(const char* s1, const char* s2, size_t n) {
 (void)s1;
 (void)s2;
 (void)n;
 return 0;
}

char* strncpy(char* dest, const char* src, size_t count) {
 (void)dest;
 (void)src;
 (void)count;
 return nullptr;
}

char* strrchr(const char* s, int c) {
 (void)s;
 (void)c;
 return nullptr;
}

char* strstr(const char* haystack, const char* needle) {
 (void)haystack;
 (void)needle;
 return nullptr;
}

long strtol(const char* str, char** endptr, int base) {
 (void)str;
 (void)endptr;
 (void)base;
 return 0;
}

unsigned long strtoul(const char* str, char** endptr, int base) {
 (void)str;
 (void)endptr;
 (void)base;
 return 0;
}

int vprintf(const char* str, VaList* c) {
 (void)str;
 (void)c;
 return 0;
}

}
