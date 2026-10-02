#ifndef STEAV_LSP_CORE_H
#define STEAV_LSP_CORE_H

#include "json.h"
#include <string>

/* the feature implementations shared between steav2-lsp (LSP-over-stdio,
 * steav2_lsp.cpp) and steav-wasm (the browser playground's embedding,
 * examples/steav_wasm.cpp) — one implementation, so both show exactly
 * what an editor running the language server does. impl in lsp_core.cpp. */
namespace steav_lsp {

// the real scanner / parser / type checker, as LSP diagnostics (one
// {range, severity, source, message} per steav_vm::check_source finding)
Json diagnostics(const std::string &text);

// keywords, base types, modules and their members, at a 0-based line/character
Json hover(const std::string &text, int line, int character);

// keywords, types, imported names, top-level names in the file, and
// module members after `vec.` (follows `jea` aliases)
Json completion(const std::string &text, int line, int character);

} // namespace steav_lsp
#endif // STEAV_LSP_CORE_H
