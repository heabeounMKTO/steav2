#ifndef STEAV_LOGGING_DIAGNOSTICS_H
#define STEAV_LOGGING_DIAGNOSTICS_H

#include <string>
#include <vector>

/* a compile error as data. `at` points into the source that was checked
 * (nullptr = only the line is known), so a tool can work out the column */
struct SteavDiagnostic {
  int line;
  const char *at;
  int length;
  std::string message;
};

/* nullptr (the default) = errors get logged to stderr. a tool (the lsp)
 * points this at a vector to collect them instead */
extern std::vector<SteavDiagnostic> *steav_diagnostic_sink;

// true = it went into the sink, the caller shouldn't log it too
static inline bool steav_capture_diagnostic(int line, const char *at, int length,
                                            const std::string &message) {
  if (steav_diagnostic_sink == nullptr) return false;
  SteavDiagnostic d = {line, at, length, message};
  steav_diagnostic_sink->push_back(d);
  return true;
}

#endif // STEAV_LOGGING_DIAGNOSTICS_H
