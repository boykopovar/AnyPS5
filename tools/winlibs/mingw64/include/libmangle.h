/*
   Copyright (c) 2009-2016  mingw-w64 project

   Contributing authors: Kai Tietz, Jonathan Yong

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#ifndef _LIBMANGLE_HXX
#define _LIBMANGLE_HXX

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Garbage collector elements.
 * Tracks allocated memory and points to the next element from the same context.
 * Opaque structure.
 * @see libmangle_gc_context_t
 */
typedef void *libmangle_gc_t;

/**
 * Garbage collector context.
 * Tracks first and last of elements in gc context.
 * @see generate_gc()
 * @see release_gc()
 */
typedef struct libmangle_gc_context_t {
  libmangle_gc_t head;                /**< Pointer to first gc element in context.*/
  libmangle_gc_t tail;                /**< Pointer to last gc element in context. */
} libmangle_gc_context_t;

/**
 * Generic token instances.
 * Type of token determined by base descriptor in members.
 * Opaque structure.
 * @see gen_tok()
 */
typedef void *libmangle_tokens_t;

/**
 * Releases memory tracked by context.
 * @param[in] gc Garbage collection context to work on.
 * @see libmangle_generate_gc()
 */
void libmangle_release_gc (libmangle_gc_context_t *gc);

/**
 * Constructs a garbage collection context token.
 * @return Pointer to context.
 * @see libmangle_release_gc()
 */
libmangle_gc_context_t *libmangle_generate_gc (void);

/**
 * Dumps pMToken to a file descriptor for debugging.
 * @param[in] fp File descriptor to print the token to.
 * @param[in] p libmangle_tokens_t chain to print.
 */
void libmangle_dump_tok (FILE *fp, libmangle_tokens_t p);

/** 
 * Prints C++ name to file descriptor.
 * @param[in] fp Output file descriptor.
 * @param[in] p Token containing information about the C++ name.
 * @see libmangle_decode_ms_name()
 */
void libmangle_print_decl (FILE *fp, libmangle_tokens_t p);

/** 
 * Get pointer to decoded C++ name string.
 * Use free() to release returned string.
 * @param[in] r C++ name token.
 * @return pointer to decoded C++ name string.
 * @see libmangle_decode_ms_name()
 */
char *libmangle_sprint_decl (libmangle_tokens_t r);

/**
 * Decodes an MSVC export name.
 * @param[in] gc libmangle_gc_context_t pointer for collecting memory allocations.
 * @param[in] name MSVC C++ mangled export string.
 * @see libmangle_sprint_decl()
 * @see libmangle_release_gc()
 * @see libmangle_tokens_t
 * @return Token containing information about the mangled string,
 * use libmangle_release_gc() to free after use.
 */
libmangle_tokens_t libmangle_decode_ms_name (libmangle_gc_context_t *gc, const char *name);
char *libmangle_encode_ms_name (libmangle_gc_context_t *gc, libmangle_tokens_t tok);

#ifdef __cplusplus
}
#endif

#endif
