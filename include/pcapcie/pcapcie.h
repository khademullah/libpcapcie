#pragma once
#include "tlp.h"
#include "filter.h"
#include "backend.h"

typedef struct pcie_ctx pcie_ctx_t;

typedef void (*pcie_cb_t)(pcie_tlp_t *t, void *user);

pcie_ctx_t *pcie_open(const char *backend_name);
void pcie_close(pcie_ctx_t *ctx);

int pcie_send(pcie_ctx_t *ctx, const pcie_tlp_t *t);
int pcie_recv(pcie_ctx_t *ctx, pcie_tlp_t *t);

int pcie_sniff(pcie_ctx_t *ctx,
               pcie_filter_t filter,
               pcie_cb_t cb,
               void *user);

void pcie_loop(pcie_ctx_t *ctx);

typedef struct {
    unsigned long sent;
    unsigned long received;
} pcie_stats_t;

typedef enum {
    PCIE_LOG_INFO,
    PCIE_LOG_WARN,
    PCIE_LOG_ERROR,
    PCIE_LOG_DEBUG
} pcie_log_level_t;

void pcie_log(pcie_log_level_t lvl, const char *fmt, ...);

const pcie_stats_t *pcie_stats(pcie_ctx_t *ctx);

uint64_t pcie_rx_addr_min(pcie_ctx_t *ctx);
uint64_t pcie_rx_addr_max(pcie_ctx_t *ctx);
