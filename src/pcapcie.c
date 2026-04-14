#include <stdlib.h>
#include "pcapcie/pcapcie.h"

struct sniff_entry {
    pcie_filter_t filter;
    pcie_cb_t cb;
    void *user;
};

struct pcie_ctx {
    const pcie_backend_ops_t *backend;
    struct sniff_entry sniffers[8];
    int sniffer_count;
};

pcie_ctx_t *pcie_open(const char *backend_name)
{
    pcie_ctx_t *ctx = calloc(1, sizeof(*ctx));
    ctx->backend = pcie_backend_dummy();
    return ctx;
}

void pcie_close(pcie_ctx_t *ctx)
{
    ctx->backend->close();
    free(ctx);
}

int pcie_send(pcie_ctx_t *ctx, const pcie_tlp_t *t)
{
    return ctx->backend->send(t);
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
        for (int i = 0; i < ctx->sniffer_count; i++) {
            if (pcie_filter_match(&ctx->sniffers[i].filter, &t)) {
                ctx->sniffers[i].cb(&t,
                                    ctx->sniffers[i].user);
            }
        }
    }
}