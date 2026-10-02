/* steav2-lsp: a language server for .sts files, LSP over stdio.
 *
 * - diagnostics: the real scanner / parser / type checker run on every
 *   change (steav_vm::check_source), so what the editor shows is exactly
 *   what `steav` would reject
 * - hover: keywords, base types, modules and their members
 * - completion: keywords, types, imported names, top level names in the
 *   file, and module members after `vec.` (follows `jea` aliases)
 *
 * diagnostics/hover/completion themselves live in lsp_core.cpp, shared
 * with examples/steav_wasm.cpp (the browser playground's embedding) so
 * both show exactly the same thing — this file is just the JSON-RPC/stdio
 * transport wrapping that shared core. */

#include "json.h"
#include "lsp_core.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

using steav_lsp::Json;

namespace {

/* ------------------------------------------------------------ transport */

bool read_message(std::string &body) {
  int length = -1;
  std::string line;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
    if (line.empty()) break;
    if (line.compare(0, 15, "Content-Length:") == 0) length = atoi(line.c_str() + 15);
  }
  if (!std::cin || length < 0) return false;
  body.assign(length, '\0');
  std::cin.read(&body[0], length);
  return (bool)std::cin;
}

void send(const Json &message) {
  std::string body = message.dump();
  std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
  std::cout.flush();
}

void respond(const Json &id, const Json &result) {
  Json m = Json::obj();
  m.set("jsonrpc", Json::of("2.0")).set("id", id).set("result", result);
  send(m);
}

} // namespace

int main() {
  std::ios::sync_with_stdio(false);
  std::map<std::string, std::string> docs;
  bool shutting_down = false;
  std::string body;

  while (read_message(body)) {
    Json msg;
    if (!Json::parse(body, msg)) continue;
    std::string method = msg["method"].string;
    const Json &id = msg["id"];
    const Json &params = msg["params"];
    bool is_request = id.kind != Json::NUL;

    if (method == "initialize") {
      Json completion_opts = Json::obj();
      completion_opts.set("triggerCharacters", Json::arr().push(Json::of(".")));
      Json caps = Json::obj();
      caps.set("textDocumentSync", Json::of(1)) // full text on every change
          .set("hoverProvider", Json::of(true))
          .set("completionProvider", completion_opts);
      Json info = Json::obj();
      info.set("name", Json::of("steav2-lsp")).set("version", Json::of("0.2"));
      Json result = Json::obj();
      result.set("capabilities", caps).set("serverInfo", info);
      respond(id, result);
    } else if (method == "textDocument/didOpen" || method == "textDocument/didChange") {
      std::string uri = params["textDocument"]["uri"].string;
      if (method == "textDocument/didOpen") {
        docs[uri] = params["textDocument"]["text"].string;
      } else if (!params["contentChanges"].array.empty()) {
        docs[uri] = params["contentChanges"].array.back()["text"].string;
      }
      Json p = Json::obj();
      p.set("uri", Json::of(uri)).set("diagnostics", steav_lsp::diagnostics(docs[uri]));
      Json note = Json::obj();
      note.set("jsonrpc", Json::of("2.0"))
          .set("method", Json::of("textDocument/publishDiagnostics"))
          .set("params", p);
      send(note);
    } else if (method == "textDocument/didClose") {
      docs.erase(params["textDocument"]["uri"].string);
    } else if (method == "textDocument/hover") {
      std::string uri = params["textDocument"]["uri"].string;
      respond(id, docs.count(uri) ? steav_lsp::hover(docs[uri], params["position"]["line"].as_int(),
                                          params["position"]["character"].as_int())
                                  : Json::null());
    } else if (method == "textDocument/completion") {
      std::string uri = params["textDocument"]["uri"].string;
      respond(id, steav_lsp::completion(docs.count(uri) ? docs[uri] : std::string(),
                             params["position"]["line"].as_int(),
                             params["position"]["character"].as_int()));
    } else if (method == "shutdown") {
      shutting_down = true;
      respond(id, Json::null());
    } else if (method == "exit") {
      return shutting_down ? 0 : 1;
    } else if (is_request) {
      Json err = Json::obj();
      err.set("code", Json::of(-32601)).set("message", Json::of("unsupported method: " + method));
      Json m = Json::obj();
      m.set("jsonrpc", Json::of("2.0")).set("id", id).set("error", err);
      send(m);
    }
  }
  return 0;
}
