#ifndef STEAV_AST_AST_H
#define STEAV_AST_AST_H

#include "frontend/token.h"
#include <deque>
#include <string>
#include <vector>

namespace steav_ast {

// dirname("a/b/c.sts") = "a/b", dirname("c.sts") = "."; forward slashes
// only, `yok "..."` paths are source syntax, not host paths
inline std::string path_dirname(const std::string &path) {
  size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) return ".";
  if (slash == 0) return "/";
  return path.substr(0, slash);
}

// collapses "." / ".." segments so the same file always normalizes to the
// same string no matter which relative spelling reached it
inline std::string path_normalize(const std::string &path) {
  bool absolute = !path.empty() && path[0] == '/';
  std::vector<std::string> parts;
  size_t i = 0;
  while (i <= path.size()) {
    size_t next = path.find('/', i);
    if (next == std::string::npos) next = path.size();
    std::string seg = path.substr(i, next - i);
    if (!seg.empty() && seg != ".") {
      if (seg == "..") {
        if (!parts.empty() && parts.back() != "..") parts.pop_back();
        else if (!absolute) parts.push_back("..");
        // ".." past an absolute root just stays at root
      } else {
        parts.push_back(seg);
      }
    }
    i = next + 1;
  }
  std::string out = absolute ? "/" : "";
  for (size_t k = 0; k < parts.size(); k++) {
    if (k > 0) out += "/";
    out += parts[k];
  }
  if (out.empty()) out = absolute ? "/" : ".";
  return out;
}

inline std::string path_join(const std::string &dir, const std::string &rel) {
  if (!rel.empty() && rel[0] == '/') return path_normalize(rel);
  if (dir.empty() || dir == ".") return path_normalize(rel);
  return path_normalize(dir + "/" + rel);
}

/* everything points at everything else by index into Ast, -1 = none
 * (same idea as keo's BVH nodes / prims) */

enum TypeRefKind { TYPEREF_NAMED, TYPEREF_ARRAY };

// type → "[" type "]" "?"? | ( IDENTIFIER "." )? IDENTIFIER "?"?
struct TypeRef {
  TypeRefKind kind;
  steav_frontend::Token name;      // TYPEREF_NAMED: Lek, Vec3, Sphere...
  steav_frontend::Token qualifier; // `vec` in vec.Vec3
  bool has_qualifier = false;
  int elem = -1;         // TYPEREF_ARRAY: index into Ast::types
  bool optional = false; // trailing ?
};

enum ExprKind {
  EXPR_ASSIGN,   // target ("=" | "+=" ...) assignment, token = the op
  EXPR_LOGICAL,  // ng / reu
  EXPR_BINARY,   // + - * / == != < <= > >=, overloads resolved later
  EXPR_UNARY,    // - !
  EXPR_CALL,     // callee(args), sampoan / tnak / rupamun all look like this
  EXPR_GET,      // object.name
  EXPR_INDEX,    // object[index], arrays + same-typed sampoans (v[0])
  EXPR_LITERAL,  // NUMBER STRING ok ort sone
  EXPR_GROUPING, // ( expression )
  EXPR_VARIABLE, // IDENTIFIER
  EXPR_THIS,     // nis
  EXPR_ARRAY     // [a, b, c]
};

struct Expr {
  ExprKind kind;
  steav_frontend::Token token; // operator / name / literal / paren
  int left = -1;  // binary + logical lhs, call callee, get/index object,
                  // assign target
  int right = -1; // binary + logical rhs, unary operand, assign value, index
  std::vector<int> args; // call arguments, array elements
};

enum StmtKind {
  STMT_EXPR,
  STMT_PRINT,  // jongyeytha
  STMT_BLOCK,  // { }
  STMT_IF,     // ber / minjengte
  STMT_WHILE,  // nvpeldae
  STMT_FOR,    // somhab
  STMT_RETURN, // morvenh
  STMT_IMPORT  // yok vector jea vec;
};

struct Stmt {
  StmtKind kind;
  steav_frontend::Token keyword;
  steav_frontend::Token name;  // STMT_IMPORT module
  steav_frontend::Token alias; // STMT_IMPORT jea alias
  bool has_alias = false;
  int expr = -1;         // expr / print / return value / condition
  int then_branch = -1;  // if, while + for body (index into decls)
  int else_branch = -1;  // minjengte
  int initializer = -1;  // for (index into decls)
  int increment = -1;    // for (index into exprs)
  std::vector<int> body; // block (indices into decls)
};

