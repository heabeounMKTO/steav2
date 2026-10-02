/* essentially just stealing smolcore's logging header if its not broken then
 * dont fix it duh */

#ifndef STEAV_LOGGING_H
#define STEAV_LOGGING_H

#include <stdio.h>
// ANSI Color Codes
#define STEAV_LOGGING_COLOR_RESET "\033[0m"
#define STEAV_LOGGING_COLOR_GREEN "\033[32m"
#define STEAV_LOGGING_COLOR_RED "\033[31m"
#define STEAV_LOGGING_COLOR_YELLOW "\033[33m"
#define STEAV_LOGGING_COLOR_BLUE "\033[34m"
#define STEAV_LOGGING_COLOR_CYAN "\033[36m"
#define STEAV_LOGGING_COLOR_BOLD "\033[1m"
#define STEAV_LOGGING_COLOR_DIM "\033[2m"

// Emojis/Icons
#define STEAV_LOGGING_ICON_SUCCESS "✓"
#define STEAV_LOGGING_ICON_ERROR "✗"
#define STEAV_LOGGING_ICON_INFO "ℹ"

#define STEAV_LOGGING_LOG(...)                                                   \
  SteavLoggingPrintGenericLog(__PRETTY_FUNCTION__, __VA_ARGS__)

enum SteavStatus {
  STEAV_LOGGING_OK,
  STEAV_LOGGING_FILE_NOT_FOUND,
  STEAV_LOGGING_EMPTY_RESULTS,
  STEAV_LOGGING_LOADING_FAILED,
  STEAV_LOGGING_PARSING_ERROR,
  STEAV_LOGGING_UNKNOWN,
  STEAV_LOGGING_INSUFFICIENT_MEMORY,
  STEAV_LOGGING_COMPILE_ERROR,
  STEAV_LOGGING_RUNTIME_ERROR
};

static inline const char *SteavGetStatus(SteavStatus status) {
  switch (status) {
  case STEAV_LOGGING_OK:
    return "OK";
  case STEAV_LOGGING_FILE_NOT_FOUND:
    return "File not found";
  case STEAV_LOGGING_LOADING_FAILED:
    return "Loading failed";
  case STEAV_LOGGING_PARSING_ERROR:
    return "Parsing error";
  case STEAV_LOGGING_EMPTY_RESULTS:
    return "Empty results";
  case STEAV_LOGGING_INSUFFICIENT_MEMORY:
    return "Insufficient memory";

  case STEAV_LOGGING_COMPILE_ERROR:
    return "Compile error";
  case STEAV_LOGGING_RUNTIME_ERROR:
    return "Runtime error";
  default:
    return "Invalid status code";
  }
}

static inline void SteavLoggingPrintGenericLog(const char *context,
                                             const char *message) {
  fprintf(
      stdout,
      "%s%s%s %s[steav2]%s %s%s INFO:%s\n"
      "  %s└─%s %s%s%s\n"
      "  %s└─%s %s**Message:**%s %s\n",
      STEAV_LOGGING_COLOR_CYAN, STEAV_LOGGING_ICON_INFO, STEAV_LOGGING_COLOR_RESET,
      STEAV_LOGGING_COLOR_BOLD, STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_CYAN,
      STEAV_LOGGING_COLOR_BOLD, STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_DIM,
      STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_BLUE, context,
      STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_DIM, STEAV_LOGGING_COLOR_RESET,
      STEAV_LOGGING_COLOR_CYAN, STEAV_LOGGING_COLOR_RESET, message);
}

static inline void SteavLoggingPrintGenericLog(const char *context,
                                             const char *message,
                                             const SteavStatus status) {
  if (status == STEAV_LOGGING_OK) {
    fprintf(
        stdout,
        "%s%s%s %s[steav2]%s %s%s SUCCESS:%s\n"
        "  %s└─%s %s%s%s\n"
        "  %s└─%s %s**Message:**%s %s\n"
        "  %s└─%s %s**Status:**%s %s%s%s\n",
        STEAV_LOGGING_COLOR_GREEN, STEAV_LOGGING_ICON_SUCCESS,
        STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_BOLD,
        STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_GREEN,
        STEAV_LOGGING_COLOR_BOLD, STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_DIM,
        STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_BLUE, context,
        STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_DIM, STEAV_LOGGING_COLOR_RESET,
        STEAV_LOGGING_COLOR_CYAN, STEAV_LOGGING_COLOR_RESET, message,
        STEAV_LOGGING_COLOR_DIM, STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_CYAN,
        STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_GREEN,
        SteavGetStatus(status), STEAV_LOGGING_COLOR_RESET);
  } else {
    fprintf(
        stderr,
        "%s%s%s %s[steav2]%s %s%s ERROR:%s\n"
        "  %s└─%s %s%s%s\n"
        "  %s└─%s %s**Message:**%s %s\n"
        "  %s└─%s %s**Status:**%s %s%s%s\n",
        STEAV_LOGGING_COLOR_RED, STEAV_LOGGING_ICON_ERROR, STEAV_LOGGING_COLOR_RESET,
        STEAV_LOGGING_COLOR_BOLD, STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_RED,
        STEAV_LOGGING_COLOR_BOLD, STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_DIM,
        STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_BLUE, context,
        STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_DIM, STEAV_LOGGING_COLOR_RESET,
        STEAV_LOGGING_COLOR_CYAN, STEAV_LOGGING_COLOR_RESET, message,
        STEAV_LOGGING_COLOR_DIM, STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_CYAN,
        STEAV_LOGGING_COLOR_RESET, STEAV_LOGGING_COLOR_RED,
        SteavGetStatus(status), STEAV_LOGGING_COLOR_RESET);
  }
}

#endif // STEAV_LOGGING_H
