#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

[[noreturn]] void abort_nid_postfix(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
 (void)arg0;
 (void)arg1;
 (void)arg2;
 (void)arg3;
 (void)arg4;
 (void)arg5;
 __builtin_unreachable();
}

int atexit_nid_postfix(atexit_func_t func) {
 (void)func;
 return 0;
}

int atoi_nid_postfix(const char* str) {
 (void)str;
 return 0;
}

double cos_nid_postfix(double x) {
 (void)x;
 return 0;
}

float cosf_nid_postfix(float x) {
 (void)x;
 return 0;
}

void Exit_nid_postfix(int code) {
 (void)code;
}

void exit_nid_postfix(int code) {
 (void)code;
}

int fflush_nid_postfix(FILE* stream) {
 (void)stream;
 return 0;
}

void init_env_nid_postfix(const InitEnvParams* params) {
 (void)params;
}

int memcmp_nid_postfix(const void* s1, const void* s2, size_t n) {
 (void)s1;
 (void)s2;
 (void)n;
 return 0;
}

void* memcpy_nid_postfix(void* dest, const void* src, size_t n) {
 (void)dest;
 (void)src;
 (void)n;
 return nullptr;
}

void* memmove_nid_postfix(void* dest, const void* src, size_t n) {
 (void)dest;
 (void)src;
 (void)n;
 return nullptr;
}

void* memset_nid_postfix(void* s, int c, size_t n) {
 (void)s;
 (void)c;
 (void)n;
 return nullptr;
}

int puts_nid_postfix(const char* s) {
 (void)s;
 return 0;
}

void sceLibcHeapGetTraceInfo_nid_postfix(Info* info) {
 (void)info;
}

int setenv_nid_postfix(const char* name, const char* value, int overwrite) {
 (void)name;
 (void)value;
 (void)overwrite;
 return 0;
}

double Sin_nid_postfix(double x) {
 (void)x;
 return 0;
}

double sin_nid_postfix(double x) {
 (void)x;
 return 0;
}

void Sincos_nid_postfix(double x, double* sinp, double* cosp) {
 (void)x;
 (void)sinp;
 (void)cosp;
}

void sincos_nid_postfix(double x, double* sinp, double* cosp) {
 (void)x;
 (void)sinp;
 (void)cosp;
}

void sincosf_nid_postfix(float x, float* sinp, float* cosp) {
 (void)x;
 (void)sinp;
 (void)cosp;
}

float sinf_nid_postfix(float x) {
 (void)x;
 return 0;
}

int snprintf_nid_postfix(VA_ARGS) {
 return 0;
}

char* strcat_nid_postfix(char* dest, const char* src) {
 (void)dest;
 (void)src;
 return nullptr;
}

const char* strchr_nid_postfix(const char* s, int c) {
 (void)s;
 (void)c;
 return nullptr;
}

int strcmp_nid_postfix(const char* s1, const char* s2) {
 (void)s1;
 (void)s2;
 return 0;
}

char* strcpy_nid_postfix(char* dest, const char* src) {
 (void)dest;
 (void)src;
 return nullptr;
}

size_t strlen_nid_postfix(const char* s) {
 (void)s;
 return 0;
}

int strncmp_nid_postfix(const char* s1, const char* s2, size_t n) {
 (void)s1;
 (void)s2;
 (void)n;
 return 0;
}

char* strncpy_nid_postfix(char* dest, const char* src, size_t count) {
 (void)dest;
 (void)src;
 (void)count;
 return nullptr;
}

char* strrchr_nid_postfix(const char* s, int c) {
 (void)s;
 (void)c;
 return nullptr;
}

char* strstr_nid_postfix(const char* haystack, const char* needle) {
 (void)haystack;
 (void)needle;
 return nullptr;
}

long strtol_nid_postfix(const char* str, char** endptr, int base) {
 (void)str;
 (void)endptr;
 (void)base;
 return 0;
}

unsigned long strtoul_nid_postfix(const char* str, char** endptr, int base) {
 (void)str;
 (void)endptr;
 (void)base;
 return 0;
}

int vprintf_nid_postfix(const char* str, VaList* c) {
 (void)str;
 (void)c;
 return 0;
}

}
