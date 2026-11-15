// Derived from src/fcformat.c (Fontconfig 72b9a48f57de6204d99ce1c217b5609ee92ece9b)
/*
 * Copyright © 2008,2009 Red Hat, Inc.
 *
 * Red Hat Author(s): Behdad Esfahbod
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * fontconfig/src/fcname.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * fontconfig/src/fcstr.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * fontconfig/src/fccharset.c
 *
 * Copyright © 2001 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * fontconfig/src/fclang.c
 *
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "prx/libfontconfig/include/PatternFormat.hpp"
#include "prx/libfontconfig/include/Sets.hpp"
#include "prx/libfontconfig/include/FormatAccess.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include "FontconfigData.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace {

constexpr FcBool FcTrue = 1;
constexpr FcBool FcFalse = 0;
using PatternOwner = std::unique_ptr<FcPattern, decltype([](FcPattern* p) { FcPatternDestroy(p); })>;

struct FcStrBuf {
    std::vector<FcChar8> storage;
    FcChar8* buf = nullptr;
    int len = 0;
    bool failed = false;
};

void FcStrBufInit(FcStrBuf*, FcChar8*, int) {}
void FcStrBufDestroy(FcStrBuf*) {}

FcBool FcStrBufData(FcStrBuf* buf, const FcChar8* data, int length) {
    if (length < 0 || length > std::numeric_limits<int>::max() - buf->len) {
        buf->failed = true;
        return FcFalse;
    }
    try {
        buf->storage.resize(buf->len + length);
    } catch (const std::bad_alloc&) {
        buf->failed = true;
        return FcFalse;
    }
    if (length) std::memcpy(buf->storage.data() + buf->len, data, length);
    buf->len += length;
    buf->buf = buf->storage.data();
    return FcTrue;
}

FcBool FcStrBufChar(FcStrBuf* buf, FcChar8 value) { return FcStrBufData(buf, &value, 1); }
FcBool FcStrBufString(FcStrBuf* buf, const FcChar8* text) {
    return FcStrBufData(buf, text, std::strlen(reinterpret_cast<const char*>(text)));
}

FcChar8* FcStrBufDone(FcStrBuf* buf) {
    if (buf->failed) return nullptr;
    auto* result = static_cast<FcChar8*>(malloc_nid_postfix(buf->len + 1));
    if (result) {
        if (buf->len) std::memcpy(result, buf->buf, buf->len);
        result[buf->len] = 0;
    }
    return result;
}

void FormatFree(FcChar8* text) { free_nid_postfix(text); }
FcChar8* CopyText(const std::string& text) {
    auto* result = static_cast<FcChar8*>(malloc_nid_postfix(text.size() + 1));
    if (result) std::memcpy(result, text.c_str(), text.size() + 1);
    return result;
}
FcChar8* FormatDowncase(const FcChar8* text) {
    return CopyText(FontconfigDowncase_nid_no_patch(reinterpret_cast<const char*>(text)));
}
const char* LastSlash(const FcChar8* text) {
    const auto* value = reinterpret_cast<const char*>(text);
    const auto* slash = std::strrchr(value, '/');
#ifdef _WIN32
    const auto* backslash = std::strrchr(value, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
#endif
    return slash;
}
FcChar8* FormatBasename(const FcChar8* text) {
    const auto* slash = LastSlash(text);
    return CopyText(slash ? slash + 1 : reinterpret_cast<const char*>(text));
}
FcChar8* FormatDirname(const FcChar8* text) {
    const auto* slash = LastSlash(text);
    return CopyText(slash ? std::string(reinterpret_cast<const char*>(text), slash) : ".");
}

FcBool AddFormatValue(FcPattern* pat, const char* name, FcValue value) {
    switch (value.type) {
    case FcTypeInteger: return FcPatternAddInteger(pat, name, value.u.i);
    case FcTypeDouble: return FcPatternAddDouble(pat, name, value.u.d);
    case FcTypeBool: return FcPatternAddBool(pat, name, value.u.b);
    case FcTypeString: return FcPatternAddString(pat, name, value.u.s);
    case FcTypeMatrix: return FcPatternAddMatrix(pat, name, value.u.m);
    case FcTypeCharSet: return FcPatternAddCharSet(pat, name, value.u.c);
    case FcTypeLangSet: return FcPatternAddLangSet(pat, name, value.u.l);
    default: NotImplemented_nid_no_patch(__func__);
    }
}

FcBool UnparseString(FcStrBuf* buf, const FcChar8* text, const char* escape) {
    while (const auto c = *text++) {
        if (escape && std::strchr(escape, c)) FcStrBufChar(buf, escape[0]);
        FcStrBufChar(buf, c);
    }
    return !buf->failed;
}

FcBool UnparseValue(FcStrBuf* buf, FcValue value, const char* escape) {
    char temp[1024];
    switch (value.type) {
    case FcTypeUnknown:
    case FcTypeVoid:
    case FcTypeFTFace: return FcTrue;
    case FcTypeString: return UnparseString(buf, value.u.s, escape);
    case FcTypeInteger: std::snprintf(temp, sizeof(temp), "%d", value.u.i); break;
    case FcTypeDouble: std::snprintf(temp, sizeof(temp), "%g", value.u.d); break;
    case FcTypeBool: std::snprintf(temp, sizeof(temp), "%s", value.u.b == 1 ? "True" : value.u.b == 0 ? "False" : "DontCare"); break;
    case FcTypeMatrix:
        std::snprintf(temp, sizeof(temp), "%g %g %g %g", value.u.m->xx, value.u.m->xy, value.u.m->yx, value.u.m->yy);
        break;
    case FcTypeLangSet: {
        bool first = true;
        for (const auto& lang : FontconfigLanguages_nid_no_patch(value.u.l)) {
            if (!first) FcStrBufChar(buf, '|');
            first = false;
            FcStrBufString(buf, reinterpret_cast<const FcChar8*>(lang.c_str()));
        }
        return !buf->failed;
    }
    case FcTypeCharSet: {
        std::uint32_t first = 0x7fffffff, last = first;
        auto number = [&](std::uint32_t n) {
            char hex[16];
            std::snprintf(hex, sizeof(hex), "%x", n);
            FcStrBufString(buf, reinterpret_cast<const FcChar8*>(hex));
        };
        for (const auto& [page, leaf] : value.u.c->leaves) {
            for (unsigned i = 0; i < 256; ++i) {
                if (!(leaf[i / 32] & (1u << (i % 32)))) continue;
                const auto point = (static_cast<std::uint32_t>(page) << 8) + i;
                if (point != last + 1) {
                    if (last != first) { FcStrBufChar(buf, '-'); number(last); }
                    if (last != 0x7fffffff) FcStrBufChar(buf, ' ');
                    first = point;
                    number(point);
                }
                last = point;
            }
        }
        if (last != first) { FcStrBufChar(buf, '-'); number(last); }
        return !buf->failed;
    }
    default: NotImplemented_nid_no_patch(__func__);
    }
    return UnparseString(buf, reinterpret_cast<const FcChar8*>(temp), nullptr);
}

FcBool UnparseList(FcStrBuf* buf, FcPattern* pat, const char* name, const char* escape) {
    FcValue value{};
    for (int i = 0; FcPatternGet(pat, name, i, &value) == FcResultMatch; ++i) {
        if (i) FcStrBufChar(buf, ',');
        if (!UnparseValue(buf, value, escape)) return FcFalse;
    }
    return !buf->failed;
}

FcChar8* FormatUnparse(FcPattern* pat) {
    FcStrBuf buf;
    UnparseList(&buf, pat, "family", "\\-:,");
    FcStrBuf sizes;
    UnparseList(&sizes, pat, "size", "\\-:,");
    if (sizes.len) { FcStrBufChar(&buf, '-'); FcStrBufData(&buf, sizes.buf, sizes.len); }
    for (const auto& object : fontconfig_data::Objects) {
        if (object.name == "family" || object.name == "size") continue;
        FcValue value{};
        if (FcPatternGet(pat, object.name.data(), 0, &value) != FcResultMatch) continue;
        FcStrBufChar(&buf, ':');
        UnparseString(&buf, reinterpret_cast<const FcChar8*>(object.name.data()), "\\=_:,");
        FcStrBufChar(&buf, '=');
        if (!UnparseList(&buf, pat, object.name.data(), "\\=_:,")) return nullptr;
    }
    return FcStrBufDone(&buf);
}

#define FCCAT_FORMAT	"\"%{file|basename|cescape}\" %{index} \"%{-file{%{=unparse|cescape}}}\""
#define FCMATCH_FORMAT	"%{file:-<unknown filename>|basename}: \"%{family[0]:-<unknown family>}\" \"%{style[0]:-<unknown style>}\""
#define FCLIST_FORMAT	"%{?file{%{file}: }}%{-file{%{=unparse}}}"
#define PKGKIT_FORMAT	"%{[]family{font(%{family|downcase|delete( )})\n}}%{[]lang{font(:lang=%{lang|downcase|translate(_,-)})\n}}"


static void
message (const char *fmt, ...)
{
    va_list	args;
    va_start (args, fmt);
    fprintf (stderr, "Fontconfig: Pattern format error: ");
    vfprintf (stderr, fmt, args);
    fprintf (stderr, ".\n");
    va_end (args);
}


typedef struct _FcFormatContext
{
    const FcChar8 *format_orig;
    const FcChar8 *format;
    int            format_len;
    FcChar8       *word;
    FcBool         word_allocated{};
    ~_FcFormatContext() { if (word_allocated) free(word); }
} FcFormatContext;

static FcBool
FcFormatContextInit (FcFormatContext *c,
		     const FcChar8   *format,
		     FcChar8         *scratch,
		     int              scratch_len)
{
    c->format_orig = c->format = format;
    c->format_len = strlen ((const char *) format);

    if (c->format_len < scratch_len)
    {
	c->word = scratch;
	c->word_allocated = FcFalse;
    }
    else
    {
	c->word = static_cast<FcChar8*>(malloc (c->format_len + 1));
	c->word_allocated = FcTrue;
    }

    return c->word != NULL;
}

static void
FcFormatContextDone (FcFormatContext *c)
{
    if (c && c->word_allocated)
    {
	free (c->word);
        c->word_allocated = FcFalse;
    }
}

static FcBool
consume_char (FcFormatContext *c,
	      FcChar8          term)
{
    if (*c->format != term)
	return FcFalse;

    c->format++;
    return FcTrue;
}

static FcBool
expect_char (FcFormatContext *c,
	      FcChar8          term)
{
    FcBool res = consume_char (c, term);
    if (!res)
    {
	if (c->format == c->format_orig + c->format_len)
	    message ("format ended while expecting '%c'",
		     term);
	else
	    message ("expected '%c' at %d",
		     term, c->format - c->format_orig + 1);
    }
    return res;
}

static FcBool
FcCharIsPunct (const FcChar8 c)
{
    if (c < '0')
	return FcTrue;
    if (c <= '9')
	return FcFalse;
    if (c < 'A')
	return FcTrue;
    if (c <= 'Z')
	return FcFalse;
    if (c < 'a')
	return FcTrue;
    if (c <= 'z')
	return FcFalse;
    if (c <= '~')
	return FcTrue;
    return FcFalse;
}

static char escaped_char(const char ch)
{
    switch (ch) {
    case 'a':   return '\a';
    case 'b':   return '\b';
    case 'f':   return '\f';
    case 'n':   return '\n';
    case 'r':   return '\r';
    case 't':   return '\t';
    case 'v':   return '\v';
    default:    return ch;
    }
}

static FcBool
read_word (FcFormatContext *c)
{
    FcChar8 *p;

    p = c->word;

    while (*c->format)
    {
	if (*c->format == '\\')
	{
	    c->format++;
	    if (*c->format)
	      *p++ = escaped_char (*c->format++);
	    continue;
	}
	else if (FcCharIsPunct (*c->format))
	    break;

	*p++ = *c->format++;
    }
    *p = '\0';

    if (p == c->word)
    {
	message ("expected identifier at %d",
		 c->format - c->format_orig + 1);
	return FcFalse;
    }

    return FcTrue;
}

static FcBool
read_chars (FcFormatContext *c,
	    FcChar8          term)
{
    FcChar8 *p;

    p = c->word;

    while (*c->format && *c->format != '}' && *c->format != term)
    {
	if (*c->format == '\\')
	{
	    c->format++;
	    if (*c->format)
	      *p++ = escaped_char (*c->format++);
	    continue;
	}

	*p++ = *c->format++;
    }
    *p = '\0';

    if (p == c->word)
    {
	message ("expected character data at %d",
		 c->format - c->format_orig + 1);
	return FcFalse;
    }

    return FcTrue;
}

static FcBool
FcPatternFormatToBuf (FcPattern     *pat,
		      const FcChar8 *format,
		      FcStrBuf      *buf);

static FcBool
interpret_builtin (FcFormatContext *c,
		   FcPattern       *pat,
		   FcStrBuf        *buf)
{
    FcChar8       *new_str;
    FcBool         ret;

    if (!expect_char (c, '=') ||
	!read_word (c))
	return FcFalse;


    if (0) { }
#define BUILTIN(name, func) \
    else if (0 == strcmp ((const char *) c->word, name))\
	do { new_str = func (pat); ret = FcTrue; } while (0)
    BUILTIN ("unparse",  FormatUnparse);

#undef BUILTIN
    else
	ret = FcFalse;

    if (ret)
    {
	if (new_str)
	{
	    FcStrBufString (buf, new_str);
	    FormatFree (new_str);
	    return FcTrue;
	}
	else
	    return FcFalse;
    }


    if (0) { }
#define BUILTIN(name, format) \
    else if (0 == strcmp ((const char *) c->word, name))\
	ret = FcPatternFormatToBuf (pat, (const FcChar8 *) format, buf)
    BUILTIN ("fccat",    FCCAT_FORMAT);
    BUILTIN ("fcmatch",  FCMATCH_FORMAT);
    BUILTIN ("fclist",   FCLIST_FORMAT);
    BUILTIN ("pkgkit",   PKGKIT_FORMAT);
#undef BUILTIN
    else
	ret = FcFalse;

    if (!ret)
	message ("unknown builtin \"%s\"",
		 c->word);

    return ret;
}

static FcBool
interpret_expr (FcFormatContext *c,
		FcPattern       *pat,
		FcStrBuf        *buf,
		FcChar8          term);

static FcBool
interpret_subexpr (FcFormatContext *c,
		   FcPattern       *pat,
		   FcStrBuf        *buf)
{
    return expect_char (c, '{') &&
	   interpret_expr (c, pat, buf, '}') &&
	   expect_char (c, '}');
}

static FcBool
maybe_interpret_subexpr (FcFormatContext *c,
			 FcPattern       *pat,
			 FcStrBuf        *buf)
{
    return (*c->format == '{') ?
	   interpret_subexpr (c, pat, buf) :
	   FcTrue;
}

static FcBool
skip_subexpr (FcFormatContext *c);

static FcBool
skip_percent (FcFormatContext *c)
{
    if (!expect_char (c, '%'))
	return FcFalse;


    if (strtol ((const char *) c->format, (char **) &c->format, 10))
        {}

    if (!expect_char (c, '{'))
	return FcFalse;

    while(*c->format && *c->format != '}')
    {
	switch (*c->format)
	{
	case '\\':
	    c->format++;
	    if (*c->format)
		c->format++;
	    continue;
	case '{':
	    if (!skip_subexpr (c))
		return FcFalse;
	    continue;
	}
	c->format++;
    }

    return expect_char (c, '}');
}

static FcBool
skip_expr (FcFormatContext *c)
{
    while(*c->format && *c->format != '}')
    {
	switch (*c->format)
	{
	case '\\':
	    c->format++;
	    if (*c->format)
		c->format++;
	    continue;
	case '%':
	    if (!skip_percent (c))
		return FcFalse;
	    continue;
	}
	c->format++;
    }

    return FcTrue;
}

static FcBool
skip_subexpr (FcFormatContext *c)
{
    return expect_char (c, '{') &&
	   skip_expr (c) &&
	   expect_char (c, '}');
}

static FcBool
maybe_skip_subexpr (FcFormatContext *c)
{
    return (*c->format == '{') ?
	   skip_subexpr (c) :
	   FcTrue;
}

static FcBool
interpret_filter_in(FcFormatContext* c, FcPattern* pat, FcStrBuf* buf) {
    if (!expect_char(c, '+')) return FcFalse;
    std::vector<std::string> names;
    do {
        if (!read_word(c)) return FcFalse;
        names.emplace_back(reinterpret_cast<char*>(c->word));
    } while (consume_char(c, ','));
    std::vector<const char*> pointers;
    for (const auto& name : names) pointers.push_back(name.c_str());
    FcObjectSet os{static_cast<int>(pointers.size()), 0, pointers.data()};
    PatternOwner subpat(FcPatternFilter(pat, &os));
    return subpat && interpret_subexpr(c, subpat.get(), buf);
}

static FcBool
interpret_filter_out(FcFormatContext* c, FcPattern* pat, FcStrBuf* buf) {
    if (!expect_char(c, '-')) return FcFalse;
    PatternOwner subpat(FcPatternDuplicate(pat));
    if (!subpat) return FcFalse;
    do {
        if (!read_word(c)) return FcFalse;
        FcPatternDel(subpat.get(), reinterpret_cast<char*>(c->word));
    } while (consume_char(c, ','));
    return interpret_subexpr(c, subpat.get(), buf);
}

static FcBool
interpret_cond (FcFormatContext *c,
		FcPattern       *pat,
		FcStrBuf        *buf)
{
    FcBool pass;

    if (!expect_char (c, '?'))
	return FcFalse;

    pass = FcTrue;

    do
    {
	FcBool negate;
	FcValue v;

	negate = consume_char (c, '!');

	if (!read_word (c))
	    return FcFalse;

	pass = pass &&
	       (negate ^
		(FcResultMatch ==
		 FcPatternGet (pat, (const char *) c->word, 0, &v)));
    }
    while (consume_char (c, ','));

    if (pass)
    {
	if (!interpret_subexpr  (c, pat, buf) ||
	    !maybe_skip_subexpr (c))
	    return FcFalse;
    }
    else
    {
	if (!skip_subexpr (c) ||
	    !maybe_interpret_subexpr  (c, pat, buf))
	    return FcFalse;
    }

    return FcTrue;
}

static FcBool
interpret_count(FcFormatContext* c, FcPattern* pat, FcStrBuf* buf) {
    if (!expect_char(c, '#') || !read_word(c)) return FcFalse;
    int count = 0;
    FcValue value{};
    while (FcPatternGet(pat, reinterpret_cast<char*>(c->word), count, &value) == FcResultMatch) ++count;
    const auto text = std::to_string(count);
    return FcStrBufString(buf, reinterpret_cast<const FcChar8*>(text.c_str()));
}

static FcBool
interpret_enumerate(FcFormatContext* c, FcPattern* pat, FcStrBuf* buf) {
    if (!expect_char(c, '[') || !expect_char(c, ']')) return FcFalse;
    std::vector<std::string> names;
    do {
        if (!read_word(c)) return FcFalse;
        names.emplace_back(reinterpret_cast<char*>(c->word));
    } while (consume_char(c, ','));
    std::vector<std::string> languages;
    FcLangSet* langset = nullptr;
    const bool isLanguages = names.size() == 1 &&
        FcPatternGetLangSet(pat, names[0].c_str(), 0, &langset) == FcResultMatch;
    if (isLanguages) languages = FontconfigLanguages_nid_no_patch(langset, true);
    PatternOwner subpat(FcPatternDuplicate(pat));
    if (!subpat) return FcFalse;
    const auto* saved = c->format;
    for (int index = 0;; ++index) {
        bool done = true;
        for (const auto& name : names) {
            FcPatternDel(subpat.get(), name.c_str());
            if (isLanguages) {
                if (static_cast<std::size_t>(index) < languages.size()) {
                    if (!FcPatternAddString(subpat.get(), name.c_str(),
                        reinterpret_cast<const FcChar8*>(languages[index].c_str()))) return FcFalse;
                    done = false;
                }
            } else {
                FcValue value{};
                if (FcPatternGet(pat, name.c_str(), index, &value) == FcResultMatch) {
                    if (!AddFormatValue(subpat.get(), name.c_str(), value)) return FcFalse;
                    done = false;
                }
            }
        }
        if (done) break;
        c->format = saved;
        if (!interpret_subexpr(c, subpat.get(), buf)) return FcFalse;
    }
    if (c->format == saved) return skip_subexpr(c);
    return FcTrue;
}

static FcBool
interpret_simple(FcFormatContext* c, FcPattern* pat, FcStrBuf* buf) {
    const bool colon = consume_char(c, ':');
    if (!read_word(c)) return FcFalse;
    const std::string name(reinterpret_cast<char*>(c->word));
    int index = -1;
    if (consume_char(c, '[')) {
        char* end;
        index = strtol(reinterpret_cast<const char*>(c->format), &end, 10);
        c->format = reinterpret_cast<const FcChar8*>(end);
        if (index < 0 || !expect_char(c, ']')) return FcFalse;
    }
    const bool label = consume_char(c, '=');
    bool hasDefault = false;
    std::string fallback;
    if (consume_char(c, ':')) {
        if (!expect_char(c, '-') || !read_chars(c, '|')) return FcFalse;
        hasDefault = true;
        fallback = reinterpret_cast<char*>(c->word);
    }
    FcValue value{};
    if (FcPatternGet(pat, name.c_str(), 0, &value) != FcResultMatch && !hasDefault) return FcTrue;
    if (colon) FcStrBufChar(buf, ':');
    if (label) {
        FcStrBufString(buf, reinterpret_cast<const FcChar8*>(name.c_str()));
        FcStrBufChar(buf, '=');
    }
    if (FcPatternGet(pat, name.c_str(), index < 0 ? 0 : index, &value) != FcResultMatch)
        return !hasDefault || FcStrBufString(buf, reinterpret_cast<const FcChar8*>(fallback.c_str()));
    if (!UnparseValue(buf, value, nullptr)) return FcFalse;
    if (index >= 0) return FcTrue;
    for (int i = 1; FcPatternGet(pat, name.c_str(), i, &value) == FcResultMatch; ++i) {
        FcStrBufChar(buf, ',');
        if (!UnparseValue(buf, value, nullptr)) return FcFalse;
    }
    return FcTrue;
}

static FcBool
cescape (FcFormatContext *c,
	 const FcChar8   *str,
	 FcStrBuf        *buf)
{


    while(*str)
    {
	switch (*str)
	{
	case '\\':
	case '"':
	    FcStrBufChar (buf, '\\');
	    break;
	}
	FcStrBufChar (buf, *str++);
    }
    return FcTrue;
}

static FcBool
shescape (FcFormatContext *c,
	  const FcChar8   *str,
	  FcStrBuf        *buf)
{
    FcStrBufChar (buf, '\'');
    while(*str)
    {
	if (*str == '\'')
	    FcStrBufString (buf, (const FcChar8 *) "'\\''");
	else
	    FcStrBufChar (buf, *str);
	str++;
    }
    FcStrBufChar (buf, '\'');
    return FcTrue;
}

static FcBool
xmlescape (FcFormatContext *c,
	   const FcChar8   *str,
	   FcStrBuf        *buf)
{


    while(*str)
    {
	switch (*str)
	{
	case '&': FcStrBufString (buf, (const FcChar8 *) "&amp;"); break;
	case '<': FcStrBufString (buf, (const FcChar8 *) "&lt;");  break;
	case '>': FcStrBufString (buf, (const FcChar8 *) "&gt;");  break;
	default:  FcStrBufChar   (buf, *str);                      break;
	}
	str++;
    }
    return FcTrue;
}

static FcBool
delete_chars (FcFormatContext *c,
	      const FcChar8   *str,
	      FcStrBuf        *buf)
{


    if (!expect_char (c, '(') ||
	!read_chars (c, ')') ||
	!expect_char (c, ')'))
	return FcFalse;

    while(*str)
    {
	FcChar8 *p;

	p = (FcChar8 *) strpbrk ((const char *) str, (const char *) c->word);
	if (p)
	{
	    FcStrBufData (buf, str, p - str);
	    str = p + 1;
	}
	else
	{
	    FcStrBufString (buf, str);
	    break;
	}

    }

    return FcTrue;
}

static FcBool
escape_chars (FcFormatContext *c,
	      const FcChar8   *str,
	      FcStrBuf        *buf)
{


    if (!expect_char (c, '(') ||
	!read_chars (c, ')') ||
	!expect_char (c, ')'))
	return FcFalse;

    while(*str)
    {
	FcChar8 *p;

	p = (FcChar8 *) strpbrk ((const char *) str, (const char *) c->word);
	if (p)
	{
	    FcStrBufData (buf, str, p - str);
	    FcStrBufChar (buf, c->word[0]);
	    FcStrBufChar (buf, *p);
	    str = p + 1;
	}
	else
	{
	    FcStrBufString (buf, str);
	    break;
	}

    }

    return FcTrue;
}

static FcBool
translate_chars (FcFormatContext *c,
		 const FcChar8   *str,
		 FcStrBuf        *buf)
{
    char *from, *to, repeat;
    int from_len, to_len;



    if (!expect_char (c, '(') ||
	!read_chars (c, ',') ||
	!expect_char (c, ','))
	return FcFalse;

    from = (char *) c->word;
    from_len = strlen (from);
    to = from + from_len + 1;


    c->word = (FcChar8 *) to;
    if (!read_chars (c, ')'))
    {
      c->word = (FcChar8 *) from;
      return FcFalse;
    }
    c->word = (FcChar8 *) from;

    to_len = strlen (to);
    repeat = to[to_len - 1];

    if (!expect_char (c, ')'))
	return FcFalse;

    while(*str)
    {
	FcChar8 *p;

	p = (FcChar8 *) strpbrk ((const char *) str, (const char *) from);
	if (p)
	{
	    int i;
	    FcStrBufData (buf, str, p - str);
	    i = strchr (from, *p) - from;
	    FcStrBufChar (buf, i < to_len ? to[i] : repeat);
	    str = p + 1;
	}
	else
	{
	    FcStrBufString (buf, str);
	    break;
	}

    }

    return FcTrue;
}

static FcBool
interpret_convert (FcFormatContext *c,
		   FcStrBuf        *buf,
		   int              start)
{
    const FcChar8 *str;
    FcChar8       *new_str;
    FcStrBuf       new_buf;
    FcChar8        buf_static[8192];
    FcBool         ret;

    if (!expect_char (c, '|') ||
	!read_word (c))
	return FcFalse;


    FcStrBufChar (buf, '\0');
    if (buf->failed)
	return FcFalse;
    str = buf->buf + start;
    buf->len = start;


    if (0) { }
#define CONVERTER(name, func) \
    else if (0 == strcmp ((const char *) c->word, name))\
	do { new_str = func (str); ret = FcTrue; } while (0)
    CONVERTER  ("downcase",  FormatDowncase);
    CONVERTER  ("basename",  FormatBasename);
    CONVERTER  ("dirname",   FormatDirname);
#undef CONVERTER
    else
	ret = FcFalse;

    if (ret)
    {
	if (new_str)
	{
	    FcStrBufString (buf, new_str);
	    FormatFree (new_str);
	    return FcTrue;
	}
	else
	    return FcFalse;
    }

    FcStrBufInit (&new_buf, buf_static, sizeof (buf_static));


    if (0) { }
#define CONVERTER(name, func) \
    else if (0 == strcmp ((const char *) c->word, name))\
	ret = func (c, str, &new_buf)
    CONVERTER ("cescape",   cescape);
    CONVERTER ("shescape",  shescape);
    CONVERTER ("xmlescape", xmlescape);
    CONVERTER ("delete",    delete_chars);
    CONVERTER ("escape",    escape_chars);
    CONVERTER ("translate", translate_chars);
#undef CONVERTER
    else
	ret = FcFalse;

    if (ret)
    {
	FcStrBufChar (&new_buf, '\0');
	FcStrBufString (buf, new_buf.buf);
    }
    else
	message ("unknown converter \"%s\"",
		 c->word);

    FcStrBufDestroy (&new_buf);

    return ret;
}

static FcBool
maybe_interpret_converts (FcFormatContext *c,
			   FcStrBuf        *buf,
			   int              start)
{
    while (*c->format == '|')
	if (!interpret_convert (c, buf, start))
	    return FcFalse;

    return FcTrue;
}

static FcBool
align_to_width (FcStrBuf *buf,
		int       start,
		int       width)
{
    int len;

    if (buf->failed)
	return FcFalse;

    len = buf->len - start;
    if (len < -width)
    {

	while (len++ < -width)
	    FcStrBufChar (buf, ' ');
    }
    else if (len < width)
    {
	int old_len;
	old_len = len;

	while (len++ < width)
	    FcStrBufChar (buf, ' ');
	if (buf->failed)
	    return FcFalse;
	len = old_len;
	memmove (buf->buf + buf->len - len,
		 buf->buf + buf->len - width,
		 len);
	memset (buf->buf + buf->len - width,
		' ',
		width - len);
    }

    return !buf->failed;
}
static FcBool
interpret_percent (FcFormatContext *c,
		   FcPattern       *pat,
		   FcStrBuf        *buf)
{
    int width, start;
    FcBool ret;

    if (!expect_char (c, '%'))
	return FcFalse;

    if (consume_char (c, '%'))
    {
	FcStrBufChar (buf, '%');
	return FcTrue;
    }


    width = strtol ((const char *) c->format, (char **) &c->format, 10);

    if (!expect_char (c, '{'))
	return FcFalse;

    start = buf->len;

    switch (*c->format) {
    case '=': ret = interpret_builtin    (c, pat, buf); break;
    case '{': ret = interpret_subexpr    (c, pat, buf); break;
    case '+': ret = interpret_filter_in  (c, pat, buf); break;
    case '-': ret = interpret_filter_out (c, pat, buf); break;
    case '?': ret = interpret_cond       (c, pat, buf); break;
    case '#': ret = interpret_count      (c, pat, buf); break;
    case '[': ret = interpret_enumerate  (c, pat, buf); break;
    default:  ret = interpret_simple     (c, pat, buf); break;
    }

    return ret &&
	   maybe_interpret_converts (c, buf, start) &&
	   align_to_width (buf, start, width) &&
	   expect_char (c, '}');
}

static FcBool
interpret_expr (FcFormatContext *c,
		FcPattern       *pat,
		FcStrBuf        *buf,
		FcChar8          term)
{
    while (*c->format && *c->format != term)
    {
	switch (*c->format)
	{
	case '\\':
	    c->format++;
	    if (*c->format)
		FcStrBufChar (buf, escaped_char (*c->format++));
	    continue;
	case '%':
	    if (!interpret_percent (c, pat, buf))
		return FcFalse;
	    continue;
	}
	FcStrBufChar (buf, *c->format++);
    }
    return FcTrue;
}

static FcBool
FcPatternFormatToBuf (FcPattern     *pat,
		      const FcChar8 *format,
		      FcStrBuf      *buf)
{
    FcFormatContext c;
    FcChar8         word_static[1024];
    FcBool          ret;

    if (!FcFormatContextInit (&c, format, word_static, sizeof (word_static)))
	return FcFalse;

    ret = interpret_expr (&c, pat, buf, '\0');

    FcFormatContextDone (&c);

    return ret;
}

}

extern "C" FcChar8* APS5_VABI FcPatternFormat(FcPattern* pat, const FcChar8* format) {
    if (!format) NotImplemented_nid_no_patch(__func__);
    try {
        PatternOwner empty(pat ? nullptr : FcPatternCreate());
        if (!pat) pat = empty.get();
        if (!pat) return nullptr;
        FcStrBuf buf;
        if (!FcPatternFormatToBuf(pat, format, &buf)) return nullptr;
        return FcStrBufDone(&buf);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}
