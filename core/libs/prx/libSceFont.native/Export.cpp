#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceFontAttachDeviceCacheBuffer(FontLibrary library, void* buffer, uint32_t size) {
 (void)library;
 (void)buffer;
 (void)size;
 return 0;
}

int sceFontBindRenderer(FontHandle font_handle, FontRenderer renderer) {
 (void)font_handle;
 (void)renderer;
 return 0;
}

int sceFontCharacterGetBidiLevel(FontTextCharacter text_character, int* bidi_level) {
 (void)text_character;
 (void)bidi_level;
 return 0;
}

int sceFontCharacterGetSyllableStringState(FontTextCharacter text_character, int* syllable_string_state) {
 (void)text_character;
 (void)syllable_string_state;
 return 0;
}

int sceFontCharacterGetTextFontCode(FontTextCharacter text_character, FontHandle* font_handle, uint32_t* text_code) {
 (void)text_character;
 (void)font_handle;
 (void)text_code;
 return 0;
}

int sceFontCharacterGetTextOrder(FontTextCharacter text_character, void** text_order) {
 (void)text_character;
 (void)text_order;
 return 0;
}

uint32_t sceFontCharacterLooksFormatCharacters(FontTextCharacter text_character) {
 (void)text_character;
 return 0;
}

uint32_t sceFontCharacterLooksWhiteSpace(FontTextCharacter text_character) {
 (void)text_character;
 return 0;
}

FontTextCharacter sceFontCharacterRefersTextBack(FontTextCharacter text_character) {
 (void)text_character;
 return {};
}

FontTextCharacter sceFontCharacterRefersTextNext(FontTextCharacter text_character) {
 (void)text_character;
 return {};
}

FontTextCodes* sceFontCharactersRefersTextCodes(FontTextCharacter text_character, FontTextCharacter term_character, FontTextCodes* text_codes) {
 (void)text_character;
 (void)term_character;
 (void)text_codes;
 return nullptr;
}

int sceFontCloseFont(FontHandle font_handle) {
 (void)font_handle;
 return 0;
}

int sceFontCreateLibrary(const FontMemory* memory, FontLibrarySelection selection, FontLibrary* library) {
 (void)memory;
 (void)selection;
 (void)library;
 return 0;
}

int sceFontCreateLibraryWithEdition(const FontMemory* memory, FontLibrarySelection selection, uint64_t edition, FontLibrary* library) {
 (void)memory;
 (void)selection;
 (void)edition;
 (void)library;
 return 0;
}

int sceFontCreateRendererWithEdition(const FontMemory* memory, FontRendererSelection selection, uint64_t edition, FontRenderer* renderer) {
 (void)memory;
 (void)selection;
 (void)edition;
 (void)renderer;
 return 0;
}

int sceFontCreateString(const FontMemory* memory, FontTextSource* font_text_source, const FontCreateStringDetail* string_detail, FontString* font_string) {
 (void)memory;
 (void)font_text_source;
 (void)string_detail;
 (void)font_string;
 return 0;
}

int sceFontCreateWritingLine(const FontMemory* memory, int writing_form, const void* writing_line_detail, FontWritingLine* writing_line) {
 (void)memory;
 (void)writing_form;
 (void)writing_line_detail;
 (void)writing_line;
 return 0;
}

int sceFontDefineAttribute(FontHandle font_handle, int attribute, int* old_attribute) {
 (void)font_handle;
 (void)attribute;
 (void)old_attribute;
 return 0;
}

int sceFontDeleteGlyph(const FontMemory* memory, void** font_glyph) {
 (void)memory;
 (void)font_glyph;
 return 0;
}

int sceFontDestroyLibrary(FontLibrary* library) {
 (void)library;
 return 0;
}

int sceFontDestroyRenderer(FontRenderer* renderer) {
 (void)renderer;
 return 0;
}

int sceFontDestroyString(FontString* font_string) {
 (void)font_string;
 return 0;
}

int sceFontDestroyWritingLine(FontWritingLine* writing_line) {
 (void)writing_line;
 return 0;
}

int sceFontGenerateCharGlyph(FontHandle font_handle, uint32_t code, const FontGenerateGlyphDetail* detail, void** font_glyph) {
 (void)font_handle;
 (void)code;
 (void)detail;
 (void)font_glyph;
 return 0;
}

