#include <stdio.h>
#include <string.h>
#include "pcapcie/backend.h"

static int dummy_send(const pcie_tlp_t *t)
{
    printf("[dummy] send TLP type=%d len=%u\n",
           t->type, t->length);
    return 0;
}

static int dummy_recv(pcie_tlp_t *t)
{
    static int once = 0;
    if (once++)
        return -1;

    memset(t, 0, sizeof(*t));
    t->type = PCIE_TLP_MEM_READ;
    t->length = 4;
    t->mem.addr = 0xdeadbeef;
    return 0;
}

static void dummy_close(void)
{
    printf("[dummy] close\n");
}

static const pcie_backend_ops_t ops = {
    .send  = dummy_send,
    .recv  = dummy_recv,
    .close = dummy_close
};

const pcie_backend_ops_t *pcie_backend_dummy(void)
{
    return &ops;
}