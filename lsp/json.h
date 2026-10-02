#ifndef STEAV_LSP_JSON_H
#define STEAV_LSP_JSON_H

/* just enough JSON for LSP messages: parse a message, poke at it, write
 * one back. no dependencies on purpose */

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace steav_lsp {

struct Json {
  enum Kind { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };
  Kind kind = NUL;
  bool boolean = false;
  double number = 0;
  std::string string;
  std::vector<Json> array;
  std::map<std::string, Json> object;

  static Json null() { return Json(); }
  static Json of(bool b) {
    Json j;
    j.kind = BOOL;
    j.boolean = b;
    return j;
  }
  static Json of(double n) {
    Json j;
    j.kind = NUMBER;
    j.number = n;
    return j;
  }
  static Json of(int n) { return of((double)n); }
  static Json of(const std::string &s) {
    Json j;
    j.kind = STRING;
    j.string = s;
    return j;
  }
  static Json of(const char *s) { return of(std::string(s)); }
  static Json arr() {
    Json j;
    j.kind = ARRAY;
    return j;
  }
  static Json obj() {
    Json j;
    j.kind = OBJECT;
    return j;
  }

  // missing keys / wrong kinds read as null, so lookups can chain
  const Json &operator[](const std::string &key) const {
    static const Json none;
    if (kind != OBJECT) return none;
    std::map<std::string, Json>::const_iterator it = object.find(key);
    return it == object.end() ? none : it->second;
  }
  Json &set(const std::string &key, const Json &value) {
    this->kind = OBJECT;
    object[key] = value;
    return *this;
  }
  Json &push(const Json &value) {
    this->kind = ARRAY;
    array.push_back(value);
    return *this;
  }
  int as_int() const { return (int)number; }

  std::string dump() const {
    std::string out;
    _dump(out);
    return out;
  }

  // returns false on malformed input
  static bool parse(const std::string &text, Json &out) {
    size_t at = 0;
    return _parse(text, at, out);
  }

private:
  static void _dump_string(const std::string &s, std::string &out) {
    out += '"';
    for (size_t i = 0; i < s.size(); i++) {
      unsigned char c = (unsigned char)s[i];
      switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += (char)c;
        }
      }
    }
    out += '"';
  }

  void _dump(std::string &out) const {
    switch (kind) {
    case NUL: out += "null"; break;
    case BOOL: out += boolean ? "true" : "false"; break;
    case NUMBER: {
      char buf[32];
      if (number == (long long)number)
        snprintf(buf, sizeof(buf), "%lld", (long long)number);
      else
        snprintf(buf, sizeof(buf), "%.17g", number);
      out += buf;
      break;
    }
    case STRING: _dump_string(string, out); break;
    case ARRAY:
      out += '[';
      for (size_t i = 0; i < array.size(); i++) {
        if (i > 0) out += ',';
        array[i]._dump(out);
      }
      out += ']';
      break;
    case OBJECT: {
      out += '{';
      bool first = true;
      for (std::map<std::string, Json>::const_iterator it = object.begin();
           it != object.end(); ++it) {
        if (!first) out += ',';
        first = false;
        _dump_string(it->first, out);
        out += ':';
        it->second._dump(out);
      }
      out += '}';
      break;
    }
    }
  }

  static void _skip(const std::string &t, size_t &at) {
    while (at < t.size() && (t[at] == ' ' || t[at] == '\n' || t[at] == '\r' || t[at] == '\t'))
      at++;
  }

  static void _utf8(unsigned cp, std::string &out) {
    if (cp < 0x80) {
      out += (char)cp;
    } else if (cp < 0x800) {
      out += (char)(0xc0 | (cp >> 6));
      out += (char)(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
      out += (char)(0xe0 | (cp >> 12));
      out += (char)(0x80 | ((cp >> 6) & 0x3f));
      out += (char)(0x80 | (cp & 0x3f));
    } else {
      out += (char)(0xf0 | (cp >> 18));
      out += (char)(0x80 | ((cp >> 12) & 0x3f));
      out += (char)(0x80 | ((cp >> 6) & 0x3f));
      out += (char)(0x80 | (cp & 0x3f));
    }
  }

  static bool _parse_string(const std::string &t, size_t &at, std::string &out) {
    if (t[at] != '"') return false;
    at++;
    while (at < t.size() && t[at] != '"') {
      char c = t[at++];
      if (c != '\\') {
        out += c;
        continue;
      }
      if (at >= t.size()) return false;
      char e = t[at++];
      switch (e) {
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'u': {
        if (at + 4 > t.size()) return false;
        unsigned cp = (unsigned)strtoul(t.substr(at, 4).c_str(), nullptr, 16);
        at += 4;
        // surrogate pair
        if (cp >= 0xd800 && cp < 0xdc00 && at + 6 <= t.size() && t[at] == '\\' &&
            t[at + 1] == 'u') {
          unsigned lo = (unsigned)strtoul(t.substr(at + 2, 4).c_str(), nullptr, 16);
          at += 6;
          cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
        }
        _utf8(cp, out);
        break;
      }
      default: out += e; break; // \" \\ \/
      }
    }
    if (at >= t.size()) return false;
    at++;
    return true;
  }

  static bool _parse(const std::string &t, size_t &at, Json &out) {
    _skip(t, at);
    if (at >= t.size()) return false;
    char c = t[at];
    if (c == '{') {
      out = obj();
      at++;
      _skip(t, at);
      if (at < t.size() && t[at] == '}') {
        at++;
        return true;
      }
      while (true) {
        _skip(t, at);
        std::string key;
        if (at >= t.size() || !_parse_string(t, at, key)) return false;
        _skip(t, at);
        if (at >= t.size() || t[at] != ':') return false;
        at++;
        Json value;
        if (!_parse(t, at, value)) return false;
        out.object[key] = value;
        _skip(t, at);
        if (at < t.size() && t[at] == ',') {
          at++;
          continue;
        }
        if (at < t.size() && t[at] == '}') {
          at++;
          return true;
        }
        return false;
      }
    }
    if (c == '[') {
      out = arr();
      at++;
      _skip(t, at);
      if (at < t.size() && t[at] == ']') {
        at++;
        return true;
      }
      while (true) {
        Json value;
        if (!_parse(t, at, value)) return false;
        out.array.push_back(value);
        _skip(t, at);
        if (at < t.size() && t[at] == ',') {
          at++;
          continue;
        }
        if (at < t.size() && t[at] == ']') {
          at++;
          return true;
        }
        return false;
      }
    }
    if (c == '"') {
      out = of("");
      return _parse_string(t, at, out.string);
    }
    if (t.compare(at, 4, "true") == 0) {
      at += 4;
      out = of(true);
      return true;
    }
    if (t.compare(at, 5, "false") == 0) {
      at += 5;
      out = of(false);
      return true;
    }
    if (t.compare(at, 4, "null") == 0) {
      at += 4;
      out = null();
      return true;
    }
    char *end = nullptr;
    double n = strtod(t.c_str() + at, &end);
    if (end == t.c_str() + at) return false;
    at = end - t.c_str();
    out = of(n);
    return true;
  }
};

} // namespace steav_lsp
#endif // STEAV_LSP_JSON_H
