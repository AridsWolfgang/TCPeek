// ============================================================================
// utils.h — Public interface for utility functions
// ============================================================================

#ifndef UTILS_H
#define UTILS_H

#include "config.h"

// Formats an integer with thousands separators into a bounded buffer.
// Example: fmt_int(buf, 12345, 16) writes "12,345" into buf.
void fmt_int(char *buf, int n, int sz);

// Returns the common service name for a well-known TCP port (e.g. 22 -> "SSH").
// Returns an empty string for unrecognised ports.
const char *get_service(int port);

#endif // UTILS_H
