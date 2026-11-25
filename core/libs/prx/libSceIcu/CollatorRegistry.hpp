#pragma once

namespace Icu::CollatorRegistry {

void* RegisterBorrowed(void* collator);
void UnregisterBorrowed(const void* collator);

}
