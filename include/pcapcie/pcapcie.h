#pragma once
#include "tlp.h"
#include "filter.h"
#include "backend.h"

typedef struct pcie_ctx pcie_ctx_t;

typedef void (*pcie_cb_t)(pcie_tlp_t *t, void *user);

pcie_ctx_t *pcie_open(const char *backend_name);
void pcie_close(pcie_ctx_t *ctx);

int pcie_send(pcie_ctx_t *ctx, const pcie_tlp_t *t);

int pcie_sniff(pcie_ctx_t *ctx,
               pcie_filter_t filter,
               pcie_cb_t cb,
               void *user);

void pcie_loop(pcie_ctx_t *ctx);