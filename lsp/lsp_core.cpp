#include "lsp_core.h"
#include "frontend/scanner.h"
#include "logging/steav_diagnostics.h"
#include "stdlib/module.h"
#include "vm/vm.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

using steav_frontend::Token;

namespace steav_lsp {
namespace {

/* ---------------------------------------------------------------- docs */

struct Doc {
  const char *word;
  const char *meaning;
  const char *text;
};

const Doc KEYWORDS[] = {
    {"bongSlanhOun", "required opening statement",
     "Every script must start with `bongSlanhOun \"...\";`. The value is never used, "
     "more of them later in the file are ignored."},
    {"akthe", "var / let", "Declare a variable: `akthe x = 1;` or `akthe x: LekKut = 1;`. "
                           "The type is inferred from the initializer."},
    {"ber", "if", "`ber (condition) { ... }`. The condition has to be a Boolean. "
                  "`ber (x != sone)` narrows an optional `x` to `T` inside."},
    {"minjengte", "else", "`ber (condition) { ... } minjengte { ... }`"},
    {"somhab", "for", "`somhab (akthe i: LekKut = 0; i < n; i += 1) { ... }`"},
    {"nvpeldae", "while", "`nvpeldae (condition) { ... }`. `nvpeldae (x != sone)` narrows `x` inside."},
    {"rupamun", "func", "`rupamun add(a: LekThom, b: LekThom) -> LekThom { morvenh a + b; }`. "
                        "Also declares operator overloads: `rupamun +(a: Vec3, b: Vec3) -> Vec3 { ... }`, "
                        "and `rupamun init(...)` inside a `tnak`."},
    {"morvenh", "return", "Return from a rupamun: `morvenh x;`"},
    {"yok", "import", "`yok vector;` puts the module's names in scope, `yok vector jea vec;` "
                      "puts them behind `vec.`. Top level only."},
    {"jea", "as", "Import behind an alias: `yok vector jea vec;`"},
    {"sampoan", "struct", "A value type: copied on assignment, stored inline, never on the heap.\n\n"
                          "```\nsampoan Vec3 {\n    akthe x: LekThom;\n    ...\n}\n```\n"
                          "Constructed from its fields in order: `Vec3(1, 2, 3)`."},
    {"tnak", "class", "A reference type: heap allocated, shared, garbage collected. "
                      "Constructed with `rupamun init(...)`: `Sphere(center, 0.5)`."},
    {"jongyeytha", "print", "Print a value: `jongyeytha x;`"},
    {"ng", "and", "Logical and, short-circuiting. Booleans only."},
    {"reu", "or", "Logical or, short-circuiting. Booleans only."},
    {"ok", "true", "Boolean true."},
    {"ort", "false", "Boolean false."},
    {"sone", "nil", "No value. Only a `T?` can be `sone`: compare with `x == sone` / `x != sone`."},
    {"nis", "this", "The value a method was called on. In a `sampoan` method it's a copy."},
    {"super", "super", "Reserved."},
};

const Doc TYPES[] = {
    {"Lek", "Number", "Another name for `LekThom`. What an untyped number literal becomes."},
    {"LekKut", "Int", "32 bit integer. Wraps on overflow, `/` truncates."},
    {"LekThom", "Double", "64 bit float."},
    {"LekThomKlang", "Long", "64 bit integer."},
    {"Ahsor", "String", "Text. `+` concatenates."},
    {"Boolean", "Boolean", "`ok` or `ort`."},
    {"OrtMean", "Nil", "Nothing, what a rupamun without `->` returns."},
};

const Doc *find_doc(const Doc *docs, size_t count, const std::string &word) {
  for (size_t i = 0; i < count; i++) {
    if (word == docs[i].word) return &docs[i];
  }
  return nullptr;
}
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------- module members */

enum MemberKind { M_FUNCTION, M_METHOD, M_CONSTANT, M_TYPE };

struct Member {
  std::string name;
  std::string owner; // methods: Vec3
  std::string signature;
  MemberKind kind;
};

const char *native_type_name(steav_stdlib::NativeType t) {
  switch (t) {
  case steav_stdlib::NATIVE_LEK_KUT: return "LekKut";
  case steav_stdlib::NATIVE_LEK_THOM: return "LekThom";
  default: return "OrtMean";
  }
}

std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t;");
  return s.substr(a, b - a + 1);
}

// everything a module exports, straight from the tables the vm uses
std::vector<Member> module_members(int si) {
  std::vector<Member> members;
  const steav_stdlib::Module *m = steav_stdlib::module_at(si);
  for (int i = 0; i < m->function_count; i++) {
    const steav_stdlib::NativeFn &f = m->functions[i];
    std::string sig = std::string("rupamun ") + f.name + "(";
    for (int p = 0; p < f.arity; p++) {
      if (p > 0) sig += ", ";
      sig += native_type_name(f.params[p]);
    }
    sig += std::string(") -> ") + native_type_name(f.ret);
    Member mem = {f.name, "", sig, M_FUNCTION};
    members.push_back(mem);
  }
  for (int i = 0; i < m->constant_count; i++) {
    char value[64];
    snprintf(value, sizeof(value), "%.17g", m->constants[i].value);
    Member mem = {m->constants[i].name, "",
                  std::string(m->constants[i].name) + ": LekThom = " + value, M_CONSTANT};
    members.push_back(mem);
  }
  if (m->source == nullptr) return members;

  // a stdlib .sts header: sampoans + `rupamun ...;` lines
  std::string owner;
  size_t owner_index = 0;
  std::string src = m->source;
  size_t at = 0;
  while (at < src.size()) {
    size_t end = src.find('\n', at);
    if (end == std::string::npos) end = src.size();
    std::string line = src.substr(at, end - at);
    at = end + 1;
    std::string t = trim(line);
    if (t.compare(0, 8, "sampoan ") == 0) {
      owner = trim(t.substr(8, t.find('{') - 8));
      Member mem = {owner, "", "sampoan " + owner + " {", M_TYPE};
      owner_index = members.size();
      members.push_back(mem);
    } else if (t == "}") {
      if (!owner.empty()) members[owner_index].signature += "\n}";
      owner.clear();
    } else if (t.compare(0, 6, "akthe ") == 0 && !owner.empty()) {
      members[owner_index].signature += "\n    " + t + ";";
    } else if (t.compare(0, 8, "rupamun ") == 0) {
      std::string name = t.substr(8, t.find('(') - 8);
      if (name.empty() || !(isalpha((unsigned char)name[0]) || name[0] == '_')) continue;
      Member mem = {name, owner, owner.empty() ? t : owner + "." + t.substr(8), owner.empty() ? M_FUNCTION : M_METHOD};
      members.push_back(mem);
    }
  }
  return members;
}

/* ------------------------------------------------------ reading the file */

struct Imports {
  std::map<std::string, int> aliases; // name you type before '.' -> module
  std::vector<int> bare;              // `yok vector;`
};

std::vector<Token> scan(const std::string &text) {
  std::vector<Token> tokens;
  steav_frontend::Scanner scanner(text.c_str());
  while (true) {
    Token t = scanner.scan_token();
    if (t.type == steav_frontend::TOKEN_EOF) break;
    if (t.type != steav_frontend::TOKEN_ERROR) tokens.push_back(t);
  }
  return tokens;
}

std::string lexeme(const Token &t) { return std::string(t.start, t.length); }

Imports imports(const std::vector<Token> &tokens) {
  Imports im;
  for (size_t i = 0; i + 1 < tokens.size(); i++) {
    if (tokens[i].type != steav_frontend::TOKEN_IMPORT) continue;
    int si = steav_stdlib::find_module(tokens[i + 1].start, tokens[i + 1].length);
    if (si < 0) continue;
    if (i + 3 < tokens.size() && tokens[i + 2].type == steav_frontend::TOKEN_AS) {
      im.aliases[lexeme(tokens[i + 3])] = si;
    } else {
      im.bare.push_back(si);
      im.aliases[lexeme(tokens[i + 1])] = si; // `vector` itself shows the module on hover
    }
  }
  return im;
}

// byte offset of an LSP position (ascii-ish: characters = bytes)
size_t offset_of(const std::string &text, int line, int character) {
  size_t at = 0;
  for (int l = 0; l < line && at < text.size(); l++) {
    size_t nl = text.find('\n', at);
    if (nl == std::string::npos) return text.size();
    at = nl + 1;
  }
  return std::min(text.size(), at + character);
}

inline bool is_word(char c) { return isalnum((unsigned char)c) || c == '_'; }

// the word under / right before the cursor, and the one before a '.' ahead of it
void word_at(const std::string &text, size_t at, std::string &word, std::string &qualifier,
             bool &after_dot) {
  size_t start = at;
  while (start > 0 && is_word(text[start - 1])) start--;
  size_t end = at;
  while (end < text.size() && is_word(text[end])) end++;
  word = text.substr(start, end - start);
  after_dot = start > 0 && text[start - 1] == '.';
  qualifier.clear();
  if (after_dot) {
    size_t qend = start - 1;
    size_t qstart = qend;
    while (qstart > 0 && is_word(text[qstart - 1])) qstart--;
    qualifier = text.substr(qstart, qend - qstart);
  }
}

Json markdown(const std::string &value) {
  Json contents = Json::obj();
  contents.set("kind", Json::of("markdown")).set("value", Json::of(value));
  Json hover = Json::obj();
  hover.set("contents", contents);
  return hover;
}

std::string module_doc(int si) {
  std::string doc = std::string("**module `") + steav_stdlib::module_at(si)->name + "`**\n";
  for (const Member &m : module_members(si)) {
    if (m.kind == M_METHOD) continue;
    doc += "\n- `" + m.name + "`";
  }
  return doc;
}

Json item(const std::string &label, int kind, const std::string &detail) {
  Json it = Json::obj();
  it.set("label", Json::of(label)).set("kind", Json::of(kind));
  if (!detail.empty()) it.set("detail", Json::of(detail));
  return it;
}

// LSP CompletionItemKind
int item_kind(MemberKind k) {
  switch (k) {
  case M_FUNCTION: return 3;
  case M_METHOD: return 2;
  case M_CONSTANT: return 21;
  default: return 22; // struct
  }
}

} // namespace

