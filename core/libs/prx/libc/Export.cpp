#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include "SceTypes.hpp"

extern "C" {

void exit_nid_postfix(int code) {
 (void)code;
}

[[noreturn]] void abort_nid_postfix(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
 (void)arg0;
 (void)arg1;
 (void)arg2;
 (void)arg3;
 (void)arg4;
 (void)arg5;
 __builtin_unreachable();
}

int* libc_error_nid_postfix() {
 return nullptr;
}

void init_env_nid_postfix(const InitEnvParams* params) {
 (void)params;
}

int atexit_nid_postfix(atexit_func_t func) {
 (void)func;
 return 0;
}

int libc_printf_nid_postfix(
 uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx,
 uint64_t r8, uint64_t r9, uint64_t overflow_arg_area,
 __m128 xmm0, __m128 xmm1, __m128 xmm2, __m128 xmm3,
 __m128 xmm4, __m128 xmm5, __m128 xmm6, __m128 xmm7, ...) {
 (void)rdi; (void)rsi; (void)rdx; (void)rcx;
 (void)r8; (void)r9; (void)overflow_arg_area;
 (void)xmm0; (void)xmm1; (void)xmm2; (void)xmm3;
 (void)xmm4; (void)xmm5; (void)xmm6; (void)xmm7;
 return 0;
}

int puts_nid_postfix(const char* s) {
 (void)s;
 return 0;
}

int setenv_nid_postfix(const char* name, const char* value, int overwrite) {
 (void)name;
 (void)value;
 (void)overwrite;
 return 0;
}

int64_t libc_time_nid_postfix(int64_t* timer) {
 (void)timer;
 return 0;
}

double libc_difftime_nid_postfix(int64_t time1, int64_t time0) {
 (void)time1;
 (void)time0;
 return 0.0;
}

std::tm* libc_gmtime_nid_postfix(const int64_t* timer) {
 (void)timer;
 return nullptr;
}

std::tm* libc_localtime_nid_postfix(const int64_t* timer) {
 (void)timer;
 return nullptr;
}

int64_t libc_mktime_nid_postfix(std::tm* timeptr) {
 (void)timeptr;
 return -1;
}

size_t libc_strftime_nid_postfix(char* str, size_t count, const char* format, const std::tm* timeptr) {
 (void)str;
 (void)count;
 (void)format;
 (void)timeptr;
 return 0;
}

void catchReturnFromMain_nid_postfix(int status) {
 (void)status;
}

int cxa_atexit_nid_postfix(void (*func)(void*), void* arg, void* d) {
 (void)func;
 (void)arg;
 (void)d;
 return 0;
}

void cxa_finalize_nid_postfix(void* d) {
 (void)d;
}

int std_execute_once_nid_postfix(int* flag, int (*func)(void*, void*, void**), void* arg) {
 (void)flag;
 (void)func;
 (void)arg;
 return 0;
}

int vprintf_nid_postfix(const char* str, VaList* c) {
 (void)str;
 (void)c;
 return 0;
}

int fflush_nid_postfix(FILE* stream) {
 (void)stream;
 return 0;
}

void* memset_nid_postfix(void* s, int c, size_t n) {
 (void)s;
 (void)c;
 (void)n;
 return nullptr;
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

int memcmp_nid_postfix(const void* s1, const void* s2, size_t n) {
 (void)s1;
 (void)s2;
 (void)n;
 return 0;
}

int strcmp_nid_postfix(const char* s1, const char* s2) {
 (void)s1;
 (void)s2;
 return 0;
}

int strncmp_nid_postfix(const char* s1, const char* s2, size_t n) {
 (void)s1;
 (void)s2;
 (void)n;
 return 0;
}

size_t strlen_nid_postfix(const char* s) {
 (void)s;
 return 0;
}

char* strcpy_nid_postfix(char* dest, const char* src) {
 (void)dest;
 (void)src;
 return nullptr;
}

char* strncpy_nid_postfix(char* dest, const char* src, size_t count) {
 (void)dest;
 (void)src;
 (void)count;
 return nullptr;
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

int atoi_nid_postfix(const char* str) {
 (void)str;
 return 0;
}

float sinf_nid_postfix(float x) {
 (void)x;
 return 0.0f;
}

float cosf_nid_postfix(float x) {
 (void)x;
 return 0.0f;
}

void sincosf_nid_postfix(float x, float* sinp, float* cosp) {
 (void)x;
 (void)sinp;
 (void)cosp;
}

double sin_nid_postfix(double x) {
 (void)x;
 return 0.0;
}

double cos_nid_postfix(double x) {
 (void)x;
 return 0.0;
}

void sincos_nid_postfix(double x, double* sinp, double* cosp) {
 (void)x;
 (void)sinp;
 (void)cosp;
}

int snprintf_nid_postfix(
 uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx,
 uint64_t r8, uint64_t r9, uint64_t overflow_arg_area,
 __m128 xmm0, __m128 xmm1, __m128 xmm2, __m128 xmm3,
 __m128 xmm4, __m128 xmm5, __m128 xmm6, __m128 xmm7, ...) {
 (void)rdi; (void)rsi; (void)rdx; (void)rcx;
 (void)r8; (void)r9; (void)overflow_arg_area;
 (void)xmm0; (void)xmm1; (void)xmm2; (void)xmm3;
 (void)xmm4; (void)xmm5; (void)xmm6; (void)xmm7;
 return 0;
}

void LibcHeapGetTraceInfo_nid_postfix(LibcHeapInfo* info) {
 (void)info;
}

int LibcInternalExtCxaThreadAtexit_nid_postfix(void (*destructor)(void*), void* object, void* module_id) {
 (void)destructor;
 (void)object;
 (void)module_id;
 return 0;
}

int LibcHeapErrorReportForGame_nid_postfix(uint64_t msp, uint64_t ptr, uint64_t error, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
 (void)msp;
 (void)ptr;
 (void)error;
 (void)arg3;
 (void)arg4;
 (void)arg5;
 return 0;
}

}
