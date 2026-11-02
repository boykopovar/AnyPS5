#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTWORDS_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTWORDS_HPP

#include "prx/libSceFont/include/FontTypes.hpp"

struct FontWords;

namespace Font {

int CreateWords(const FontMemory* memory, FontTextSource* source, FontWords** words);
int DestroyWords(FontWords** words);
int WordsFindWordCharacters(FontWords* words, FontTextCharacter* firstCharacter, FontTextCharacter* endCharacter, FontTextCharacter** lastCharacter, FontTextCharacter** nextCharacter);

}

#endif