/* ---------------------------------------------------------- the features */

Json diagnostics(const std::string &text) {
  std::vector<SteavDiagnostic> found;
  steav_diagnostic_sink = &found;
  steav_vm::check_source(text.c_str());
  steav_diagnostic_sink = nullptr;

  Json list = Json::arr();
  const char *begin = text.c_str();
  const char *end = begin + text.size();
  for (const SteavDiagnostic &d : found) {
    int line = d.line > 0 ? d.line - 1 : 0;
    int col = 0;
    int length = d.length;
    std::string message = d.message;
    if (d.at != nullptr && d.at >= begin && d.at <= end) {
      const char *line_start = d.at;
      while (line_start > begin && line_start[-1] != '\n') line_start--;
      col = (int)(d.at - line_start);
    } else if (d.at != nullptr) {
      // an error inside a stdlib module, pin it to the top of the file
      line = 0;
      length = 0;
      message = "in a yok'd module: " + message;
    } else {
      length = 1000; // only the line is known, underline all of it
    }
    Json start = Json::obj(), stop = Json::obj(), range = Json::obj(), diag = Json::obj();
    start.set("line", Json::of(line)).set("character", Json::of(col));
    stop.set("line", Json::of(line)).set("character", Json::of(col + (length > 0 ? length : 1)));
    range.set("start", start).set("end", stop);
    diag.set("range", range).set("severity", Json::of(1)).set("source", Json::of("steav2"))
        .set("message", Json::of(message));
    list.push(diag);
  }
  return list;
}

