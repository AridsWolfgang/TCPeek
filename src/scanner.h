// ============================================================================
// scanner.h — Public interface for the port scanning engine
// ============================================================================

#ifndef SCANNER_H
#define SCANNER_H

#include "config.h"

int  scanner_init(void);
void scanner_cleanup(void);
int  scanner_resolve(const char *hostname);
void scanner_setup_range(int start, int end);
void scanner_start_workers(void);
void scanner_wait_for_completion(void);
void scanner_free(void);

// Worker entry point (void (*)(void*) signature for plat_thread_spawn)
void worker_thread(void *arg);
port_status_t scan_port(int port);

#endif // SCANNER_H
