#include <stdio.h>
#include <string.h>
#include "pcapcie/backend.h"
#include "pcapcie/pcapcie.h"   // for pcie_log()

static int dummy_send(const pcie_tlp_t *t)
{
     pcie_log(PCIE_LOG_DEBUG,
             "[dummy] TX TLP type=%d len=%u",
             t->type, t->length);
    return 0;
}

static int dummy_recv(pcie_tlp_t *t)
{
    static int count = 0;

    if (count >= 1000)
        return -1;  // stop loop

    memset(t, 0, sizeof(*t));
    t->type = PCIE_TLP_MEM_READ;
    t->length = 4;
    t->mem.addr = 0x1000 + count * 4;
    
    count++;
    
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