Json hover(const std::string &text, int line, int character) {
  std::string word, qualifier;
  bool after_dot;
  word_at(text, offset_of(text, line, character), word, qualifier, after_dot);
  if (word.empty()) return Json::null();

  if (const Doc *d = find_doc(KEYWORDS, COUNT(KEYWORDS), word))
    return markdown(std::string("**`") + d->word + "`** (" + d->meaning + ")\n\n" + d->text);
  if (const Doc *d = find_doc(TYPES, COUNT(TYPES), word))
    return markdown(std::string("**`") + d->word + "`** (" + d->meaning + ")\n\n" + d->text);

  std::vector<Token> tokens = scan(text);
  Imports im = imports(tokens);

  std::vector<int> search;
  bool on_module = after_dot && im.aliases.count(qualifier) > 0;
  bool on_value = after_dot && !on_module; // `v.length`: a method or field
  if (on_module) {
    search.push_back(im.aliases[qualifier]);
  } else {
    if (!after_dot && im.aliases.count(word)) return markdown(module_doc(im.aliases[word]));
    search = im.bare;
    // `v.length` on a Vec3: look through every imported module's methods
    if (after_dot) {
      for (std::map<std::string, int>::iterator it = im.aliases.begin(); it != im.aliases.end(); ++it)
        search.push_back(it->second);
    }
  }
  std::string found;
  for (int si : search) {
    for (const Member &m : module_members(si)) {
      if (m.name != word) continue;
      if (on_value != (m.kind == M_METHOD)) continue;
      if (found.find(m.signature) != std::string::npos) continue;
      found += (found.empty() ? "" : "\n") + m.signature;
    }
  }
  if (found.empty()) return Json::null();
  return markdown("```steav2\n" + found + "\n```");
}

