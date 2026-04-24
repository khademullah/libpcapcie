#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pcapcie/pcapcie.h"
#include "pcapcie/backend.h"
#include <stdarg.h>
#include <time.h>

struct sniff_entry {
    pcie_filter_t filter;
    pcie_cb_t cb;
    void *user;
};

struct pcie_ctx {
    const pcie_backend_ops_t *backend;
    struct sniff_entry sniffers[8];
    int sniffer_count;
    uint64_t rx_addr_min;
    uint64_t rx_addr_max;

    pcie_stats_t stats;
};

pcie_ctx_t *pcie_open(const char *backend_name)
{
    pcie_log(PCIE_LOG_INFO,
         "Opening PCIe backend '%s'", backend_name);
    pcie_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        pcie_log(PCIE_LOG_ERROR, "Failed to allocate context");
        return NULL;
    }
    ctx->rx_addr_min = UINT64_MAX;
    ctx->rx_addr_max = 0;

    // Select backend based on name
    if (strcmp(backend_name, "dummy") == 0) {
        ctx->backend = pcie_backend_dummy();
    } else if (strcmp(backend_name, "fpga") == 0) {
        ctx->backend = pcie_backend_fpga();
    } else if (strcmp(backend_name, "armds") == 0) {
        ctx->backend = pcie_backend_armds();
    } else if (strcmp(backend_name, "xgig") == 0) {
        ctx->backend = pcie_backend_xgig();
    } else if (strcmp(backend_name, "pci") == 0) {
        ctx->backend = pcie_backend_pci();
    } else {
        pcie_log(PCIE_LOG_ERROR, "Unknown backend '%s'. Supported: dummy, fpga, armds, xgig, pci", backend_name);
        free(ctx);
        return NULL;
    }

    if (!ctx->backend) {
        pcie_log(PCIE_LOG_ERROR, "Failed to initialize backend '%s'", backend_name);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void pcie_close(pcie_ctx_t *ctx)
{
    pcie_log(PCIE_LOG_INFO,
         "Closing PCIe backend");
    ctx->backend->close();
    free(ctx);
}

int pcie_send(pcie_ctx_t *ctx, const pcie_tlp_t *t)
{
    ctx->stats.sent++;

    printf("pcie_send: ctx=%p, t=%p, backend=%p\n", (void*)ctx, (void*)t, (void*)ctx->backend);
    if (ctx->backend && ctx->backend->send) {
        printf("pcie_send: calling backend send function at %p\n", (void*)ctx->backend->send);
        return ctx->backend->send(t);
    } else {
        printf("pcie_send: ERROR - backend or send function is NULL\n");
        return -1;
    }
}

int pcie_recv(pcie_ctx_t *ctx, pcie_tlp_t *t)
{
    int result = ctx->backend->recv(t);
    if (result == 0) {
        ctx->stats.received++;
    }
    return result;
}

int pcie_sniff(pcie_ctx_t *ctx,
               pcie_filter_t filter,
               pcie_cb_t cb,
               void *user)
{
    int i = ctx->sniffer_count++;
    ctx->sniffers[i].filter = filter;
    ctx->sniffers[i].cb = cb;
    ctx->sniffers[i].user = user;
    return 0;
}

void pcie_loop(pcie_ctx_t *ctx)
{
    pcie_tlp_t t;

    while (ctx->backend->recv(&t) == 0) {
        
        ctx->stats.received++;

        if (ctx->stats.received == 1) {
            pcie_log(PCIE_LOG_INFO,
                     "RX started (first TLP type=%d addr=0x%lx)",
                     t.type,
                     t.mem.addr);
        }

        if ((ctx->stats.received % 100) == 0) {
            pcie_log(PCIE_LOG_DEBUG,
                     "RX %lu packets (last addr=0x%lx)",
                     ctx->stats.received,
                     t.mem.addr);
        }

        for (int i = 0; i < ctx->sniffer_count; i++) {
            if (pcie_filter_match(&ctx->sniffers[i].filter, &t)) {
                ctx->sniffers[i].cb(&t,
                                    ctx->sniffers[i].user);
            }
        }
    }

    pcie_log(PCIE_LOG_INFO,
             "RX stopped after %lu packets",
             ctx->stats.received);
}

const pcie_stats_t *pcie_stats(pcie_ctx_t *ctx)
{
    return &ctx->stats;
}

void pcie_log(pcie_log_level_t lvl, const char *fmt, ...)
{
    static const char *lvl_str[] = {
        "INFO", "WARN", "ERR", "DBG"
    };

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    printf("[%ld.%06ld] %-4s ",
           ts.tv_sec,
           ts.tv_nsec / 1000,
           lvl_str[lvl]);

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
}


uint64_t pcie_rx_addr_min(pcie_ctx_t *ctx)
{
    return ctx->rx_addr_min;
}

uint64_t pcie_rx_addr_max(pcie_ctx_t *ctx)
{
    return ctx->rx_addr_max;
}
