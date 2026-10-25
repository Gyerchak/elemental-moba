#pragma once
// [U-239] Languages. Keys ARE the English texts, so English needs no file and a missing translation
// simply shows English. A language = one plain file `lang/<code>.txt` with lines
//     English text=Translated text
// Copy lang/pl.txt to add a language. Voices: resourcepacks/<pack>/sounds/<code>/… (see Resources).
#include <string>
#include <string_view>
#include <vector>

namespace em::loc {

void load(const std::string& code);                    // "en", "pl", …
[[nodiscard]] const std::string& tr(std::string_view english);
[[nodiscard]] std::vector<std::string> languages();    // codes found in lang/ (+ "en")
[[nodiscard]] const std::string& current();

}  // namespace em::loc

namespace em {
using loc::tr;
}
