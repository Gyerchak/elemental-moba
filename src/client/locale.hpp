#pragma once
// [U-239] Languages. Keys ARE the English texts, so English needs no file and a missing translation
// simply shows English. A language = one plain file `lang/<code>.txt` with lines
//     English text=Translated text
// Copy lang/pl.txt to add a language. Voices: resourcepacks/<pack>/sounds/<code>/… (see Resources).
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace em::loc {

void load(const std::string& code);                    // "en", "pl", …
[[nodiscard]] const std::string& tr(std::string_view english);
// [U-508] text with numbers in it (server messages): "ready in 12 s" is looked up as "ready in {} s", numbers put back
[[nodiscard]] std::string tr_numbers(std::string_view english);
// [U-508] a match event from the server with names in it ("Aang slew Zuko") → the translated sentence (cached)
[[nodiscard]] const std::string& tr_feed(std::string_view line);
// [U-508] a translated format string: the key keeps its {} ; a translation that does not fit falls back to English
template <class... A>
[[nodiscard]] std::string trf(std::string_view fmt, const A&... a) {
    try {
        return std::vformat(tr(fmt), std::make_format_args(a...));
    } catch (const std::format_error&) {
        return std::vformat(fmt, std::make_format_args(a...));
    }
}
[[nodiscard]] std::vector<std::string> languages();    // codes found in lang/ (+ "en")
[[nodiscard]] const std::string& current();

}  // namespace em::loc

namespace em {
using loc::tr;
using loc::tr_numbers;
using loc::tr_feed;
using loc::trf;
}