Json completion(const std::string &text, int line, int character) {
  std::string word, qualifier;
  bool after_dot;
  size_t at = offset_of(text, line, character);
  word_at(text, at, word, qualifier, after_dot);
  std::vector<Token> tokens = scan(text);
  Imports im = imports(tokens);
  Json items = Json::arr();

  if (after_dot) {
    if (im.aliases.count(qualifier)) {
      for (const Member &m : module_members(im.aliases[qualifier])) {
        if (m.kind != M_METHOD) items.push(item(m.name, item_kind(m.kind), m.signature));
      }
      return items;
    }
    // some value: offer fields + methods of the vector types, and arrays'
    const char *fields[] = {"x", "y", "z", "w"};
    for (const char *f : fields) items.push(item(f, 5, "LekThom"));
    std::map<std::string, bool> seen;
    for (std::map<std::string, int>::iterator it = im.aliases.begin(); it != im.aliases.end(); ++it) {
      for (const Member &m : module_members(it->second)) {
        if (m.kind != M_METHOD || seen[m.name]) continue;
        seen[m.name] = true;
        items.push(item(m.name, 2, m.signature));
      }
    }
    items.push(item("len", 2, "[T].len() -> LekKut"));
    items.push(item("push", 2, "[T].push(x: T)"));
    return items;
  }

  for (const Doc &d : KEYWORDS) items.push(item(d.word, 14, d.meaning));
  for (const Doc &d : TYPES) items.push(item(d.word, 22, d.meaning));
  for (std::map<std::string, int>::iterator it = im.aliases.begin(); it != im.aliases.end(); ++it)
    items.push(item(it->first, 9, std::string("module ") + steav_stdlib::module_at(it->second)->name));
  for (int si : im.bare) {
    for (const Member &m : module_members(si)) {
      if (m.kind != M_METHOD) items.push(item(m.name, item_kind(m.kind), m.signature));
    }
  }
  // names declared in this file
  std::map<std::string, bool> seen;
  for (size_t i = 0; i + 1 < tokens.size(); i++) {
    int kind = 0;
    switch (tokens[i].type) {
    case steav_frontend::TOKEN_FUN: kind = 3; break;
    case steav_frontend::TOKEN_STRUCT: kind = 22; break;
    case steav_frontend::TOKEN_CLASS: kind = 7; break;
    case steav_frontend::TOKEN_VAR: kind = 6; break;
    default: break;
    }
    if (kind == 0 || tokens[i + 1].type != steav_frontend::TOKEN_IDENTIFIER) continue;
    std::string name = lexeme(tokens[i + 1]);
    if (seen[name]) continue;
    seen[name] = true;
    items.push(item(name, kind, ""));
  }
  return items;
}

} // namespace steav_lsp