int sceFontGetCharGlyphMetrics(FontHandle font_handle, uint32_t code, FontGlyphMetrics* metrics) {
 (void)font_handle;
 (void)code;
 (void)metrics;
 return 0;
}

int sceFontGetHorizontalLayout(FontHandle font_handle, FontHorizontalLayout* layout) {
 (void)font_handle;
 (void)layout;
 return 0;
}

int sceFontGetRenderCharGlyphMetrics(FontHandle font_handle, uint32_t code, FontGlyphMetrics* metrics) {
 (void)font_handle;
 (void)code;
 (void)metrics;
 return 0;
}

int sceFontGetVerticalLayout(FontHandle font_handle, FontVerticalLayout* layout) {
 (void)font_handle;
 (void)layout;
 return 0;
}

int sceFontGlyphDefineAttribute(void* font_glyph, int attribute, int* old_attribute) {
 (void)font_glyph;
 (void)attribute;
 (void)old_attribute;
 return 0;
}

int sceFontMemoryInit(FontMemory* font_memory, void* address, uint32_t size_byte, const FontMemoryInterface* mem_interface, void* mspace_object, FontMemoryDestroyCallback destroy_callback, void* destroy_object) {
 (void)font_memory;
 (void)address;
 (void)size_byte;
 (void)mem_interface;
 (void)mspace_object;
 (void)destroy_callback;
 (void)destroy_object;
 return 0;
}

int sceFontMemoryTerm(FontMemory* font_memory) {
 (void)font_memory;
 return 0;
}

int sceFontOpenFontInstance(FontHandle font_handle, void* setup_font, FontHandle* out_font_handle) {
 (void)font_handle;
 (void)setup_font;
 (void)out_font_handle;
 return 0;
}

int sceFontOpenFontMemory(FontLibrary library, const void* font_address, uint32_t font_size, const FontOpenDetail* detail, FontHandle* handle) {
 (void)library;
 (void)font_address;
 (void)font_size;
 (void)detail;
 (void)handle;
 return 0;
}

int sceFontOpenFontSet(FontLibrary library, uint32_t font_set_type, uint32_t open_mode, const FontOpenDetail* detail, FontHandle* handle) {
 (void)library;
 (void)font_set_type;
 (void)open_mode;
 (void)detail;
 (void)handle;
 return 0;
}

int sceFontRebindRenderer(FontHandle font_handle) {
 (void)font_handle;
 return 0;
}

int sceFontRenderCharGlyphImageHorizontal(FontHandle font_handle, uint32_t code, FontRenderSurface* surf, float x, float y, FontGlyphMetrics* metrics, FontRenderResult* result) {
 (void)font_handle;
 (void)code;
 (void)surf;
 (void)x;
 (void)y;
 (void)metrics;
 (void)result;
 return 0;
}

void sceFontRenderSurfaceInit(FontRenderSurface* surf, void* buffer, int buf_width_byte, int pixel_size_byte, int width, int height) {
 (void)surf;
 (void)buffer;
 (void)buf_width_byte;
 (void)pixel_size_byte;
 (void)width;
 (void)height;
}

void sceFontRenderSurfaceSetScissor(FontRenderSurface* surf, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
 (void)surf;
 (void)x0;
 (void)y0;
 (void)x1;
 (void)y1;
}

int sceFontSetEffectSlant(FontHandle font_handle, float slant_ratio) {
 (void)font_handle;
 (void)slant_ratio;
 return 0;
}

int sceFontSetEffectWeight(FontHandle font_handle, float weight_x_scale, float weight_y_scale, uint32_t mode) {
 (void)font_handle;
 (void)weight_x_scale;
 (void)weight_y_scale;
 (void)mode;
 return 0;
}

int sceFontSetScalePixel(FontHandle font_handle, float w, float h) {
 (void)font_handle;
 (void)w;
 (void)h;
 return 0;
}

int sceFontSetupRenderEffectSlant(FontHandle font_handle, float slant_ratio) {
 (void)font_handle;
 (void)slant_ratio;
 return 0;
}

int sceFontSetupRenderEffectWeight(FontHandle font_handle, float weight_x_scale, float weight_y_scale, uint32_t mode) {
 (void)font_handle;
 (void)weight_x_scale;
 (void)weight_y_scale;
 (void)mode;
 return 0;
}