struct Param {
  steav_frontend::Token name;
  int type = -1; // index into Ast::types
};

enum DeclKind { DECL_STRUCT, DECL_CLASS, DECL_FUN, DECL_VAR, DECL_STMT };

enum FunNameKind { FUN_NAMED, FUN_OPERATOR, FUN_INIT };

struct Decl {
  DeclKind kind;
  steav_frontend::Token name; // type / fun / var name, or the operator token
  int module = 0;             // index into Ast::modules

  // DECL_STRUCT + DECL_CLASS (typeBody)
  std::vector<int> fields;  // DECL_VAR indices
  std::vector<int> methods; // DECL_FUN indices

  // DECL_FUN
  FunNameKind fun_name_kind = FUN_NAMED;
  std::vector<Param> params;
  int return_type = -1; // -1 = no `->`, means OrtMean
  std::vector<int> body; // decl indices
  bool is_native = false; // `rupamun f(...) -> T;`, body is C++ (stdlib only)
  int owner = -1;        // method: the sampoan / tnak decl it belongs to

  // DECL_VAR (fieldDecl reuses this with no initializer)
  int var_type = -1;    // -1 = infer from initializer
  int initializer = -1; // index into exprs

  // DECL_STMT
  int stmt = -1;
};

/* the main script is module 0, every `yok`ed .sts module gets parsed into
 * the same Ast under its own index. native modules (math, random) get an
 * entry too, just with no program */
struct Module {
  // native/stdlib: the short name (e.g. "vector"); a file import: its
  // path, normalized relative to the importing module's own `dir` — so
  // the same file always dedupes to one module no matter which of its
  // importers' relative spellings reached it first
  std::string name;
  bool is_native = false; // math/random: no .sts source at all
  bool is_stdlib = false; // vector: .sts text baked into the binary, may declare native rupamuns
  bool is_file = false;   // a user's own `yok "path/to/file.sts";`
  std::string dir;        // is_file: the dir its own relative `yok`s resolve against
  std::vector<int> program; // top level decls, in source order
};

struct Ast {
  std::vector<Expr> exprs;
  std::vector<Stmt> stmts;
  std::vector<Decl> decls;
  std::vector<TypeRef> types;
  std::vector<Module> modules;
  std::vector<int> module_order; // dependencies first, main script last

  // owns the text of every `yok`ed file module: a deque, not a vector, so
  // a later push_back can never invalidate an earlier module's tokens
  // (which point straight into this text, same as the caller-owned main
  // script source does for the whole compile)
  std::deque<std::string> file_sources;

  inline int add_expr(const Expr &e) {
    exprs.push_back(e);
    return (int)exprs.size() - 1;
  }
  inline int add_stmt(const Stmt &s) {
    stmts.push_back(s);
    return (int)stmts.size() - 1;
  }
  inline int add_decl(const Decl &d) {
    decls.push_back(d);
    return (int)decls.size() - 1;
  }
  inline int add_type(const TypeRef &t) {
    types.push_back(t);
    return (int)types.size() - 1;
  }
  inline int find_module(const char *name, int length) const {
    for (int i = 0; i < (int)modules.size(); i++) {
      if ((int)modules[i].name.size() == length &&
          modules[i].name.compare(0, length, name, length) == 0)
        return i;
    }
    return -1;
  }

  // what a `yok` target resolves to for find_module(): the literal name
  // for a native/stdlib module, or `name` normalized against the
  // importing module's own `dir` for a `yok "path/to/file.sts";`
  inline std::string resolve_import(int importer_module,
                                     const steav_frontend::Token &name) const {
    if (name.type != steav_frontend::TOKEN_STRING)
      return std::string(name.start, name.length);
    // the scanner keeps the surrounding quotes, strip them
    std::string rel(name.start + 1, (size_t)(name.length - 2));
    return path_join(modules[importer_module].dir, rel);
  }
};

} // namespace steav_ast
#endif // STEAV_AST_AST_H