int sceFontSetupRenderScalePixel(FontHandle font_handle, float w, float h) {
 (void)font_handle;
 (void)w;
 (void)h;
 return 0;
}

uint32_t sceFontStringGetTerminateCode(FontString font_string) {
 (void)font_string;
 return 0;
}

void* sceFontStringGetTerminateOrder(FontString font_string) {
 (void)font_string;
 return nullptr;
}

int sceFontStringGetWritingForm(FontString font_string) {
 (void)font_string;
 return 0;
}

FontRenderCharacter sceFontStringRefersRenderCharacters(FontString font_string, FontTextCharacter start_character, FontTextCharacter last_character, uint32_t* character_count) {
 (void)font_string;
 (void)start_character;
 (void)last_character;
 (void)character_count;
 return {};
}

FontTextCharacter sceFontStringRefersTextCharacters(FontString font_string, uint32_t* character_count) {
 (void)font_string;
 (void)character_count;
 return {};
}

int sceFontSupportExternalFonts(FontLibrary library, uint32_t font_max, uint32_t formats) {
 (void)library;
 (void)font_max;
 (void)formats;
 return 0;
}

int sceFontSupportSystemFonts(FontLibrary library) {
 (void)library;
 return 0;
}

FontTextCodes* sceFontTextCodesStepBack(FontTextCodes* text_codes_step) {
 (void)text_codes_step;
 return nullptr;
}

FontTextCodes* sceFontTextCodesStepNext(FontTextCodes* text_codes_step) {
 (void)text_codes_step;
 return nullptr;
}

int sceFontTextSourceInit(FontTextSource* font_text_source, const void* text_address, uint32_t text_size_byte, FontTextParseFunction text_parser, void* text_object) {
 (void)font_text_source;
 (void)text_address;
 (void)text_size_byte;
 (void)text_parser;
 (void)text_object;
 return 0;
}

int sceFontTextSourceRewind(FontTextSource* font_text_source) {
 (void)font_text_source;
 return 0;
}

int sceFontTextSourceSetDefaultFont(FontTextSource* font_text_source, FontHandle default_font) {
 (void)font_text_source;
 (void)default_font;
 return 0;
}

int sceFontTextSourceSetWritingForm(FontTextSource* font_text_source, int writing_form) {
 (void)font_text_source;
 (void)writing_form;
 return 0;
}

int sceFontUnbindRenderer(FontHandle font_handle) {
 (void)font_handle;
 return 0;
}

int sceFontWritingGetRenderMetrics(FontWriting* font_writing, FontWritingMetrics* writing_metrics) {
 (void)font_writing;
 (void)writing_metrics;
 return 0;
}

int sceFontWritingInit(FontWriting* font_writing, FontString font_string, FontRenderCharacter font_character) {
 (void)font_writing;
 (void)font_string;
 (void)font_character;
 return 0;
}

int sceFontWritingLineClear(FontWritingLine writing_line) {
 (void)writing_line;
 return 0;
}

int sceFontWritingLineGetOrderingSpace(FontWritingLine writing_line, float* head_space, float* inline_space, float* tail_space, float* advance_space) {
 (void)writing_line;
 (void)head_space;
 (void)inline_space;
 (void)tail_space;
 (void)advance_space;
 return 0;
}

int sceFontWritingLineGetRenderMetrics(FontWritingLine writing_line, FontWritingMetrics* writing_metrics) {
 (void)writing_line;
 (void)writing_metrics;
 return 0;
}

FontWritingLineStep* sceFontWritingLineRefersRenderStep(FontWritingLine writing_line) {
 (void)writing_line;
 return nullptr;
}

int sceFontWritingLineWritesOrder(FontWritingLine writing_line, uint64_t writing_attribute, const FontWritingMetrics* writing_metrics, void* writing_orderer) {
 (void)writing_line;
 (void)writing_attribute;
 (void)writing_metrics;
 (void)writing_orderer;
 return 0;
}

const FontWritingStep* sceFontWritingRefersRenderStep(FontWriting* font_writing) {
 (void)font_writing;
 return nullptr;
}

FontTextCharacter sceFontWritingRefersRenderStepCharacter(FontWriting* font_writing, const void** letter_step) {
 (void)font_writing;
 (void)letter_step;
 return {};
}

int sceFontWritingSetMaskInvisible(FontWriting* font_writing, int mask) {
 (void)font_writing;
 (void)mask;
 return 0;
}

}
